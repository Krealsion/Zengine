# The Surface package — visual intent in, output out. One tiny vocabulary
# (SurfaceText + SurfaceReady + the zengine.skin role), and the Skins: the
# replaceable weaves that claim the actual surface and paint. Two terminal
# skins (classic / block — the old snake drawers' looks, now living where
# drawing lives) and one SDL skin (a real window; the proof the intent never
# leaked a TUI assumption).

# The vocabulary travels as a header-only interface target, same as input's:
# consumers spell `#include "surface/vocabulary.hpp"`. Portable — it exists
# even where no kernel does.
add_library(zengine-surface-vocabulary INTERFACE)
target_include_directories(zengine-surface-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})

# The skins are loadable weaves; they need a hosting Loom (the same honest
# question every package asks: does this install export loom::kernel?).
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: surface skins skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# Skins ask the Timer package for their own beat (skin.hpp), so every skin
# speaks that vocabulary alongside its own.
zengine_weave(zengine-skin-tui-classic skin_tui_classic.cpp)
target_link_libraries(zengine-skin-tui-classic PRIVATE zengine-surface-vocabulary
                                                       zengine-timer-vocabulary
                                                       zengine-activation)
zengine_weave(zengine-skin-tui-block skin_tui_block.cpp)
target_link_libraries(zengine-skin-tui-block PRIVATE zengine-surface-vocabulary
                                                     zengine-timer-vocabulary
                                                     zengine-activation)

# ---- The SDL skin (a real window; the agnosticism proof) --------------------------------
#
# GATED, but ON by default: only this one target sees SDL — the vocabulary, the terminal
# skins, and the pure frame-planner (skin_sdl_plan.hpp, pinned by the suite on every lane)
# stay SDL-free. Prefers a system SDL3; otherwise fetches the PINNED release tarball
# (static, checksum part of the build — the Loom's SDL2 pattern, carried to SDL3: this is a
# NEW package, so it starts on the current SDL rather than deepening the old one). With no
# display the skin still loads and runs — SDL_Init fails, the medium stays politely dark —
# and the suite drives the whole path under SDL's dummy driver.
option(ZENGINE_SDL_SKIN "Build the SDL skin (fetches a pinned static SDL3 if none is installed)" ON)

if(ZENGINE_SDL_SKIN)
    # NO_..._ENVIRONMENT_PATH: a system SDL3 is honored only from a REAL prefix
    # (CMAKE_PREFIX_PATH, the toolchain's own roots), never one derived from the
    # environment's PATH — on WSL the appended Windows PATH otherwise offers the
    # HOST'S MinGW SDL3 (PE binaries, MinGW headers) to an ELF build, and the
    # poison shows up as "#error Only Win32 target is supported!" three targets
    # later. Found once, fenced forever.
    find_package(SDL3 CONFIG QUIET NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
    if(NOT (TARGET SDL3::SDL3-static OR TARGET SDL3::SDL3))
        # On WSL building from a Windows-mounted checkout (/mnt/...), drvfs cannot hold the
        # symlinks inside SDL's tarball (and compiles dependencies slowly) — keep the fetched
        # trees on the WSL-native filesystem. Only the MODULE DEFAULT is replaced; a user's
        # own FETCHCONTENT_BASE_DIR is respected. Keyed per repo + build dir so nothing
        # shares dependency objects across flag sets (zengine-build vs zengine-build-san),
        # and nothing collides with the Loom's cache for the same build-dir names.
        if(CMAKE_SOURCE_DIR MATCHES "^/mnt/"
           AND (CMAKE_HOST_SYSTEM_VERSION MATCHES "[Mm]icrosoft" OR DEFINED ENV{WSL_DISTRO_NAME}))
            get_filename_component(zengine_bindir_name "${CMAKE_BINARY_DIR}" NAME)
            if(NOT DEFINED FETCHCONTENT_BASE_DIR
               OR FETCHCONTENT_BASE_DIR STREQUAL "${CMAKE_BINARY_DIR}/_deps")
                set(FETCHCONTENT_BASE_DIR "$ENV{HOME}/.cache/zen-fetch/zengine-${zengine_bindir_name}"
                    CACHE PATH
                    "FetchContent trees (WSL-native: drvfs cannot hold SDL's symlinks)" FORCE)
                message(STATUS "zengine: FetchContent trees under ${FETCHCONTENT_BASE_DIR} "
                               "(drvfs symlink workaround)")
            endif()
        endif()
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        # SDL3 refuses to CONFIGURE on a unix box with no X11/Wayland dev
        # libraries; this skips exactly that one guard (verified: the flag is
        # consulted nowhere else), so a headless WSL still builds the skin —
        # the suite drives it under the dummy driver, and the honest posture
        # stands in skin_sdl.cpp: no display, no photons, everything else
        # real. Where the display libs DO exist, SDL still picks them up and
        # real windows come with them.
        set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
        # The static SDL archives into a SHARED weave library: every object in
        # it must be position-independent (without this the link dies on
        # "relocation against ... in read-only section"). Directory-scoped, so
        # it rides into the fetched subproject as its default.
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
        FetchContent_Declare(sdl3_fetched
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz
            URL_HASH SHA256=f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7)
        FetchContent_MakeAvailable(sdl3_fetched)
    endif()
    if(TARGET SDL3::SDL3-static)
        set(zengine_sdl3_lib SDL3::SDL3-static)
    else()
        set(zengine_sdl3_lib SDL3::SDL3)
    endif()

    zengine_weave(zengine-skin-sdl skin_sdl.cpp)
    target_link_libraries(zengine-skin-sdl PRIVATE zengine-surface-vocabulary
                                                   zengine-timer-vocabulary zengine-activation
                                                   ${zengine_sdl3_lib})
endif()
