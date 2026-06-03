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
// Created on 2025/10/11.
//

#ifndef HARMONY_BTRACE_THREADLOCALSTORAGE_H
#define HARMONY_BTRACE_THREADLOCALSTORAGE_H

#include <cstdint>
#include <pthread.h>

namespace btrace {

struct  TLSRecord {
    bool backtracing {false};
    uint64_t lastActiveTimerRound {0};
    void* threadNameInfoPtr {nullptr};
};

class ThreadLocalStorage {
public:
    static ThreadLocalStorage& get();
    bool setup();
    bool  cleanup();
    TLSRecord& tlsRecord();
    TLSRecord* getMainTLSRecord();
private:
    pthread_key_t tlsKey;
};

}


#endif //HARMONY_BTRACE_THREADLOCALSTORAGE_H
