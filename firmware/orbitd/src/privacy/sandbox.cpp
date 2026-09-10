/**
 * @file sandbox.cpp
 * @brief Seccomp and privilege isolation implementation.
 */
#include "privacy/sandbox.h"
#include <iostream>
#include <unistd.h>
#include <sys/prctl.h>

// seccomp and capability headers (Linux-specific)
#ifdef __linux__
#include <seccomp.h>
#include <sys/capability.h>
#endif

namespace orbit::privacy {

bool Sandbox::apply() {
#ifdef __linux__
    // 1. Set no_new_privs — prevents gaining privileges via execve
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        std::cerr << "[sandbox] Failed to set no_new_privs" << std::endl;
        return false;
    }

    // 2. Apply seccomp filter — whitelist essential syscalls
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
    if (!ctx) {
        std::cerr << "[sandbox] seccomp_init failed" << std::endl;
        return false;
    }

    // Whitelist essential syscalls
    const int allowed_syscalls[] = {
        SCMP_SYS(read), SCMP_SYS(write), SCMP_SYS(close),
        SCMP_SYS(fstat), SCMP_SYS(lseek), SCMP_SYS(mmap),
        SCMP_SYS(mprotect), SCMP_SYS(munmap), SCMP_SYS(brk),
        SCMP_SYS(ioctl),   // For ALSA and watchdog
        SCMP_SYS(futex),   // For threading
        SCMP_SYS(clone),   // For threading
        SCMP_SYS(openat),  // File access
        SCMP_SYS(mlock), SCMP_SYS(munlock),  // Volatile memory
        SCMP_SYS(madvise),
        SCMP_SYS(clock_gettime), SCMP_SYS(clock_nanosleep),
        SCMP_SYS(nanosleep),
        SCMP_SYS(sched_setscheduler), SCMP_SYS(sched_yield),
        SCMP_SYS(set_robust_list), SCMP_SYS(get_robust_list),
        SCMP_SYS(rt_sigaction), SCMP_SYS(rt_sigprocmask),
        SCMP_SYS(exit_group), SCMP_SYS(exit),
        SCMP_SYS(getrandom),
        SCMP_SYS(prctl),
        SCMP_SYS(sched_getaffinity), SCMP_SYS(sched_setaffinity),
    };

    for (int sc : allowed_syscalls) {
        seccomp_rule_add(ctx, SCMP_ACT_ALLOW, sc, 0);
    }

    // BLOCK: All network syscalls
    // socket, connect, bind, listen, accept, sendto, recvfrom — all killed

    if (seccomp_load(ctx) != 0) {
        std::cerr << "[sandbox] seccomp_load failed" << std::endl;
        seccomp_release(ctx);
        return false;
    }
    seccomp_release(ctx);

    std::cout << "[sandbox] Applied: no_new_privs + seccomp whitelist ("
              << sizeof(allowed_syscalls) / sizeof(int)
              << " syscalls allowed)" << std::endl;
    return true;
#else
    std::cout << "[sandbox] Not on Linux — sandbox disabled" << std::endl;
    return true;
#endif
}

}  // namespace orbit::privacy
