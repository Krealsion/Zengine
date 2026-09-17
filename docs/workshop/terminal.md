# Terminal

**How-to.** Talking to the weaves of a running Workshop: typing a command, recalling one you
have already run, reading back through what the session has said, and choosing where a message
goes from what is actually on the bus.

## Opening the pane, and what it is

The Terminal is an ordinary pane. Press **`p`**, choose **Terminal**, press Return, then press
into it — its keys are its own only while it holds the keyboard, exactly like every other pane.

What you are looking at is **one participant on this Workshop's own bus**: a weave with its own
identity, its own vocabulary and its own narrow authority. You type a line; it authors the
message. Its header names it (`TERMINAL -- weave #3`), and its record is the participant's, not
the pane's — the pane is a presentation of it, and a second presentation of the same participant
would show the same record.

A line is four words and then arguments:

```text
send @zengine.skin SurfaceText 1 slot=hello text=there
ask  @zengine.editor-switch EditorSwitchStatusRequested 1
```

`send` authors one message; `ask` authors one and remembers it until Loom's answer arrives. Those
are the two verbs, and they are the two the submitter runs — nothing is offered that would not run.

## Recalling a command you have already run

**Up** and **Down** walk the commands this session has run, newest first, when no command is being
composed — an empty line with no completion list asked for. While you are browsing, they keep
walking: a recalled command sitting on the line does not turn the arrows back into completion.

The row above the line says where you are, `history 2 of 7`, and how to leave:

| key | while browsing history |
|---|---|
| `↑` `↓` | older, newer |
| `Enter` or `Tab` | **keep this line for editing** — and nothing else on that press: it does not run and does not complete |
| any editing key | leave history, and do its ordinary work once (a character is typed once, an erase erases one) |
| `Esc` | back to the line you had before the first recall, whole |

So `↑` `↑` `Enter` leaves the older command on the line, ready to edit, having run nothing; the
**next** Enter runs it. The same is true of Tab: the first press keeps the line, the next asks
what can follow it, and the one after that takes a candidate.

**What history is, exactly.** The commands the participant recorded — never its answers, notices or
refusals — kept as long as its record keeps them, which is the newest 256 entries of every kind.
Running the same command twice in a row is one step when you walk back. It is **this run's**:
Workshop does not write your command history to a file, and a new launch starts with none.

## Reading back through the record

The pane shows a **view** onto every row of the record, wrapped at its own width. It follows the
newest output until you read away from it:

| gesture | what it does |
|---|---|
| wheel over the pane | three rows a notch |
| `Ctrl`+`↑` `Ctrl`+`↓` | a page of the view, up or down |
| `Ctrl`+`Home` `Ctrl`+`End` | the oldest row the record keeps, and back to the newest |
| a press on the row below the view | back to the newest |
| running a line | back to the newest, where its output arrives |

The row above the view says what is above it, and while you are reading away from the end, the row
below says what is below it and is the press back:

```text
... 37 more rows above; 12 older entries dropped for good
... 9 more rows below -- press here for the newest
```

Those two numbers are **different facts**. Rows *above* are rows you can scroll to. Entries
*dropped for good* were evicted from the participant's own bounded record and no scrolling reaches
them; if the entry you were reading is evicted while you read it, the view moves to the oldest kept
and says so. The unit is a row, not a message, because a long entry is cut across rows and calling
part of one "an entry" would be a count you could not act on.

Reading moves nothing else: not the line you are typing, not a recalled command, not the completion
list, not a refusal standing beside your line. New output while you are reading leaves your place
where it is and counts itself below.

**A long line is read by scrolling**, not by widening the pane: following the newest row shows the
*end* of a long entry, and its beginning is above.

## Choosing where a message goes

**Tab** asks what can follow what you have typed. At the address — the second word — the list is
what is on this bus at that moment:

```text
where it goes -- on this bus now; not permission, nor a promise at send
> *                 everyone that accepts the shape
  @zengine.editor   held by #14 now; reaches whoever holds it when sent
  @zengine.skin     held by #4 now; reaches whoever holds it when sent
  #3                no office; accepts zen.Ack, zen.Refused (this terminal)
  #4                @zengine.skin; accepts SurfaceText, SurfaceCanvas +3
```

Everyone first, then the offices held right now by name, then every weave by id — each saying what
it is, so you can choose by identity rather than by guessing a number. Typing narrows it: `@zen`
lists offices, `#1` lists ids beginning with 1.

It is **read at the moment you ask**, from the bus's own facts, and nothing is kept: a weave loaded,
removed or replaced shows up in the next list. Two things it does not claim:

- **not permission.** Being able to name a destination is not authority to send to it. Authority is
  per shape and per target, and a send you are not granted is refused with that reason.
- **not a promise.** `#4` is that exact weave, and if it is gone when you send, Loom refuses the
  delivery and the refusal is on your record — it is never quietly sent somewhere else. `@office`
  reaches **whoever holds that office at delivery**, which the list says beside it; if nobody holds
  it, the send is refused the same way.

The rest of the line completes from the participant's own vocabulary: shapes it knows with their
versions, and field names (never values — a value is yours to type). The heading over the arguments
is the composer's verdict on what you have typed so far, `ready; Return submits it` or what is
missing.

## Leaving the pane

`Esc` sheds one layer per press, the most specific first:

| the pane holds | `Esc` |
|---|---|
| a completion list | dismisses the list; the line is untouched |
| a recalled command | back to the line before the recall |
| a line | clears the line |
| nothing of its own | **puts the pane down** — your keys go back to Workshop |

So `Esc` `Esc` from a half-typed command clears it and then hands the keys back; the band at the
bottom stops saying typing goes to the Terminal, and the pane keeps its record, its place and its
size. Nothing closes.

That last step is the pane's own word: Workshop cannot see whether a pane spent a key, so a pane
that has something to do with `Esc` keeps it. **Both editors keep every `Esc`** — the standard
Editor by design, Neovim because it is Neovim's — and the way out of those is a press elsewhere,
which the keyboard band names while they hold the keys.

## What it does not do

- **No shell.** There is no process, no `cd`, no pipeline: the two verbs author messages on this
  bus. A refused line says why on the record.
- **No history file**, and no history from another launch.
- **No second grammar.** The line is read by Loom's own command grammar, the same one
  `loom-host`'s console uses.
- **A press does not sweep a selection** across the line: a press places the caret, a second press
  in the same word selects it, and `Shift` with the caret keys selects by keyboard.

The exact contracts behind all of this — what crosses between the pane and the host, what the
record is, what a completion answer applies to — are in
[the pane protocol reference](../reference/workshop-panes.md) and the Workshop law the repository
keeps beside its source.
