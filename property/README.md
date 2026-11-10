# Property

**A typed connection to one property of one thing, the words a value of a type is written and read
in, and an inspector's line over one property: what a weaver edits by text, checked before it is
taken.**

A `Property<T>` reads its value and tries to write one, and a write comes back as a `Written`
(`persist/written.hpp`): taken, or refused with its reason. `TextForm<T>` is how a type becomes text
and text becomes it again, one for each type, shared by every property of that type. A `Row` is an
inspector's line over a property, read fresh at every look and written only by finished text: text
that is not a value of the row's type is told apart from a value the property will not take.

## What is in this folder

| | |
|---|---|
| `property.hpp` | `Property`, `TextForm`, `Commit` and `Row` |
| `CMakeLists.txt` | the build: `zengine-property`, header-only |

Working on it: [AGENTS.md](AGENTS.md).
