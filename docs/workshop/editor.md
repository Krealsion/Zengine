# The source editor

**Reference, current state.** Workshop's built-in editor for one source file at a time —
open it, edit it like an ordinary text document, save exactly what you meant to save, and
continue through the existing build-and-realize loop without leaving the application.

## Opening a source

Two things can ask the editor to open a file, and both ask the **same door**, so both get
the same behaviour, the same refusals and the same words:

- the **Files** pane — browse the project you launched Workshop in and open any file in it
  (see [Files](files.md));
- the **Builder** — choose a recipe (`c` steps through them) and press **`e`**, which opens
  that recipe's source.

A recipe that names no single source — a `cmake_target` recipe builds a project of its own —
refuses in those words. There is no path argument and no multi-file session: the editor holds
**one** document, and asking for a different source while the current one has unsaved edits
is refused until you save them (`Ctrl`+`s`) or deliberately discard them (`Ctrl`+`d`). Asking
for the source that is *already* open just brings the pane and your keys back to it, edits,
caret and selection intact — including when you ask for it the *other* way, because the two
doors resolve a file to the same identity.

### The file you edit is the file the build reads

A `single_source` recipe may spell its source **relative** — `src/main.cpp` — and a relative
spelling only means something once you say what it is relative to. Workshop's answer is the
**project**: the directory you launched it in, printed on the startup banner as
`zengine-workshop - project: ...`.

That resolution happens **once**, when the recipe catalog is read, and everything afterwards
spends the same answer — the editor's door, the build's own check that the file is there, and
the generated project that actually compiles it. So the file `e` opens and the file `b`
compiles are the same file, whether the recipe spelled it relatively or absolutely.

An **absolute** source keeps exactly the meaning it has. If Workshop could not determine a
launch directory at all it says so on the banner, and a relative source is then refused rather
than guessed at.

The Editor is an ordinary pane: the picker (`p`) opens and removes it, arranging moves and
resizes it, and its row in a saved setup comes back like any other. **None of that touches
the document** — the source, its unsaved edits, its caret and its scroll position live in
the session, not in the pane, so hiding or rearranging the presentation can never lose a
byte of your work.

## Editing

While the editor holds the keyboard (press into its body; press anywhere else to leave),
typing edits the source and the ordinary editing keys work **across lines** — a newline is a
boundary between lines, not a place where editing stops:

| key | does |
|---|---|
| `Enter` | insert a newline |
| `Tab` | insert a tab byte |
| `Backspace` / `Delete` | erase; at a line's edge, join with the neighbouring line |
| arrows | move, crossing line edges; `↑` `↓` keep a **preferred column** through short lines |
| `Home` / `End` | the line's ends; `Ctrl`+`Home` / `Ctrl`+`End` the document's |
| `Ctrl`+`←` / `Ctrl`+`→` | word by word, crossing line edges |
| `Shift` + any movement | extend the selection |
| `Ctrl`+`a` | select the whole document |
| `Ctrl`+`c` / `x` / `v` | copy / cut / paste, newlines included |
| `Ctrl`+`z` / `Ctrl`+`y` (or `Ctrl`+`Shift`+`z`) | undo / redo, this document's own history |
| `Ctrl`+`s` | **save the source** |
| `Ctrl`+`d` | discard unsaved edits — back to the last saved state, deliberately (and undoably) |

The mouse places the caret exactly where the paint says it is — tabs included — and dragging
sweeps a selection across as many lines as the hand covers. The wheel scrolls the editor's
body without moving the caret; the next caret gesture brings the view back. On a terminal
with no wheel, the keyboard is the viewport: arrows, `Ctrl`+`Home`/`End`.

These editing keys are the editor's own mechanics and are **not remappable** — the hotkey
view (`Ctrl`+`k`) lists them for the editor exactly as it lists a text box's. The four
actions that *are* keymap rows (`editor.save`, `editor.newline`, `editor.tab`,
`editor.discard`) remap like any other.

## `Ctrl`+`s` follows the keyboard

Workshop has two things worth saving and one save chord, and the chord follows your hands:

- **the editor holds the keyboard and a source is open** → `Ctrl`+`s` saves **the source**;
- **anywhere else** → `Ctrl`+`s` keeps its standing meaning: save the **object document**.

These are two separate actions in the keymap (`editor.save` and `document.save`), declared
so that no state has both active — remap either without touching the other. `Ctrl`+`o` keeps
its one meaning everywhere: open the object document.

## Save, dirty, and never losing work

The editor's header says it plainly: **`UNSAVED`** while the buffer differs from the file,
**`saved`** when they match — computed by comparison, so editing back to the saved text is
clean again. Beside it: the caret's line and column (`L12:C5/40` — line 12, column 5, of 40
lines) and the file's path.

A save writes atomically: the previous good file is never at risk from a failed write, and a
refusal leaves the buffer and its dirty state exactly as they were. The build reads the file
on disk — **an unsaved buffer is not built**, which is exactly what the `UNSAVED` word is
telling you before you press `b`.

Nothing ordinary can throw dirty source away:

- opening a **different** source is refused until you save or discard;
- hiding, moving or removing the **pane** touches the presentation only;
- an orderly **quit** (`q`, `Ctrl`+`c`, the close box) is refused with the two ways out
  named — save, or discard — and proceeds the moment the buffer is clean.

The one thing that loses an unsaved buffer is the one thing that loses every draft here: the
process dying disorderly. The discard chord works from command mode too, so the quit
refusal's remedy is pressable wherever you are standing.

## Bytes, exactly

The editor edits what is in the file and writes what you edited — nothing else:

- **tabs are preserved** and displayed at a fixed four-column stop; the caret, the mouse
  and selections map truthfully between bytes and displayed columns around them;
- **line endings are preserved** — an LF file stays LF, a CRLF file stays CRLF, inserted
  newlines follow the file's own convention, and whether the file ends in a final newline
  survives round trips exactly;
- a file that **mixes** endings, carries a bare carriage return, a control byte, or bytes
  outside plain ASCII is **refused whole**, with the offending line named, and the file left
  untouched — this editor never rewrites bytes you did not edit, and never pretends to show
  text the media cannot place truthfully (see [limitations](limitations.md));
- pasting text with foreign line breaks converts them to line breaks; pasting bytes outside
  plain ASCII is refused rather than silently mangled.

## The loop

```
open a file (Files pane, or the Builder's e) → edit → save (Ctrl+s)
    → build (b) or build & realize (Shift+b / f) → inspect (Project / Powers / Loaded)
    → press back into the editor and go again
```

Everything after **save** is the Builder's and the project's, unchanged — the editor adds no
second build path, no auto-save and no auto-build; every compile still starts from your own
gesture, over the bytes you explicitly saved. Opening a file you have no recipe for is fine:
you can read and edit anything in the project, and what can be *built* is still whatever the
recipes say.
