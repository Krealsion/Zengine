# Supported toolchains and platforms

**Contributing.** Which compilers and platforms this repository builds on, what each one does
and does not cover, and the platform rules that are load-bearing rather than incidental.

Every row below is classified from measurement on lanes that exist, not from a flag's spelling.

## The matrix

| platform / compiler | state | notes |
|---|---|---|
| **Linux (incl. WSL) / GCC 11.4+** | **fully supported.** The canonical lane | The Loom's OS sandbox exists only here |
| Linux / Clang | builds | Not a routine lane; the sanitizer flags are the same |
| **Windows / MinGW-w64 GCC 13.1+** (libstdc++) x64 | **supported.** The required Windows lane, and the build this project is developed on day to day | A program's runtime DLLs must be on `PATH` or beside it; every weave carries its own. Loom's weave build contract refuses a libstdc++ built without the C runtime's `__cxa_thread_atexit`, so CLion 2025.2's bundled GCC 13.1 builds no weave, and no Zengine. Binutils 2.40 (that bundled MinGW's) assembles the Workshop application's translation units since Workshop's bodies were split out of its headers and compiled once (measured 2026-09-03: the host translation unit into a 28.6 MB object); before that it refused the application target with `file too big` while 2.45 assembled it, and the test suites built either way |
| **Windows / MSVC 19.50** (Visual Studio 2026) x64 | **supported.** The advisory Windows lane, and the toolchain released Windows users are expected to build with | clang-cl and ARM64 are **unverified** |
| macOS / Apple Clang | **never built.** Unclassified | Not the same as "unsupported" |

Both Windows toolchains are meant to be supported, and neither is evidence for the other: they
are two standard libraries, and a Windows answer in this documentation names the one it was
measured on. The MinGW-w64 lane is **required** — its red is the run's red — and the MSVC
lane is **advisory** — its red is reported in the run's job list and does not fail the run —
until MSVC can be proven routinely outside hosted CI.

Target the **C++20** floor. Avoid features needing GCC 12 or newer.

## Windows

### There is no OS isolation on Windows

The Loom's isolation backend is Linux-only. On Windows the Kernel exists as an explicit
**development and demo backend** — it loads libraries and hosts weaves, and it isolates
nothing. It prints a banner saying so, and `Kernel::containment_note()` says so at every
surface. Read that sentence literally; it is not boilerplate.

Because the demos need a kernel, asking for `-DZEN_LOOM_DEV=ON` on Windows *is* asking for the
demo workflow, so it defaults `LOOM_ENABLE_WINDOWS_KERNEL=ON`. An explicit
`-DLOOM_ENABLE_WINDOWS_KERNEL=OFF` on the command line still wins.

Packages that need a kernel gate on `if(TARGET loom::kernel)`, so a Windows Loom install
without one still configures — the package simply reports that it is skipped and why.

### Every program runs in the UTF-8 code page

Every program this repository's CMake project defines on Windows — Workshop, its tools and every
test program — carries a manifest naming UTF-8 its active code page, which Windows 10 version 1903
and later honour. So `argv`, the environment and every narrow path are UTF-8 on Windows: a folder
named with any letter, given as an argument or holding `%APPDATA%` or `%LOCALAPPDATA%`, is read
and written as itself on both toolchains, and a pane image loads from it. One step at the end of
the top-level `CMakeLists.txt` gives the manifest to every executable the project defines
(`cmake/ZengineCodePage.cmake`); a sibling Loom's programs under the sibling override keep their
own, and a program built against the installed package takes it by one line,
`zengine_code_page(<target>)` ([getting started](../getting-started.md#using-zengine-from-another-project)).
It keeps what the toolchain's own manifest says:

- **MSVC**'s linker writes a manifest naming the execution level, and CMake merges this one into
  it.
- **MinGW-w64** may link a default manifest into every program — MSYS2's names the execution
  level and the Windows versions the program supports — and a program's own replaces it, so this
  one is that default with the code page added, written into a resource script and compiled by
  CMake's resource compiler (`CMAKE_RC_COMPILER`, a `windres` CMake finds when none is named). A
  toolchain that links no default gets the code page alone.

A doctest program that finds itself in any other code page refuses to run (exit 71), so the
Windows lanes test the narrow strings that ship. The console keeps its own code page: the
terminal window sets its output to UTF-8 while it draws, and a line printed to a console before
that is shown in the console's page.

### The paths the build tools carry

The manifest above is Zengine's programs' own: it does not change how the build tools read a
path. Under MinGW-w64 several of them read their command line, or convert a path, in the system's
code page, so a letter outside that page in a path they are given can stop a build that the same
letter in a running program's path does not. Measured on Windows 11 with the system code page
1252, a folder named `Zoë Ж` (`Ж` is outside 1252), MSYS2's GCC 15.2 and binutils 2.45, CMake 4.1,
Ninja 1.13.1, MSYS2's `mingw32-make` 4.4.1 and ccache 4.11.3:

| the path | what carries it | measured |
|---|---|---|
| the build directory | Ninja; GCC's assembler and linker, given each object relative to it | **builds**, with or without ccache, the code-page manifest's resource step included, and a changed header rebuilds what includes it, while the source and every prefix lie elsewhere |
| the build directory | MinGW Makefiles | builds, but its dependency step prints `CMake Error: Target DependInfo.cmake file not found` and keeps no header dependency: a changed header rebuilds nothing, and the build exits 0 with the stale program |
| the source directory | the GCC driver and compiler, given each source by its absolute path | **builds** under Ninja, a changed header tracked |
| the source directory | MinGW Makefiles | **fails**: `No rule to make target '<source>'` |
| the source directory | ccache as the compiler launcher | **fails** on the first compile: `filesystem error: Cannot convert character sequence: Illegal byte sequence` |
| a dependency prefix (`CMAKE_PREFIX_PATH`) | the compiler, for its headers; the linker, for its libraries by absolute path | headers read; the link **fails**: `ld.exe: cannot find <path>: Invalid argument`, the path printed mangled |
| any file GCC's assembler (`as`) or linker (`ld`) is given by an absolute path | binutils, reading the command line in the system code page | **fails**; the same file named relative to the working directory is read |
| a running program's paths: `argv`, the environment, files it opens | the program | read as themselves, on both toolchains ([above](#every-program-runs-in-the-utf-8-code-page)) |

So the route, where any path holds such a letter: generate with **Ninja**, and build **without a
compiler launcher** (leave `CMAKE_CXX_COMPILER_LAUNCHER` unset) while the source path holds it.
Install the **Loom prefix**, and every other dependency prefix, under a path the system code page
can spell. Under Ninja, a build directory holding the letter is no obstacle.

Only the tools and versions named above were measured, on one machine. Other ccache
and binutils releases, the Visual Studio generators, MSVC's compiler and linker in such a
directory, and a system whose code page is UTF-8 are unverified, neither working nor failing.

The reproducer, from Git Bash with MSYS2's `mingw64/bin` first on `PATH`; each failing line
prints the error quoted in the table, and the Ninja build and the relative link are its controls:

```sh
mkdir -p "Zoë Ж/src" && cd "Zoë Ж"
printf 'cmake_minimum_required(VERSION 3.20)\nproject(paths CXX)\nadd_executable(paths main.cpp)\n' > src/CMakeLists.txt
echo 'int main() { return 0; }' > src/main.cpp
cmake -S src -B ninja -G Ninja && cmake --build ninja; echo "ninja: $?"
cmake -S src -B make -G "MinGW Makefiles" && cmake --build make; echo "make: $?"
cmake -S src -B ccache -G Ninja -DCMAKE_CXX_COMPILER_LAUNCHER=ccache && cmake --build ccache; echo "ccache: $?"
g++ -c src/main.cpp -o main.o && g++ main.o -o relative.exe; echo "relative: $?"
g++ "$PWD/main.o" -o absolute.exe; echo "absolute: $?"
```

It prints `ninja: 0`, `make: 2`, `ccache: 3`, `relative: 0` and `absolute: 1`. A dependency
prefix fails the same way as the last line: a library installed under `Zoë Ж/prefix` and found
through `CMAKE_PREFIX_PATH` reaches the linker by its absolute path.

### MSVC

```powershell
# from a Developer PowerShell, or after vcvars64.bat
cmake -S . -B build-msvc -G Ninja -DZEN_LOOM_DEV=OFF "-DCMAKE_PREFIX_PATH=<loom prefix>"
cmake --build build-msvc
cmake -DZEN_BUILD_DIR=build-msvc -P tests/verify.cmake
```

**Zengine adds no MSVC-specific flag of its own, and that is the point.** The Loom's public
macro surface needs `/Zc:preprocessor`, and `loom::core` carries it as an *interface*
requirement, so a consumer inherits it by linking. A compatibility flag copied into this
repository would mean the package had stopped carrying its own law.

Two things behave differently on MSVC and are recorded rather than smoothed over:

- **Some compile-negative diagnostics differ in wording.** Where GCC reaches an authored
  `static_assert` and prints the sentence that names the fix, MSVC's requires-expression can
  accept the probe and the wall lands one layer out as a conversion failure instead. The
  **wall** holds identically on both — the thing cannot be built either way, which is the
  property under test — but the **guidance** is GCC-only on those entries. The lane's patterns
  accept both spellings, and each alternative is specific enough to disappear if the wall ever
  stopped working.
- **Exported ABI declarations use the existing export macro** rather than a second mechanism.

### MinGW

Two shapes, and which one you want follows from what you are asking.

**The lane** — what CI runs, and the shape a released consumer has: a stranger against an
installed Loom prefix. The prefix is a Debug Loom configured with
`-DLOOM_ENABLE_WINDOWS_KERNEL=ON` (against a kernel-less prefix Zengine configures, reports
every weave skipped, and its `tests/` refuse to configure). Ninja, Debug, SDL off:

```powershell
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Debug `
      -DCMAKE_C_COMPILER=<mingw>/bin/gcc.exe -DCMAKE_CXX_COMPILER=<mingw>/bin/g++.exe `
      "-DCMAKE_PREFIX_PATH=<loom prefix>" -DZEN_LOOM_DEV=OFF -DBUILD_TESTING=ON -DZENGINE_TUI_ONLY=ON
cmake --build build-win
cmake -DZEN_BUILD_DIR=build-win -P tests/verify.cmake
```

**The sibling override** — for editing both trees together without an install round-trip, and
for the demos (on Windows it implies the kernel, above). It reaches the whole Loom build tree,
so a dependency on an unexported Loom target stays invisible here; the lane shape is the one
that says a change is done:

```powershell
cmake -S . -B build-win-dev -G Ninja -DZEN_LOOM_DEV=ON
cmake --build build-win-dev
cmake -DZEN_BUILD_DIR=build-win-dev -P tests/verify.cmake
```

MinGW objects are allowed to be large — the embedded typeface makes one translation unit big
enough to need it.

## The reloadable-weave build contract

A loadable weave's library must be built so that unloading it actually ends that image's static
lifetime. The Loom exports one function for this, and it is a **mechanism rather than a sentence
in a guide** because forgetting it does not produce a build error — it produces a use-after-free
on unload, and the loader reports success on the way there.

```cmake
add_library(my-weave SHARED my_weave.cpp)
target_link_libraries(my-weave PRIVATE loom::core loom::switchboard)
loom_weave_build_contract(my-weave)
```

What it actually does, per platform:

| platform | verdict |
|---|---|
| Linux / GCC targeting ELF | **applied**: `-fno-gnu-unique`, verified with a compiler-flag check rather than assumed |
| Windows / MinGW (PE-COFF) | **not applicable**: PE has no unique symbol binding at all. This compiler *accepts* the option, which is exactly why the predicate must be semantic |
| Apple / Mach-O | **not applicable**, same reasoning |
| anything else | **unclassified**, and said out loud rather than assumed either way |

The verdict is recorded on the target as the `LOOM_WEAVE_BUILD_CONTRACT` property, so what was
imposed is readable rather than assumed.

**The contract covers a compilation, not a file.** Every translation unit that ends up inside a
loadable image needs it — the weave's own sources *and* any static library linked into it. One
unique symbol is enough to mark a whole image as non-deletable, so `STATIC` and `OBJECT`
libraries are legitimate subjects. An executable is not: it is never the thing being loaded.

Inside this repository, `zengine_weave()` applies it. Outside, call it yourself; see
[getting started](../getting-started.md#using-zengine-from-another-project).

## SDL

The OS window is the only weave that sees SDL. It fetches a **pinned** shared SDL3 (with a
checksum in the build) where none is installed, along with SDL_ttf and its vendored FreeType on
the same pinned-and-checksummed fetch. Decline the whole thing with `-DZENGINE_TUI_ONLY=ON`: the
terminal window alone. `ZENGINE_SDL_SKIN` is retired, and a configure naming it is refused, with
the fix.

Licences and provenance for all of it are in
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) and
[graphics/window/fonts/PROVENANCE.md](../../graphics/window/fonts/PROVENANCE.md).

## Terminals

The POSIX terminal backend parses raw-mode bytes with a *stateful, incremental* parser: an OS
read boundary is not an event boundary, so a mouse report split across reads is rejoined rather
than translated into the keystrokes its bytes happen to spell. The Win32 console backend reads
`INPUT_RECORD`s. Terminal *size* is asked of the operating system through one file —
[`graphics/window/terminal_io.hpp`](../../graphics/window/terminal_io.hpp) — which is the only place
in this repository that names an operating system for that question.

A redirected, piped or captured run measures nothing and says nothing, which is different from
reporting zeroes: "I have no opinion" and "there is no room" are different sentences.
