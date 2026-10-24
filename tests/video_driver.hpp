// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_VIDEO_DRIVER_HPP
#define ZENGINE_TESTS_VIDEO_DRIVER_HPP

// SDL's video driver, named for this process while a `VideoDriver` lives, as a weaver's environment
// names it, and given back as it was after: the OS window (`zengine-window-sdl`) told a driver SDL
// lacks cannot open, and refuses to be made in SDL's words. The load and shapes suites hold it.

#include <cstdlib>
#include <optional>
#include <string>

namespace zengine::testing {

struct VideoDriver {
    static constexpr const char* kNames[] = {"SDL_VIDEO_DRIVER", "SDL_VIDEODRIVER"};
    std::optional<std::string> was[2];

    explicit VideoDriver(const char* driver) {
        for (int i = 0; i < 2; ++i) {
            if (const char* v = std::getenv(kNames[i])) {
                was[i] = v;
            }
            put(kNames[i], driver);
        }
    }
    ~VideoDriver() {
        for (int i = 0; i < 2; ++i) {
            put(kNames[i], was[i] ? was[i]->c_str() : nullptr);
        }
    }
    VideoDriver(const VideoDriver&) = delete;
    VideoDriver& operator=(const VideoDriver&) = delete;

    /// `value` for `name`, or `name` removed when there is none.
    static void put(const char* name, const char* value) {
#if defined(_WIN32)
        ::_putenv_s(name, value != nullptr ? value : "");
#else
        if (value != nullptr) {
            ::setenv(name, value, 1);
        } else {
            ::unsetenv(name);
        }
#endif
    }
};

} // namespace zengine::testing

#endif // ZENGINE_TESTS_VIDEO_DRIVER_HPP
