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


#include <errno.h>
#include <unistd.h>
#include "RingBuffer.h"
#include "common_write.h"

namespace rheatrace {

class Dumper {
public:
    virtual uint32_t dumpRecord(JNIEnv* env, void* addr, void* r) = 0;
    virtual bool hasMapping() = 0;
    virtual bool dumpMapping(int fd) = 0;
    virtual ~Dumper() {}
};

template<typename T>
class PerfBuffer {
private:
    std::atomic<int64_t> mTicket;
    RingBuffer<T>* mMajorBuffer;
    RingBuffer<T>* mBackupBuffer;
    bool mUseBackupBuffer;
    void* mMemoryArea;
    size_t mMemroyAreaSize;

    class AutoSwitchBufferHandler {
    private:
        PerfBuffer<T>& mPerfBuffer;
        int64_t mRoughStartTicket;
    public:
        AutoSwitchBufferHandler(PerfBuffer<T>& perfBuffer) : mPerfBuffer(perfBuffer) {
            mPerfBuffer.mBackupBuffer->clear();
            mRoughStartTicket = mPerfBuffer.mMajorBuffer->getCurrentTicket();
            mPerfBuffer.mUseBackupBuffer = true;
        }

        int64_t getMarkedTicket() {
            return mRoughStartTicket;
        }

        ~AutoSwitchBufferHandler() {
            auto* backupBuffer = mPerfBuffer.mBackupBuffer;
            mPerfBuffer.mUseBackupBuffer = false;
            int64_t roughEndTicket = mPerfBuffer.mMajorBuffer->getCurrentTicket();
            int64_t accurateStartTicket, accurateEndTicket;
            if (backupBuffer->findValidTicketRange(mRoughStartTicket, roughEndTicket,
                                                   &accurateStartTicket,
                                                   &accurateEndTicket)) {
                mPerfBuffer.mMajorBuffer->writesBack(*backupBuffer, accurateStartTicket, accurateEndTicket);
            }
        }
    };

public:

    static PerfBuffer<T>* create(uint64_t capacity, GetTimeFn<T> getTimeFn) {
        size_t singleBufferSize = RingBuffer<T>::calculateAllocationSize(capacity);
        size_t memorySize = ((singleBufferSize * 2) + ~PAGE_MASK) & PAGE_MASK;
        void* memory = mmap(nullptr, memorySize, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (memory == MAP_FAILED) {
            return nullptr;
        }
        return new PerfBuffer<T>(capacity, memory, memorySize, getTimeFn);
    }

    PerfBuffer(uint64_t capacity, void* addr, size_t memorySize, GetTimeFn<T> getTimeFn)
            : mTicket(0), mMajorBuffer(nullptr), mBackupBuffer(nullptr),
              mUseBackupBuffer(false), mMemoryArea(addr), mMemroyAreaSize(memorySize) {
        char* memory = reinterpret_cast<char*>(mMemoryArea);
        mMajorBuffer = RingBuffer<T>::allocateAt(capacity, mTicket, getTimeFn, memory);
        mBackupBuffer = RingBuffer<T>::allocateAt(capacity,
                                                  mTicket,
                                                  getTimeFn,
                                                  (void*) (memory + memorySize / 2));
    }

    ~PerfBuffer() {
        munmap(mMemoryArea, mMemroyAreaSize);
    }

    int64_t capacity() {
        return mMajorBuffer->capacity();
    }

    int64_t write(T& value) {
        return getCurrentRingBuffer()->write(value);
    }

    int64_t mark() {
        return getCurrentRingBuffer()->getCurrentTicket();
    }

    T& acquire() {
        return getCurrentRingBuffer()->acquire();
    }

    int
    dump(JNIEnv* env, int fd, int mappingFd, uint32_t type, uint32_t version, uint64_t time,
         const char* extra, int32_t extraLen, bool dumpRaw, Dumper* dumper) {
        AutoSwitchBufferHandler handler(*this);
        int64_t endTicket = handler.getMarkedTicket();
        uint32_t count = mMajorBuffer->availableCount(endTicket);
        return innerDump(env, fd, mappingFd, type, version, time, extra, extraLen, dumpRaw, dumper,
                         endTicket - count, endTicket);
    }

    int dumpPart(JNIEnv* env, int fd, int mappingFd, uint32_t type, uint32_t version, uint64_t time,
                 const char* extra, int32_t extraLen, bool dumpRaw, Dumper* dumper, int64_t startTicket, int64_t endTicket) {
        AutoSwitchBufferHandler handler(*this);
        int64_t curBufferStartTicket = std::max(
                handler.getMarkedTicket() - mMajorBuffer->capacity(), int64_t(0));
        int64_t realStartTicket = std::max(curBufferStartTicket, startTicket);
        int64_t realEndTicket = std::min(handler.getMarkedTicket(), endTicket);
        if (realStartTicket < realEndTicket) {
            return innerDump(env, fd, mappingFd, type, version, time, extra, extraLen, dumpRaw,
                             dumper, realStartTicket, realEndTicket);
        } else {
            return 8;
        }
    }

    int dumpTimedPart(JNIEnv* env, int fd, int mappingFd, uint32_t type, uint32_t version,
                      uint64_t time, const char* extra, int32_t extraLen, bool dumpRaw,
                      Dumper* dumper, int64_t endTicket, uint64_t startTimeMillis) {
        AutoSwitchBufferHandler handler(*this);
        int64_t startTicket;
        if (mMajorBuffer->findTimeTicket(startTimeMillis, endTicket, &startTicket) && startTicket < endTicket) {
            return innerDump(env, fd, mappingFd, type, version, time, extra, extraLen, dumpRaw,
                             dumper, startTicket, endTicket);
        }
        return 9;
    }

private:
    RingBuffer<T>* getCurrentRingBuffer() {
        if (__builtin_expect(mUseBackupBuffer, false)) {
            return mBackupBuffer;
        } else {
            return mMajorBuffer;
        }
    }

    int innerDump(JNIEnv* env, int fd, int mappingFd, uint32_t type, uint32_t version, uint64_t time,
                  const char* extra, int32_t extraLen, bool dumpRaw, Dumper* dumper, int64_t startTicket, int64_t endTicket) {
        uint32_t count = endTicket - startTicket;
        uint32_t magicNumber = 0x01020304;
        if (dumpRaw) {
            int64_t dumpSize =
                    sizeof(magicNumber) + sizeof(type) + sizeof(version) + sizeof(time) +
                    sizeof(count) + sizeof(extraLen) + extraLen +
                    (count * sizeof(T));
            if (ftruncate(fd, dumpSize) != 0) {
                return 3;
            }
            int64_t mmapSize = (dumpSize + ~PAGE_MASK) & PAGE_MASK;
            void* addr = mmap(nullptr, mmapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (addr == MAP_FAILED) {
                return errno;
            }
            char* writeAddr = static_cast<char*>(addr);
            uint32_t offset = rheatrace::writeBuf(writeAddr, magicNumber); // magic number
            offset += rheatrace::writeBuf(writeAddr + offset, type); // type
            offset += rheatrace::writeBuf(writeAddr + offset, version); // version
            offset += rheatrace::writeBuf(writeAddr + offset, time); // time
            offset += rheatrace::writeBuf(writeAddr + offset, count); // count
            // dump extra info
            if (extraLen > 0 && extra != nullptr) {
                offset += rheatrace::writeBuf(writeAddr + offset, extraLen);
                memcpy(writeAddr + offset, extra, extraLen);
                offset += extraLen;
            } else {
                offset += rheatrace::writeBuf(writeAddr + offset, int32_t(0));
            }
            mMajorBuffer->quickDump(reinterpret_cast<T*>(writeAddr + offset),
                                    startTicket, endTicket);
            msync(addr, dumpSize, MS_SYNC);
            munmap(addr, mmapSize);
            return 0;
        } else if (dumper != nullptr) {
            int64_t pageSize = sysconf(_SC_PAGE_SIZE);
            int64_t mapUnit = 128 * 1024;
            if (pageSize > mapUnit) {
                mapUnit = pageSize;
            }
            int64_t mmapSize = mapUnit * 4;
            if (ftruncate(fd, mmapSize) != 0) {
                return 5;
            }
            void* addr = mmap(nullptr, mmapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (addr == MAP_FAILED) {
                return errno;
            }
            char* writeAddr = static_cast<char*>(addr);
            uint32_t offset = rheatrace::writeBuf(writeAddr, magicNumber); // magic number
            offset += rheatrace::writeBuf(writeAddr + offset, type); // type
            offset += rheatrace::writeBuf(writeAddr + offset, version); // version
            offset += rheatrace::writeBuf(writeAddr + offset, time); // time
            offset += rheatrace::writeBuf(writeAddr + offset, count); // count
            // dump extra info
            if (extraLen > 0 && extra != nullptr) {
                offset += rheatrace::writeBuf(writeAddr + offset, extraLen);
                memcpy(writeAddr + offset, extra, extraLen);
                offset += extraLen;
            } else {
                offset += rheatrace::writeBuf(writeAddr + offset, int32_t(0));
            }
            int64_t currentFileMmapOffset = 0;
            for (int64_t i = endTicket - count; i < endTicket; i++) {
                if (mmapSize + currentFileMmapOffset - offset < mapUnit) {
                    // sync already dumped
                    msync(addr, mmapSize, MS_SYNC);
                    munmap(addr, mmapSize);
                    // grow file size and create new map
                    currentFileMmapOffset += mmapSize - mapUnit;
                    if (ftruncate(fd, currentFileMmapOffset + mmapSize) != 0) {
                        return 7;
                    }
                    addr = mmap(nullptr, mmapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                currentFileMmapOffset);
                    if (addr == MAP_FAILED) {
                        return 8;
                    }
                }
                offset += dumper->dumpRecord(env, static_cast<char*>(addr) + offset -
                                                  currentFileMmapOffset,
                                             &(mMajorBuffer->getAt(i)));
            }
            msync(addr, mmapSize, MS_SYNC);
            munmap(addr, mmapSize);
            ftruncate(fd, offset);

            if (dumper->hasMapping() && mappingFd != -1) {
                dumper->dumpMapping(mappingFd);
            }

            return 0;
        } else {
            return 9;
        }
    }
};

} // namespace rheatrace