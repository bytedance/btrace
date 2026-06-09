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
// Created on 2025/10/16.
//

#include "ThreadNames.h"
#include <bits/alltypes.h>
#include <unistd.h>
#include <sstream>
#include <new>

#include "OSThread.h"

namespace btrace {


ThreadNames& ThreadNames::get() {
    static ThreadNames inst{};
    return inst;
}

std::string ThreadNames::batchUnwindThreadNames(std::set<pid_t> tids) {
    OSThreadIterator it;
    
    std::ostringstream oss;
    while (it.HasNext()) {
        auto t = it.Next();
        pid_t tid = t->tid();
        
        if (tids.find(tid) != tids.end()) {
            const std::string &name = t->name();
        
            if (0 < name.size()) {
                oss << tid << ':' << name << std::endl;
            } else {
                oss << tid << ':' << "unknown" << std::endl;
            }
        }
    }
    
    return oss.str();
}

}