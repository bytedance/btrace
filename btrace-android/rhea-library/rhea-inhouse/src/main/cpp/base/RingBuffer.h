/*
 * Copyright (C) 2021 ByteDance Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <atomic>
#include <iterator>
#include <type_traits>
#include <algorithm>
#include <sys/mman.h>

namespace rheatrace {

template<typename T>
using GetTimeFn = uint64_t (*)(T&);

namespace {
//
// Constructs n objects of type typename iterator_traits<ForwardIt>::value_type
// in the uninitialized storage starting at first. As
// std::uninitialized_default_construct_n symbol is not yet available for our
// Android builds using this analogous implementation.
//
template <class ForwardIt, class Size>
ForwardIt _uninitialized_default_construct_n(ForwardIt first, Size n) {
    using T = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = first;
    for (; n > 0; (void)++current, --n) {
        new (static_cast<void*>(std::addressof(*current))) T;
    }
    return current;
}

//
// Destruction of flexible array member
//
template <class ForwardIt, class Size>
ForwardIt _destroy_n(ForwardIt first, Size n) {
    using T = typename std::iterator_traits<ForwardIt>::value_type;
    for (; n > 0; (void)++first, --n)
        std::addressof(*first)->~T();
    return first;
}

template<typename T, bool kFat>
class Slot {};

template<typename T>
class Slot<T, false> {
public:
    T data;

    T& ref() {
        return data;
    }

    void write(int64_t newTicket, T& value) {
        memcpy(&data, &value, sizeof(T));
    }

    bool read(T& dest) {
        memcpy(&dest, &data, sizeof(T));
        return true;
    }

    void atomicWrite(int64_t newTicket, T& value) {
        reinterpret_cast<std::atomic<T>*>(&data)->store(value, std::memory_order_release);
    }

    bool atomicTryWrite(int64_t expectedTicket, int64_t newTicket, T& value, GetTimeFn<T> getTime) {
        std::atomic<T>* atomicTicket = reinterpret_cast<std::atomic<T>*>(&data);
        T curValue = atomicTicket->load(std::memory_order_acquire);
        if (getTime(value) > getTime(curValue)) {
            return std::atomic_compare_exchange_weak_explicit(atomicTicket, &curValue, value,
                                                              std::memory_order_release,
                                                              std::memory_order_relaxed);
        }
        return false;
    }
};

template<typename T>
class Slot<T, true> {
public:
    std::atomic_int64_t preTicket;
    T data;
    std::atomic_int64_t postTicket;

    T& ref() {
        return data;
    }

    void write(int64_t newTicket, T& value) {
        preTicket.store(newTicket, std::memory_order_release);
        memcpy(&data, &value, sizeof(T));
        postTicket.store(newTicket, std::memory_order_release);
    }

    bool read(T& dest) {
        int64_t ticketPre = preTicket.load(std::memory_order_acquire);
        int64_t ticketPost = postTicket.load(std::memory_order_acquire);
        if (ticketPre == ticketPost) {
            memcpy(&dest, &data, sizeof(T));
            return true;
        }
        return false;
    }

    void atomicWrite(int64_t newTicket, T& value) {
        write(newTicket, value);
    }

    bool atomicTryWrite(int64_t expectedTicket, int64_t newTicket, T& value, GetTimeFn<T> getTime) {
        bool result = std::atomic_compare_exchange_weak_explicit(&preTicket, &expectedTicket,
                                                                 newTicket,
                                                                 std::memory_order_release,
                                                                 std::memory_order_relaxed);
        if (result) {
            memcpy(&data, &value, sizeof(T));
            postTicket.store(newTicket, std::memory_order_release);
        }
        return result;
    }
};

template<typename T>
struct IsFatType {
    static constexpr bool value = sizeof(T) > 8;
};

} // namespace

template<typename T>
class PerfBuffer;

/**
 * A loosely time sorted ring buffer which supports time based binary search.
 */
template<typename T, bool kFat = IsFatType<T>::value>
class RingBuffer {
    static_assert(
            std::is_nothrow_default_constructible<T>::value,
            "Element type must be nothrow default constructable");

    static_assert(
            std::is_trivially_copyable<T>::value,
            "Element type must be trivially copyable");
private:
    const uint32_t mCapacity;
    std::atomic<int64_t>& mTicket;
    GetTimeFn<T> mGetTimeFn;
    bool mInConcurrentSafeMode;
    Slot<T, kFat> mSlots[];
public:
    static constexpr size_t calculateAllocationSize(size_t entryCount) {
        return sizeof(RingBuffer<T>) +
               entryCount * sizeof(Slot<T, kFat>);
    }

    RingBuffer() = delete;
    RingBuffer(RingBuffer<T> const &) = delete;
    RingBuffer<T> &operator=(RingBuffer<T> const &) = delete;

    uint32_t capacity() {
        return mCapacity;
    }

    uint32_t availableCount(int64_t currentTicket) {
        return uint32_t(std::min(int64_t(mCapacity), currentTicket));
    }

    int64_t getCurrentTicket() {
        return mTicket.load(std::memory_order_relaxed);
    }

    T& acquire() {
        return mSlots[index(mTicket.fetch_add(1, std::memory_order_relaxed))].ref();
    }

    T& getAt(int64_t ticket) {
        return mSlots[index(ticket)].ref();
    }

    // Find earliest ticket whose slot record time is great than or equal to target time
    // Note: We assume that RingBuffer have been protected from writing during searching.
    bool findTimeTicket(uint64_t startTimeMillis, int64_t endTicket, int64_t* outTicket) {
        int64_t end = endTicket - 1;
        if (end < 0) {
            // empty
            return false;
        }
        int64_t start = end > mCapacity ? end - mCapacity : 0;

        T value;
        while (end >= 0 && !mSlots[index(end)].read(value)) {
            end -= 1;
        }
        uint64_t latestTime = mGetTimeFn(value);
        if (end > 0) {
            // for not strictly time sorted records, use the second latest record to correct the time
            uint64_t tmpTime;
            if (mSlots[index(end - 1)].read(value) && (tmpTime = mGetTimeFn(value)) > latestTime) {
                latestTime = tmpTime;
            }
        }
        uint64_t targetTime = startTimeMillis;
        if (latestTime < targetTime) {
            return false;
        }
        while (start <= end) {
            int64_t mid = start + (end - start) / 2;
            while (mid > start && !mSlots[index(mid)].read(value)) {
                mid -= 1;
            }
            if (!mSlots[index(mid)].read(value) || mGetTimeFn(value) >= targetTime) {
                end = mid - 1;
            } else {
                start = mid + 1;
            }
        }
        // when code reach here
        // time in [start] slot is great than or equal to target time,
        // and time in [end] slot is less than target time
        *outTicket = start;
        return true;
    }

    int64_t write(T& value) {
        int64_t ticket = mTicket.fetch_add(1);
        if (__builtin_expect(mInConcurrentSafeMode, false)) {
            mSlots[index(ticket)].atomicWrite(ticket, value);
        } else {
            mSlots[index(ticket)].write(ticket, value);
        }
        return ticket;
    }

    void writesBack(RingBuffer<T>& backupBuffer, int64_t startTicket, int64_t endTicket) {
        mInConcurrentSafeMode = true;
        for (auto i = endTicket; i > startTicket ; --i) {
            auto idx = index(i);
            if (!mSlots[idx].atomicTryWrite(std::max(int64_t(0), i - mCapacity), i, backupBuffer.mSlots[idx].data, mGetTimeFn)) {
                // This means data in current position is probably expired, trying to write previous slot.
                // if still expired, break the write back process.
                auto preIdx = index(i - 1);
                if (!mSlots[preIdx].atomicTryWrite(std::max(int64_t(0), i - mCapacity - 1), i - 1, backupBuffer.mSlots[preIdx].data, mGetTimeFn)) {
                    break;
                } else {
                    // This means data is not strictly time sorted, but not expired, we can write it back.
                    mSlots[idx].atomicWrite(i, backupBuffer.mSlots[idx].data);
                }
            }
        }
        auto startIdx = index(startTicket);
        mSlots[startIdx].atomicTryWrite(std::max(int64_t(0), startTicket), startTicket, backupBuffer.mSlots[startIdx].data, mGetTimeFn);
        mInConcurrentSafeMode = false;
    }

    /**
     * For backup buffer, find the accurate range of tickets tha has valid slot record.
     * Note: We assume that the timestamp of invalid slot record is 0.
     * @param roughStartTicket rough start ticket, less than or equal to accurate start ticket
     * @param roughEndTicket rough end ticket, greater than or equal to accurate end ticket
     * @return true if finding succeed, false otherwise
     */
    bool findValidTicketRange(int64_t roughStartTicket, int64_t roughEndTicket, int64_t *outStart,
                              int64_t *outEnd) {
        *outStart = -1;
        *outEnd = -1;
        T value;
        for (auto i = roughStartTicket; i <= roughEndTicket; ++i) {
            if (mSlots[index(i)].read(value) && mGetTimeFn(value) > 0) {
                *outStart = i;
                break;
            }
        }
        for (auto i = roughEndTicket; i >= roughStartTicket; --i) {
            if (mSlots[index(i)].read(value) && mGetTimeFn(value) > 0) {
                *outEnd = i;
                break;
            }
        }
        return *outStart >= 0 && *outEnd >= 0 && *outEnd > *outStart;
    }

    int64_t quickDump(T* dest, int64_t startTicket, int64_t endTicket) {
//        int64_t endTicket = currentTicket;
//        int64_t startTicket = std::max(int64_t(0), endTicket - mCapacity);
        int64_t splitTicket;
        if (splitTicketRange(startTicket, endTicket, &splitTicket)) {
            memcpy(dest, mSlots + index(startTicket),
                   sizeof(T) * (splitTicket - startTicket));
            memcpy(dest + splitTicket - startTicket, mSlots + index(splitTicket),
                   sizeof(T) * (endTicket - splitTicket));
        } else {
            memcpy(dest, mSlots + index(startTicket), sizeof(T) * (endTicket - startTicket));
        }
        return endTicket - startTicket;
    }


    void clear() {
        memset(mSlots, 0, sizeof(T) * mCapacity);
    }

private:

    static RingBuffer<T>*
    allocateAt(uint32_t capacity, std::atomic<int64_t>& ticket, GetTimeFn<T> getTimeFn, void* addr) {
        RingBuffer<T>* buffer = new(addr) RingBuffer<T>(capacity, ticket, getTimeFn);
        _uninitialized_default_construct_n(buffer->mSlots, capacity);
        return buffer;
    }

    explicit RingBuffer(uint32_t capacity, std::atomic<int64_t>& ticket, GetTimeFn<T> getTimeFn) noexcept
            :mCapacity(capacity), mTicket(ticket), mGetTimeFn(getTimeFn), mInConcurrentSafeMode(false) {}

    ~RingBuffer() {
        _destroy_n(mSlots, mCapacity);
    }

    uint32_t index(int64_t ticket) {
        return ticket % mCapacity;
    }

    bool splitTicketRange(int64_t start, int64_t end, int64_t* splitTicket) {
        int64_t startTurn = start / mCapacity;
        int64_t endTurn = end / mCapacity;
        if (startTurn == endTurn) {
            return false;
        } else {
            *splitTicket = endTurn * mCapacity;
            return true;
        }
    }

    friend class PerfBuffer<T>;

}; // RingBuffer

} // namespace rheatrace
