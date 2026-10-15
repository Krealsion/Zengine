# What crosses a switch

**Reference.** The handoff every Editor speaks when the editor switch moves the Editor's office
from one editor to another, and exactly what of the document crosses each way: what is adjusted,
what is reset, what needs your consent and what is refused. It is a conversation between the switch
and both Editors, so no one manual holds it: the switch's commands are
[`zengine.editor-switch`](zengine.editor-switch.md), the Editors' are
[`zengine.editor`](zengine.editor.md), and switching while you work is
[Neovim in Workshop](neovim.md). The shapes are
[`workshop/editor_handoff_vocabulary.hpp`](../../workshop/editor_handoff_vocabulary.hpp). The law
is `WL-SWITCH` and `WL-NVIM` (`agents/workshop/editor-switch.md`, `agents/workshop/neovim.md`).

## The handoff

A switch replaces the weave holding the office through Loom's prepared replacement, and carries the
document through an authored handoff every editor speaks, as incumbent and as candidate:

| step | who | what |
|---|---|---|
| judge | incumbent | what a switch away would lose (for consent), reset and refuse; nothing is held or loaded |
| load | the office | the destination's artifact, sealed: it may speak only to the coordinator |
| warm | candidate | start what it needs, in the incumbent's room; the coordinator relays a beat while it starts |
| boundary | incumbent | author the exact document and hold still: no input is applied until the outcome, and what is refused is counted |
| adopt | candidate | Loom's preparation ask: adopt the document, or refuse it in words |
| commit | Loom | the admission, which is also the successor's activation; traffic queued behind it reaches the successor |
| prove | successor | say it serves; the retired editor is then unloaded |
| retire | retired | say how many inputs it refused while it held still |

Before the commitment, every ending moves nothing: the candidate is discarded and the incumbent is
told, resumes, and says so on its pane. After it, nothing is rolled back. An orderly quit is refused
while an editor holds still for a switch.

## What crosses

The transfer is the standard Editor's model: file bytes, the saved comparison, a line convention,
the modified flag, a caret and an anchor as (line, byte) with an exclusive selection end, and the
first visible line and column.

| state | standard → Neovim | Neovim → standard |
|---|---|---|
| bytes | adopted exactly: the file is loaded, its lines replaced with no undo step | exported exactly |
| line endings | LF or CRLF, set on the buffer | LF or CRLF; `mac` is **refused** |
| final newline | set on the buffer (`endofline`) | carried; a byte-order mark or `binary` is **refused** |
| unsaved edits | the modified flag carried | the saved comparison is the file on disk; a file not yet written counts as unsaved |
| caret | Normal mode on the next character; past a line's last byte, Insert mode there | Normal before the cursor character; Insert and Replace exact |
| selection | charwise Visual with both ends and its direction | charwise exact; linewise as the same lines; blockwise as its cursor (each said) |
| viewport | first line and column restored | first line; first column only when lines do not wrap |
| **adjusted, and said** | a caret after the final newline; a one-character backward selection | Neovim's mode other than Normal, Insert and Replace |
| **reset, and said** | the standard Editor's undo history | Neovim's undo history, registers, marks and jumplist; extra windows, tab pages and clean buffers; a macro being recorded; an unfinished count (cancelled with Escape) |
| **needs consent** | never | unsaved changes in other Neovim buffers; running terminal jobs |
| **refused** | an open, a relay or a paste in flight | a prompt; a buffer that is not a file; a modified buffer with no file name; an open or paste in flight |
| **refused by the standard Editor** | — | bytes it does not edit (outside printable ASCII and tab), mixed line endings, or a file past the size it opens, naming the line |
