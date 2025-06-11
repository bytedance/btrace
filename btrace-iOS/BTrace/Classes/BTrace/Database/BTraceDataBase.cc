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
//  BTraceDataBase.cc
//  BTrace
//
//  Created by Bytedance on 11/23/22.
//

#include <pthread.h>
#include <sqlite3.h>
#include <string.h>

#include "BTraceDataBase.hpp"

BTraceDataBase::BTraceDataBase(const char *path) {
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    db_ = new btrace::DataBase(path, flags);
    db_->setWalMode();
    db_->setAutoVacuum();
}


BTraceDataBase::~BTraceDataBase() {
    delete db_;
    db_ = nullptr;
}
