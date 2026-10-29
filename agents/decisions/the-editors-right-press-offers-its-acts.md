# The Editor's right press offers its acts, each beside its key

**Decision record.** One decision, its alternatives, and why this one. The laws it supports are in
[editor-transfers](../workshop/editor-transfers.md) and [pane-menu](../workshop/pane-menu.md).

**Context.** A right press in the standard Editor offered Extract on the highlight and the file's
location on the status row, and handed every other press back to Workshop's own pane menu. The
founder asked for a right-press menu with close, save, undo and redo, the keys shown, and the
expected basics. A menu row was an id and a label, so no row could show its key, and a key a weaver
moved in the keymap file is known to the keymap's projection (`KeymapShown`), not to the pane.

**Decision.** A right press on the Editor's rows opens its own menu, Workshop's standard rows
beneath: undo and redo while a step is there, cut and copy with a selection, paste, select all,
extract with a selection, the location, save, discard while there is unsaved work, and close. The
press's own subject keeps the first row. Each row does what its key does, through the same act. The
Editor follows the keymap's projection and writes each row's key in its label after a mark
(`kMenuKeyMark`); the shipped presenter draws the key apart, beside the words, and cuts the words
first. Close is a declared row, `editor.close`, unbound, and refuses unsaved work as the Editor's
other exits do.

**Alternatives considered.**
- *A key field on a new version of the row* -- not chosen: four shapes would take new versions and
  Workshop would accept two, for what a label can carry; a presenter that knows no mark shows a
  space.
- *The presenter reading the keymap* -- refused: the shipped presenter builds from installed
  headers alone, and the keymap's projection is not installed.
- *Workshop writing the keys into the grant* -- not chosen: the row's words are the pane's, and the
  host would grow a reading of rows it otherwise passes through unread.
- *Close discarding unsaved work, or asking* -- refused: no document is lost but by the weaver's
  discard; a close refuses as a replacement and a quit do, and the menu offers save and discard
  beside it.

**Consequences.** A key the weaver moves shows moved at the next menu. Workshop's standard rows
show no key. The Neovim-backed Editor's menu is unchanged.

**Laws supported.** [WL-EDIT-22](../workshop/editor-transfers.md),
[WL-CTX-11](../workshop/pane-menu.md).
