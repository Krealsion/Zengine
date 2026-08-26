# Agent law — Packaging

Routed detail behind [`AGENTS.md`](../AGENTS.md), for tasks touching `cmake/ZengineInstall.cmake`,
any public header, or any exported target's link line. The installed-package witness lane is in
[`verification.md`](verification.md). Phase tags like (PKG-0) are provenance markers into this
repository's history; the law here is current.

## Zengine is a package a stranger installs (PKG-0)

`cmake/ZengineInstall.cmake` is the whole public consumer surface, in one file: which targets
are exported, which headers are installed, which artifacts ride along, and the reason each
package is or is not in the set. Change the boundary there, not in a package's own
`CMakeLists.txt`.

```cmake
find_package(zengine 0.1 CONFIG REQUIRED)     # resolves Zengine's Loom dependency too
target_link_libraries(my-weave PRIVATE zengine::timer loom::switchboard)
```

**Eight exported targets**, `EXPORT_NAME`d to match their in-tree `zengine::` aliases so the
house and a guest spell them identically: `activation`, `timer`, `surface`, `input`, `ui`,
`component`, `operator`, `operator-consumer`. A plain hyphenated name on a link line means the
target is internal, and that difference is the boundary made visible.

**Five artifacts** install to `lib/zengine/`, named by `ZENGINE_RUNTIME_ARTIFACTS` and located
by `ZENGINE_ARTIFACT_DIR`: `zengine-timer`, `zengine-input`, the two TUI skins, and
`zengine-operators-basic`. They install as FILES, not as exported targets — an artifact is
opened by path and never linked, and an imported target would offer a link line that must
never be written.

**ARTIFACT is the noun, and the distinction is load-bearing (QR-5).** An artifact is the
physical loadable file; *weave* and *provider* are runtime SURFACES an artifact may expose.
Four of the five above are weaves; `zengine-operators-basic` is a provider and explicitly not
a weave (enforced by `zengine_provider()` in the top-level `CMakeLists.txt`). The public
package variables must therefore name the physical thing: a variable named after one surface
is false of its own contents the moment the list holds another. The `package_vocabulary`
CTest entry keeps the retired spellings from coming back — it owns the list of them, so this
page does not spell them.

**What is deliberately out, and why** (each is a limit, not an oversight): the SDL skin and
SDL input reader, because a fetched SDL is a build-tree library this install does not own;
Workshop, because its executable compiles its own build directory into itself for Builder;
the Workshop/Builder/introspection/composer vocabularies, because there is no way for an
externally-built pane to arrive in a run; the Timer's service headers, because using the
Timer is supported and *being* one has not been measured.

Header spelling does not change across the boundary: installed headers land under
`include/zengine/<package>/`, so `#include "timer/vocabulary.hpp"` is the same sentence from
either side.

## Do not assume

- Adding a header to a package makes it public — the installed set is named one file at a
  time in `cmake/ZengineInstall.cmake`, and anything a public header includes must be
  installed with it or the stranger stops compiling.
- An exported target may rely on a link it gets transitively in this tree — in this tree
  every include path is the same directory, so a missing `loom::core` is invisible here and
  fatal from an installed prefix. Exported targets link what their public headers use, on
  their own line.
- `PACKAGE_PREFIX_DIR` still means this package after `find_dependency(...)` — it is an
  ordinary variable, and the dependency's own config overwrites it. `zengineConfig.cmake.in`
  resolves every path of its own **before** it finds the Loom, and says so.
- Installing under `ZEN_LOOM_DEV=ON` produces a usable package — it is refused, because the
  export would name `loom::core` with no installed Loom to resolve it.
