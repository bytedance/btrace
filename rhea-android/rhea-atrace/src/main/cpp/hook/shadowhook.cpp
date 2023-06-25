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
#include "shadowhook.h"
#include <dlfcn.h>

int (*shadowhook_init_)(shadowhook_mode_t mode, bool debuggable) = nullptr;

int (*shadowhook_get_init_errno_)() = nullptr;

shadowhook_mode_t (*shadowhook_get_mode_)() = nullptr;

bool (*shadowhook_get_debuggable_)() = nullptr;

void (*shadowhook_set_debuggable_)(bool debuggable) = nullptr;

int (*shadowhook_get_errno_)() = nullptr;

const char *(*shadowhook_to_errmsg_)(int error_number) = nullptr;

void *(*shadowhook_hook_sym_addr_)(void *sym_addr, void *new_addr, void **orig_addr) = nullptr;

void *(*shadowhook_hook_sym_name_)(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr) = nullptr;

void *(*shadowhook_hook_sym_name_callback_)(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr, shadowhook_hooked_t hooked, void *hooked_arg) = nullptr;

int (*shadowhook_unhook_)(void *stub) = nullptr;

char *(*shadowhook_get_records_)(uint32_t item_flags) = nullptr;

void (*shadowhook_dump_records_)(int fd, uint32_t item_flags) = nullptr;

void *(*shadowhook_dlopen_)(const char *lib_name) = nullptr;

void (*shadowhook_dlclose_)(void *handle) = nullptr;

void *(*shadowhook_dlsym_)(void *handle, const char *sym_name) = nullptr;

void *(*shadowhook_dlsym_dynsym_)(void *handle, const char *sym_name) = nullptr;

void *(*shadowhook_dlsym_symtab_)(void *handle, const char *sym_name) = nullptr;

void *(*shadowhook_get_prev_func_)(void *func) = nullptr;

void (*shadowhook_pop_stack_)(void *return_address) = nullptr;

void (*shadowhook_allow_reentrant_)(void *return_address) = nullptr;

void (*shadowhook_disallow_reentrant_)(void *return_address) = nullptr;

void *(*shadowhook_get_return_address_)() = nullptr;

static void load_symbols() {
    void *handle = dlopen("libshadowhook.so", 0);
    if (handle != nullptr) {
        shadowhook_init_ = (int (*)(shadowhook_mode_t, bool)) dlsym(handle, "shadowhook_init");
        shadowhook_get_init_errno_ = (int (*)()) dlsym(handle, "shadowhook_get_init_errno");
        shadowhook_get_mode_ = (shadowhook_mode_t (*)()) dlsym(handle, "shadowhook_get_mode");
        shadowhook_get_debuggable_ = (bool (*)()) dlsym(handle, "shadowhook_get_debuggable");
        shadowhook_set_debuggable_ = (void (*)(bool)) dlsym(handle, "shadowhook_set_debuggable");
        shadowhook_get_errno_ = (int (*)()) dlsym(handle, "shadowhook_get_errno");
        shadowhook_to_errmsg_ = (const char *(*)(int)) dlsym(handle, "shadowhook_to_errmsg");
        shadowhook_hook_sym_addr_ = (void *(*)(void *, void *, void **)) dlsym(handle, "shadowhook_hook_sym_addr");
        shadowhook_hook_sym_name_ = (void *(*)(const char *, const char *, void *, void **)) dlsym(handle, "shadowhook_hook_sym_name");
        shadowhook_hook_sym_name_callback_ = (void *(*)(const char *, const char *, void *, void **, shadowhook_hooked_t, void *)) dlsym(handle, "shadowhook_hook_sym_name_callback");
        shadowhook_unhook_ = (int (*)(void *)) dlsym(handle, "shadowhook_unhook");
        shadowhook_get_records_ = (char *(*)(uint32_t)) dlsym(handle, "shadowhook_get_records");
        shadowhook_dump_records_ = (void (*)(int, uint32_t)) dlsym(handle, "shadowhook_dump_records");
        shadowhook_dlopen_ = (void *(*)(const char *)) dlsym(handle, "shadowhook_dlopen");
        shadowhook_dlclose_ = (void (*)(void *)) dlsym(handle, "shadowhook_dlclose");
        shadowhook_dlsym_ = (void *(*)(void *, const char *)) dlsym(handle, "shadowhook_dlsym");
        shadowhook_dlsym_dynsym_ = (void *(*)(void *, const char *)) dlsym(handle, "shadowhook_dlsym_dynsym");
        shadowhook_dlsym_symtab_ = (void *(*)(void *, const char *)) dlsym(handle, "shadowhook_dlsym_symtab");
        shadowhook_get_prev_func_ = (void *(*)(void *)) dlsym(handle, "shadowhook_get_prev_func");
        shadowhook_pop_stack_ = (void (*)(void *)) dlsym(handle, "shadowhook_pop_stack");
        shadowhook_allow_reentrant_ = (void (*)(void *)) dlsym(handle, "shadowhook_allow_reentrant");
        shadowhook_disallow_reentrant_ = (void (*)(void *)) dlsym(handle, "shadowhook_disallow_reentrant");
        shadowhook_get_return_address_ = (void *(*)()) dlsym(handle, "shadowhook_get_return_address");
    }
}

// impl
int shadowhook_init(shadowhook_mode_t mode, bool debuggable) {
    load_symbols();
    if (shadowhook_init_ != nullptr) {
        return shadowhook_init_(mode, debuggable);
    }
    return -1;
}

int shadowhook_get_init_errno(void) {
    if (shadowhook_get_init_errno_ != nullptr) {
        return shadowhook_get_init_errno_();
    }
    return -1;
}

shadowhook_mode_t shadowhook_get_mode(void) {
    if (shadowhook_get_mode_ != nullptr) {
        return shadowhook_get_mode_();
    }
    return SHADOWHOOK_MODE_SHARED;
}

bool shadowhook_get_debuggable(void) {
    if (shadowhook_get_debuggable_ != nullptr) {
        return shadowhook_get_debuggable_();
    }
    return false;
}

void shadowhook_set_debuggable(bool debuggable) {
    if (shadowhook_set_debuggable_ != nullptr) {
        shadowhook_set_debuggable_(debuggable);
    }
}

int shadowhook_get_errno(void) {
    if (shadowhook_get_errno_ != nullptr) {
        return shadowhook_get_errno_();
    }
    return 0;
}

const char *shadowhook_to_errmsg(int error_number) {
    if (shadowhook_to_errmsg_ != nullptr) {
        return shadowhook_to_errmsg_(error_number);
    }
    return nullptr;
}

void *shadowhook_hook_sym_addr(void *sym_addr, void *new_addr, void **orig_addr) {
    if (shadowhook_hook_sym_addr_ != nullptr) {
        return shadowhook_hook_sym_addr_(sym_addr, new_addr, orig_addr);
    }
    return nullptr;
}

void *shadowhook_hook_sym_name(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr) {
    if (shadowhook_hook_sym_name_ != nullptr) {
        return shadowhook_hook_sym_name_(lib_name, sym_name, new_addr, orig_addr);
    }
    return nullptr;
}

void *shadowhook_hook_sym_name_callback(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr, shadowhook_hooked_t hooked, void *hooked_arg) {
    if (shadowhook_hook_sym_name_callback_ != nullptr) {
        return shadowhook_hook_sym_name_callback_(lib_name, sym_name, new_addr, orig_addr, hooked, hooked_arg);
    }
    return nullptr;
}

int shadowhook_unhook(void *stub) {
    if (shadowhook_unhook_ != nullptr) {
        return shadowhook_unhook_(stub);
    }
    return 0;
}

char *shadowhook_get_records(uint32_t item_flags) {
    if (shadowhook_get_records_ != nullptr) {
        return shadowhook_get_records_(item_flags);
    }
    return nullptr;
}

void shadowhook_dump_records(int fd, uint32_t item_flags) {
    if (shadowhook_dump_records_ != nullptr) {
        return shadowhook_dump_records_(fd, item_flags);
    }
}

void *shadowhook_dlopen(const char *lib_name) {
    if (shadowhook_dlopen_ != nullptr) {
        return shadowhook_dlopen_(lib_name);
    }
    return nullptr;
}

void shadowhook_dlclose(void *handle) {
    if (shadowhook_dlclose_ != nullptr) {
        return shadowhook_dlclose_(handle);
    }
}

void *shadowhook_dlsym(void *handle, const char *sym_name) {
    if (shadowhook_dlsym_ != nullptr) {
        return shadowhook_dlsym_(handle, sym_name);
    }
    return nullptr;
}

void *shadowhook_dlsym_dynsym(void *handle, const char *sym_name) {
    if (shadowhook_dlsym_dynsym_ != nullptr) {
        return shadowhook_dlsym_dynsym_(handle, sym_name);
    }
    return nullptr;
}

void *shadowhook_dlsym_symtab(void *handle, const char *sym_name) {
    if (shadowhook_dlsym_symtab_ != nullptr) {
        return shadowhook_dlsym_symtab_(handle, sym_name);
    }
    return nullptr;
}

void *shadowhook_get_prev_func(void *func) {
    if (shadowhook_get_prev_func_ != nullptr) {
        return shadowhook_get_prev_func_(func);
    }
    return nullptr;
}

void shadowhook_pop_stack(void *return_address) {
    if (shadowhook_pop_stack_ != nullptr) {
        return shadowhook_pop_stack_(return_address);
    }
}

void shadowhook_allow_reentrant(void *return_address) {
    if (shadowhook_allow_reentrant_ != nullptr) {
        return shadowhook_allow_reentrant_(return_address);
    }
}

void shadowhook_disallow_reentrant(void *return_address) {
    if (shadowhook_disallow_reentrant_ != nullptr) {
        return shadowhook_disallow_reentrant_(return_address);
    }
}

void *shadowhook_get_return_address(void) {
    if (shadowhook_get_return_address_ != nullptr) {
        return shadowhook_get_return_address_();
    }
    return nullptr;
}
