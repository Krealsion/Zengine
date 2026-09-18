# The source editor

**Reference, current state.** Workshop's editor for one source file at a time — open it, edit
it like an ordinary text document, save exactly what you meant to save, and continue through
the existing build-and-realize loop without leaving the application.

**Neovim can be the Editor instead.** Both shipped plans author a second editor for the same
office, and a maker switches between them while working, carrying the document: see
[Neovim in Workshop](neovim.md). This page is the standard Editor.

The Editor is a **loaded pane** (`zengine-editor-pane`, named by both shipped load plans), like
Files, the Builder and the Terminal. It is listed in the Pane Manager (`Ctrl`+`p`) as
`Editor`, is moved, resized, covered and closed like any other pane, and its row in a saved setup comes back like
any other — a setup written while the Editor was still compiled into Workshop is converted at
read, so an old desk keeps its Editor. What makes it different from every other loaded pane is
what it holds: **the document is the Editor's own.** The file's bytes, your unsaved edits, the
caret, the selection, the undo history and the scroll position all live in the pane's weave,
and Workshop holds none of them.

## Opening a source

Three things can ask the Editor to open a file, and all of them ask the **same door**, so all
get the same behaviour, the same refusals and the same words:

- the **Files** pane — browse from where you launched Workshop, anywhere your system lets
  this process read, and open any file you find (see [Files](files.md));
- the **Builder** — choose a recipe (`c` steps through them) and press **`e`**, which asks the
  project which file that recipe names and then opens it;
- **`edit code`** on a pane's context menu — Workshop follows the pane to the artifact it was
  loaded from and the one recipe that builds it, and opens that recipe's source
  ([edit a running pane](edit-a-running-pane.md)); its answers land on the notice line.

The file a recipe names is its `source` when it builds one source file, and its **editing entry**
when it builds a CMake target — the file a reader of that artifact starts at, which is how
Workshop's own panes open ([develop Workshop](develop-workshop.md)). A `cmake_target` recipe with
no entry names no file, and refuses in those words. There is no path argument and no multi-file session: the Editor holds
**one** document, and asking for a different source while the current one has unsaved edits
is refused until you save them (`Ctrl`+`s`) or deliberately discard them (`Ctrl`+`d`). Asking
for the source that is *already* open just brings the pane and your keys back to it — edits,
caret, selection **and the place you had scrolled to** all intact, because a reveal moves the
pane and never the view. That holds when you ask for it the *other* way too, because the two
doors resolve a file to the same identity.

Opening a file is **one action, and it either happens or it does not**. The Editor reads and
judges the file and prepares it beside the document you have; Workshop checks that the pane
would have a seat and room; and then the two change together, in one step: the new document is
the Editor's and the pane is seated, selected, with your keys in it. Until that step the
document you already had is the one you are looking at and typing into — nothing you type or
paste is held back or moved somewhere else — and if you edit it, move its caret or change your
desk while the open is on its way, the open is **refused** rather than allowed to replace what
you just did; your work is where you put it, and you simply ask again. On a screen with no
room for another pane **nothing opens**: the document you already had, its caret and its
history stand exactly as they were, your desk is untouched, and the refusal you get says so
(`no room for Editor on this screen -- make the window taller, then try again`). Make the window
taller and ask again. A window you shrink *after* the open has taken
hides the pane the way it hides any other, with the new file in it; grow it back and the file
is there.

The same is true of a refusal for any other reason — a file that is not there, bytes the Editor
cannot carry, unsaved edits in the document you have open, a paste of yours still on its way.
Nothing is added to your desk, your keys do not move, and whoever asked is told why — in the
Files pane's or the Builder's own row, or on the notice line for `edit code`. If the Editor pane
is not loaded at all (a load plan without it), whoever asked is told so at once — `no Editor and
desk are present to open <file>` — and nothing waits. A refusal from the Editor, or from the open itself, says what
stopped it before it names a path, so a narrow row cuts the path and keeps the reason:
`the Editor holds unsaved changes to ...` still says why. When Files or the Builder cannot send
the open at all, their own row starts with the name you chose.

### While an open is on its way, and if it stalls

An open takes a few turns of Workshop's bus, and while it is on its way the Attention pane
shows a standing condition for it — `opening <file>`, and which side it is waiting for. A
second request while the first is still being prepared replaces it: the first is refused as
superseded, naming the newer file, and the newer one goes on. If a side never answers while
the open is being prepared (a broken Editor image, for instance), the open stays pending and
says so; nothing times out, nothing is retried behind your back, every other pane keeps
working, and your next open replaces it. Once the two sides have changed together, a second
request replaces nothing while they are still taking it up: it is refused — ask again a moment
later.

Rarely, an open is **published but not applied**: the two changed together, and then one side
did not take up its half. Whoever asked is told which side, and in what words:

- **could not apply** — a defect in that side's code. That side is *held*: it takes no keys,
  answers nothing and opens nothing until it is reloaded. Nothing is rolled back — whatever the
  other side took up stays as it is — so the Editor pane can stand seated, with your keys,
  in front of an Editor that is held; press elsewhere and Workshop's keys are yours again.
- **did not apply** — that side is working and kept what it had; open the file again.
- **was removed before it could apply** — that side went away first; open the file again once
  it is back.

**A held Editor is repaired by reloading its image**: rebuild `zengine-editor-pane` with a
recipe that builds it and load it in place from the Builder (see
[load after build](builder.md#load-after-build-and-reload-in-place)). Closing the Editor's pane
repairs nothing — it takes the pane off your desk, and the Editor behind it, its document and
its hold stay exactly as they were — and Workshop has no way to unload the Editor while it
runs. The reloaded Editor still has the document it had before the open, and nothing of the
file it could not apply: open that file again, a new open rather than a replay of the old one.

**A quit while the Editor is held is refused.** An orderly quit asks the Editor about unsaved
work, and a held Editor cannot be asked, so Workshop says so at once —
`the quit could not ask zengine.editor (weave 7) -- it is held until it is reloaded or removed
(ApplicationFailed); Workshop stays open, and a quit after the repair asks again` — and stays open
with nothing saved and nothing closed. Your keys are yours again straight away, and whatever you
pressed while the quit was asking is replayed in order. Reload the Editor as above, then quit
again: that is a fresh quit, and the reloaded Editor answers it. The same is true of any other
part of Workshop that takes part in the quit and cannot be asked, whether or not it shows a pane;
it is named the same way. A part that is asked and simply never answers still keeps the quit
waiting — pressing the close box then says what it is waiting for.

If the side that could not apply is **Workshop's own desk**, nothing inside the running
Workshop repairs it: Workshop cannot reload its own desk, and a held desk takes none of your
keys, so it cannot be asked to quit either. Workshop writes which side is held, in that side's
own words, to the console it was started from (and to its log, when started with `--log`). End
the Workshop process from outside and start it again; anything unsaved in Workshop itself is
lost.

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
absolutely. A `cmake_target` recipe's `entry` is completed by the same rule at the same moment,
and it is the file `e` opens; what `b` compiles for that recipe is whatever its CMake project says
the target is made of, of which the entry is where reading starts.

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
the pane's declared rows and remap like any other pane's. The Hotkeys pane (`Ctrl`+`k`) lists
those four under the Editor's own heading (`pane Editor @zengine.editor`); the rest of its
vocabulary is described as the pane's own, the way every loaded pane's is.

An **empty** Editor — no file open — still takes the keys when you press into it, like every
loaded pane, and does nothing with them; press elsewhere for Workshop's own keys.

## `Ctrl`+`s` is the Editor's

`Ctrl`+`s` saves **the source** while the Editor holds the keyboard: it is the Editor's own
`save source` row (`editor.save`), and moves in your keymap file like any pane's row. Anywhere
else it does nothing. It used to save the prototype **object document** there, and `Ctrl`+`o`
opened one; both retired with the object canvas, and Workshop has no document of its own left
for either chord to mean (a desk is written with `s` in command mode — [Setups](setups.md)).

The Editor's row still declares that it stands in for the old `document.save`. In this Workshop
that declaration stands in for nothing; in one from before the canvas retired it is what keeps
`Ctrl`+`s` the Editor's while you type in the source rather than a collision — so one Editor
image loads in both.

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
- hiding, moving or closing the **pane** touches the presentation only — bring it back from
  the Pane Manager and the document, its caret and its undo history are exactly where they were;
- an orderly **quit** (`q`, `Ctrl`+`c` where nothing takes text, the close box) **asks** the
  Editor first and is refused while source is unsaved, with the two ways out named — save, or
  discard — and proceeds the moment the buffer is clean (a *held* Editor cannot be asked, so the
  quit is refused until you [reload it](#while-an-open-is-on-its-way-and-if-it-stalls));
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
open a file (Files pane, the Builder's e, or edit code on a pane) → edit → save (Ctrl+s)
    → build (b; to load it too, Shift+b once turns load after build on, then b each time; or f)
    → inspect (Project / Powers / Loaded) → press back into the Editor and go again
```

Everything after **save** is the Builder's and the project's, unchanged — the Editor adds no
second build path, no auto-save and no auto-build; every compile still starts from your own
gesture, over the bytes you explicitly saved. Opening a file you have no recipe for is fine,
and so is opening one outside your project: you can read and edit anything this process can
read, what can be *built* is still whatever the recipes say, and the file you opened does not
become part of your project by having been opened.
