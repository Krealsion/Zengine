# An entry is where reading starts

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [code](../workshop/code.md).

**Context.** Edit Code opened a pane's source only through a single-source recipe, because a
`cmake_target` recipe names a configured tree and a target and no file. Every pane Zengine ships
is built by a CMake target, so none of Workshop's own panes could be reached from the pane.

**Decision.** A `cmake_target` recipe may name an `entry`: the one source a reader of that
artifact begins at. Recipe catalog format 2 writes it as a string whose empty value is none;
a relative entry completes against the project, as a single source does; one host rule reads a
recipe's file for both Edit Code and the Builder's `e`. Format 1 is admitted by its own retained
shapes, answered from the envelope's claim before a row is judged, and reads as no entry; the
writer writes format 2.

**Alternatives considered.**
- *Guessing the file from the target, stem or pane title* — argued: a name that happens to agree
  today is not provenance; the development catalog records the source the build was handed.
- *An `entry` list, or every source of the target* — argued: an editing entry is where a reader
  begins, and a list would claim to enumerate what a CMake project already knows.
- *Admitting a version-1 row as "entry absent" under the current shape* — tried: admission has no
  optional field and refuses the missing one; pinned by case `"a catalog is refused by a version
  this Workshop does not read, and a version-2 row without its entry is refused by admission"`.
- *A conversion provider for format 1, as the session has* — argued: a catalog is a file a maker
  names with no session to ride, which is the setup file's reason for a retained reader.

**Consequences.** A catalog a Workshop rewrites (Files' `a`) becomes format 2, and a Workshop
from before this change refuses it by number. Files' `a` still writes no entry for a tree.

**Laws supported.** [WL-CODE-05](../workshop/code.md).
