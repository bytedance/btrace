/*
 * Copyright (C) 2025 ByteDance Inc.
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

//
//  BTraceRecord.hpp
//  record
//
//  Created by Bytedance.
//

#ifndef BTRACE_RECORD_H_
#define BTRACE_RECORD_H_

#include <stdint.h>

#ifdef __cplusplus

#include "ring_buffer.hpp"
#include "CPUSampleModel.hpp"
#include "MemSampleModel.hpp"
#include "DispatchSampleModel.hpp"
#include "DateSampleModel.hpp"
#include "LockSampleModel.hpp"

namespace btrace {

class RecordBuffer;

extern RecordBuffer *g_record_buffer;

enum class RecordType : uint32_t
{
    kNone = 0,
    kCPURecord,
    kMemRecord,
    kDispatchRecord,
    kDateRecord,
    kLockRecord
};

struct Record {
    
    RecordType record_type;
    
    union {
        CPURecord cpu_record;
        MemRecord mem_record;
        DispatchRecord dispatch_record;
        DateRecord date_record;
        LockRecord lock_record;
    };
    
    Record(): record_type(RecordType::kNone) {};
    
    Record(const CPURecord &val) {
        record_type = RecordType::kCPURecord;
        cpu_record = val;
    }
    
    Record(const MemRecord &val) {
        record_type = RecordType::kMemRecord;
        mem_record = val;
    }
    
    Record(const DispatchRecord &val) {
        record_type = RecordType::kDispatchRecord;
        dispatch_record = val;
    }
    
    Record(const DateRecord &val) {
        record_type = RecordType::kDateRecord;
        date_record = val;
    }

    Record(const LockRecord &val) {
        record_type = RecordType::kLockRecord;
        lock_record = val;
    }
};

class RecordBuffer: public RingBuffer
{
public:
    
    RecordBuffer(uintptr_t size): RingBuffer(size) {}
    
    uintptr_t PutRecord(Record &record) {
        return Put(&record, sizeof(record));
    }
    
    uintptr_t OverWrittenRecord(Record &record, Record &overwritten_record) {
        PointerPositions pos;
        pos.write_pos = write_pos_.load(std::memory_order_acquire);
        pos.read_pos = read_pos_.load(std::memory_order_relaxed);
        
        const uint64_t size = Utils::RoundUp(sizeof(record), kAlignment);
        
        if (size > write_avail(pos)) {
            GetRecord(overwritten_record);
        }
        uintptr_t len = Put(&record, sizeof(record));
        nums_++;
        return len;
    }
    
    bool GetRecord(Record &record) {
        intptr_t size = Get(&record, sizeof(record));
        nums_--;
        return 0 < size;
    }

    void Dump(uint64_t begin_time=0, uint64_t end_time=UINT64_MAX, ThreadId tid=0);
    
    static void ProcessOverWrittenRecord(std::vector<Record> &overwritten_records);
    
private:
    void ViewAndAdvanceRecord(uint64_t &read_pos, Record &record) {
        uint8_t *src = at(read_pos);
        Get(&record, src, sizeof(record));
        read_pos += sizeof(record);
    }
    DISALLOW_COPY_AND_ASSIGN(RecordBuffer);
};
}


#endif

#endif // BTRACE_RECORD_H_
