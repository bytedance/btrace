#ifndef BYTEDANCE_BYTEHOOK_H
#define BYTEDANCE_BYTEHOOK_H 1

#include <stdbool.h>

#define BYTEHOOK_STATUS_CODE_OK                  0
#define BYTEHOOK_STATUS_CODE_UNINIT              1
#define BYTEHOOK_STATUS_CODE_INITERR_INVALID_ARG 2
#define BYTEHOOK_STATUS_CODE_INITERR_SYM         3
#define BYTEHOOK_STATUS_CODE_INITERR_TASK        4
#define BYTEHOOK_STATUS_CODE_INITERR_HOOK        5
#define BYTEHOOK_STATUS_CODE_INITERR_ELF         6
#define BYTEHOOK_STATUS_CODE_INITERR_ELF_REFR    7
#define BYTEHOOK_STATUS_CODE_INITERR_TRAMPO      8
#define BYTEHOOK_STATUS_CODE_INITERR_SIG         9
#define BYTEHOOK_STATUS_CODE_INITERR_DLMTR       10
#define BYTEHOOK_STATUS_CODE_INVALID_ARG         11
#define BYTEHOOK_STATUS_CODE_UNMATCH_ORIG_FUNC   12
#define BYTEHOOK_STATUS_CODE_NOSYM               13
#define BYTEHOOK_STATUS_CODE_GET_PROT            14
#define BYTEHOOK_STATUS_CODE_SET_PROT            15
#define BYTEHOOK_STATUS_CODE_SET_GOT             16
#define BYTEHOOK_STATUS_CODE_NEW_TRAMPO          17
#define BYTEHOOK_STATUS_CODE_APPEND_TRAMPO       18
#define BYTEHOOK_STATUS_CODE_GOT_VERIFY          19
#define BYTEHOOK_STATUS_CODE_REPEATED_FUNC       20
#define BYTEHOOK_STATUS_CODE_READ_ELF            21

#define BYTEHOOK_MODE_AUTOMATIC 0
#define BYTEHOOK_MODE_MANUAL    1

#ifdef __cplusplus
extern "C" {
#endif

typedef void* bytehook_stub_t;

typedef void (*bytehook_hooked_t)(
    bytehook_stub_t task_stub,
    int status_code,
    const char *caller_path_name,
    const char *sym_name,
    void *new_func,
    void *prev_func,
    void *arg);

typedef bool (*bytehook_caller_allow_filter_t)(
    const char *caller_path_name,
    void *arg);

int bytehook_init(int mode, bool debug);

bytehook_stub_t bytehook_hook_single(
    const char *caller_path_name,
    const char *callee_path_name,
    const char *sym_name,
    void *new_func,
    bytehook_hooked_t hooked,
    void *hooked_arg);

bytehook_stub_t bytehook_hook_partial(
    bytehook_caller_allow_filter_t caller_allow_filter,
    void *caller_allow_filter_arg,
    const char *callee_path_name,
    const char *sym_name,
    void *new_func,
    bytehook_hooked_t hooked,
    void *hooked_arg);

bytehook_stub_t bytehook_hook_all(
    const char *callee_path_name,
    const char *sym_name,
    void *new_func,
    bytehook_hooked_t hooked,
    void *hooked_arg);

int bytehook_unhook(bytehook_stub_t stub);

void bytehook_set_debug(bool debug);

// for internal use
void *bytehook_get_prev_func(void *func);

// for internal use
void bytehook_pop_stack(void *return_address);

// for internal use
void *bytehook_get_return_address(void);

// for internal use
int bytehook_get_mode(void);

typedef void (*bytehook_pre_dlopen_t)(
    const char *filename,
    void *data);

typedef void (*bytehook_post_dlopen_t)(
    const char *filename,
    int result, // 0: OK  -1: Failed
    void *data);

void bytehook_add_dlopen_callback(
    bytehook_pre_dlopen_t pre,
    bytehook_post_dlopen_t post,
    void *data);

void bytehook_del_dlopen_callback(
    bytehook_pre_dlopen_t pre,
    bytehook_post_dlopen_t post,
    void *data);

#ifdef __cplusplus
}
#endif

// call previous function in hook-function
#ifdef __cplusplus
#define BYTEHOOK_CALL_PREV(func, ...) ((decltype(&(func)))bytehook_get_prev_func((void *)(func)))(__VA_ARGS__)
#else
#define BYTEHOOK_CALL_PREV(func, func_sig, ...) ((func_sig)bytehook_get_prev_func((void *)(func)))(__VA_ARGS__)
#endif

// get return address in hook-function
#define BYTEHOOK_RETURN_ADDRESS() ((void *)(BYTEHOOK_MODE_AUTOMATIC == bytehook_get_mode() ? bytehook_get_return_address() : __builtin_return_address(0)))

// pop stack in hook-function (for C/C++)
#define BYTEHOOK_POP_STACK() do{if(BYTEHOOK_MODE_AUTOMATIC == bytehook_get_mode()) bytehook_pop_stack(__builtin_return_address(0));}while(0)

// pop stack in hook-function (for C++ only)
#ifdef __cplusplus
class BytehookStackScope {
    public:
        BytehookStackScope(void *return_address) : return_address_(return_address) {
        }

        ~BytehookStackScope() {
            if(BYTEHOOK_MODE_AUTOMATIC == bytehook_get_mode())
                bytehook_pop_stack(return_address_);
        }

    private:
        void *return_address_;
};
#define BYTEHOOK_STACK_SCOPE() BytehookStackScope __bytehook_stack_scope(__builtin_return_address(0))
#endif

#endif
