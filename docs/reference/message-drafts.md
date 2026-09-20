# Reusable message and value drafts

**Reference.** `zengine::message-draft` provides typed value editing and named preset
libraries. Include `message-draft/draft.hpp` for editing, or
`message-draft/library.hpp` for persistence. The package uses Loom schemas, values,
composition and admission; it does not send messages or choose their destination.

## Drafts and forms

`message_draft::Draft(schema)` begins with every field absent, including required
fields. `Draft(value)` copies existing content. A draft may be unfinished: missing
required fields are allowed while editing and saving. Incorrect kinds, null nested
messages and mismatched nested schemas are refused. A refused edit leaves the
previous draft intact.

`Path` is a vector of field names (`std::string`) and list indices (`std::size_t`).
Names containing dots or brackets remain literal names. `path_label` produces a
display label, not a parseable address.

| operation | meaning |
|---|---|
| `get(path)`, `type(path)` | inspect a field or existing list item |
| `set(path, cell)` | assign a Loom cell of the declared type |
| `set_text(path, text)` | compose an Int, Float, Text or Bool through Loom |
| `unset(path)` | remove a field's value; list items cannot be absent |
| `create_message(path)` | set an empty nested message with its declared schema |
| `create_list(path)` | set an empty list |
| `append(path, cell)`, `erase(path, index)` | change a present list |
| `rows()` | project fields and present containers into form rows |
| `errors()`, `ready()` | diagnose the draft against its original schema |
| `admit()`, `admit(expected_schema)` | produce a complete value or Loom's errors |

Create an absent parent before editing its children. Creating a container replaces
its contents, and unsetting a field removes its value. Neither operation preserves
a hidden disabled value. `rows()` includes paths, types, presence, required status,
labels and summaries; a presentation owns selection, clipping and text-entry state.

Scalar text follows the existing Composer's meaning: numeric-looking Text stays
Text, and Int may widen to Float. Bytes must be supplied as `loom::Cell::bytes`;
there is no invented hex spelling. Message and List fields use the container
operations. Invalid numeric text is refused rather than stored as a typed value.
A frontend that preserves unfinished text must retain that text separately.

Copies and `snapshot()` own independent nested values. Treat `value()` and cells
returned by `get()` as immutable inspection, as with Loom's ordinary values.
Draft traversal is bounded to 64 nested fields or list elements.

## Named libraries and compatibility

`Library::put(title, draft)` creates or replaces a named preset. Titles must be
nonempty; `find(title)` returns its draft, `erase(title)` removes it, and `entries()`
provides the ordered `{title, draft}` collection. An empty draft is also a reusable
schema template. A library refuses conflicting definitions of the same schema
name and version, including conflicts in nested components.

`library_bytes` and `read_library` serialize and reopen a library in memory.
`save_library(path, library)` and `open_library(path)` use explicit file paths.
Saved libraries carry the original schema closure and the authored values, so they
can reopen without the process that supplied their schemas. Reopening can also
take a current `loom::Registry` pointer: any known schema with the same name and
version must agree, and checking it does not change that registry. An unknown
schema may still reopen; this does not mean a running participant accepts it.

For example, after creating a draft from a discovered schema:

```cpp
namespace md = zengine::message_draft;
md::Draft draft(schema);
draft.set_text({std::string("count")}, "17");
md::Library library;
library.put("Small sample", draft);
auto reopened = md::read_library(md::library_bytes(library));
auto admitted = reopened.find("Small sample")->admit(*schema);
```

The schema must declare `count` as Int or Float for that edit. The installed-package
witness exercises construction, nested editing, persistence and admission through
these public headers.

The native envelope `zengine.message_draft.Library v1` contains Loom's schema
descriptors and `zengine.message_draft.Preset v1` entries. A private recursively
optional projection encodes missing fields without relaxing the original schema's
runtime contract. Restored drafts retain that original schema and must pass normal
admission before use. The format carries no sender, destination, correlation,
reply provenance or grants. Reusing a value never reuses authority.

Libraries have the maker file reader's 1 MiB limit. Saves validate before writing a
sibling candidate and replacing the previous file; a failed save preserves the
previous file. As with [Flow projects](flow.md#authoring-and-persistence), one writer
owns a save path. This is value persistence, not a queue or process checkpoint.
