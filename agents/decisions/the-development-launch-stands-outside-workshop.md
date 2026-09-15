# The development launch stands outside Workshop

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [code](../workshop/code.md).

**Context.** A development runtime is only safe if it is what runs. An IDE's default run
configuration for `zengine-workshop` starts the build tree's host, selects no development catalog
and leaves Workshop's project files wherever it starts; getting it right by hand meant a script,
a directory of one's own and two arguments, every time.

**Decision.** A small executable, `zengine-workshop-develop`, compiled with the tree's facts, asks
the runtime script to make or reuse its runtime and then starts that runtime's host with its
graphical plan and development catalog in a project directory beside the runtime, holding it
until it exits. Each step that refuses starts nothing, a runtime whose host is running refuses a
second launch, and the build tree's host is never a fallback. CLion's shared "Develop Workshop"
configuration in the repository's `.run/` names only that target; the IDE's build before a run
builds it and the host, and builds start nothing.

**The boundary.** The launch is an act outside the application: its executable is no weave,
artifact, plan row or recipe row, so nothing inside Workshop can launch, reload, edit or relaunch
the process that hosts it, and starting it again after an orderly quit is the maker's act. That
host may still speak on the bus like any participant. The intended next step, not built here, is
for a small Loom host that establishes the user, session and authority context to stand where
this launcher stands and boot Workshop; today the launcher starts a copy of Workshop's own host.
Nothing here claims security from a Windows file lock or forbids editing host sources outside.

**Alternatives considered.**
- *A shared configuration whose executable is the runtime's host* — argued: it would carry the
  runtime's path, a second copy of what configuration generates.
- *A shared configuration running `cmake -P` on a launch script* — argued: it names CMake's
  path, and CMake's `execute_process` owns how a GUI child's window first appears.
- *Preparing the runtime in a before-launch custom target* — argued: a refusal would read as a
  failed build, and the paths would still be in the shared file.
- *Leaving "Allow multiple instances" off* — argued: a second Run then offers to stop the
  running Workshop, unsaved work and all; the launch refuses in words instead.

**Consequences.** The IDE's Debug action debugs the launcher; Workshop is attached to by process.
The in-use check is the host image refusing to open for writing: Windows refuses a running image,
and on Linux the answer is the kernel's (`ETXTBSY`), not pinned. A changed host is a refused
launch until a new runtime is made.

**Laws supported.** [WL-CODE-08](../workshop/code.md).
