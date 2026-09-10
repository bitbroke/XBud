/**
 * @file sandbox.h / .cpp
 * @brief Process isolation via privilege dropping and seccomp.
 */
#pragma once

namespace orbit::privacy {

class Sandbox {
public:
    Sandbox() = default;

    /**
     * @brief Apply security sandbox:
     *   1. Drop to unprivileged user
     *   2. chroot to /opt/orbitd
     *   3. Drop all capabilities except CAP_NET_RAW + CAP_SYS_NICE
     *   4. Apply seccomp filter (whitelist ~30 syscalls)
     */
    bool apply();
};

}  // namespace orbit::privacy
