// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-workshop-develop: the development launch (workshop/develop.hpp) with this build tree's
// facts, which workshop/CMakeLists.txt compiles in. Its output is its own lines, then the runtime
// script's and the Workshop's as they arrive, byte for byte.

#include "workshop/develop.hpp"

#include <cstdio>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

// WL-CODE-08 -- agents/workshop/code.md
int main(int argc, char** argv) {
#if defined(_WIN32)
    // What the children wrote is passed on as they wrote it, line endings included.
    (void)::_setmode(::_fileno(stdout), _O_BINARY);
#endif
    namespace develop = zengine::workshop::develop;
    const develop::Facts facts{ZENGINE_DEVELOP_CMAKE,   ZENGINE_DEVELOP_SCRIPT,
                               ZENGINE_DEVELOP_BUILD,   ZENGINE_DEVELOP_RUNTIME,
                               ZENGINE_DEVELOP_PROJECT, ZENGINE_DEVELOP_HOST,
                               ZENGINE_DEVELOP_PLAN,    ZENGINE_DEVELOP_CATALOG};
    const std::vector<std::string> args =
        argc > 1 ? std::vector<std::string>(argv + 1, argv + argc) : std::vector<std::string>();
    develop::World world;
    world.in_use = &develop::image_in_use;
    world.run = [](const zengine::builder::BuildCommand& command) {
        return zengine::builder::run_forwarding(command, stdout);
    };
    world.directory = &develop::project_directory;
    world.say = [](const std::string& line) {
        std::printf("zengine-workshop-develop - %s\n", line.c_str());
        std::fflush(stdout);
    };
    return develop::launch(facts, develop::choose(args, facts), world);
}
