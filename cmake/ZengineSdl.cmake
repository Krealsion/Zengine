# The SDL dependency — ONE acquisition, for every package that needs it.
#
# WHY THIS FILE EXISTS, AND WHY SDL IS NOW SHARED (G-1).
#
# Until G-1 exactly one target saw SDL — the window Skin — so `surface/` fetched
# a STATIC SDL3 and archived it into that one weave library. G-1 adds a second
# SDL-backed weave, the SDL Input reader, and a static SDL cannot serve both:
#
#   the Loom loads weaves with dlopen(RTLD_LOCAL) on ELF and LoadLibraryA on
#   Windows, so each weave library's symbols are its own. Two libraries each
#   carrying an archived copy of SDL are two INDEPENDENT SDLs: two event
#   queues, two video subsystems, two SDL_Init refcounts. The reader would
#   poll a queue the window it is meant to listen for was never posted to.
#
# That is not a packaging preference, it is the Loom's isolation discipline
# meeting SDL's process-global event queue. SDL's queue has exactly one owner
# (the Input reader) and SDL's window has exactly one owner (the Skin), and both
# of those facts require the two weaves to be talking to ONE SDL. So the library
# is shared, and the check below refuses a static one out loud rather than
# building the broken thing quietly.
#
# The option keeps its name. `ZENGINE_SDL_SKIN=OFF` is the spelling the Windows
# stranger lane and this repo's docs already use, and renaming it would move
# every lane's invocation for nothing; what it gates is now stated as the whole
# SDL-backed set rather than the skin alone.

option(ZENGINE_SDL_SKIN
       "Build the SDL-backed weaves — the window Skin and the SDL Input reader (fetches a pinned shared SDL3 if none is installed)"
       ON)

# What a package links, and what a host must stage beside itself. Empty when
# this configuration has no SDL at all, which is a legitimate configuration and
# is declared rather than assumed (tests/test_population.txt's `sdl` gate).
set(ZENGINE_SDL_LIB "")
set(ZENGINE_SDL_RUNTIME "")

# zengine_sdl_weave(<target>) — a weave that links SDL. One place, so the two
# SDL-backed weaves cannot come to disagree about how they reach it.
function(zengine_sdl_weave target)
    target_link_libraries(${target} PRIVATE ${ZENGINE_SDL_LIB})
    if(NOT WIN32)
        # A staged copy of this weave sits beside the host, and so does the
        # staged SDL. The automatic build RPATH already covers running from the
        # build tree; $ORIGIN covers the copy.
        set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "$ORIGIN")
    endif()
endfunction()

# zengine_stage_sdl(<host target>) — put the SDL runtime beside a host that
# loads SDL-backed weaves. A no-op where SDL came from the system, and a no-op
# where there is no SDL at all: BOTH helpers are defined before the early return
# below, because a host says "stage whatever SDL this configuration needs"
# without knowing whether the answer is "none". A function that exists only in
# the SDL configuration would make every caller carry an `if` about it — and the
# Windows stranger lane, which is the configuration with no SDL, is exactly the
# lane least likely to be the one someone tests that `if` on.
function(zengine_stage_sdl host)
    if(ZENGINE_SDL_RUNTIME)
        add_custom_command(TARGET ${host} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${ZENGINE_SDL_RUNTIME} $<TARGET_FILE_DIR:${host}>
            COMMENT "${host}: staging the shared SDL3 beside the host")
    endif()
endfunction()

if(NOT ZENGINE_SDL_SKIN)
    message(STATUS "zengine: SDL-backed weaves skipped (ZENGINE_SDL_SKIN=OFF)")
    return()
endif()

# NO_..._ENVIRONMENT_PATH: a system SDL3 is honored only from a REAL prefix
# (CMAKE_PREFIX_PATH, the toolchain's own roots), never one derived from the
# environment's PATH — on WSL the appended Windows PATH otherwise offers the
# HOST'S MinGW SDL3 (PE binaries, MinGW headers) to an ELF build, and the poison
# shows up as "#error Only Win32 target is supported!" three targets later.
# Found once, fenced forever.
find_package(SDL3 CONFIG QUIET NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH)

if(NOT TARGET SDL3::SDL3)
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
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    # SDL3 refuses to CONFIGURE on a unix box with no X11/Wayland dev
    # libraries; this skips exactly that one guard (verified: the flag is
    # consulted nowhere else), so a headless WSL still builds the SDL weaves —
    # the suites drive them under the dummy/offscreen drivers, and the honest
    # posture stands in skin_sdl.cpp: no display, no photons, everything else
    # real. Where the display libs DO exist, SDL still picks them up and real
    # windows come with them.
    set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    FetchContent_Declare(sdl3_fetched
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz
        URL_HASH SHA256=f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7)
    FetchContent_MakeAvailable(sdl3_fetched)
    set(zengine_sdl_fetched ON)
endif()

if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR
        "zengine: ZENGINE_SDL_SKIN is ON but no SDL3::SDL3 target exists after find_package "
        "and FetchContent. Configure with -DZENGINE_SDL_SKIN=OFF to build without SDL.")
endif()

# THE SHARED CHECK, out loud. See the header note: two weave libraries each
# holding their own archived SDL are two SDLs, and the symptom is a graphical
# Workshop whose window is deaf while every test passes. A configuration that
# cannot satisfy the requirement says so here, at configure time, naming the
# reason — rather than producing a build whose defect only a live window shows.
set(zengine_sdl_real SDL3::SDL3)
get_target_property(zengine_sdl_alias SDL3::SDL3 ALIASED_TARGET)
if(zengine_sdl_alias)
    set(zengine_sdl_real ${zengine_sdl_alias})
endif()
get_target_property(zengine_sdl_type ${zengine_sdl_real} TYPE)
if(NOT zengine_sdl_type STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR
        "zengine: SDL3::SDL3 is a ${zengine_sdl_type}, and Zengine needs a SHARED SDL3.\n"
        "WHY\n"
        "  Two weave libraries link SDL here — the window Skin (surface/) and the SDL Input\n"
        "  reader (input/). The Loom loads weaves with dlopen(RTLD_LOCAL) / LoadLibraryA, so a\n"
        "  statically archived SDL would give each of them its own event queue and its own\n"
        "  SDL_Init state. The reader would poll a queue the Skin's window never posts to, and\n"
        "  nothing would fail until a real window sat there deaf.\n"
        "WHAT TO DO\n"
        "  Install a shared SDL3, or remove the static one from CMAKE_PREFIX_PATH so this build\n"
        "  fetches its own, or configure with -DZENGINE_SDL_SKIN=OFF.")
endif()

set(ZENGINE_SDL_LIB SDL3::SDL3)

# What a host must put beside its executable. Only for an SDL WE BUILT: a system
# SDL3 is already wherever the platform's loader looks, and asking a generator
# expression for an imported target's SONAME file is not answerable in general.
#
# On ELF the weave libraries also carry $ORIGIN (set where they are defined), so
# a staged copy finds a staged SDL; on Windows there is no such thing and the
# DLL must simply be in the application directory, which is what this copies.
if(zengine_sdl_fetched)
    if(WIN32)
        set(ZENGINE_SDL_RUNTIME "$<TARGET_FILE:SDL3::SDL3>")
    else()
        set(ZENGINE_SDL_RUNTIME "$<TARGET_SONAME_FILE:SDL3::SDL3>")
    endif()
endif()
