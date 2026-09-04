# Verification method — platforms

Register `VM-PLAT`: platform and toolchain traps — the two COFF ceilings, what spends them, the
9p mount, the two CMakes, the developer shell, and what a silent compiler is telling you. One
method per heading; cite by ID. Router: [`../verification.md`](../verification.md).

## VM-PLAT-01 — MinGW needs big objects on the host's translation unit

METHOD — MinGW needs `-mbig-obj` on `workshop.cpp`'s object, and that is not optional: one more inline function in a header fails the link with a relocation-truncated diagnostic that reads like a broken tree.
BECAUSE — its debug COMDATs sit near COFF's section-relative limit, and the diagnostic names a
truncated relocation against a debug frame section; the ceiling is the toolchain's, not the
object's, and a build that links today says nothing about tomorrow's header.
SEEN — `CMakeLists.txt` `-mbig-obj`.

## VM-PLAT-02 — COFF has a second ceiling that big objects do not lift

METHOD — COFF has a second ceiling `-mbig-obj` does not lift — the section-name string table stops at 9,999,999 bytes — and the diagnostic says which you met; it is the assembler's, lifted by a newer binutils.
BECAUSE — the eight-byte name field holds a slash plus seven decimal digits; a string table
overflow names this ceiling and too many sections names the other, and binutils 2.45 assembled
into a 50 MB object what 2.40 refused.
SEEN — `CMakeLists.txt` `-mbig-obj`; `docs/contributing/supported-toolchains.md`.

## VM-PLAT-03 — What spends the table is instantiations per object

METHOD — What spends that table is instantiations per object, four names each: the remedy is another source under the same suite, never a flag or fewer cases; a finer cut buys less each time, so probe the floor first.
BECAUSE — every vague-linkage function gets a COMDAT and the debug flag mirrors its name into
four sections; a unit that only constructs the shared rigs spent 69% of the table before asserting
anything, and a finer cut bought four points, not ten.
SEEN — `tests/CMakeLists.txt` `workshop_panes`; `tools/workshop-split/census.py`.

## VM-PLAT-04 — Prove the toolchain on a hello-world first

METHOD — Prove the toolchain on a hello-world before believing a reproduction: a compiler that exits non-zero with NO diagnostic is the invocation (a backend that cannot load its libraries), not the tree.
BECAUSE — a driver reports a child's failure and a backend that cannot load its libraries has no
message; a hello-world failing the same way is what separated the environment from the tree.
SEEN — nowhere yet

## VM-PLAT-05 — The population file stays plain ASCII

METHOD — `tests/test_population.txt` is read with `file(STRINGS)` and stays plain ASCII, prose included: one non-ASCII glyph in a comment splits the line and the parser blames the wrong word.
BECAUSE — one non-ASCII glyph in a comment split the line, and the parser reported a known word
as an unknown kind, blaming a word three columns from the cause.
SEEN — `tests/test_population.txt`; `tests/check_population.cmake`.

## VM-PLAT-06 — MinGW's stat reports inode zero

METHOD — MinGW's `stat` reports `st_ino=0`, so libstdc++'s same-file guard refuses `copy_file` with `overwrite_existing` on any existing destination and leaves the old bytes; invisible on Linux and MSVC.
BECAUSE — libstdc++ asks whether source and destination are the same file first, by device and
inode, and MinGW answers inode zero for every file on the drive; measured with a probe on both
paths, a source changed to v2 left the destination reading v1.
SEEN — `tests/test_workshop_load.cpp` `Stage::put`.

## VM-PLAT-07 — Live resize is provable on POSIX only

METHOD — A Windows console client cannot resize its own window under the default terminal, and a pseudoconsole attribute can be accepted while the child still inherits the handles: live resize is provable on POSIX only.
BECAUSE — the console window call returns success and does nothing under the default terminal,
then fails with an invalid parameter once the buffer moved; the pseudoconsole attribute was
accepted while the child still inherited the parent's handles.
SEEN — `surface/terminal_size.hpp` `native_terminal_size`.

## VM-PLAT-08 — Windows narrows a wall-clock margin

METHOD — Windows narrows a wall-clock margin: a 10 ms beat against the default ~15.6 ms timer granularity fits fewer beats than Linux, so each stray beat is a larger share of any tolerance.
BECAUSE — the default timer granularity fits about eight ten-millisecond beats where Linux fits
eleven, so a wall-clock probe green locally has been tested on the wrong machine.
SEEN — `tests/test_audit_probes.cpp`.

## VM-PLAT-09 — A stale MinGW build tree can crash at startup

METHOD — A stale MinGW build tree can crash at startup before the first doctest line (heap corruption), unlike an entry-point-not-found exit (a foreign runtime DLL first on PATH): delete the tree before hunting a bug.
BECAUSE — objects half-compiled against a changed header made a binary that died of heap
corruption before its first doctest line, and a fresh tree was green ten of ten.
SEEN — nowhere yet

## VM-PLAT-10 — On a 9p mount an artifact can be byte-garbage

METHOD — On a 9p-mounted tree a built artifact can be byte-garbage from a partial write and a fresh source can look older than its object: scan with `file -b` before believing a mass failure; heed a clock-skew warning.
BECAUSE — two built libraries on such a mount were byte-garbage from partially flushed writes,
thirty-two kernel failures and two red entries, none real; deleting and relinking each artifact
fixed it with no source change.
SEEN — nowhere yet

## VM-PLAT-11 — The MSVC lane's compile-negative fixtures need the developer shell

METHOD — The MSVC lane's compile-negative fixtures spawn their own compiler: without the developer environment in the shell five entries die on a missing standard header, which reads exactly like a regression.
BECAUSE — the build works from a bare shell because the cache holds the compiler's absolute
path, but each compile-negative fixture spawns its own compiler, which has no INCLUDE without the
developer shell: twenty-four of twenty-nine before, all after.
SEEN — `.github/workflows/ci.yml` `Enter-VsDevShell`;
`docs/contributing/supported-toolchains.md`.

## VM-PLAT-12 — A CMake feature must clear the declared floor, guarded

METHOD — Two CMakes configure this repository and a feature must clear the declared floor, guarded, not the newest CMake present: a policy is set behind `if(POLICY …)`, in the file that reads it.
BECAUSE — the newer CMake prints warnings the older has never heard of, and the per-call
spelling its warning suggests is an unknown argument on the floor version; a policy set in an
included file is scoped to that file, where the fetch reads it.
SEEN — `cmake/ZengineSdl.cmake` `CMP0135`; `CMakeLists.txt` `cmake_minimum_required`.

## VM-PLAT-13 — Never move or copy a build tree

METHOD — Never move or copy a build tree: the cache bakes in absolute source paths, and a moved tree is dead weight, not a cache.
BECAUSE — the cache bakes in absolute source paths, so a moved tree neither configures nor
builds and is not a cache of anything.
SEEN — nowhere yet

## VM-PLAT-14 — The census measures what a translation unit spends on the name table

METHOD — Measure what a unit spends on the COFF name table with the census over its ELF objects: groups, name bytes, the four-name estimate against the ceiling, owner buckets; an object whose source is gone is skipped.
BECAUSE — a build tree keeps the object of a source a phase removed, and two such objects would
have named the wrong largest suite; the estimate on Linux objects (host 107% before the split, 59%
after) is what the Windows assembler then witnesses.
SEEN — `tools/workshop-split/census.py`.

## VM-PLAT-15 — The assembler on a bare compile is the direct witness of the ceiling

METHOD — The direct witness of the name-table ceiling is the toolchain's own assembler on a bare `-c` compile of the one unit with the older binutils; the object's size and the assembler's sentence are the measurement.
BECAUSE — the estimate is arithmetic over another platform's objects; binutils 2.40 refused the
host unit before the split and assembled it after into 28,609,284 bytes, and that sentence is what
the flag's own comment rests on.
SEEN — nowhere yet
