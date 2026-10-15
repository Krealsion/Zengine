# zengine.editor-switch -- the editor switch

**Manual.** The editor switch: the weave that moves the Editor's office, `zengine.editor`, from the
editor holding it to another the load plan authors, carrying the document across, answering to the
office `zengine.editor-switch` (an office is the name a weave answers to). Workshop mounts it; it is
not a file you load. A weaver asks it from Workshop's Terminal; another weave reaches it by the
messages below. The Editor it switches is [`zengine.editor`](zengine.editor.md).

## About

The load plan authors the *choices* an office may be switched between, each an artifact under a
name, and which one holds the office at the start ([the load plan's
choices](../../workshop/docs/load-plan.md)). Both shipped plans author `standard`, the standard
Editor, which starts, and `neovim`, the Neovim-backed Editor. Every other choice is loaded only when
a switch asks for it.

A switch replaces the weave holding the office through Loom's prepared replacement, and carries the
document through a handoff every editor speaks: the editor in office judges what a switch would
lose, the destination is loaded sealed and started, the editor in office hands over the exact
document and holds still, the destination adopts it, the office moves, the successor proves it
serves, and the retired editor is unloaded. What crosses exactly, what is reset and what needs your
consent is [what crosses a switch](editor-switch.md).

Before the office moves, every ending moves nothing: the destination is discarded, and the editor in
office is told, resumes and says so on its pane. After it, nothing is rolled back. One switch runs
at a time, and nothing times out: a participant that never answers leaves the switch pending and
cancellable.

## Use

Open the Terminal, <!-- key desktop.terminal -->`ctrl+t`<!-- /key -->, and type
`ask @zengine.editor-switch EditorSwitchRequested 1 destination=neovim`. What came of it is said
on Workshop's notice line, `editor switch 1: switched -- ...`, and while a switch is under way the
Attention pane keeps it standing, with the line that cancels it. When a switch would lose
something, the notice gives the exact line that confirms it, `op` and `consent` filled in; type it,
or cancel. [`EditorSwitchStatusRequested`](#editorswitchstatusrequested) says where you are.

Who may send: Workshop's Terminal is granted the four commands to this office and the answer, and
nothing else of it. Every command is an ask, answered with `EditorSwitchAnswered` v1 to the asker.

## Commands

### `EditorSwitchRequested`

`EditorSwitchRequested` v1 asks to switch the Editor to the authored choice with this name.

- **Takes:** `destination`, a choice's name.
- **Answers:** `EditorSwitchAnswered` once the switch ends or stops to ask, its `outcome` one of
  `switched`
  (`` switched zengine.editor from `standard` to `neovim` ``, then what was reset and what else was
  said), `already-active` (`` `neovim` already holds zengine.editor; nothing was loaded ``),
  `needs-confirmation` (`` switching to `standard` would lose <the losses>; confirm with op 3 and
  consent <the digest>, or cancel ``, its `losses` and `consent` filled in), `refused` or
  `failed-after-commit`; with `active`, the choice holding the office, and `choices`, every authored
  name.
- **Refuses:** with `outcome` `refused`, nothing moved: `name the choice to switch to`; `` this
  project authors no editor choice named `vim` ``; `nobody holds zengine.editor -- there is no
  editor to switch from`; `` switch 2 to `neovim` is under way (warming) -- ask again once it has
  settled ``; and the words of whatever refused, as `` `neovim` could not start: Neovim is not
  available: ... `` or `the editor in office refused: <its words>`; `the editor in office does not
  take part in switches: ...` when it cannot be asked. A request while another only awaits
  confirmation replaces it; a confirmation or cancel of that one is answered `superseded`.
- **Example:** in the Terminal,
  `ask @zengine.editor-switch EditorSwitchRequested 1 destination=neovim`.

### `EditorSwitchConfirmed`

`EditorSwitchConfirmed` v1 agrees to the losses a switch named, by their digest, and the switch
goes on.

- **Takes:** `op`, the switch; `consent`, the digest its `needs-confirmation` answer carried.
- **Answers:** `EditorSwitchAnswered` as [`EditorSwitchRequested`](#editorswitchrequested) once the
  switch ends. When what would be lost changed before the editor in office held still, the answer is
  `needs-confirmation` again, with a new consent, and nothing crosses on the old one.
- **Refuses:** `switch 3 is not awaiting confirmation`; `` that consent is not the one this switch
  asked for -- confirm with `<the digest>` ``, as `needs-confirmation`; `switch 3 was replaced by
  switch 4 before it was confirmed`, as `superseded`.
- **Example:** `ask @zengine.editor-switch EditorSwitchConfirmed 1 op=3 consent=c4f2a91c7`, with
  the numbers your notice gave. A consent begins with a letter, so the Terminal reads it as text.

### `EditorSwitchCancelled`

`EditorSwitchCancelled` v1 stops a switch that has not committed; nothing moves.

- **Takes:** `op`, the switch.
- **Answers:** `` switch 3 to `standard` was cancelled; nothing moved ``, as `cancelled`; the
  destination is discarded and the editor in office resumes.
- **Refuses:** `switch 3 is not under way`; `switch 3 has already committed and cannot be
  cancelled`; `switch 3 was replaced by switch 4 before it was confirmed`, as `superseded`.
- **Example:** `ask @zengine.editor-switch EditorSwitchCancelled 1 op=3`.

### `EditorSwitchStatusRequested`

`EditorSwitchStatusRequested` v1 asks which choice holds the office, which are authored, and what
is under way.

- **Takes:** nothing.
- **Answers:** `EditorSwitchAnswered` with `outcome` `status`: `` `standard` holds zengine.editor
  (choices: standard, neovim); no switch is under way ``, or the switch under way and its stage,
  with its consent while it awaits confirmation; after a switch that failed once committed, `the
  retired <stem> is kept after a failure`. It ends nothing.
- **Refuses:** nothing.
- **Example:** `ask @zengine.editor-switch EditorSwitchStatusRequested 1`.

## Hears

### `EditorHandoffJudged`

`EditorHandoffJudged` v1, from the editor in office: what a switch away would lose, reset or
refuse, and the room the document is shown in. A refusal ends the switch, `the editor in office
refused: <its words>`; losses ask for your consent first. The handoff's shapes are
[`workshop/editor_handoff_vocabulary.hpp`](../../workshop/editor_handoff_vocabulary.hpp).

### `EditorWarmed`

`EditorWarmed` v1, from the destination, loaded sealed: it started what it needs, in the room the
editor in office had, or why not, `` `neovim` could not start: <its words> ``.

### `EditorHandoffOffered`

`EditorHandoffOffered` v1, from the editor in office: the exact document, and that it holds still,
taking no input until the outcome. A refusal ends the switch, `the editor in office refused at the
boundary: <its words>`.

### `EditorAdopted`

`EditorAdopted` v1, from the destination, Loom's preparation answer: it adopted the document, and
the office moves; or `` `neovim` could not adopt the document: <its words> ``, and nothing moves.

### `EditorLive`

`EditorLive` v1, from the successor holding the office: it serves. When it does not, the outcome is
`failed-after-commit`, `` `neovim` holds zengine.editor and did not prove it serves: ... -- the
retired `standard` was kept ``, and the next switch releases the retired editor.

### `EditorRetired`

`EditorRetired` v1, from the retired editor: how many inputs it refused while it held still, said
among the answer's notes before it is unloaded.

### `TimerReady`

`TimerReady` v1, from the Timer, `zengine.timer`: a Timer is there, and the switch orders its beat
from it again.

### `TimerFired`

`TimerFired` v1, the switch's beat, `zengine.editor-switch.beat`: while the destination starts or
adopts, each beat is handed on to it; idle, the beat stops itself.

### `TimerResolution`

`TimerResolution` v1, from the Timer: what became of the switch's order for its beat.

## Follow

The switch publishes its own account as `zengine.editor-switch`: `EditorSwitchProgress` v1 at each
stage while a switch is under way, `pending` true, with the stage, whom it waits on and the consent
to type; and once for every answer, `pending` false, with the outcome and its words. Workshop keeps
a pending switch as a standing condition, `switching the Editor to <choice>`, and says each outcome
on its notice line, `editor switch <op>: <outcome> -- <words>`. The stages are `judging`,
`awaiting-confirmation`, `warming`, `boundary`, `adopting`, `proving` and `retiring`.

## See also

- [`zengine.editor`](zengine.editor.md), the Editor the switch moves.
- [What crosses a switch](editor-switch.md): the document, its caret and selection, what is reset
  and what needs consent, each way.
- [Neovim in Workshop](neovim.md): switching while you work, walked through.
