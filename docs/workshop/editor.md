# The source editor

**Reference, current state.** Workshop's editor for one source file at a time — open it, edit
it like an ordinary text document, save exactly what you meant to save, and continue through
the existing build-and-realize loop without leaving the application.

The Editor is a **loaded pane** (`zengine-editor-pane`, named by both shipped load plans), like
Files, the Builder and the Terminal. It arrives in the picker (`p`) as `Editor`, is moved,
resized, covered and removed like any other pane, and its row in a saved setup comes back like
any other — a setup written while the Editor was still compiled into Workshop is converted at
read, so an old desk keeps its Editor. What makes it different from every other loaded pane is
what it holds: **the document is the Editor's own.** The file's bytes, your unsaved edits, the
caret, the selection, the undo history and the scroll position all live in the pane's weave,
and Workshop holds none of them.

## Opening a source

Two things can ask the Editor to open a file, and both ask the **same door**, so both get
the same behaviour, the same refusals and the same words:

- the **Files** pane — browse from where you launched Workshop, anywhere your system lets
  this process read, and open any file you find (see [Files](files.md));
- the **Builder** — choose a recipe (`c` steps through them) and press **`e`**, which asks the
  project which file that recipe names and then opens it.

A recipe that names no single source — a `cmake_target` recipe builds a project of its own —
refuses in those words. There is no path argument and no multi-file session: the Editor holds
**one** document, and asking for a different source while the current one has unsaved edits
is refused until you save them (`Ctrl`+`s`) or deliberately discard them (`Ctrl`+`d`). Asking
for the source that is *already* open just brings the pane and your keys back to it, edits,
caret and selection intact — including when you ask for it the *other* way, because the two
doors resolve a file to the same identity.

Opening a file is **one action, and it either happens or it does not**. The Editor reads and
judges the file, asks Workshop to show the pane, and takes the document only if Workshop did:
seated if it was not on your desk, selected, with your keys in it. On a screen with no room for
another pane **nothing opens** — the document you already had, its caret and its history stand
exactly as they were, and the refusal you get is the picker's own words (`no room for Editor on
this screen -- make the window taller, then p again`). Make the window taller and ask again. If
the Editor pane is not loaded at all (a load plan without it), Files and the Builder have nobody
to ask, and their open does nothing.

### The file you edit is the file the build reads

A `single_source` recipe may spell its source **relative** — `src/main.cpp` — and a relative
spelling only means something once you say what it is relative to. Workshop's answer is the
**project**: the directory you launched it in, printed on the startup banner as
`zengine-workshop - project: ...`. The Editor asks Workshop where that is, once, and resolves
every path against it.

That resolution happens **once**, when the recipe catalog is read, and everything afterwards
spends the same answer — the project's answer to the Builder's `e`, the build's own check that
the file is there, and the generated project that actually compiles it. So the file `e` opens
and the file `b` compiles are the same file, whether the recipe spelled it relatively or
absolutely.

An **absolute** source keeps exactly the meaning it has, always. A **relative** one means
nothing until the project has answered: until then the Editor refuses it and says so, rather
than opening whatever file of that name happens to sit beside the running program. If Workshop
determined no launch directory at all it says so on the banner, and a relative source is then
spelled as you wrote it.

## Editing

While the Editor holds the keyboard (press into it; press anywhere else to leave), typing
edits the source and the ordinary editing keys work **across lines** — a newline is a
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
sweeps a selection across as many lines as the hand covers; dragging past the top or bottom
edge scrolls the document a line at a time under the hand. The wheel scrolls the Editor's
body without moving the caret; the next caret gesture brings the view back. On a terminal
with no wheel, the keyboard is the viewport: arrows, `Ctrl`+`Home`/`End`.

The editing keys are the Editor's own mechanics and are **not remappable**; the four actions
that *are* keymap rows (`editor.save`, `editor.newline`, `editor.tab`, `editor.discard`) are
the pane's declared rows and remap like any other pane's. The hotkey view (`Ctrl`+`k`) lists
those four while the Editor holds the keys; the rest of its vocabulary is described as the
pane's own, the way every loaded pane's is.

An **empty** Editor — no file open — still takes the keys when you press into it, like every
loaded pane, and does nothing with them; press elsewhere for Workshop's own keys.

## `Ctrl`+`s` follows the keyboard

Workshop has two things worth saving and one save chord, and the chord follows your hands:

- **the Editor holds the keyboard** → `Ctrl`+`s` saves **the source**;
- **anywhere else** — command mode, the picker, a layout's name, a draft, another pane --
  `Ctrl`+`s` keeps its standing meaning: save the **object document**.

These are two separate actions (`editor.save`, declared by the pane, and `document.save`,
Workshop's own), and what tells them apart is the Editor's **declaration** that its row stands
in for the document's save. So no state has both active, and remapping either one leaves that
true: move `editor.save` to `Ctrl`+`e` and `Ctrl`+`s` still will not save the object document
while you are typing in the source. `Ctrl`+`o` keeps its one meaning everywhere: open the
object document.

## Save, dirty, and never losing work

The pane's status row says it plainly: **`UNSAVED`** while the buffer differs from the file,
**`saved`** when they match — computed by comparison, so editing back to the saved text is
clean again. Beside it: the caret's line and column (`L12:C5/40` — line 12, column 5, of 40
lines) and the file's path, cut from its *front* when the pane is narrow so the file's name is
what survives. What the last act came to — `editing ...`, `saved`, a refusal — stands on a row
of its own under the status row until your next act; in a pane two rows tall it takes the
status row's place so the document keeps its row.

A save writes atomically: the previous good file is never at risk from a failed write, and a
refusal leaves the buffer and its dirty state exactly as they were. The build reads the file
on disk — **an unsaved buffer is not built**, which is exactly what the `UNSAVED` word is
telling you before you press `b`.

Nothing ordinary can throw dirty source away:

- opening a **different** source is refused until you save or discard;
- hiding, moving or removing the **pane** touches the presentation only — bring it back from
  the picker and the document, its caret and its undo history are exactly where they were;
- an orderly **quit** (`q`, `Ctrl`+`c` where nothing takes text, the close box) **asks** the
  Editor first and is refused while source is unsaved, with the two ways out named — save, or
  discard — and proceeds the moment the buffer is clean;
- a **reload** of the Editor's own image (a rebuild of `zengine-editor-pane` loaded in place)
  carries the document across whole: bytes, unsaved edits, caret, selection and scroll
  position. What does not survive a reload is the undo history and a paste still on its way.

The one thing that loses an unsaved buffer is the one thing that loses every draft here: the
process dying disorderly. The discard chord is the Editor's own row now, so it works while the
Editor holds the keys; the quit refusal names the acts rather than the keys, because your keys
may not be in the Editor when you read it.

## Bytes, exactly

The Editor edits what is in the file and writes what you edited — nothing else:

- **tabs are preserved** and displayed at a fixed four-column stop; the caret, the mouse
  and selections map truthfully between bytes and displayed columns around them;
- **line endings are preserved** — an LF file stays LF, a CRLF file stays CRLF, inserted
  newlines follow the file's own convention, and whether the file ends in a final newline
  survives round trips exactly;
- a file that **mixes** endings, carries a bare carriage return, a control byte, or bytes
  outside plain ASCII is **refused whole**, with the offending line named, and the file left
  untouched — this editor never rewrites bytes you did not edit, and never pretends to show
  text the media cannot place truthfully (see [limitations](limitations.md));
- a file over **4 MiB** is refused before a byte of it is judged;
- pasting text with foreign line breaks converts them to line breaks; pasting bytes outside
  plain ASCII is refused rather than silently mangled, and so is typing them.

## The loop

```
open a file (Files pane, or the Builder's e) → edit → save (Ctrl+s)
    → build (b), load after build (Shift+b then b, or f) → inspect (Project / Powers / Loaded)
    → press back into the Editor and go again
```

Everything after **save** is the Builder's and the project's, unchanged — the Editor adds no
second build path, no auto-save and no auto-build; every compile still starts from your own
gesture, over the bytes you explicitly saved. Opening a file you have no recipe for is fine,
and so is opening one outside your project: you can read and edit anything this process can
read, what can be *built* is still whatever the recipes say, and the file you opened does not
become part of your project by having been opened.
