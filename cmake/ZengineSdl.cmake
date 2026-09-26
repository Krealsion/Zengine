# The SDL dependency, acquired once for every package that needs it. The window Skin and the SDL
# Input reader are two weave libraries the Loom loads with local symbols (dlopen RTLD_LOCAL,
# LoadLibraryA), so each holding an archived static SDL would hold its own: two event queues, and
# a reader polling a queue the window never posts to. So SDL is shared, and a static one is
# refused out loud. `ZENGINE_SDL_SKIN=OFF` declines the whole SDL-backed set.

# Extracted archives are stamped at extraction (CMP0135), so a newly pinned tarball's sources are
# newer than the last pin's objects and rebuild them. At file scope, since the SDL_ttf pins below
# are reached even when a system SDL3 skips the first fetch; guarded for the older CMake
# (VM-PLAT-12).
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(ZENGINE_SDL_SKIN
       "Build the SDL-backed weaves — the window Skin and the SDL Input reader (fetches a pinned shared SDL3 if none is installed)"
       ON)

# What a package links, and what a host must stage beside itself; empty when this configuration
# has no SDL, which is declared rather than assumed (the manifest's `sdl` gate). The runtime is a
# list: SDL3 and SDL3_ttf are two shared libraries a Windows host must find beside itself.
set(ZENGINE_SDL_LIB "")
set(ZENGINE_SDL_TEXT_LIB "")
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

# zengine_stage_sdl(<host target>) -- put the SDL runtime beside a host that loads SDL-backed
# weaves; a no-op where SDL came from the system or there is none. Both helpers are defined
# before the early return below, so a host asks without an `if` of its own, which the lane with
# no SDL, the Windows stranger lane, is the least likely to test.
function(zengine_stage_sdl host)
    foreach(runtime IN LISTS ZENGINE_SDL_RUNTIME)
        add_custom_command(TARGET ${host} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${runtime} $<TARGET_FILE_DIR:${host}>
            COMMENT "${host}: staging ${runtime} beside the host")
    endforeach()
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
    # SDL3 refuses to configure on a unix box with no X11 or Wayland development libraries; this
    # skips only that guard (the flag is read nowhere else), so a headless WSL builds the SDL
    # weaves the suites drive under the dummy and offscreen drivers. Where display libraries
    # exist, SDL still uses them.
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

# What a host must put beside its executable, only for an SDL we built: a system SDL3 is where
# the platform's loader looks. On ELF the weaves carry $ORIGIN (set where they are defined); on
# Windows the DLL must sit in the application directory, which this copies.
if(zengine_sdl_fetched)
    if(WIN32)
        list(APPEND ZENGINE_SDL_RUNTIME "$<TARGET_FILE:SDL3::SDL3>")
    else()
        list(APPEND ZENGINE_SDL_RUNTIME "$<TARGET_SONAME_FILE:SDL3::SDL3>")
    endif()
endif()

# ---- SDL_ttf: a real typeface for the graphical Skin -------------------------------------
# A font engine that pairs with this SDL3 (docs/reference/surface.md says why the Skin needs one).
# SDL_ttf requires FreeType and its tarball does not bundle it, and both of its doors were
# measured shut here: SDLTTF_VENDORED=OFF finds no FreeType on the WSL lane or in CLion's MinGW,
# and ON needs external/freetype to exist. So FreeType is fetched, pinned, into that directory;
# HarfBuzz and PlutoSVG (shaping, colour emoji) are off, neither being readability.
include(FetchContent) # already included above when SDL3 itself was fetched; idempotent

if(NOT TARGET SDL3_ttf::SDL3_ttf)
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
    set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)

    # Populated, not added: `zen-not-a-project` contains no CMakeLists.txt, which is
    # how FetchContent is told to fetch and stop. The tree is then completed below
    # and added by hand, in that order, because SDL_ttf's own configure step is what
    # requires the completed tree.
    FetchContent_Declare(sdl3_ttf_fetched
        URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-3.2.2.tar.gz
        URL_HASH SHA256=63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d
        SOURCE_SUBDIR zen-not-a-project)
    FetchContent_MakeAvailable(sdl3_ttf_fetched)

    # FreeType lands INSIDE SDL_ttf's own external/ directory, which is the path its
    # vendored branch reads. Same populate-without-adding form, so nothing here adds
    # FreeType to the build either -- SDL_ttf's add_subdirectory below is what does,
    # with the options it wants set on it.
    FetchContent_Declare(freetype_fetched
        URL https://downloads.sourceforge.net/project/freetype/freetype2/2.14.3/freetype-2.14.3.tar.gz
        URL_HASH SHA256=e61b31ab26358b946e767ed7eb7f4bb2e507da1cfefeb7a8861ace7fd5c899a1
        SOURCE_DIR "${sdl3_ttf_fetched_SOURCE_DIR}/external/freetype"
        SOURCE_SUBDIR zen-not-a-project)
    FetchContent_MakeAvailable(freetype_fetched)

    # THE ONE WAY THIS ASSEMBLY CAN COME APART, named before it happens. The two
    # contents have independent download stamps, and re-populating SDL_ttf wipes
    # its source tree -- external/freetype with it -- while FreeType's stamp still
    # says "done". The result would be SDL_ttf's own FATAL about missing sources,
    # which is true and unhelpful, three layers down somebody else's CMakeLists.
    # So the assembly checks itself, and says what to delete.
    if(NOT EXISTS "${sdl3_ttf_fetched_SOURCE_DIR}/external/freetype/CMakeLists.txt")
        message(FATAL_ERROR
            "zengine: SDL_ttf's vendored FreeType is missing from\n"
            "  ${sdl3_ttf_fetched_SOURCE_DIR}/external/freetype\n"
            "This happens when SDL_ttf was re-downloaded (wiping its tree) while FreeType's\n"
            "own FetchContent stamp still says it is populated. Delete the FreeType content\n"
            "and configure again:\n"
            "  rm -rf \"${FETCHCONTENT_BASE_DIR}/freetype_fetched-subbuild\"")
    endif()

    add_subdirectory("${sdl3_ttf_fetched_SOURCE_DIR}" "${sdl3_ttf_fetched_BINARY_DIR}"
                     EXCLUDE_FROM_ALL)
    set(zengine_sdl_ttf_fetched ON)
endif()

if(NOT TARGET SDL3_ttf::SDL3_ttf)
    message(FATAL_ERROR
        "zengine: ZENGINE_SDL_SKIN is ON but no SDL3_ttf::SDL3_ttf target exists after "
        "FetchContent. Configure with -DZENGINE_SDL_SKIN=OFF to build without SDL.")
endif()

set(ZENGINE_SDL_TEXT_LIB SDL3_ttf::SDL3_ttf)

if(zengine_sdl_ttf_fetched)
    if(WIN32)
        list(APPEND ZENGINE_SDL_RUNTIME "$<TARGET_FILE:SDL3_ttf::SDL3_ttf>")
    else()
        list(APPEND ZENGINE_SDL_RUNTIME "$<TARGET_SONAME_FILE:SDL3_ttf::SDL3_ttf>")
    endif()
endif()
