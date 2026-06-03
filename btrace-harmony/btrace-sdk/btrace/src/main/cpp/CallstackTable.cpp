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

#include "CallstackTable.h"
#include "util/globals.h"

#include <cstdint>
#include <hilog/log.h>

#include <cassert>

#undef LOG_TAG
#define LOG_TAG "btrace:CallstackTable"

namespace btrace {

static constexpr uint32_t kMaxSize = 1 << 20;

void CallstackTable::incrementNodeRef(Node *node) {
    while (node != nullptr) {
//                ASSERT(stack_set_.count(node) == 1);
        node->refcount += 1;
        node = reinterpret_cast<Node *>(node->parent);
    }
}

void CallstackTable::decrementNodeRef(Node *node, uint32_t cnt, const Callback cb) {
    assert(cnt <= node->refcount);

    Node *parent = nullptr;
    while (node != nullptr) {
        if (cb) {
            cb(node->address, cnt);
        }
        
        node->refcount -= cnt;
        parent = reinterpret_cast<Node *>(node->parent);
        
        if (node->refcount == 0) {
            stack_set_.erase(node);
            delete node;
        }
        
        node = parent;
    }
}

void CallstackTable::incrementStackRef(uint64_t stack_id) {
    std::lock_guard<std::mutex> ml(callstack_table_lock_);
    
    auto callstack_ref_it = ref_map_.find(stack_id);
    if (callstack_ref_it == ref_map_.end()) {
        bool inserted;
        std::tie(callstack_ref_it, inserted) = ref_map_.emplace(stack_id, 0);

        auto node = (Node *)stack_id;
        incrementNodeRef(node);
    }
    callstack_ref_it->second.Increase(1);
}

void CallstackTable::decrementStackRef(uint64_t stack_id, uint32_t cnt, const Callback cb) {
    std::lock_guard<std::mutex> ml(callstack_table_lock_);
    
    auto ref_it = ref_map_.find(stack_id);
    if (ref_it == ref_map_.end()) {
        return;
    }
    
    auto &ref = ref_it->second;
    assert(cnt <= ref.refcount);
    ref.refcount -= cnt;
    uint32_t refcnt = 0;
    
    if (ref.refcount == 0) {
        refcnt = ref.node_refcount;
        ref_map_.erase(ref_it);
    } else {
        // Avoid infinite increase of node_defcount
        auto refcnt_diff = ref.node_refcount - ref.refcount;
        
        if (kNodeRefDiffThres_ <= refcnt_diff) {
            refcnt = refcnt_diff;
            ref.node_refcount = ref.refcount;
        } else if (kForceDecrementThres_ <= (++force_decrement_cnt_)) {
            refcnt = refcnt_diff;
            force_decrement_cnt_ = 0;
            ref.node_refcount = ref.refcount;
        }
    }
    
    if (0 < refcnt) {
        auto node = (Node *)stack_id;
        decrementNodeRef(node, refcnt, cb);
    }
}

// The stack must be ordered from caller to callee.
uint64_t CallstackTable::insert(uint64_t *stack, size_t size, const Callback cb) {
    uint64_t stack_id = 0;
    uint64_t parent = 0; // use 0 to represent termination.

    std::lock_guard<std::mutex> ml(callstack_table_lock_);
    
    for (int64_t i = size - 1; 0 <= i; --i) {
        uint64_t pc = stack[i] & kPointerMask;
        auto search_node = Node(parent, pc);
        auto it = stack_set_.find(&search_node);
        
        if (it != stack_set_.end()) {
            Node *found_node = *it;
            found_node->refcount += 1;
            parent = reinterpret_cast<uint64_t>(found_node);
        } else {
#if DEBUG
#else
            if (kMaxSize <= stack_set_.size()) {
                break;
            }
#endif
            auto new_node = new Node(parent, pc, 1);
            bool inserted;
            std::tie(it, inserted) = stack_set_.insert(new_node);
            parent = reinterpret_cast<uint64_t>(*it);
        }
        
        if (cb) {
            cb(pc, 1);
        }
    }

    stack_id = parent;
    
    auto callstack_ref_it = ref_map_.find(stack_id);
    if (callstack_ref_it == ref_map_.end()) {
        bool inserted;
        std::tie(callstack_ref_it, inserted) = ref_map_.emplace(stack_id, 1);
    } else {
        callstack_ref_it->second.Increase(1);
    }
    
    return stack_id;
}

std::vector<uint64_t> CallstackTable::query(uint64_t stack_id) {
    std::vector<uint64_t> res;

    if (stack_id == 0) { return res; }

    Node *parent = nullptr;
    auto node = (Node *)stack_id;

    std::lock_guard<std::mutex> ml(callstack_table_lock_);
    
    // Warning! Concurrently calls to decrementStackRef() and query() can trigger this assertion.
    assert(ref_map_.find(stack_id) != ref_map_.end());
    
    while (node != nullptr) {
        res.push_back(node->address);
        parent = reinterpret_cast<Node *>(node->parent);
        node = parent;
    }
    
    return res;
}

} // namespace btrace
