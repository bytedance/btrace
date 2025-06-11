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

#ifndef BTRACE_CALLSTACK_TABLE_H_
#define BTRACE_CALLSTACK_TABLE_H_

#include <malloc/malloc.h>

#ifdef __cplusplus

#include <unordered_set>

#include "utils.hpp"
#include "assert.hpp"
#include "globals.hpp"
#include "phmap.hpp"

namespace btrace
{
    class CallstackTable;
    class CallstackTableIterator;

    extern CallstackTable *g_callstack_table;

    class CallstackTable
    {
    public:
        
        #pragma pack(push,16)
        struct Node
        {
            uint64_t parent:48;
            uint64_t refcount:32;
            uint64_t address:48;
            
            Node(uint64_t parent, uint64_t address): parent(parent), refcount(0), address(address) {}
        };
        #pragma pack(pop)
        
        struct NodeHash {
            size_t operator()(const Node* node) const {
                auto hasher = std::hash<uint64_t>();
                size_t h = hasher(node->parent);
                h ^= hasher(node->address);
                return h;
            }
        };
        
        struct NodeEqual
        {
            bool operator()(const Node* node1, const Node* node2) const noexcept
            {
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
        
        CallstackTable() {
            zone_ = malloc_create_zone(0, 0);
            malloc_set_zone_name(zone_, "btrace::CallstackTable");
            callstack_table_lock_ = new std::mutex;
            stack_set_ = new CallStackSet();
            callstack_ref_map_ = new CallStackRefMap();
        }
        
        ~CallstackTable() {
//            for (auto it=stack_set_->begin(); it!= stack_set_->end(); ++it) {
//                Node *node = *it;
//                delete node;
//            }
            delete callstack_table_lock_;
            callstack_table_lock_ = nullptr;
            delete stack_set_;
            stack_set_ = nullptr;
            delete callstack_ref_map_;
            callstack_ref_map_ = nullptr;
            malloc_destroy_zone(zone_);
            zone_ = nullptr;
        }
        
        void IncrementNodeRef(Node *node);
        
        void DecrementNodeRef(Node *node, uint32_t cnt=1);
        
        void IncrementStackRef(uint32_t stack_id);
        
        void DecrementStackRef(uint32_t stack_id);
        
        size_t size() {
            return stack_set_->size();
        }

        // The stack must be ordered from caller to callee.
        uint32_t insert(uword *stack, size_t size);

        using CallStackSet = phmap::parallel_flat_hash_set<Node *, NodeHash, NodeEqual>;
        using CallStackRefMap = phmap::parallel_flat_hash_map<uint32_t, CallstackRef>;
    private:
        malloc_zone_t *zone_;
        std::mutex *callstack_table_lock_ = nullptr;
        CallStackSet *stack_set_ = nullptr;
        CallStackRefMap *callstack_ref_map_ = nullptr;
        
        friend class CallstackTableIterator;
    };

    class CallstackTableIterator : public ValueObject
    {
    public:
        CallstackTableIterator(CallstackTable *callstack_table): callstack_table_(callstack_table)
        {
            it = callstack_table_->stack_set_->begin();
        }

        ~CallstackTableIterator() {};
        
        // Returns false when there are no more threads left.
        bool HasNext() const
        {
            return it != callstack_table_->stack_set_->end();
        }
        
        // Returns the current thread and moves forward.
        CallstackTable::Node *Next()
        {
            CallstackTable::Node *current = *it;
            ++it;
            return current;
        }
        
    private:
        CallstackTable *callstack_table_;
        CallstackTable::CallStackSet::iterator it;
    };

} // namespace btrace

#endif // #ifdef __cplusplus
#endif // BTRACE_CALLSTACK_TABLE_H_
