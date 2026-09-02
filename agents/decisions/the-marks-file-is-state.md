# The marks file is state

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [files](../workshop/files.md).

**Context.** Maker marks needed a durable home (`0cf8a94`, "Look somewhere without making it
your project"). The prefs header says in its own words that non-presentation facts belong
somewhere with their own name; the prefs format has one version and no migration; and a mark is
an absolute path.

**Decision.** `workshop/marks_persist.hpp` is its own file, version 1, under the machine-local
state root beside the session. The file's claims — format word, version, shape — refuse it
whole; a row that is not an absolute location this build can carry is skipped alone, as a
standing condition, because the next mark writes the list without it. `marks_refused_` guards
the first write. "Unusable" is a spelling test, never an existence test.

**Alternatives considered.**
- *A field on the prefs file* — rejected: growing a field there would refuse every existing
  prefs file by number and cost makers a preference they had stated.
- *The configuration root* — rejected: an absolute path describes this machine's disks, the
  same criterion that already puts the viewport and the desktop placement under the state root.
- *Dropping an uncarriable row silently, or refusing the file for it* — rejected: skipped and
  said; pinned by case `"PROJ-2: a persisted mark is admitted, never re-based, and never quietly
  dropped"`.
- *Checking that a marked directory still exists* — rejected: nothing here asks the filesystem
  anything; a marked directory that is gone today is kept.
- *Writing without the refused flag* — rejected: the first `m` a maker pressed would replace
  bytes this run could not read with an empty list; pinned by case `"PROJ-2: a marks file this
  run could not read is never overwritten"`.

**Consequences.** The eighth durable artifact. `marks_refused_` is the session's own
never-write-over law one durable fact over. Marks survive a restart and the browsing location
deliberately does not.

**Laws supported.** [WL-FILES-08](../workshop/files.md).
