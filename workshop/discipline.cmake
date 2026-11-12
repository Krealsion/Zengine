# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# Workshop's own build discipline, declared as Zengine declares its own beside Loom's link-only
# one: `workshop-warnings` and `workshop-sanitize` (WORKSHOP_SANITIZE, Zengine's sanitizer switch
# unless given), and the pair every weave of the including folder takes (ZENGINE_WEAVE_DISCIPLINE).
if(NOT TARGET workshop-warnings)
    add_library(workshop-warnings INTERFACE)
    target_compile_options(workshop-warnings INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion
        -Wsign-conversion -Werror>)
    if(MINGW)
        target_compile_options(workshop-warnings INTERFACE -Wa,-mbig-obj)
    endif()
    if(MSVC)
        target_compile_options(workshop-warnings INTERFACE /bigobj)
    endif()
    if(NOT DEFINED WORKSHOP_SANITIZE)
        set(WORKSHOP_SANITIZE "${ZENGINE_SANITIZE}")
    endif()
    add_library(workshop-sanitize INTERFACE)
    if(WORKSHOP_SANITIZE)
        target_compile_options(workshop-sanitize INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        target_link_options(workshop-sanitize INTERFACE -fsanitize=address,undefined)
    endif()
endif()
set(ZENGINE_WEAVE_DISCIPLINE workshop-warnings workshop-sanitize)
