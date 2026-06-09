/*
 * Copyright (c) 2026 Bytedance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created on 2026/4/8.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef BTRACE_HARMONY_CALLSTACKTABLE_H
#define BTRACE_HARMONY_CALLSTACKTABLE_H

#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

namespace btrace {
class CallstackTable;
class CallstackTableIterator;

class CallstackTable {
public:
    #pragma pack(push,4)
    struct Node {
        uint64_t parent;
        uint64_t address;
        uint32_t refcount;
        
        Node(uint64_t parent, uint64_t address, uint32_t refcount=0):
            parent(parent), address(address), refcount(refcount) {}
    };
    #pragma pack(pop)
    
    struct NodeHash {
        size_t operator()(const Node* node) const {
            auto hasher = std::hash<uint64_t>();
            size_t h = hasher(node->parent);
            // 优化：使用类似 boost::hash_combine 的方式降低对称碰撞率
            h ^= hasher(node->address) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    
    struct NodeEqual {
        bool operator()(const Node* node1, const Node* node2) const noexcept {
            bool result = (node1->parent == node2->parent && node1->address == node2->address);
            return result;
        }
    };
    
    struct CallstackRef {
        explicit CallstackRef(uint32_t cnt) : refcount(cnt), node_refcount(cnt) {}
        
        ~CallstackRef() {}
        
        void Increase(uint32_t cnt) {
            refcount += cnt;
            node_refcount += cnt;
        }
        
        uint32_t refcount = 0;
        uint32_t node_refcount = 0;
    };
    
    using Callback = void(*)(uint64_t addr, uint32_t refcount);
    
    CallstackTable() {}
    
    ~CallstackTable() {
        for (auto it=stack_set_.begin(); it!= stack_set_.end(); ++it) {
            Node *node = *it;
            delete node;
        }
        stack_set_.clear();
        ref_map_.clear();
    }
    
    size_t size() { return stack_set_.size(); }

    // The stack must be ordered from caller to callee.
    uint64_t insert(uint64_t *stack, size_t size, const Callback cb);
    std::vector<uint64_t> query(uint64_t stack_id);
    void decrementStackRef(uint64_t stack_id, uint32_t cnt=1, const Callback cb=nullptr);

    using CallStackSet = std::unordered_set<Node *, NodeHash, NodeEqual>;
    using CallStackRefMap = std::unordered_map<uint64_t, CallstackRef>;
private:
    void incrementNodeRef(Node *node);
    void decrementNodeRef(Node *node, uint32_t cnt=1, const Callback cb=nullptr);
    void incrementStackRef(uint64_t stack_id);
    
    int32_t force_decrement_cnt_ = 0;
    static constexpr int32_t kForceDecrementThres_ = 1024;
    static constexpr int32_t kNodeRefDiffThres_ = 1024;
    
    std::mutex callstack_table_lock_;
    CallStackSet stack_set_;
    CallStackRefMap ref_map_;
    
    friend class CallstackTableIterator;
};

class CallstackTableIterator{
public:
    CallstackTableIterator(CallstackTable *callstack_table)
        : callstack_table_(callstack_table),
          lock_(callstack_table_->callstack_table_lock_) {
        it = callstack_table_->stack_set_.begin();
    }

    ~CallstackTableIterator() {};
    
    // Returns false when there are no more threads left.
    bool HasNext() const {
        return it != callstack_table_->stack_set_.end();
    }
        
    // Returns the current thread and moves forward.
    CallstackTable::Node *Next() {
        CallstackTable::Node *current = *it;
        ++it;
        return current;
    }
private:
    CallstackTable *callstack_table_;
    std::unique_lock<std::mutex> lock_;
    CallstackTable::CallStackSet::iterator it;
};
}


#endif //BTRACE_HARMONY_CALLSTACKTABLE_H
