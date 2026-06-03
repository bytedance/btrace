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
// Created on 2026/4/7.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "SlowSyscallProxy.h"

#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/epoll.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/msg.h>
 #include <sys/sem.h>
#include <time.h>

#include "StackTrace.h"
#include "OSThread.h"
#include "util/hook_utils.h"

namespace btrace {


class SlowSyscallScopedHookAction {
public:
    static void bindAction(bool disable_sig_mask, int sig, SlowSysCallProxy::Action f) {
        sig_no_ = sig;
        action_ = f;
        disable_sig_mask_ = disable_sig_mask;
    }
    
    SlowSyscallScopedHookAction(Type type, void* fp) : type_(type), fp_(fp) {
        disable_interrupt_ = true;
        action_(type_, fp_, &disable_interrupt_);
        
        if (!disable_sig_mask_) {
            sigset_t sig_set;
            sigemptyset(&sig_set);
            sigaddset(&sig_set, sig_no_);
            pthread_sigmask(SIG_BLOCK, &sig_set, &ori_sig_mask_);
        }
    }
    
    ~SlowSyscallScopedHookAction() {
        if (!disable_sig_mask_) {
            pthread_sigmask(SIG_SETMASK, &ori_sig_mask_, NULL);
        }
        
        disable_interrupt_ = false;
        action_(type_, fp_, &disable_interrupt_);
    }
private:
    static inline int sig_no_;
    static inline SlowSysCallProxy::Action action_;
    static inline bool disable_sig_mask_ = false;
    Type type_;
    void* fp_;
    bool disable_interrupt_ = false;
    sigset_t ori_sig_mask_;
};

template<Type kType, class Fp>
class SlowSyscallHooker;

template<Type kType, class Rp, class... ArgTypes>
class SlowSyscallHooker<kType, Rp(ArgTypes...)> {
public:
    static void hook(const char* name) {
//        OH_LOG_INFO(LOG_APP, "hooking %{public}s", name);
        hook_all(name, (void *)proxy, (void **)&origin);
    }
private: 
    using F = Rp(*)(ArgTypes...);
    static Rp proxy(ArgTypes... args) {
        SlowSyscallScopedHookAction sha(kType, __builtin_frame_address(0));
        return reinterpret_cast<F>(origin)(args...);
    }
    static void* origin;

    SlowSyscallHooker() = delete;
    ~SlowSyscallHooker() = delete;
};

#define FUNCTION_HOOKER_GENERATOR(alias_name, type, sig) \
using alias_name = SlowSyscallHooker<type, sig>; \
template<> void* alias_name::origin = nullptr;

// "Input" socket interfaces
FUNCTION_HOOKER_GENERATOR(AcceptHooker, Type::kAccept, int(int, struct sockaddr *__restrict, socklen_t *__restrict))
FUNCTION_HOOKER_GENERATOR(RecvHooker, Type::kRecv, ssize_t(int, void *, size_t, int))
FUNCTION_HOOKER_GENERATOR(RecvfromHooker, Type::kRecvfrom, ssize_t(int, void *__restrict, size_t, int, struct sockaddr *__restrict, socklen_t *__restrict))
FUNCTION_HOOKER_GENERATOR(RecvmmsgHooker, Type::kRecvmmsg, int(int, struct mmsghdr *, unsigned int, unsigned int, struct timespec *))
FUNCTION_HOOKER_GENERATOR(RecvmsgHooker, Type::kRecvmsg, ssize_t(int, struct msghdr *, int))
// "Output" socket interfaces
FUNCTION_HOOKER_GENERATOR(ConnectHooker, Type::kConnect, int(int, const struct sockaddr *, socklen_t))
FUNCTION_HOOKER_GENERATOR(SendHooker, Type::kSend, ssize_t(int, const void *, size_t, int))
FUNCTION_HOOKER_GENERATOR(SendtoHooker, Type::kSendto, ssize_t(int, const void *, size_t, int, const struct sockaddr *, socklen_t))
FUNCTION_HOOKER_GENERATOR(SendmsgHooker, Type::kSendmsg, ssize_t(int, const struct msghdr *, int))
// Interfaces used to wait for signals
FUNCTION_HOOKER_GENERATOR(PauseHooker, Type::kPause, int(void))
FUNCTION_HOOKER_GENERATOR(SigsuspendHooker, Type::kSigsuspend, int(const sigset_t *))
FUNCTION_HOOKER_GENERATOR(SigtimedwaitHooker, Type::kSigtimedwait, int(const sigset_t *__restrict, siginfo_t *__restrict, const struct timespec *__restrict))
FUNCTION_HOOKER_GENERATOR(SigwaitinfoHooker, Type::kSigwaitinfo, int(const sigset_t *__restrict, siginfo_t *__restrict))
// File descriptor multiplexing interfaces
FUNCTION_HOOKER_GENERATOR(EpollWaitHooker, Type::kEpollWait, int(int, struct epoll_event *, int, int))
FUNCTION_HOOKER_GENERATOR(EpollPwaitHooker, Type::kEpollPwait, int(int, struct epoll_event *, int, int, const sigset_t *))
FUNCTION_HOOKER_GENERATOR(PollHooker, Type::kPoll, int(struct pollfd *, nfds_t, int))
FUNCTION_HOOKER_GENERATOR(PpollHooker, Type::kPpoll, int(struct pollfd *, nfds_t, const struct timespec *, const sigset_t *));
FUNCTION_HOOKER_GENERATOR(SelectHooker, Type::kSelect, int(int, fd_set *__restrict, fd_set *__restrict, fd_set *__restrict, struct timeval *__restrict))
FUNCTION_HOOKER_GENERATOR(PselectHooker, Type::kPselect, int(int, fd_set *__restrict, fd_set *__restrict, fd_set *__restrict, const struct timespec *__restrict, const sigset_t *__restrict))
// System V IPC interfaces
FUNCTION_HOOKER_GENERATOR(MsgrcvHooker, Type::kMsgrcv, ssize_t(int, void *, size_t, long, int))
FUNCTION_HOOKER_GENERATOR(MsgsndHooker, Type::kMsgsnd, int(int, const void *, size_t, int))
FUNCTION_HOOKER_GENERATOR(SemopHooker, Type::kSemop, int(int, struct sembuf *, size_t))
FUNCTION_HOOKER_GENERATOR(SemtimedopHooker, Type::kSemtimedop, int(int, struct sembuf *, size_t, const struct timespec *))
// Sleep interfaces
FUNCTION_HOOKER_GENERATOR(ClockNanosleepHooker, Type::kClockNanosleep, int(clockid_t, int, const struct timespec *, struct timespec *))
FUNCTION_HOOKER_GENERATOR(NanosleepHooker, Type::kNanosleep, int(const struct timespec *, struct timespec *))
FUNCTION_HOOKER_GENERATOR(UsleepHooker, Type::kUsleep, int(unsigned))
FUNCTION_HOOKER_GENERATOR(SleepHooker, Type::kSleep, unsigned(unsigned))
//// Others
//FUNCTION_HOOKER_GENERATOR(IoGeteventsHooker, Type::kIoGetevents, ssize_t(int, const void*, size_t))


bool SlowSysCallProxy::Setup(bool disableSigMask, int sig, Action hookAction) {
    hook_init();

    SlowSyscallScopedHookAction::bindAction(disableSigMask, sig, hookAction);
    std::call_once(slow_syscall_proxy_flag_, InstallProxy);

    return true;
}

void SlowSysCallProxy::InstallProxy() {
    // "Input" socket interfaces
    AcceptHooker::hook("accept");
    RecvHooker::hook("recv");
    RecvfromHooker::hook("recvfrom");
    RecvmmsgHooker::hook("recvmmsg");
    RecvmsgHooker::hook("recvmsg");
    
    // "Output" socket interfaces
    ConnectHooker::hook("connect");
    SendHooker::hook("send");
    SendtoHooker::hook("sendto");
    SendmsgHooker::hook("sendmsg");

    // Interfaces used to wait for signals
    PauseHooker::hook("pause");
    SigsuspendHooker::hook("sigsuspend");
    SigtimedwaitHooker::hook("sigtimedwait");
    SigwaitinfoHooker::hook("sigwaitinfo");

    // File descriptor multiplexing interfaces
    EpollWaitHooker::hook("epoll_wait");
    EpollPwaitHooker::hook("epoll_pwait");
    PollHooker::hook("poll");
    PpollHooker::hook("ppoll");
    SelectHooker::hook("select");
    PselectHooker::hook("pselect");

    // System V IPC interfaces
    MsgrcvHooker::hook("msgrcv");
    MsgsndHooker::hook("msgsnd");
    SemopHooker::hook("semop");
    SemtimedopHooker::hook("semtimedop");

    // Sleep interfaces
    ClockNanosleepHooker::hook("clock_nanosleep");
    NanosleepHooker::hook("nanosleep");
    UsleepHooker::hook("usleep");
    SleepHooker::hook("sleep");
}

}