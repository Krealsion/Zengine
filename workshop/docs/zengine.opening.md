# zengine.opening -- the opening office

**Manual.** The opening office opens a source file in the Editor. It is Workshop's opening manager,
the weave that answers to the office `zengine.opening` (an office is the name a weave answers to).
A pane that wants a file shown in the Editor asks it once; it has the Editor prepare the file and
the desk make room for it, publishes both at once, and answers when both have shown it. Workshop
mounts it; it is not a file you load.

## About

Opening a file changes two things that belong to two owners: the Editor's document and the desk's
presentation of the Editor pane (its seat, its room, its picture). The opening office owns neither.
It owns the one open in flight: its path, who asked, the stage it has reached and the office it
waits on. It asks the desk whether the Editor pane would seat and with what room (the trial), the
Editor to prepare the file as a whole candidate beside the current document (the preparation), and
the desk to admit that candidate (the admission). Then it publishes both owners' claims in one
protected step, through Loom's joint publication, and answers only once the bus records that both
owners applied them.

Until that step, the document you have stays current and editable, and nothing is moved: input to
the Editor pane, a change to the desk's panes or a replaced owner ends the open, and your work is
kept. After it, nothing is rolled back: an owner that could not apply what was published is named.
There is one open at a time, no queue and no retry: a newer request ends one still preparing, and a
refused one is simply asked again. An owner that never answers leaves the open waiting, readable
and on the desk as a standing condition, with no timeout.

## Use

Ask with [`OpenSourceRequested`](#opensourcerequested), as your office, and wait for the one
`SourceOpened` answered to you. Files, the Builder pane and a pane's Edit Code ask it, each after
Workshop has judged your hand may `open`; the Editor relays an `OpenSourceRequested` sent to
`zengine.editor` here, in the asker's right. A guest row needs the `open` power. The shapes are
declared in [`workshop/pane_seam_vocabulary.hpp`](../pane_seam_vocabulary.hpp) and the conversation
in [`workshop/open_seam_vocabulary.hpp`](../open_seam_vocabulary.hpp).

## Commands

### `OpenSourceRequested`

`OpenSourceRequested` v1 asks the opening office to open a file as the Editor's document, shown in
the Editor pane.

- **Takes:** `path`, the file to open.
- **Answers:** `SourceOpened` v1, to the asker alone, under its correlation, once the open ends:
  `accepted` true when the document and the desk were published together and both owners applied
  them; otherwise `accepted` false and `refusal` in the words below. Meanwhile the desk is told the
  stage (`trial`, `prepare`, `admit`, `apply`) and the office awaited.
- **Refuses:** in `refusal`, with `a.cpp` for the path: `the room or the document changed while
  opening a.cpp -- your work is kept; try again`; `no Editor and desk are present to open a.cpp`;
  `too many opens on this bus are still unsettled -- a.cpp was not opened; try again`; `the Editor
  or the desk was replaced while opening a.cpp -- try again`; `zengine.editor could not be reached
  (<the bus's reason>) -- a.cpp was not opened`, or the desk's office; `the Editor could not be
  asked to prepare a.cpp`, and the same for the desk to show or admit it; `the desk and the Editor are still applying a.cpp
  -- b.cpp was not opened; try again`; to the older asker, `superseded by a newer request to open
  b.cpp -- a.cpp was not opened`; and the desk's or the Editor's own refusal, in its words (a
  document with unsaved changes, a paste still arriving). After publication: `zengine.editor did not
  apply a.cpp after it was published -- it kept what it had; open it again`, or `... could not
  apply ...` and `... was removed before it could apply ...`, naming the owner. An ask with no
  office behind it is not answered.
- **Example:** `mail.as_role("example.tool").send_to_role("zengine.opening",
  zengine::workshop::OpenSourceRequested{"/home/me/game/tally.cpp"}, ask);`, as the Builder pane's
  [`builder.edit-source`](../../builder/docs/zengine.builder-pane.md#builderedit-source) does.

## Hears

Each answer below counts only when it is the answer to this office's own ask, under the open's
number and at the stage it waits on; any other is ignored. It also reads three of Loom's words
against the bus's own record of the open: `zen.DispatchRefused` (an ask it sent was refused before
it arrived), `zen.JointEnded` (the bus ended the open, a participant replaced or a claim moved) and
`zen.JointApplied` (both owners' showings settled).

### `PresentationTrial`

`PresentationTrial` v1 and v2, from the desk: whether the Editor pane would seat and with what
room; v2 also reserves the canvas room the pane is shown in, so the Editor prepares a picture for it
rather than rows. A refused trial ends the open in the desk's words.

### `SourcePrepared`

`SourcePrepared` v1 and v2, from the Editor: the candidate document, as rows (v1) or as its picture
for the reserved room (v2), or the Editor's refusal. A candidate is handed to the desk to admit.

### `PresentationAdmitted`

`PresentationAdmitted` v1, from the desk: the candidate admitted, and the office publishes both
claims at once; or refused, in the desk's words, and the open ends with nothing moved.

## Follow

It publishes nothing to everyone. It tells the two owners, and only them, what stage an open is at
(`ManagedOpenProgress` v1; its `apply` word is the delivery that shows each owner what was
published) and how it ended (`ManagedOpenSettled` v1). Its state, `OpeningState` v1, answers a poke:
the open in flight (`op`, `path`, `stage`, `awaiting`, `attempt`, `requester`), the latest result
(`last_path`, `last_outcome`, `last_refusal`, `last_op`, 0 for a request refused before an open
began), what it has counted (`committed`, `unapplied`, `refused`, `answers_lost`), and `retained`,
the one commitment it keeps after an owner could not apply it, for the word about its repair.

## See also

- [`workshop/opening.hpp`](../opening.hpp), the opening manager.
- [Develop Workshop from inside Workshop](develop-workshop.md), where a pane's Edit Code opens its
  source.
