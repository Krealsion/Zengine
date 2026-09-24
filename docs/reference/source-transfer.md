# Source material: text, commands and file locations between the Editors and Inventory

**Reference.** `zengine::source-transfer` is the one public header
`source-transfer/vocabulary.hpp`: the shapes both Editors carry out to a receiving pane and take
back in. A pane of your own can make them (so an Editor takes them in) or recognize them (when an
Editor hands one to it). The rules the Editors apply to them — the two byte laws, the Terminal
line, reading a dropped pair, the C++ generator — live beside it in `source-transfer/` and are the
Editors' own, not part of the installed package.

How a maker uses all of this is in [the source editor](../workshop/editor.md#carrying-text-commands-and-file-places)
and [Neovim in Workshop](../workshop/neovim.md#carrying-text-commands-and-file-places); the carry
itself is Workshop's typed carry ([Workshop panes](workshop-panes.md)); where the copies are kept
is [Inventory](inventory.md).

## The shapes

Every carried value is an **Inventory pair**: one item, and metadata beside it
(`inventory::encode_pair`). The item is the thing; the metadata is an observation of where it came
from, and never authority over anything.

| item | metadata beside it | meaning |
|---|---|---|
| `SourceText { text }` | `SourceSelection` | text an Editor copied |
| `SourceLocation { path, line, column }` | `SourceLocationContext` | a place in a file |

`SourceText.text` is UTF-8 with every line break one LF, whatever the file used; the file's own
convention is recorded in the observation instead.

`SourceSelection` records `editor` (which Editor, in words), `path` (the absolute, normalized
file, or empty for a buffer with no file), `project_root`, `kind` (`characters`, `lines` or
`block`; the standard Editor always records `characters`), the range, `line_ending` (`LF` or
`CRLF`), `unsaved` (the buffer differed from its file when copied) and `captured_at_epoch_s` (the
local clock, as a fact about the copy and nothing more). The range is `first_line`,
`first_column` (1-based) and `end_line`, `end_column`, exclusive: for `lines` it ends on the line
after the last one, column 1; for `block` the two positions are the opposite corners Neovim held,
and the block spans the columns between them.

`SourceLocation.path` is absolute and normalized with forward slashes; `line` and `column` are
1-based, and 0 means "none". `SourceLocationContext` adds `editor`, `project_root`, `relative`
(the path under that root, when it is under it), `line_text` (the caret's line as it read, at
most 240 bytes) and `unsaved` and `captured_at_epoch_s` as above.

## What an Editor does with each

**Text in.** Inserted where it lands as one undoable edit; dropped onto the painted selection it
replaces it. The standard Editor holds tab and printable ASCII, reading LF or CRLF as one break
and writing the document's own; anything else is refused whole. Neovim holds any UTF-8 except
NUL, inserted as data — an Escape, `<Esc>`-style key notation or a bare carriage return stays a
character, never a key.

**A command in.** Any other typed message dropped on an Editor — a Terminal capture, a preset
saved in Info — becomes its Workshop Terminal line, `send <address> <Shape> <version> field=value
…`: the fields present, in declaration order, Text always quoted, proved by Loom's own lexer to
read back as the same values. The destination is the capture's role (`@office`) or publish (`*`)
address when its `TerminalCaptureFacts` recorded one; otherwise the line reads `<address>`, which
the Terminal refuses until it is replaced. A missing required field stays missing and is named. A
Text value holding `"` or a control byte, a non-finite Float, Bytes, a List or a nested Message
cannot be spelled on one Terminal line, and the drop is refused. Nothing is ever sent.

**C++ in.** In a C++ document — `.cpp`, `.cc`, `.cxx`, `.c++`, `.hpp`, `.hh`, `.hxx`, `.h++`,
`.ipp`, `.tpp`, `.inl`, `.ixx`, `.cppm`, or in Neovim any buffer whose filetype is `cpp` — a
separate choice generates one function instead: `inline loom::Value make_<shape>_v<N>()`, which
builds the value with `loom::SchemaBuilder` and `loom::Value`, the schema spelled exactly as the
value's own (a receiver refuses a value whose schema differs), string bytes escaped as data, and a
required field the value never had left as a `// FILL` hole. Its header comment names the
includes it needs (`<zen/schema.hpp>` and `<zen/value.hpp>`, Loom's `loom::core`) and which of
them the document lacks; they are never written in for you. A `.h` may be C, so it gets the
choice only as "this .h is C++". Nested or list fields are refused.

**A location in.** Opened through the managed opening, by its exact absolute path, after
Workshop approves the dropping actor; the caret moves only where the saved line still reads as it
did. A location is never inserted as text and never follows its relative name to another root.

## Limits

- A carried pair is at most 64 KiB; a larger selection is refused, never cut.
- The standard Editor has no block selection; Neovim's block copies its rows joined by LF, with a
  tab that the block's edge cuts turned into spaces, as Neovim's own yank does.
- A NUL byte is refused by both Editors (Neovim's selection functions cannot tell it from a line
  break).
