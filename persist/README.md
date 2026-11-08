# Persist

**The file doors every durable file shares: a whole file read within a bound, a file written
without putting the last good copy at risk, and what a write came to, in words a weaver can act
on.**

A read names the most bytes the file may hold and refuses a larger one by its size, so a file never
chooses the cost of refusing it. A write goes to a candidate beside the file (`<file>.saving`) and
replaces the file only once the candidate is whole; a write that fails leaves the file as it was and
says so. A per-user root is made on its first write; a project's folder never is, so a missing one is
refused out loud. Every outcome is a `Written`: accepted, or refused with its reason.

## What is in this folder

| | |
|---|---|
| `file.hpp` | the bounded read, the safe write, a relative spelling resolved, and JSON read as text before a shape can judge it |
| `written.hpp` | `Written`, what a write came to |
| `CMakeLists.txt` | the build: `zengine-persist`, header-only |

Working on it: [AGENTS.md](AGENTS.md).
