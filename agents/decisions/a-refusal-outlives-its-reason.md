# A refusal outlives its reason

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [files](../workshop/files.md).

**Context.** The browser refused linked directories to keep its entered-name stack honest; when
the stack went, the refusal stayed with no property left to protect, at a measured cost of six
of the twenty-three directories at POSIX `/` (`0cf8a94`). Two filesystem names could end the
application: on MSVC (ACP 1252) `string()`, `generic_string()` and `u8string()` raise
`filesystem_error` — for an unpaired surrogate in a filename, out of the enumeration walk, and
for a working directory outside the code page, out of `main`, where the process printed nothing
and hung in the C runtime's abort handler (`3920bdb`, "Refuse a path this Workshop cannot say,
rather than dying on it").

**Decision.** Parent is lexical and stops at the filesystem's fixed point `p.parent_path() ==
p`. A linked directory is marked (`symlink_status()` disagreeing with following) and enterable.
Every write to `current_dir` and every persisted mark goes through `admit_location`. Filenames
are `std::string` everywhere: a printable-ASCII name is exact and openable; any other keeps its
row as a `?`-marked projection and refuses activation; what is inside a file is the editor's
question. `path_admission.hpp` is the only place allowed to ask for a path's bytes, and it
answers with values.

**Alternatives considered.**
- *`has_parent_path()` as a root test* — rejected: true at POSIX `/`, a drive root and
  `//server/`, so a boundary built on it never fires; pinned by case `"PROJ-2: parent walks
  straight past the project and stops at the filesystem"`.
- *`weakly_canonical` on the way up* — rejected: it silently relocates the maker to a place they
  never navigated to (`git log -S'weakly_canonical'` → `0cf8a94`).
- *`is_symlink()`* — rejected, measured on Windows/MSVC: a directory junction answers false
  while `symlink_status().type()` is a platform extension that is not `directory`; probe and
  table in Zen/reportbacks/EDIT-1-evidence.md.
- *Keeping the link refusal* — rejected: no property survived it (`0cf8a94`).
- *The byte test alone for openability* — rejected: a refused name's `?` projection is entirely
  printable ASCII and would read as openable, handing a door a path naming a different file or
  none; the `exact` flag is what stands between the two (`3920bdb`).
- *A file-type registry or extension list* — none: a `.png` walks into the refusal that knows
  why.
- *A second `generic_string()` anywhere* — forbidden: a second way for the process to die.

**Consequences.** Going up from a linked directory returns the maker where they walked in.
"Absolute, lexically normal, carriable" holds after the seed, an enter, a parent and a jump
rather than at four sites. `launch_project_dir()` is the capture `main` runs, so what a case
proves is what ships. A hand-edited durable file is the one place uncarriable bytes can arrive
from, and it meets the same door.

**Laws supported.** [WL-FILES-03](../workshop/files.md), [WL-FILES-09](../workshop/files.md),
[WL-FILES-10](../workshop/files.md), [WL-FILES-11](../workshop/files.md).
