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
it into, and the source `zengine_weave` recorded as its entry. `development-runtime.cmake` sets
the tree's facts for `workshop/prepare-runtime.cmake`, which copies the host, its staged
artifacts, both plans and both catalogs into a runtime directory and writes a manifest last: the
tree, the configuration, each copy and the digest of each copy only a build changes. A later run
reuses that runtime while the tree still builds those digests, and refuses, copying nothing,
another tree's or configuration's runtime, an incomplete or stale one, and any other non-empty
directory. The pane artifacts carry no digest: a promotion rewrites them there, and a development
build rebuilds them in the tree.

**Alternatives considered.**
- *Running the build tree's own Workshop* — tried: with one running on Windows, an ordinary
  build failed copying over the pane DLL it had loaded, and succeeded once it quit. Its separation
  from the build lasts until someone builds, and that build also changes what its next launch
  loads.
- *Pane recipes in the shipped default catalog* — argued: Files' `a` seeds a maker's catalog
  from the shipped rows, so every project would inherit Workshop's own panes.
- *An install component for Workshop* — argued: the package ships no Workshop, and an install
  rule would claim a distributable product this setup is not.
- *A custom target that makes the runtime* — argued: a build of it, which any recipe can ask
  for, would write a runtime. What makes or reuses one is running the script, which the
  development launch does ([the launch](the-development-launch-stands-outside-workshop.md));
  building that launcher writes only its own executable.
- *Refreshing a stale runtime in place* — argued: it would write over images a Workshop may have
  mapped and over what a maker promoted there.

**Consequences.** A changed host, service, plan or catalog needs a new runtime; the old one is
refused, not removed, so its promotions stay where they were made. Two development roots never
share a runtime unless a maker points one at the other's directory, which the manifest refuses.
What a reuse reads is worth being exact about: the recorded digests are compared against the files
in the BUILD TREE, which says whether the tree has built any of them anew, and the runtime is
looked at for the names it copied, which says whether every copy is still there. What is in the
runtime's own copies is never read again — so a reuse says the runtime is current, not that it is
unharmed, and a copy changed inside a runtime is reused as it stands.

**Laws supported.** [WL-CODE-06](../workshop/code.md), [WL-CODE-07](../workshop/code.md).
