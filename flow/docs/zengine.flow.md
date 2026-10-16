# zengine.flow -- the Flow pane

**Manual.** The pane you author a running rule in. You draw a graph from the operators the host
already has: the messages it accepts, the state it keeps, the steps a message runs and the messages
it sends. You run it without compiling anything, change it while it runs and keep its state, and
write it out as C++. It is a *weave*: a piece of Workshop that is loaded, replaced and spoken to by
messages, built as `zengine-flow-pane`. Other weaves reach it by its *office*, the name it answers
to, `zengine.flow`. New to it? Start with [Author a running graph in Workshop](flow.md).

## About

What you author is a *definition*: a state shape, the messages it accepts, a *trigger* for each
accepted message, which runs a row of operator *nodes* in order and writes one state field, and the
messages it emits after that write. The pane keeps the definition you are editing as a *draft*,
with its layout, your examples and your unfinished forms, in one *workspace*.

The pane runs nothing itself. A run, an apply, a send, a stop, an inspection and a catalog read
are each asked of the Flow host, the office [`zengine.flow.host`](zengine.flow.host.md), as one
request answered once ([its vocabulary](../flow-host/vocabulary.hpp),
[the Flow host](flow-runtime.md)); the pane shows what it answered. The pane always names one
session there, `workshop`. The operators you can add are found through the host's discovery door, `zengine.powers`
([its manual](../../introspection/docs/zengine.powers.md)), which answers Flow, the Powers pane and
the Terminal alike. It asks only for what a contributor offers, with a port selected only for what
yields its type, and with a fold's body slot open only for what a fold can spend; it asks again when
the question changes, when Workshop grants it room, and after a run, an apply, a stop or a catalog
read. The ports a graph checks come from the Flow host's catalog. A file is written only after
Workshop says the hand that confirmed it may write one.

The pane draws on a canvas Workshop grants it. Given text rows instead, it shows the definition as
lines under `Flow: graphical editing needs a canvas-capable Workshop. Use FlowEdit or the
standalone workbench.` A picture too large for the canvas is replaced by one that says `View limit
reached: pan/zoom or choose a smaller graph.` and `Draft intact. Save or export it before changing
the graph.`, with Save, Export, the pages and the zoom controls. A pane too small says `Flow needs
more room`. Text outside printable ASCII is drawn as `?`, and text too long for its place ends in
an ellipsis; neither changes what is stored.

## Use

Open it from the Pane Manager, <!-- key desktop.panes -->`ctrl+p`<!-- /key -->, and give it room.
The bar along its top holds New, Open, Import, Export, Save, Run, Apply, Stop, Inspect, Catalog and
Discard, then the pages: Graph, State, Messages, Library and Events. Each page draws its own
buttons, and every button with no argument is one of the commands below; a press on a node, a port,
a row or a field is the page's own ([Shows](#shows)). The last two lines are the latest notice, and
the definition's name and draft revision with `interpreted live` or `stopped`.

The ordinary path: [`ask-new`](#ask-new), a state field ([`ask-state-field`](#ask-state-field)),
a message ([`ask-message`](#ask-message), [`ask-message-field`](#ask-message-field)), a trigger
([`ask-trigger`](#ask-trigger)), operators found on the Graph page and wired port by port, then
[`run`](#run) and [`send`](#send). Change the graph and [`apply`](#apply): the running participant
keeps its state.

Most commands open a dialog first; [`dialog-confirm`](#dialog-confirm) does what it asks. While a
dialog is open every other command says `Confirm or cancel the open dialog before another edit`.
Outside a dialog, what you type goes to the search line on the Graph page; the Escape key puts down
one thing per press (a dropped value, the wire in hand, the selected port, an open body slot, the
preview, the selected node, the search line) and only then hands the pane back to Workshop; the
Delete key removes the selected node; Return adds the previewed row.

## Commands

### `ask-save`

Saves the whole workspace: the draft, its layout and view, your examples and retained forms, and
the saved state.

- **Takes:** the workspace. Default key <!-- key ask-save -->`ctrl+s`<!-- /key -->; the bar's
  `[Save]`, `[Save*]` while there is unsaved work.
- **Answers:** the dialog `Save complete workspace`, its Path the file last saved or opened, else
  `my-flow.flow-workspace`. Confirmed, it asks Workshop for the write (`Saving is waiting for
  Workshop's approval`), then writes and says `Saved <path>`.
- **Refuses:** the writes' refusals ([`dialog-confirm`](#dialog-confirm)), and the file writer's
  reason, the dialog kept for repair.
- **Example:** press <!-- key ask-save -->`ctrl+s`<!-- /key -->, then
  <!-- key dialog-confirm -->`return`<!-- /key -->.

### `ask-open`

Opens a saved workspace in place of the draft.

- **Takes:** a path and what to do with unsaved work. Default key
  <!-- key ask-open -->`ctrl+o`<!-- /key -->; the bar's `[Open]`.
- **Answers:** the dialog `Open workspace (replaces the current draft)`, with Path and
  `Unsaved disposition` set to `discard`. Confirmed: `open complete`, the Graph page, scrolled so
  its nodes show.
- **Refuses:** `save the draft or explicitly choose discard`, with unsaved work and another
  disposition; `stop this project's session before opening another` while it runs; the file
  reader's reason.
- **Example:** press <!-- key ask-open -->`ctrl+o`<!-- /key -->, type the path, confirm.

### `run`

Runs the definition: the Flow host makes a fresh participant from it, with the draft's state.

- **Takes:** the draft, checked first. Default key <!-- key run -->`ctrl+r`<!-- /key -->; the bar's
  `[Run]`, `[Running]` once it runs.
- **Answers:** asks the Flow host ([`FlowRun`](flow-runtime.md#conversation)); the notice says `run
  queued`, which means only that the request went. The answer then says `run: observed`, or its
  reason, the bar reads `[Running]`, and the discovery door is asked again
  ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)), since running mounts the
  definition's operators.
- **Refuses:** the draft's first problem, such as an unwired port or `incompatible type`, naming
  the place; `run refused: <the host's reason>`; `run was not delivered: <the bus's reason>`;
  `Flow has 16 unanswered requests; inspect the host before retrying`.
- **Example:** press <!-- key run -->`ctrl+r`<!-- /key -->, then [`send`](#send) an example.

### `ask-generate`

Writes the definition as a C++ project that builds into a weave like any other.

- **Takes:** the draft, checked first. Default key <!-- key ask-generate -->`ctrl+g`<!-- /key -->;
  no control of its own.
- **Answers:** the dialog `Generate native C++ for this definition into a directory`, Directory
  `flow-generated`. Confirmed, it asks Workshop for the write, then writes `generated.cpp`,
  `CMakeLists.txt`, `definition.bin` and `graph.svg` there and says `Generated <the source's
  path>`.
- **Refuses:** a definition with a fold, in the generator's words, `the trigger on <message> folds
  at node <n>: a fold is spent by the evaluator, so this definition is not generated as C++ and
  runs interpreted`; `output belongs to a different definition`; `CMakeLists.txt has local edits;
  choose a new output directory`; the draft's first problem; the writes' refusals. Nothing is
  written then.
- **Example:** press <!-- key ask-generate -->`ctrl+g`<!-- /key -->, confirm.

### `ask-discard`

Lets the pane close without saving. Workshop's quit is refused while Flow holds something it would
lose: `Flow has unsaved work. Save it or explicitly Discard before quitting.`, `Flow has an
unfinished dialog. Confirm or cancel it before quitting.`, or `Flow is waiting for its host to
settle work. Quit again after the result arrives.`

- **Takes:** the word `discard`, typed. No default key; the bar's `[Discard]`.
- **Answers:** the dialog `Allow closing without saving (type discard)`, one field,
  Confirmation. Confirmed with `discard`: `Unsaved work may now be discarded on close`. Nothing is
  deleted.
- **Refuses:** `type discard to acknowledge unsaved work`.
- **Example:** press `[Discard]`, type `discard`, confirm.

### `ask-new`

Starts a new definition in place of the draft.

- **Takes:** a project name and what to do with unsaved work. No default key; the bar's `[New]`.
- **Answers:** the dialog `New project (replaces the current draft)`, with `Project name`
  `my_flow` and `Unsaved disposition` `discard`. Confirmed: `new complete`, an empty draft on the
  Graph page, no file path.
- **Refuses:** `save the draft or explicitly choose discard`; `stop this project's session before
  creating another`; `use a project name of 1 to 120 bytes`.
- **Example:** press `[New]`, type `water`, confirm.

### `ask-import`

Brings in an executable Flow project, as the standalone workbench saves one, in place of the draft.

- **Takes:** a path and what to do with unsaved work. No default key; the bar's `[Import]`.
- **Answers:** the dialog `Import executable Flow project (replaces this draft)`, Path
  `my-flow.flow`, `Unsaved disposition` `discard`. Confirmed: `import-project complete`, the Graph
  page.
- **Refuses:** `save the draft or explicitly choose discard`; `stop the session before importing
  another project`; the project reader's reason.
- **Example:** press `[Import]`, type `thermostat.flow`, confirm.

### `ask-export`

Writes the definition and its state as an executable project for the standalone workbench. A
workspace keeps unfinished work too; a project does not.

- **Takes:** the draft, checked first. No default key; the bar's `[Export]`.
- **Answers:** the dialog `Export executable project for the standalone Flow workbench`, Path
  `my-flow.flow`. Confirmed, it asks Workshop for the write, then writes it:
  `export-project complete`.
- **Refuses:** the draft's first problem; the writes' refusals; the file writer's reason.
- **Example:** press `[Export]`, confirm.

### `apply`

Changes the running participant's behaviour to the draft's, keeping its state.

- **Takes:** the draft, checked first; a running participant. No default key; the bar's
  `[Apply]`.
- **Answers:** asks the Flow host ([`FlowApply`](flow-runtime.md#conversation)): `apply queued`,
  then `apply: observed`, and the discovery door is asked again
  ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)). The participant keeps its
  state and its accepted, emitted and state shapes; a change to any of those needs [`stop`](#stop)
  and [`run`](#run).
- **Refuses:** the draft's first problem; `apply refused: <the host's reason>`, such as a changed
  shape; `apply was not delivered: <the bus's reason>`.
- **Example:** bind a port to another constant, press `[Apply]`, send the same example again.

### `stop`

Stops the running participant; its last state comes back as the draft's.

- **Takes:** the session. No default key; the bar's `[Stop]`.
- **Answers:** asks the Flow host ([`FlowStop`](flow-runtime.md#conversation)): `stop queued`, then
  `stop: observed`; the bar reads `[Run]` again and the discovery door is asked again
  ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)).
- **Refuses:** `stop refused: <the host's reason>`, such as while a send is still being
  delivered; `stop was not delivered: <the bus's reason>`.
- **Example:** press `[Stop]` before a schema change.

### `inspect`

Reads the running participant again: its state and what it did.

- **Takes:** the session. No default key; the bar's `[Inspect]`.
- **Answers:** asks the Flow host ([`FlowInspect`](flow-runtime.md#conversation)): `inspect queued`,
  then the Events page's rows ([`events`](#events)), and, unless you have edited the state since,
  the draft's state becomes the observed one. The pane inspects by itself when it starts and
  whenever the host says the session changed ([Hears](#flowchanged)).
- **Refuses:** `inspect refused: <the host's reason>` while it runs; while nothing runs it says
  nothing.
- **Example:** press `[Inspect]`, then `[Events]`.

### `catalog`

Reads the host's operators again: the ports the graph checks its wiring against.

- **Takes:** nothing. No default key; the bar's `[Catalog]`.
- **Answers:** asks the Flow host ([`FlowCatalog`](flow-runtime.md#conversation)) and the discovery
  door again ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)): `catalog
  queued`, then `Host operators refreshed`.
- **Refuses:** the host's reason; `operator descriptor identity disagrees`; `catalog was not
  delivered: <the bus's reason>`.
- **Example:** after a provider is loaded, press `[Catalog]`.

### `page-graph`

Shows the Graph page ([`graph`](#graph)).

- **Takes:** nothing. No default key; the bar's `[Graph]`.
- **Answers:** the Graph page, from its first row.
- **Refuses:** nothing.
- **Example:** press `[Graph]`.

### `page-state`

Shows the State page ([`state`](#state)), with the state's form open.

- **Takes:** the definition's state. No default key; the bar's `[State]`.
- **Answers:** the State page and the form `Initial / saved state`: `state-open complete`.
- **Refuses:** nothing.
- **Example:** press `[State]`.

### `page-messages`

Shows the Messages page ([`messages`](#messages)).

- **Takes:** nothing. No default key; the bar's `[Messages]`.
- **Answers:** the Messages page, from its first row.
- **Refuses:** nothing.
- **Example:** press `[Messages]`.

### `page-library`

Shows the Library page ([`library`](#library)).

- **Takes:** nothing. No default key; the bar's `[Library]`.
- **Answers:** the Library page, from its first row.
- **Refuses:** nothing.
- **Example:** press `[Library]`.

### `page-events`

Shows the Events page ([`events`](#events)).

- **Takes:** nothing. No default key; the bar's `[Events]`.
- **Answers:** the Events page, from its first row.
- **Refuses:** nothing.
- **Example:** press `[Events]` after a send.

### `fit`

Resets the graph's pan and zoom, then scrolls so its nodes show.

- **Takes:** the view. No default key; the Graph page's `[Reset view]`.
- **Answers:** the graph redrawn; the view is part of the workspace, so it is unsaved work.
- **Refuses:** nothing.
- **Example:** press `[Reset view]` after panning away.

### `zoom-in`

Zooms the graph in one step, up to the largest.

- **Takes:** the view. No default key; the Graph page's `[+]`.
- **Answers:** node positions and widths grow; text and port rows keep their size.
- **Refuses:** nothing; at the largest nothing moves.
- **Example:** press `[+]`.

### `zoom-out`

Zooms the graph out one step, down to the smallest.

- **Takes:** the view. No default key; the Graph page's `[-]`.
- **Answers:** node positions and widths shrink; text and port rows keep their size.
- **Refuses:** nothing; at the smallest nothing moves.
- **Example:** press `[-]`.

### `ask-trigger`

Adds a trigger: an accepted message that runs a row of nodes and writes one state field.

- **Takes:** a message, by its place among the accepted ones, and a state field. No default key;
  the Graph page's `[Add trigger]`.
- **Answers:** the dialog `An input message updates one state field`, with `Message index` (the
  selected message) and `State output field` `value`. Confirmed: `trigger complete`, and the new
  trigger is the active one.
- **Refuses:** `select a declared state output field`; `message already has a trigger`.
- **Example:** press `[Add trigger]`, keep `0` and `value`, confirm.

### `ask-emit`

Makes the active trigger publish an emitted message after its write.

- **Takes:** an emitted message by its place, and each field's source, `field=$state` or
  `field=constant`. No default key; the Graph page's `[Emit]`, while there are a trigger and an
  emitted message.
- **Answers:** the dialog `After the write, publish an emitted message`, with `Emitted index` `0`
  and `Fields (field=$state or field=constant)` filled from the state fields of the same names,
  `total=$total`. Confirmed: `emit complete`; the trigger lists `emits <name>` with `[x]` to
  remove it.
- **Refuses:** `add the trigger's nodes before its emits: an empty trigger is kept without them`;
  `<message> has no field <name>`.
- **Example:** press `[Emit]`, confirm `total=$total`.

### `add-fold`

Adds a fold at the end of the active trigger: the form that runs one operator for each count from a
start toward a limit by a step, threading an answer through.

- **Takes:** the active trigger. No default key; a fold row's `[Add]` under Forms on the Graph page.
- **Answers:** a node `body = [choose]`, its `step` bound to `1`: `add-fold complete`, and a fold's
  open body slot shut, so the discovery door is asked again
  ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)). Press its body row to
  choose the body ([`graph`](#graph)).
- **Refuses:** `add a trigger before adding a node`, with no trigger yet.
- **Example:** type `count`, press the fold's row, press `[Add]`.

### `ask-state-field`

Adds a field to the state.

- **Takes:** a name, a type and a presence. No default key; the State page's
  `[Add state field]`.
- **Answers:** the dialog `State field: Int, Bool, Float, Text, Bytes, List:Kind, Message:preset`,
  with Name `value`, Kind `Int`, Presence `required`. Confirmed: `state-field complete`, the State
  page and its form.
- **Refuses:** `state field name is empty or already used`; `presence must be required or
  optional`; `scalar authoring accepts Int, Bool, Float, Text; import-json carries full maker
  schemas`; `Message:<saved draft name> needs an existing library schema`.
- **Example:** confirm `value`, `Int`, `required`.

### `keep-state`

Makes the state form's value the definition's state: what a run starts with and a save keeps.

- **Takes:** the open state form, complete. No default key; the State page's `[Keep state]`.
- **Answers:** `keep-state complete`. Until a run takes it, the state you kept is not replaced by
  what the running participant reports.
- **Refuses:** `open a value first`; `state schema changed; reopen its form`; Loom's words for a
  value that does not fit its shape.
- **Example:** set `value` to `5`, press `[Keep state]`.

### `ask-message`

Declares a message the definition accepts.

- **Takes:** a name; one without a `.` is put inside the definition's own name. No default key; the
  Messages page's `[New message]`.
- **Answers:** the dialog `Declare an input message`, `Message name` `Set`. Confirmed:
  `message complete`, and the message's form opens.
- **Refuses:** `a message needs a name`; `a message named <name> is already declared`.
- **Example:** type `Reading`, confirm: `water.Reading`.

### `ask-message-field`

Adds a field to an accepted message.

- **Takes:** the message by its place, a name, a type and a presence. No default key; the Messages
  page's `[Add field]`.
- **Answers:** the dialog `Add field to the selected message`, with `Message index` (the selected
  message), Name `input`, Kind `Int`, Presence `required`. Confirmed: `message-field complete`.
- **Refuses:** `message field name is empty or already used`; `presence must be required or
  optional`; the type's refusals, as [`ask-state-field`](#ask-state-field).
- **Example:** confirm `0`, `input`, `Int`, `required`.

### `ask-emitted-message`

Declares a message the definition publishes. It must be inside the definition's own name, so a
definition speaks only for itself.

- **Takes:** a name. No default key; the Messages page's `[New emitted]`.
- **Answers:** the dialog `Declare a message this definition publishes`, `Message name` `Said`.
  Confirmed: `emitted-message complete`.
- **Refuses:** `` an emitted message's name must begin `water.`; `other.Said` does not ``; `a
  message named <name> is already declared`.
- **Example:** type `Total`, confirm: `water.Total`.

### `ask-emitted-field`

Adds a field to an emitted message.

- **Takes:** the emitted message by its place, a name, a type and a presence. No default key; the
  Messages page's `[Add emitted field]`.
- **Answers:** the dialog `Add a field to an emitted message`, with `Emitted index` `0`, Name
  `value`, Kind `Int`, Presence `required`. Confirmed: `emitted-field complete`; an emit already
  sending that message fills the field from the state field of its name.
- **Refuses:** `` the emit of <message> on <trigger> would write no `<name>`; remove that emit or
  declare state.<name> ``; the field and type refusals, as
  [`ask-message-field`](#ask-message-field).
- **Example:** confirm `0`, `total`, `Int`, `required`.

### `ask-library-open`

Brings named examples in from a library file, beside the ones the workspace holds.

- **Takes:** a path. No default key; the Library page's `[Import]`.
- **Answers:** the dialog `Reusable value library`, Path `examples.flow-values`. Confirmed:
  `library-open complete`; an example of the same name is replaced.
- **Refuses:** the library reader's reason.
- **Example:** press the Library page's `[Import]`, confirm.

### `ask-library-save`

Writes the workspace's named examples to a library file of their own.

- **Takes:** a path. No default key; the Library page's `[Export]`.
- **Answers:** the dialog `Reusable value library`, Path `examples.flow-values`. Confirmed, it asks
  Workshop for the write (`Saving the library is waiting for Workshop's approval`), then writes:
  `library-save complete`.
- **Refuses:** the writes' refusals; the library writer's reason.
- **Example:** press the Library page's `[Export]`, confirm.

### `ask-preset`

Saves the open form's value as a named example, complete or not.

- **Takes:** the open form; a name. No default key; a form's `[Save example]`.
- **Answers:** the dialog `Save this value, complete or unfinished`, `Example name` the form's
  title. Confirmed: `preset complete`; the Library page lists it, `(unfinished)` when it is.
- **Refuses:** `open a value first`.
- **Example:** with `input` set to `3`, save it as `low`.

### `send`

Sends the open message form's value to the running participant.

- **Takes:** an open message or example form, complete; a running participant. No default key; a
  form's `[Send]`.
- **Answers:** asks the Flow host ([`FlowSend`](flow-runtime.md#conversation)): `send queued`, then
  `send: <the host's word>`, which says the message was queued, not what it did. What it did is on
  the Events page ([`events`](#events)).
- **Refuses:** `open a message or reusable value to send`; Loom's words for an incomplete value;
  `send refused: <the host's reason>`; `send was not delivered: <the bus's reason>`.
- **Example:** open the example `low`, press `[Send]`, then `[Events]`.

### `drop-send`

Sends a value you dropped on the pane as an example, when the definition accepts its shape.

- **Takes:** the dropped value ([`dropped`](#dropped)). No default key; its page's
  `[Send as example]`.
- **Answers:** asks the Flow host ([`FlowSend`](flow-runtime.md#conversation)), as [`send`](#send);
  the value is put down.
- **Refuses:** `Nothing dropped is waiting`; as [`send`](#send).
- **Example:** drop a `water.Reading` value, press `[Send as example]`.

### `drop-accept`

Declares the dropped value's shape, or the shape a dropped description describes, as a message the
definition accepts, exactly as it came.

- **Takes:** the dropped value. No default key; its page's `[Declare <name> vN as an accepted
  message]`.
- **Answers:** `Declared <name> as an accepted message; Add trigger reacts to it`.
- **Refuses:** `Nothing dropped is waiting`; `That description names shapes it does not carry`;
  `a message named <name> is already declared`.
- **Example:** drop a carried shape, press its `[Declare ...]`.

### `drop-emit`

Declares the dropped shape as a message the definition publishes; it is offered only for a name
inside the definition's own.

- **Takes:** the dropped value. No default key; its page's `[Declare <name> vN as an emitted
  message]`.
- **Answers:** `Declared <name> as an emitted message`.
- **Refuses:** `Nothing dropped is waiting`; `That description names shapes it does not carry`;
  the emitted-name refusal of [`ask-emitted-message`](#ask-emitted-message).
- **Example:** drop a `water.Total` example, declare it as emitted.

### `drop-cancel`

Puts the dropped value down; nothing is done with it.

- **Takes:** the dropped value. No default key; its page's `[Cancel]`.
- **Answers:** the page you were on.
- **Refuses:** `Nothing dropped is waiting`.
- **Example:** press `[Cancel]`.

## Commands in a dialog

These are declared beside the others only while a dialog is open. Tab moves to the next field, and
a field takes what you type and its editing keys; the dialog's last line says so, `Tab changes
field; Enter confirms; Escape cancels`.

### `dialog-confirm`

Does what the dialog asks, with the words in its fields. Refused, the dialog stays with your words,
to repair.

- **Takes:** the open dialog. Default key <!-- key dialog-confirm -->`return`<!-- /key -->; the
  dialog's `[Confirm]`.
- **Answers:** the dialog's own outcome, in its command's section; one that replaces the draft or
  binds a port changes what the discovery door is asked, and it is asked again
  ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)). A save, an export, a
  generate and a library export ask Workshop first whether the confirming hand may write a file,
  `<Saving> is waiting for Workshop's approval`, and write only when it allows.
- **Refuses:** `<Saving> was refused: <Workshop's reason>`; `<Saving> is already waiting for
  Workshop's approval`; `` <Saving> waits: the confirm came as a raw key, which Workshop cannot
  approve -- confirm with the dialog's `dialog-confirm` row ``; `<Saving> was not done: Workshop
  could not be asked`, with the bus's reason when it is known; `<Saving> was not done: its dialog
  is no longer open`; `<Saving> was not done: confirm it again` after the pane was replaced. The
  word is `Saving`, `Exporting`, `Generating` or `Saving the library`.
- **Example:** type a path, press <!-- key dialog-confirm -->`return`<!-- /key -->.

### `dialog-cancel`

Closes the dialog; nothing is done, and a write it was waiting for is not made.

- **Takes:** the open dialog. No default key; the dialog's `[Cancel]`, or the Escape key.
- **Answers:** the page you were on; `<Saving> was cancelled with its dialog` when it was waiting
  on Workshop.
- **Refuses:** nothing.
- **Example:** press `[Cancel]`.

## Hears

### `FlowEdit`

`FlowEdit` v1 `{action, arguments}`, from a weave granted it: one edit the pane's controls make,
its arguments separate strings, answered to the asker with `FlowEdited` v1 `{ok, reason,
workspace, problems}` ([the vocabulary](../flow-pane/vocabulary.hpp)). The action `describe`
answers the grammar in `reason`. While a dialog is open every `FlowEdit` is refused, `Confirm or
cancel the open dialog before another edit`. A refused edit changes nothing; a run's `ok` says
only that it was queued.

### `FlowChanged`

`FlowChanged` v1, from `zengine.flow.host`: the `workshop` session moved. The pane inspects it
again ([`inspect`](#inspect)) unless an inspection is already asked.

## Follow

The pane publishes nothing of its own. What the running participant does is the Flow host's: a
weave follows it as the pane does, by `FlowChanged` and `FlowInspect` to `zengine.flow.host`
([its vocabulary](../flow-host/vocabulary.hpp)); the messages the participant emits are published
on the bus like any weave's, and any subscriber hears them.

## Shows

### `graph`

The Graph page: the trigger list with `[Add trigger]` and `[Emit]`, the search line `find: `, the
rail of what is in scope and what the door found, the active trigger's nodes and wires, and a
preview of the row you chose with its `[Add]`. A press selects a node, a port or a row; drag a node
to move it, blank room to pan.

### `state`

The State page: `[Add state field]`, `[Keep state]`, and the state's form, `Initial / saved state`,
one row per field, `<label> : <type> = <value>`, each with `[unset]`. Press a row to edit it.

### `messages`

The Messages page: `[New message]`, `[Add field]` and each accepted message; under `Emitted`,
`[New emitted]`, `[Add emitted field]` and each emitted one, `0 water.Total {total: Int}`. Drag a
declared message, or right-press it and choose `Carry`, to carry its shape to another pane.

### `library`

The Library page: `[Import]`, `[Export]`, each named example, `(unfinished)` when it is, and under
`Retained forms` each form the workspace keeps, by its title and its shape's content id.

### `events`

The Events page: `Dispatch pending: <n> / older events omitted: <n>`, the live state, `state.value
= 7`, then each event the host kept, `#<sequence> <kind> (send <correlation>) <detail>`, a long one
continued on the rows beneath; then the draft's problems.

### `dropped`

A value dropped on the pane, `Dropped <name> vN`, `Dropped the shape <name> vN` or `Dropped a
carried shape`, its fields, and what it can become here: [`drop-send`](#drop-send), a `[Use
<field> = <value> on %<n> <port>]` per field, [`drop-accept`](#drop-accept),
[`drop-emit`](#drop-emit), [`drop-cancel`](#drop-cancel).

## Files

- **A workspace**, `.flow-workspace`: the draft, layout, view, examples, retained forms and saved
  state, written by [`ask-save`](#ask-save) and read by [`ask-open`](#ask-open).
- **A project**, `.flow`: a definition and its state, for the standalone workbench, written by
  [`ask-export`](#ask-export) and read by [`ask-import`](#ask-import).
- **A library**, `.flow-values`: named examples, by
  [`ask-library-save`](#ask-library-save) and [`ask-library-open`](#ask-library-open).
- **A generated project**, a directory, by [`ask-generate`](#ask-generate).

What each holds is [the Flow reference](flow-reference.md)'s.
A reload of the pane keeps the workspace, its file's path, whether it is saved, the page and an
open dialog's words; it keeps no wait on the host, no search, preview or selected port.

## See also

- [Author a running graph in Workshop](flow.md), the walk through it.
- [Make and run a Flow definition](flow-guide.md), the standalone workbench.
- [The Flow host](flow-runtime.md), its manual [`zengine.flow.host`](zengine.flow.host.md), and
  [the Flow reference](flow-reference.md).
- The pane protocol's shapes: [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp).
