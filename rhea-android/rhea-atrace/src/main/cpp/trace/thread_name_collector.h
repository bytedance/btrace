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

#ifndef RHEATRACE_THREAD_NAME_COLLECTOR_H
#define RHEATRACE_THREAD_NAME_COLLECTOR_H

#include <string>

namespace rheatrace {

    class thread_name_collector {
    public:
        static void start();

        static void stop();

        static std::string dump();
    };

} // rheatrace

#endif //RHEATRACE_THREAD_NAME_COLLECTOR_H
