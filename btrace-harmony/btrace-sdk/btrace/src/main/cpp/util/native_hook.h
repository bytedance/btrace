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

#if !defined(__NATIVE_HOOKER_H__)
#define __NATIVE_HOOKER_H__

class elf_hooker;

class native_hooker {
private:
    elf_hooker *elf_hooker;

public:
    native_hooker();
    ~native_hooker();

    /**
     * phrase the proc maps to init
     */
    void phrase_proc_maps();
    /**
     * phrase the proc maps to init with given so name
     * @param so_name specific so file
     */
    void phrase_proc_maps_specified(const char *so_name);
    /**
     * dump the phrased module list to log
     */
    void dump_module_list();
    /** *
        prehook_cb invoked before really hook,
        if prehook_cb NOT set or return true, this module will be hooked,
        if prehook_cb set and return false, this module will NOT be hooked,
    */
    void set_prehook_cb(bool (*pfn)(const char *, const char *));
    /**
     * hook the function in all phrased modules
     */
    void hook_all_modules(const char *func_name, void *pfn_new, void **ppfn_old, bool replace);
    /**
     * hook the function in the given so
     */
    void hook_module(const char *so_name, const char *func_name, void *pfn_new, void **ppfn_old, bool replace);
};

#endif
