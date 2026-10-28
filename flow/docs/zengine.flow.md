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
the question changes, when the window gives its area a room, and after a run, an apply, a stop or a catalog
read. The ports a graph checks come from the Flow host's catalog. A file is written only after
Workshop says the hand that confirmed it may write one.

The pane draws in its own area of the window that shows it. A picture too large for the area is
replaced by one that says `View limit
reached: pan/zoom or choose a smaller graph.` and `Draft intact. Save or export it before changing
the graph.`, with Save, Export, the pages and the zoom controls. A pane too small says `Flow needs
more room`. Text outside printable ASCII is drawn as `?`, and text too long for its place ends in
an ellipsis; neither changes what is stored.

## Use

Open it from the Pane Manager, <!-- key desktop.panes -->`ctrl+p`<!-- /key -->, and give it room.
The bar holds Run, Apply -- Restart in its place when the draft changed what the running rule
hears, keeps or says -- and Stop, beside the rule's name and revision, running or stopped; then
File, View and a gear for the rest; under it, a tab a page: Graph, State, Messages, Library and
Events. Every button or menu row with no argument is one of the commands below; a press on a node,
a port, a row or a field is the page's own ([Shows](#shows)), and so is one in a graphics window
([Hears](#arearoom)). The last line is the latest notice.

The ordinary path is the Graph page's Next line: it names the one thing the rule still needs, and
its button does it -- a message ([`ask-message`](#ask-message)), a field the rule keeps
([`ask-keep`](#ask-keep)), the trigger, a step found in the rail and wired, [`run`](#run), then a
message it says ([`ask-say`](#ask-say)) and [`restart`](#restart). Change a step and
[`apply`](#apply): the running participant keeps its state.

Most commands open a dialog first; [`dialog-confirm`](#dialog-confirm) does what it asks. While a
dialog is open every other command says `Confirm or cancel the open dialog before another edit`.
Outside a dialog, what you type goes to the search line on the Graph page; the Escape key puts down
one thing per press (a wire's search and its wire, a dropped value, the wire in hand, the selected
port, an open body slot, the preview, the selected node, the search line) and only then hands the
pane back to Workshop; the Delete key removes the selected node; Return adds the previewed row.
While a wire's search is open, the arrow keys move among its rows and Return makes the one marked.

## Commands

### `ask-save`

Saves the whole workspace: the draft, its layout and view, your examples and retained forms, and
the saved state.

- **Takes:** the workspace. Default key <!-- key ask-save -->`ctrl+s`<!-- /key -->; File's
  `Save`.
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
  <!-- key ask-open -->`ctrl+o`<!-- /key -->; File's `Open...`.
- **Answers:** the dialog `Open workspace (replaces the current draft)`, with Path and
  `Unsaved disposition` set to `discard`. Confirmed: the Graph page, scrolled so its nodes show.
- **Refuses:** `save the draft or explicitly choose discard`, with unsaved work and another
  disposition; `stop this project's session before opening another` while it runs; the file
  reader's reason.
- **Example:** press <!-- key ask-open -->`ctrl+o`<!-- /key -->, type the path, confirm.

### `run`

Runs the definition: the Flow host makes a fresh participant from it, with the draft's state.

- **Takes:** the draft, checked first. Default key <!-- key run -->`ctrl+r`<!-- /key -->; the bar's
  `Run`, gold while the rule is ready, `Running` once it runs; the Next line's `[Run]` once nothing
  is missing.
- **Answers:** asks the Flow host ([`FlowRun`](flow-runtime.md#conversation)); the notice says `run
  queued`, which means only that the request went. The answer then says `run: observed`, or its
  reason, the bar reads `Running`, and the discovery door is asked again
  ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)), since running mounts the
  definition's operators.
- **Refuses:** the draft's first problem, such as an unwired port or `incompatible type`, naming
  the place, and the Next line leads with `Run needs this first:` and the press that does it; `run
  refused: <the host's reason>`; `run was not delivered: <the bus's reason>`; `Flow has 16
  unanswered requests; inspect the host before retrying`.
- **Example:** press <!-- key run -->`ctrl+r`<!-- /key -->, then [`send`](#send) an example.

### `ask-generate`

Writes the definition as a C++ project that builds into a weave like any other.

- **Takes:** the draft, checked first. Default key <!-- key ask-generate -->`ctrl+g`<!-- /key -->;
  File's `Generate C++...`.
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

### `find-wire`

Opens a wire's search for the wire in hand, or the port selected: what a node made from that wire
could be. Letting a dragged wire go on empty graph room opens the same search where it was let go
([`graph`](#graph)).

- **Takes:** a wire held by a press on an output or a field under In scope, or a selected input
  port. Default key <!-- key find-wire -->`ctrl+n`<!-- /key -->; View's `Find a wire`.
- **Answers:** a list at the graph's centre, `From <%n or $field> : <type> -- what takes it` or
  `Into %n <port> : <type> -- what fills it`, the search line above its rows, and asks the discovery
  door ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)): from an output, each
  power with a port of exactly that type, a row a port (`math.add -> lhs`), and the fold's counting
  ports for an `Int`; from a port, what is in scope of its type, a typed constant for `Int` and
  `Bool`, then each power that yields it. A row chosen makes the node, placed after what it reads
  (at the trigger's start for a field) or before the port's node, where the list was opened, wired,
  and selected, in one edit: `Added %k <power>, wired from <%n or $field>` or `Added %k <power>,
  wired into %m <port>`; an In scope row binds the port, `Bound %n <port> to <binding>`; the
  constant row opens the binding editor. Escape, or a press outside the list, closes it and puts the
  wire down: `The wire was put down: <why>; nothing was made`.
- **Refuses:** `Hold a wire first -- click an output or a field In scope, or select a port -- then
  ask for a new node from it`; `A new node from the wire needs the graph shown`; a row whose ports
  changed since it was found, `'<power>' is not the operator this reference was found at: its
  ports changed since; find it again`; `The graph changed since the wire was let go; nothing was
  made -- let it go again`. Nothing is made then.
- **Example:** press a node's output, press <!-- key find-wire -->`ctrl+n`<!-- /key -->, type `add`,
  press Return.

### `ask-discard`

Lets the pane close without saving. Workshop's quit is refused while Flow holds something it would
lose: `Flow has unsaved work. Save it or explicitly Discard before quitting.`, `Flow has an
unfinished dialog. Confirm or cancel it before quitting.`, or `Flow is waiting for its host to
settle work. Quit again after the result arrives.`

- **Takes:** the word `discard`, typed. No default key; File's `Discard`.
- **Answers:** the dialog `Allow closing without saving (type discard)`, one field,
  Confirmation. Confirmed with `discard`: `Unsaved work may now be discarded on close`. Nothing is
  deleted.
- **Refuses:** `type discard to acknowledge unsaved work`.
- **Example:** choose File's `Discard`, type `discard`, confirm.

### `ask-new`

Starts a new definition in place of the draft.

- **Takes:** a project name and what to do with unsaved work. No default key; File's `New...`.
- **Answers:** the dialog `New project (replaces the current draft)`, with `Project name`
  `my_flow` and `Unsaved disposition` `discard`. Confirmed: an empty draft on the Graph page, no
  file path.
- **Refuses:** `save the draft or explicitly choose discard`; `stop this project's session before
  creating another`; `use a project name of 1 to 120 bytes`.
- **Example:** choose File's `New...`, type `water`, confirm.

### `ask-import`

Brings in an executable Flow project, as the standalone workbench saves one, in place of the draft.

- **Takes:** a path and what to do with unsaved work. No default key; File's `Import...`.
- **Answers:** the dialog `Import executable Flow project (replaces this draft)`, Path
  `my-flow.flow`, `Unsaved disposition` `discard`. Confirmed: the Graph page.
- **Refuses:** `save the draft or explicitly choose discard`; `stop the session before importing
  another project`; the project reader's reason.
- **Example:** choose File's `Import...`, type `thermostat.flow`, confirm.

### `ask-export`

Writes the definition and its state as an executable project for the standalone workbench. A
workspace keeps unfinished work too; a project does not.

- **Takes:** the draft, checked first. No default key; File's `Export...`.
- **Answers:** the dialog `Export executable project for the standalone Flow workbench`, Path
  `my-flow.flow`. Confirmed, it asks Workshop for the write, then writes it: `Exported <path>`.
- **Refuses:** the draft's first problem; the writes' refusals; the file writer's reason.
- **Example:** choose File's `Export...`, confirm.

### `apply`

Changes the running participant's behaviour to the draft's, keeping its state.

- **Takes:** the draft, checked first; a running participant. No default key; the bar's `Apply`,
  and the Next line's `[Apply]` while the draft differs from what runs.
- **Answers:** asks the Flow host ([`FlowApply`](flow-runtime.md#conversation)): `apply queued`,
  then `apply: observed`, and the discovery door is asked again
  ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)). The participant keeps its
  state and its accepted, emitted and state shapes; a change to any of those needs
  [`restart`](#restart).
- **Refuses:** the draft's first problem; `apply refused: <the host's reason>`, such as a changed
  shape; `apply was not delivered: <the bus's reason>`.
- **Example:** bind a port to another constant, press `Apply`, send the same example again.

### `stop`

Stops the running participant; its last state comes back as the draft's.

- **Takes:** the session. No default key; the bar's `Stop`.
- **Answers:** asks the Flow host ([`FlowStop`](flow-runtime.md#conversation)): `stop queued`, then
  `stop: observed`; the bar reads `Run` again and the discovery door is asked again
  ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)).
- **Refuses:** `stop refused: <the host's reason>`, such as while a send is still being
  delivered; `stop was not delivered: <the bus's reason>`.
- **Example:** press `Stop`.

### `restart`

Stops the running participant and runs the draft in its place, for a change [`apply`](#apply)
cannot make: a message it hears, a field it keeps or a message it says. What it kept carries on
where the kept fields' shape is unchanged; otherwise it starts again from the draft's state.

- **Takes:** the draft, checked first; a running participant. No default key; the bar's `Restart`,
  in Apply's place while the draft changed one of those; the Next line's `[Restart]`; the gear's
  `Restart`.
- **Answers:** asks the Flow host to stop ([`FlowStop`](flow-runtime.md#conversation)), `Restarting:
  stopping, then running the draft`; once it has stopped, the draft runs
  ([`FlowRun`](flow-runtime.md#conversation)), `Restarting: the draft runs, keeping what it kept`,
  then `run: observed`. Each asks the discovery door again
  ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)).
- **Refuses:** `Nothing runs to restart; press Run`; the draft's first problem; a stop's or a run's
  refusals, as [`stop`](#stop) and [`run`](#run).
- **Example:** with the rule running, add `+ say tally.Total`, then press `Restart`.

### `inspect`

Reads the running participant again: its state and what it did.

- **Takes:** the session. No default key; View's `Inspect`.
- **Answers:** asks the Flow host ([`FlowInspect`](flow-runtime.md#conversation)): `inspect queued`,
  then the Events page's rows ([`events`](#events)), and, unless you have edited the state since,
  the draft's state becomes the observed one. The pane inspects by itself when it starts and,
  saying nothing, whenever the host says the session changed ([Hears](#flowchanged)).
- **Refuses:** `inspect refused: <the host's reason>` while it runs; while nothing runs it says
  nothing.
- **Example:** choose View's `Inspect`, then the `Events` tab.

### `catalog`

Reads the host's operators again: the ports the graph checks its wiring against.

- **Takes:** nothing. No default key; View's `Catalog`.
- **Answers:** asks the Flow host ([`FlowCatalog`](flow-runtime.md#conversation)) and the discovery
  door again ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)): `catalog
  queued`, then `Host operators refreshed`.
- **Refuses:** the host's reason; `operator descriptor identity disagrees`; `catalog was not
  delivered: <the bus's reason>`.
- **Example:** after a provider is loaded, choose View's `Catalog`.

### `page-graph`

Shows the Graph page ([`graph`](#graph)).

- **Takes:** nothing. No default key; the `Graph` tab.
- **Answers:** the Graph page, from its first row.
- **Refuses:** nothing.
- **Example:** press the `Graph` tab.

### `page-state`

Shows the State page ([`state`](#state)), with the state's form open.

- **Takes:** the definition's state. No default key; the `State` tab.
- **Answers:** the State page and the form `Initial / saved state`.
- **Refuses:** nothing.
- **Example:** press the `State` tab.

### `page-messages`

Shows the Messages page ([`messages`](#messages)).

- **Takes:** nothing. No default key; the `Messages` tab.
- **Answers:** the Messages page, from its first row.
- **Refuses:** nothing.
- **Example:** press the `Messages` tab.

### `page-library`

Shows the Library page ([`library`](#library)).

- **Takes:** nothing. No default key; the `Library` tab.
- **Answers:** the Library page, from its first row.
- **Refuses:** nothing.
- **Example:** press the `Library` tab.

### `page-events`

Shows the Events page ([`events`](#events)).

- **Takes:** nothing. No default key; the `Events` tab.
- **Answers:** the Events page, from its first row.
- **Refuses:** nothing.
- **Example:** press the `Events` tab after a send.

### `fit`

Resets the graph's pan and zoom, then scrolls so its nodes show.

- **Takes:** the view. No default key; View's `Fit`; the recovery picture's `[Reset view]`.
- **Answers:** the graph redrawn; the view is part of the workspace, so it is unsaved work.
- **Refuses:** nothing.
- **Example:** choose View's `Fit` after panning away.

### `zoom-in`

Zooms the graph in one step, up to the largest.

- **Takes:** the view. No default key; View's `Zoom in`.
- **Answers:** node positions and widths grow; text and port rows keep their size.
- **Refuses:** nothing; at the largest nothing moves.
- **Example:** choose View's `Zoom in`.

### `zoom-out`

Zooms the graph out one step, down to the smallest.

- **Takes:** the view. No default key; View's `Zoom out`.
- **Answers:** node positions and widths shrink; text and port rows keep their size.
- **Refuses:** nothing; at the smallest nothing moves.
- **Example:** choose View's `Zoom out`.

### `ask-trigger`

Adds a trigger: an accepted message that runs a row of nodes and writes one state field.

- **Takes:** a message, by its place among the accepted ones, and a state field. No default key;
  the gear's `Add a trigger...`. The Next line's `[Use <message>]` makes the first trigger at a
  press, writing the first field the rule keeps.
- **Answers:** the dialog `An input message updates one state field`, with `Message index` (the
  selected message) and `State output field` `value`. Confirmed: the new trigger is the active
  one.
- **Refuses:** `select a declared state output field`; `message already has a trigger`.
- **Example:** choose the gear's `Add a trigger...`, keep `0` and `value`, confirm.

### `ask-emit`

Makes the active trigger publish an emitted message after its write.

- **Takes:** an emitted message by its place, and each field's source, `field=$state` or
  `field=constant`. No default key; the gear's `Add an emit...`, while there are a trigger and an
  emitted message; [`ask-say`](#ask-say) declares a message and says it at once.
- **Answers:** the dialog `After the write, publish an emitted message`, with `Emitted index` `0`
  and `Fields (field=$state or field=constant)` filled from the state fields of the same names,
  `total=$total`. Confirmed: the Then lane shows a card `say <name>` with `x` to remove it.
- **Refuses:** `add the trigger's nodes before its emits: an empty trigger is kept without them`;
  `<message> has no field <name>`.
- **Example:** choose the gear's `Add an emit...`, confirm `total=$total`.

### `ask-keep`

Adds a field the rule keeps between messages: a state field, at a press. Where a message is declared
and no trigger reacts to one yet, that message's trigger is made too, writing this field.

- **Takes:** a name and a type. No default key; the When lane's `+ keep a field`, the Next line's
  `[+ keep a field]`, the gear's `Keep a field...`.
- **Answers:** the dialog `Keep a field between messages: what the rule remembers`, with Name
  `total` and Kind `Int`. Confirmed: the When lane's card `kept between messages` lists it, `total
  Int = 0`, and the Graph page shows.
- **Refuses:** `state field name is empty or already used`; the type's refusals, as
  [`ask-state-field`](#ask-state-field).
- **Example:** press `+ keep a field`, confirm `total`, `Int`.

### `ask-say`

Makes the rule say a message after its change: a message declared inside the definition's own name,
carrying the kept fields named, and said by the active trigger after its write, in one edit. A
message already declared under that name is said as it is.

- **Takes:** a name and the kept fields it carries. No default key; the Then lane's `+ say
  <definition>.<Field>`, the Next line's `[+ say <definition>.<Field>]` once the rule runs, the
  gear's `Say a message...`.
- **Answers:** the dialog `Say a message after the change, with what the rule keeps`, with `Message
  name` the kept field's name capitalised, `Total`, and `With` the field the trigger writes,
  `total`. Confirmed: the Then lane shows the card `say tally.Total`, and the Next line asks for a
  [`restart`](#restart), since a running rule says only the messages it was run with.
- **Refuses:** `A rule says a message after its change: give it a message and what it keeps
  first`; `` the rule keeps no `<field>` to say ``; the emitted-name refusal of
  [`ask-emitted-message`](#ask-emitted-message).
- **Example:** press `+ say tally.Total`, confirm.

### `add-fold`

Adds a fold at the end of the active trigger: the form that runs one operator for each count from a
start toward a limit by a step, threading an answer through.

- **Takes:** the active trigger. No default key; a fold row's `[Add to the rule]`, under `FORMS` in
  the Graph page's rail.
- **Answers:** a node `body: [choose]`, its `step` bound to `1`, and a fold's open body slot shut,
  so the discovery door is asked again
  ([`FindPowers`](../../introspection/docs/zengine.powers.md#findpowers)). Press its body row to
  choose the body ([`graph`](#graph)).
- **Refuses:** `add a trigger before adding a node`, with no trigger yet.
- **Example:** type `count`, press the fold's row, press `[Add to the rule]`.

### `ask-state-field`

Adds a field to the state.

- **Takes:** a name, a type and a presence. No default key; the State page's
  `[Add state field]`.
- **Answers:** the dialog `State field: Int, Bool, Float, Text, Bytes, List:Kind, Message:preset`,
  with Name `value`, Kind `Int`, Presence `required`. Confirmed: the State page and its form.
- **Refuses:** `state field name is empty or already used`; `presence must be required or
  optional`; `scalar authoring accepts Int, Bool, Float, Text; import-json carries full maker
  schemas`; `Message:<saved draft name> needs an existing library schema`.
- **Example:** confirm `value`, `Int`, `required`.

### `keep-state`

Makes the state form's value the definition's state: what a run starts with and a save keeps.

- **Takes:** the open state form, complete. No default key; the State page's `[Keep state]`.
- **Answers:** the form's value as the draft's state. Until a run takes it, the state you kept is
  not replaced by what the running participant reports.
- **Refuses:** `open a value first`; `state schema changed; reopen its form`; Loom's words for a
  value that does not fit its shape.
- **Example:** set `value` to `5`, press `[Keep state]`.

### `ask-message`

Declares a message the definition accepts.

- **Takes:** a name; one without a `.` is put inside the definition's own name. No default key; the
  Messages page's `[New message]`, and the When lane's while no message is declared.
- **Answers:** the dialog `Declare an input message`, `Message name` `Set`. Confirmed: the
  message's form opens.
- **Refuses:** `a message needs a name`; `a message named <name> is already declared`.
- **Example:** type `Reading`, confirm: `water.Reading`.

### `ask-message-field`

Adds a field to an accepted message.

- **Takes:** the message by its place, a name, a type and a presence. No default key; the Messages
  page's `[Add field]`.
- **Answers:** the dialog `Add field to the selected message`, with `Message index` (the selected
  message), Name `input`, Kind `Int`, Presence `required`. Confirmed: the message's form, with the
  field.
- **Refuses:** `message field name is empty or already used`; `presence must be required or
  optional`; the type's refusals, as [`ask-state-field`](#ask-state-field).
- **Example:** confirm `0`, `input`, `Int`, `required`.

### `ask-emitted-message`

Declares a message the definition publishes. It must be inside the definition's own name, so a
definition speaks only for itself.

- **Takes:** a name. No default key; the Messages page's `[New emitted]`.
- **Answers:** the dialog `Declare a message this definition publishes`, `Message name` `Said`.
  Confirmed: the Messages page lists it under `Emitted`.
- **Refuses:** `` an emitted message's name must begin `water.`; `other.Said` does not ``; `a
  message named <name> is already declared`.
- **Example:** type `Total`, confirm: `water.Total`.

### `ask-emitted-field`

Adds a field to an emitted message.

- **Takes:** the emitted message by its place, a name, a type and a presence. No default key; the
  Messages page's `[Add emitted field]`.
- **Answers:** the dialog `Add a field to an emitted message`, with `Emitted index` `0`, Name
  `value`, Kind `Int`, Presence `required`. Confirmed: an emit already sending that message fills
  the field from the state field of its name.
- **Refuses:** `` the emit of <message> on <trigger> would write no `<name>`; remove that emit or
  declare state.<name> ``; the field and type refusals, as
  [`ask-message-field`](#ask-message-field).
- **Example:** confirm `0`, `total`, `Int`, `required`.

### `ask-library-open`

Brings named examples in from a library file, beside the ones the workspace holds.

- **Takes:** a path. No default key; the Library page's `[Import]`.
- **Answers:** the dialog `Reusable value library`, Path `examples.flow-values`. Confirmed: the
  Library page lists them; an example of the same name is replaced.
- **Refuses:** the library reader's reason.
- **Example:** press the Library page's `[Import]`, confirm.

### `ask-library-save`

Writes the workspace's named examples to a library file of their own.

- **Takes:** a path. No default key; the Library page's `[Export]`.
- **Answers:** the dialog `Reusable value library`, Path `examples.flow-values`. Confirmed, it asks
  Workshop for the write (`Saving the library is waiting for Workshop's approval`), then writes:
  `Saved the library to <path>`.
- **Refuses:** the writes' refusals; the library writer's reason.
- **Example:** press the Library page's `[Export]`, confirm.

### `ask-preset`

Saves the open form's value as a named example, complete or not.

- **Takes:** the open form; a name. No default key; a form's `[Save example]`.
- **Answers:** the dialog `Save this value, complete or unfinished`, `Example name` the form's
  title. Confirmed: the Library page lists it, `(unfinished)` when it is.
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
- **Example:** open the example `low`, press `[Send]`, then the `Events` tab.

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
- **Answers:** `Declared <name> as an accepted message; a trigger on it wakes the rule`.
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
  Workshop's approval`; `` <Saving> waits: the confirm came under no gesture Workshop numbered, as
  a raw key or a press in a graphics window, which Workshop cannot approve -- confirm with the
  dialog's `dialog-confirm` row in Workshop ``; `<Saving> was not done: Workshop
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

### `AreaRoom`

`AreaRoom` v4, from the graphics window that shows the area `zengine.flow`, as that window's office
([`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp)): the pane's room there, in the
window's unit and text. It draws its picture for that room on its ground, whole in a new window,
incarnation, size or measure, or an empty area, then only what changed; one more than a window
keeps says `View limit reached`. Each time it starts it asks `zengine.graphics` for this room
(`RoomAsked` v1).

### `DrawnRefused`

`DrawnRefused` v1, from that window: a drawing it refused, and why. What it shows stays as it was.
A refused change is drawn again whole at once; a refused whole picture is said, `The graphics window
refused Flow's picture: <the reason>`, and the pane draws there again at its next change.

### `ActionRequested`

`ActionRequested` v1, from that window, where it shows Flow's area deaf to the pointer: a press on
one of the pane's places there, by the name its Workshop picture gives that place -- `node:<id>`,
`port:<id>.<n>`, `control:<action>` -- which is a click on that place: a node is selected and
anything else is done. It begins no drag and carries nothing, and a write it confirms waits, since
Workshop numbered no gesture for it.

### `AreaButtonChanged`

`AreaButtonChanged` v3, from that window: a button pressed or let go in Flow's area, on the part the
window drew there: a node held for a drag, a wire's end held, a control done, a declared message
offered to carry out, the graph panned by a press on nothing, a right press handed back. One on
another part than Flow shows there now, or on another dialog's, is refused in words. Its moves and
release carry the window's number for the press, which a carry it asks continues.

### `AreaPointerMoved`

`AreaPointerMoved` v1, from that window: the pointer moved while a press in Flow's area is held,
which moves the held node, pans the graph or draws the held wire to the hand, as on Workshop's
canvas.

### `AreaWheelTurned`

`AreaWheelTurned` v1, from that window: the wheel over Flow's area, which scrolls the graph or the
page as on Workshop's canvas.

### `AreaHoldEnded`

`AreaHoldEnded` v1, from that window, about Flow's area: the press held there ended with no
release, and the drag ends where it stands.

## Follow

The pane publishes nothing of its own. What the running participant does is the Flow host's: a
weave follows it as the pane does, by `FlowChanged` and `FlowInspect` to `zengine.flow.host`
([its vocabulary](../flow-host/vocabulary.hpp)); the messages the participant emits are published
on the bus like any weave's, and any subscriber hears them.

## Shows

### `graph`

The Graph page: the rule as one sentence, a part still to choose dashed; the rail, with the search
line, what is in scope, the door's rows grouped and the chosen row's preview; the graph in three
lanes, WHEN a message arrives, DO its steps, THEN what changes; the Next line; the Events band. A
press selects; drag a node to move it, blank room to pan, a wire to bind where it lands -- on empty
room it opens [`find-wire`](#find-wire)'s list.

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

The Events page: one line a message the running rule took, newest first -- what came in, what
changed, what was said, `tally.panel.Count  amount 3  ->  total 0 -> 3  ->  said tally.Total 3` --
then the runs before, the host's other words, the live state, `state.total = 3`, and `Dispatch
pending: <n> / older events omitted: <n>`; a long line continues beneath.

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
A reload of the pane keeps the workspace, its file's path, whether it is saved, the page, an open
dialog's words, the state the running rule was run with and the Events of the runs before it, so
the Events read as they did; it keeps no wait on the host, no search, preview or selected port.

## See also

- [Author a running graph in Workshop](flow.md), the walk through it.
- [Make and run a Flow definition](flow-guide.md), the standalone workbench.
- [The Flow host](flow-runtime.md), its manual [`zengine.flow.host`](zengine.flow.host.md), and
  [the Flow reference](flow-reference.md).
- The pane protocol's shapes: [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp).
