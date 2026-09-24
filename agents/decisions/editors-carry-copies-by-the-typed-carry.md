# Editors carry copies by the typed carry

**Decision record.** One decision, its alternatives, and why this one. The laws it supports are in
[editor-transfers](../workshop/editor-transfers.md) and
[neovim-transfers](../workshop/neovim-transfers.md).

**Context.** A maker wants to keep text, a Terminal command and a place in a file in Inventory and
bring them back into either Editor, by pointer or by keyboard, without an editor ever running what
was dropped or losing unsaved work, and without a copy that silently changes meaning.

**Decision.** Both Editors join the existing typed carry: a pane asks Workshop to approve a carry
for the actor's own gesture, and Workshop moves an owned pair to the receiver. The material is a
small package, `source-transfer/`: `SourceText`, `SourceLocation`, their observations, the two
byte laws, the Terminal line (proved by Loom's lexer), the reading of a dropped pair, and the C++
generator. A copy leaves only from an established selection -- a drag begun on the painted
highlight, a right-click Extract, or a key -- and an insertion is one undoable edit at the painted
landing, replacing the highlight only when dropped onto it. Both Editors number their pictures, so a
drop aimed at a screen that moved is refused. A command inserts its Terminal line; C++ is a
separate choice in C++ documents. A location reopens through the managed opening, approved for the
dropping actor like any other operation, and the caret moves only where the saved line still reads
as it did.

**Alternatives considered.**
- *Holding the selection on a press inside it* -- refused: the painted selection and the next key's
  effect disagreed (a paste replaced what the maker had clicked away from).
- *`nvim_paste` or keys for Neovim* -- refused: mode-dependent, and a Terminal buffer would run it.
- *Opening a location's relative name under this root* -- refused: another worktree's same-named
  file is a different file; rebinding is an explicit edit of the path.
- *Executing or sending a dropped command* -- refused: the drop is text to edit, never an act.

**Consequences.** The standard Editor's byte law refuses what Neovim holds as data, in words. A
guest's gesture needs a new `open` power to reopen a location. In Neovim the carry key is
`ctrl+r`, declared only in Visual and Select mode, where Neovim gives it no meaning; `ctrl+k`
stays the desktop's Hotkeys, and the location has no default key.

**Laws supported.** [WL-EDIT-17](../workshop/editor-transfers.md),
[WL-EDIT-18](../workshop/editor-transfers.md), [WL-EDIT-19](../workshop/editor-transfers.md),
[WL-EDIT-20](../workshop/editor-transfers.md), [WL-EDIT-21](../workshop/editor-transfers.md),
[WL-NVIM-10](../workshop/neovim-transfers.md), [WL-NVIM-11](../workshop/neovim-transfers.md),
[WL-NVIM-12](../workshop/neovim-transfers.md).
