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

#include "callstack_table.hpp"

namespace btrace
{
    CallstackTable *g_callstack_table;

    static constexpr uint32_t kMaxSize = 1 << 20;

    void CallstackTable::IncrementNodeRef(Node *node)
    {
        while (node != nullptr)
        {
//                ASSERT(stack_set_->count(node) == 1);
            node->refcount += 1;
            node = reinterpret_cast<Node *>(node->parent);
        }
    }

    void CallstackTable::DecrementNodeRef(Node *node, uint32_t cnt)
    {
        ASSERT(node->refcount >= cnt);

        Node *parent = nullptr;
        while (node != nullptr)
        {
//                ASSERT(stack_set_->count(node) == 1);
            node->refcount -= cnt;
            parent = reinterpret_cast<Node *>(node->parent);
            if (node->refcount == 0) {
                stack_set_->erase(node);
//                    delete node;
                malloc_zone_free(zone_, node);
            }
            node = parent;
        }
    }

    void CallstackTable::IncrementStackRef(uint32_t stack_id)
    {
        std::lock_guard<std::mutex> ml(*callstack_table_lock_);
        
        auto callstack_ref_it = callstack_ref_map_->find(stack_id);
        if (callstack_ref_it == callstack_ref_map_->end())
        {
            bool inserted;
            std::tie(callstack_ref_it, inserted) = callstack_ref_map_->emplace(stack_id, 0);

            auto node = (Node *)((uint64_t)stack_id << 4);
            IncrementNodeRef(node);
        }
        callstack_ref_it->second.Increase(1);
    }

    void CallstackTable::DecrementStackRef(uint32_t stack_id)
    {
        std::lock_guard<std::mutex> ml(*callstack_table_lock_);
        
        auto callstack_ref_it = callstack_ref_map_->find(stack_id);
        if (callstack_ref_it == callstack_ref_map_->end())
        {
            return;
        }

        callstack_ref_it->second.refcount -= 1;

        if (callstack_ref_it->second.refcount == 0)
        {
            uint32_t node_refcount = callstack_ref_it->second.node_refcount;
            callstack_ref_map_->erase(callstack_ref_it);

            auto node = (Node *)((uint64_t)stack_id << 4);
            DecrementNodeRef(node, node_refcount);
        }
    }

    // The stack must be ordered from caller to callee.
    uint32_t CallstackTable::insert(uword *stack, size_t size)
    {
        uint32_t stack_id = 0;
        uint64_t parent = 0; // use 0 to represent termination.

        std::lock_guard<std::mutex> ml(*callstack_table_lock_);
        
        for (int64_t i = size - 1; 0 <= i; --i)
        {
            uint64_t pc = stack[i];
            auto node = Node(parent, pc);
            auto it = stack_set_->find(&node);
            if (it != stack_set_->end())
            {
                Node *node = *it;
                node->refcount += 1;
                parent = reinterpret_cast<uint64_t>(node);
            }
            else
            {
#if DEBUG || INHOUSE_TARGET || TEST_MODE || READING_DEV
#else
                if (unlikely(kMaxSize <= stack_set_->size()))
                {
                    break;
                }
#endif
                Node *node = (Node *)malloc_zone_malloc(zone_, sizeof(Node));
                node->parent = parent;
                node->refcount = 1;
                node->address = pc;
                //                    auto node = new Node(parent, pc);
                bool inserted;
                std::tie(it, inserted) = stack_set_->insert(node);
                parent = reinterpret_cast<uint64_t>(*it);
            }
            // malloced address is usually less than 2 ** 36
            // TODO: make sure that parent is less than 2 ** 36
//            ASSERT(parent < (uint64_t)1 << 36);
        }

        stack_id = (uint32_t)(parent >> 4);
        
        auto callstack_ref_it = callstack_ref_map_->find(stack_id);
        if (callstack_ref_it == callstack_ref_map_->end())
        {
            bool inserted;
            std::tie(callstack_ref_it, inserted) = callstack_ref_map_->emplace(stack_id, 1);
        } else {
            callstack_ref_it->second.Increase(1);
        }
        
        return stack_id;
    }

} // namespace btrace
