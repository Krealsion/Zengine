# Four facts that coincide at launch

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [files](../workshop/files.md).

**Context.** The Files browser spelled where a maker was standing as a stack of names below the
project root, and that spelling was the containment promise: nothing could say a place above
it. Looking at something was never the same act as choosing it (`0cf8a94`, "Look somewhere
without making it your project"). Before that, the browser was designed so it could not become a
second owner of source truth (`5a302ae`).

**Decision.** A row is a name, a kind, a linked flag and an openable flag and nothing else; what
it denotes is derived at activation. Four facts stay apart: the project is what a
project-relative spelling means (one writer, `main`); the origin is where this run began
(generated once, never persisted, never moved); the location is where somebody is looking (one
absolute, lexically normal, generic-slash string); the operating system is what may be read
(never modelled). A mark is a destination and nothing else, owned on the session outside
`FilesPane`. The traversal set is built at the gesture and held nowhere. Roots are host-reported,
asked at the gesture, never on the paint path. A listing is not a per-paint population. Bounds
and order are fixed.

**Alternatives considered.**
- *The entered-name stack as the representation* — retired: one absolute string where there
  was a vector and two derived functions (`0cf8a94`).
- *Renaming the origin "the project"* — refused: the two coincide today, and no expression in
  the application derives one from the other.
- *A standing selected mark* — rejected: the cycle is found from where the browser is, so
  nothing can drift out of agreement with the screen.
- *Asking the OS for roots inside `files_has_keyboard`* — refused: it answers at every keystroke
  and paint; the residual (no origin and no marks declines the keyboard) is named in
  `docs/workshop/limitations.md`.
- *A watcher, a timer, a poll for the listing* — none; a finished build refreshes it, gated on
  `build_news` rather than on a status arriving.
- *A locale, a natural sort, extension grouping* — none: bytewise over admitted name bytes.
- *A resolved path, recipe, artifact or build state on a row* — none.

**Consequences.** Browsing, marking, jumping and choosing a foreign recipe catalog leave
`HostContext::project_dir` exactly where it was, structurally. `kMaxListedEntries` stops the walk
and the header says `stopped counting`. A directory that cannot be listed is a refusal, not an
empty listing. A UNC share is reachable by spelling and in no drive list. A fact inside a pane is
a fact `close_panel` can destroy, which is why the marks live on the session.

**Laws supported.** [WL-FILES-01](../workshop/files.md), [WL-FILES-02](../workshop/files.md),
[WL-FILES-05](../workshop/files.md), [WL-FILES-06](../workshop/files.md),
[WL-FILES-07](../workshop/files.md), [WL-FILES-12](../workshop/files.md),
[WL-FILES-13](../workshop/files.md).
