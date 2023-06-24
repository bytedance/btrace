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

#include <dirent.h>
#include "thread_name_collector.h"
#include "trace_provider.h"
#include "bytehook.h"
#include "debug.h"
#include <map>
#include <pthread.h>
#include <unistd.h>
#include <mutex>

namespace rheatrace {
    static int pthread_t_tid_index_;
    static std::map<std::string, std::string> map;
    static std::mutex mtx_;
    static bytehook_stub_t stub_;

    int my_pthread_setname_np(pthread_t thread, const char *name) {
        BYTEHOOK_STACK_SCOPE();
        pid_t tid = (((pid_t *) thread)[pthread_t_tid_index_]);
        {
            std::lock_guard<std::mutex> lk(mtx_);
            map.emplace(std::to_string(tid), strdup(name));
        }
        return BYTEHOOK_CALL_PREV(my_pthread_setname_np, thread, name);
    }

    void collect_proc_self_tasks() {
        auto procdir = opendir("/proc/self/task");
        if (procdir != nullptr) {
            dirent *entry;
            char comm_path[35];
            char tname[50];
            while ((entry = readdir(procdir)) != nullptr) {
                if (entry->d_name[0] == '.') {
                    continue;
                }
                auto tid = strdup(entry->d_name);
                snprintf(comm_path, sizeof(comm_path), "/proc/self/task/%s/comm", tid);
                auto *file = fopen(comm_path, "r");
                if (file == nullptr) {
                    continue;
                }
                fscanf(file, "%s", tname);
                fclose(file);
                {
                    std::lock_guard<std::mutex> lk(mtx_);
                    map.emplace(tid, strdup(tname));
                }
            }
            closedir(procdir);
        }
    }

    void thread_name_collector::start() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            map.clear();
        }
        collect_proc_self_tasks();
        // calculate tid offset
        auto *self = (pid_t *) pthread_self();
        for (int i = 0; i < 50; ++i) {
            if (self[i] == gettid()) {
                pthread_t_tid_index_ = i;
                break;
            }
        }
        // hook
        stub_ = bytehook_hook_all(nullptr, "pthread_setname_np",
                                  (void *) my_pthread_setname_np, nullptr, nullptr);
    }

    void thread_name_collector::stop() {
        bytehook_unhook(stub_);
    }

    std::string thread_name_collector::dump() {
        std::string out;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            for (const auto &item: map) {
                out.append(item.first).append(",").append(item.second).append("\n");
            }
        }
        return out;
    }

} // rheatrace