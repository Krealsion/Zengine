# On Windows, the host says what leaves the tree

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [files](../workshop/files.md).

**Context.** Windows is two standard libraries, and a filesystem claim names the one it was
measured on. The `linked` mark was decided by disagreement — following says directory, not
following does not — and measured on MSVC's STL, whose `symlink_status()` reports a junction as
a platform kind. libstdc++ on Windows reports the same junction as `directory` from the entry
and from the path, and does not implement `create_directory_symlink`; the disagreement never
happens there, so the case `"PROJ-2: a linked directory is marked, entered, and left again
LEXICALLY"` was red on every MinGW lane run since it landed — 21 of 21 through 2026-09-03 —
and the job's `continue-on-error` kept that red out of every run's conclusion. MinGW/libstdc++
is the maker's daily build and its lane is required; MSVC is the toolchain released Windows
users are expected to build with, and its lane is advisory until it can be proven locally as a
matter of course. Both are supported.

**Decision.** On Windows the mark is the host's word: `leaves_the_tree` asks
`GetFileAttributesW` of the entry's path for `FILE_ATTRIBUTE_REPARSE_POINT`, and a query that
fails keeps the standing policy of marking the row. The query is by path, never the listing's
cached copy, which Windows documents as possibly stale under load. Elsewhere the disagreement
predicate is unchanged. One attribute, in one place, on one platform; the mark says "leaves
the tree", not symbolic link versus junction.

**Alternatives considered.**
- *Tried: the standard-only disagreement predicate* — measured false on libstdc++/Windows: a
  junction is `directory` followed and unfollowed, so the production predicate answers
  `linked=0`, and the MinGW lane was red 20 of 20 runs on `main`
  (Zen/reportbacks/linked-mark-asks-the-path-RB.md).
- *Tried: `symlink_status(path)` instead of the entry's copy* — measured not to move it: the
  path answers `directory` on libstdc++/Windows too (`linked(path)=0`, same report). What it
  would have bought, MSVC hardened against a stale listing copy, the host query buys by
  construction.
- *Argued: a per-toolchain early return in the case* — rejected: it would declare the mark
  unwitnessed on the maker's daily build while the public docs claim it for Windows.

**Consequences.** The junction mark is a Windows host claim, the same on both libraries, and
`files.hpp` includes `<windows.h>` the way `filesystem_roots.hpp` does. The MSVC stale-copy
window — `FindFirstFileW`'s attributes served from the entry — is closed by a query by path,
noted as unobserved by any lane. The MinGW lane is required and this case is its acceptance; a
Windows claim in this repository names its standard library.

**Laws supported.** [WL-FILES-04](../workshop/files.md).
