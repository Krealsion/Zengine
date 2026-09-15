# Workshop develops itself from a copy

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [code](../workshop/code.md).

**Context.** A maker editing one of Workshop's own panes needs a catalog that builds it, an
entry that opens it, and a running Workshop the build cannot hurt. A Workshop run from its build
tree maps the pane copies `zengine-workshop-staging` writes beside the host, and an ordinary
`cmake --build` writes those copies again: over an image the process has mapped, and into the
file the next launch loads, which is a promotion nobody asked for. And an artifact is named by
its stem beside the host, so every project one host directory serves shares those files.

**Decision.** Configuration generates two files beside the host. `development-build-recipes.json`
has one `cmake_target` row per listed shipped pane weave: its target, the directory CMake builds
it into, and the source `zengine_weave` recorded as its entry. `development-runtime.cmake` copies
the host, its staged artifacts, both plans and both catalogs into a runtime directory, writes a
manifest naming the build tree, and refuses another tree's runtime, one already made and any
non-empty directory. A maker launches the runtime with `--recipes` naming its catalog.

**Alternatives considered.**
- *Running the build tree's own Workshop* — argued: the files it maps are an ordinary build's
  staging output, so its separation from the build lasts until someone builds, and that build
  also changes what its next launch loads.
- *Pane recipes in the shipped default catalog* — argued: Files' `a` seeds a maker's catalog
  from the shipped rows, so every project would inherit Workshop's own panes.
- *An install component for Workshop* — argued: the package ships no Workshop, and an install
  rule would claim a distributable product this setup is not.
- *A custom target that makes the runtime* — argued: a recipe names targets, so a build could
  write over a running runtime; a `-P` script is out of every recipe's reach.

**Consequences.** A changed host or a newly listed pane needs a new runtime, and promotions made
in the old one go with it. Two development roots never share a runtime unless a maker points one
at the other's directory, which the manifest refuses.

**Laws supported.** [WL-CODE-06](../workshop/code.md), [WL-CODE-07](../workshop/code.md).
