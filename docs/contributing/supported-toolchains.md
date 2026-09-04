# Supported toolchains and platforms

**Contributing.** Which compilers and platforms this repository builds on, what each one does
and does not cover, and the platform rules that are load-bearing rather than incidental.

Every row below is classified from measurement on lanes that exist, not from a flag's spelling.

## The matrix

| platform / compiler | state | notes |
|---|---|---|
| **Linux (incl. WSL) / GCC 11.4+** | **fully supported.** The canonical lane | The Loom's OS sandbox exists only here |
| Linux / Clang | builds | Not a routine lane; the sanitizer flags are the same |
| **Windows / MinGW-w64 GCC 13.1+** (libstdc++) x64 | **supported.** The required Windows lane, and the build this project is developed on day to day | Runtime DLLs must be on `PATH` or beside the binaries. Binutils has a floor of its own: an assembler at or below 2.40 cannot assemble the Workshop application target (the test suites build either way); 2.45 can |
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
      "-DCMAKE_PREFIX_PATH=<loom prefix>" -DZEN_LOOM_DEV=OFF -DBUILD_TESTING=ON -DZENGINE_SDL_SKIN=OFF
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

The SDL skin is the only target that sees SDL. It fetches a **pinned** shared SDL3 (with a
checksum in the build) where none is installed, along with SDL_ttf and its vendored FreeType on
the same pinned-and-checksummed fetch. Decline the whole thing with `-DZENGINE_SDL_SKIN=OFF`.

Licences and provenance for all of it are in
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) and
[surface/fonts/PROVENANCE.md](../../surface/fonts/PROVENANCE.md).

## Terminals

The POSIX terminal backend parses raw-mode bytes with a *stateful, incremental* parser: an OS
read boundary is not an event boundary, so a mouse report split across reads is rejoined rather
than translated into the keystrokes its bytes happen to spell. The Win32 console backend reads
`INPUT_RECORD`s. Terminal *size* is asked of the operating system through one file —
[`surface/terminal_size.hpp`](../../surface/terminal_size.hpp) — which is the only place in this
repository that names an operating system for that question.

A redirected, piped or captured run measures nothing and says nothing, which is different from
reporting zeroes: "I have no opinion" and "there is no room" are different sentences.
