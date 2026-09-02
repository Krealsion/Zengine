# The project is several mechanisms

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [project](../workshop/project.md).

**Context.** "A project is where you are standing" was documented law with no value behind it:
no line of production code read the launch directory, and it was spent implicitly by whichever
relative path happened to resolve against the process's working directory (`5a302ae`). Then a
working directory outside the active code page ended the process before the banner's first line
(`3920bdb`).

**Decision.** `HostContext::project_dir` is the launch directory, captured once by the host
through `launch_project_dir()`. Empty is the designed absence, said on the banner. It is not
`dir` — where the binary is, installation truth — and nothing derives it from that, from
`--document`, from `--recipes`, a workspace or a prefix. There is no `--project`. Two roads reach
the absence (a platform that will not report a working directory, and one that reports a
directory this build cannot carry), joined deliberately because a maker meets one fact.

**Alternatives considered.**
- *A `--project` flag* — rejected: one install serves two projects by being launched in two
  places, the law `user_paths.hpp` already wrote down (`5a302ae`).
- *Substituting the executable's directory or a parent when the launch directory is absent* —
  rejected: nothing adjacent is substituted (`3920bdb`); pinned by case `"QR-12: a launch
  directory this Workshop cannot say is an absence, not an exit"`.
- *Two separately worded absences* — rejected: the banner's sentence moved with the fact, from
  "the system did not report a working directory" (wrong half the time) to what is missing.
- *Auto-wrapping host state for the catalog* — rejected: exposure stays an act, and the anchor
  is one of two Source samples the host reads from the owner at the sample (`e46b536`, "Ask the
  catalog for what it can answer with nothing in hand").

**Consequences.** With the anchor absent, Files refuses to browse and relative recipe sources
refuse. Browsing, marking and choosing a foreign catalog leave it where it was.
`zengine.project.anchor` answers the owner's anchor, absence included.

**Laws supported.** [WL-PROJ-01](../workshop/project.md).
