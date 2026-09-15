// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A HARMLESS STAND-IN FOR WORKSHOP'S HOST, for the development launch's cases that need separate
// processes (tests/test_workshop_files.cpp). A launch starts it from a runtime in a project
// directory; it writes `started <pid>` to `hosts.log` there, so a case can count the hosts that
// started, waits until the case creates `host-gate` there, then writes `ended <pid>` and exits 0.
// It opens no window and loads nothing, and it gives up after two minutes so a failed case cannot
// leave it waiting forever.

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

int main() {
#if defined(_WIN32)
    const long pid = static_cast<long>(::_getpid());
#else
    const long pid = static_cast<long>(::getpid());
#endif
    {
        std::ofstream log("hosts.log", std::ios::app);
        log << "started " << pid << "\n";
    }
    for (int waited = 0; waited < 2400; ++waited) {
        std::error_code ec;
        if (std::filesystem::exists("host-gate", ec)) {
            std::ofstream log("hosts.log", std::ios::app);
            log << "ended " << pid << "\n";
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 3;
}
