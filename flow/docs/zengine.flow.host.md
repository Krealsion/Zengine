# zengine.flow.host -- the Flow host

**Manual.** The Flow host: the weave that runs Flow projects on the host's own bus, answering to
the office `zengine.flow.host` (an office is the name a weave answers to). Given a project, it
registers the project's definition as a participant of its own, changes its behaviour, sends it
examples, reports what it did, and stops it. A Zengine host mounts it -- Workshop, and an app's
own host, `zengine-app` -- so it is not a file you load. In Workshop the [Flow pane](zengine.flow.md)
is the weave that asks it, and in an app the app's own weave
([running an app on its own](../../app/docs/app.md)).

## About

The Flow host is `flow_host::RuntimeHost` and its office's weave, `flow_host::Manager`, in
[`flow/flow-host/runtime.hpp`](../flow-host/runtime.hpp). It runs interpreted Flow projects
(`flow::Project`) as ordinary maker participants (`maker::Weave`) on the host's existing
Switchboard, resolving their operators through the host's live operator catalog: it starts no
second session and publishes no operators of its own ([Flow in an existing host](flow-runtime.md)).

Every running project belongs to a session, named by the asker and scoped to it: to the office the
ask was sent as, while that office's holder is still the one asking, or else to the asker's own
identity on the bus. Another weave cannot reach a session by guessing its name. Each session sends
its examples from a participant of its own, granted only the definition's accepted shapes to its
subject, and keeps a journal of what happened: each input queued, delivered or refused, each output,
each refusal the subject said.

Every bound is the host's own, declared in [`flow/flow-host/runtime.hpp`](../flow-host/runtime.hpp):
`kMaxSessions` running sessions; `kMaxPendingInputs` inputs a session holds before they settle;
`kMaxEvents` journal events, or `kMaxJournalBytes` of their payloads and details, a session keeps,
the oldest dropped and counted; `kMaxEventBytes` of one event's payload, past which it is left out
and the event says so; and the maker file limit, `maker::kMaxFileBytes`, for a project, a
definition, an input and a catalog answer. The host looks at its sessions on its own beat,
`zengine.flow.observe`.

## Use

The Flow pane, `zengine.flow`, or any weave granted `flow_host::allow_flow_requests`, sends
[`FlowRun`](#flowrun), [`FlowApply`](#flowapply), [`FlowSend`](#flowsend),
[`FlowInspect`](#flowinspect) and [`FlowStop`](#flowstop), each answered once, to the asker alone,
with `FlowAnswer` v1, and [`FlowCatalog`](#flowcatalog), answered with `FlowCatalogAnswer` v1. When a
session moves, its owner hears [`FlowChanged`](#follow) and inspects it again. Any weave granted
`ShapeDocumentRequested` to this office may ask what a shape it accepts does, and is answered from
this page.

## Commands

### `FlowRun`

`FlowRun` v1 runs a Flow project as a fresh participant, in a session of the asker's.

- **Takes:** `session`, a name of 1 to 128 bytes; `project`, a Flow project's file bytes.
- **Answers:** `FlowAnswer` with `action` `run`, `ok`, `subject` the new participant's id, `project`
  the project as it now stands, `pending`, `first_sequence`, `last_sequence`, `dropped` and the
  journal's `events`.
- **Refuses:** `ok` false, nothing registered, with `reason`: `a live requesting participant is
  required`; `session name must be between 1 and 128 bytes`; `the request's authored office has
  moved to another participant`; `session already runs; apply behavior or explicitly stop it before
  running a fresh project`; `Flow already owns eight running sessions`, at `kMaxSessions`; `project
  exceeds 1 MiB`; `the definition's role is already held: tally.counter`; or the reason the project
  is not read, the definition not registered, or its state not taken.
- **Example:** from the Flow pane,
  `mail.as_role("zengine.flow").send_to_role(zengine::flow_host::kFlowHostRole, zengine::flow_host::FlowRun{"workshop", bytes});`

### `FlowApply`

`FlowApply` v1 changes the behaviour of the participant a session runs, keeping its state.

- **Takes:** `session`, one this asker runs; `definition`, a definition's file bytes, a newer
  revision with the same state, accepted and emitted shapes.
- **Answers:** `FlowAnswer` with `action` `apply`, `ok`, and the session as it now stands; the
  running participant keeps its live state.
- **Refuses:** `ok` false with `reason`: `a live requesting participant is required`; `session
  name must be between 1 and 128 bytes`; `the request's authored office has moved to another
  participant`; `no session by this name belongs to this requesting participant`; `definition
  exceeds 1 MiB`; `inputs still have pending dispatch; try the edit after they settle`;
  or the reason the definition is not read, or the edit not taken.
- **Example:**
  `mail.as_role("zengine.flow").send_to_role(zengine::flow_host::kFlowHostRole, zengine::flow_host::FlowApply{"workshop", bytes});`

### `FlowSend`

`FlowSend` v1 sends the session's participant an example input.

- **Takes:** `session`, one this asker runs; `payload`, a value's bytes, which must be one of the
  shapes the definition accepts.
- **Answers:** `FlowAnswer` with `action` `send`, `ok`, `reason` `queued; inspect subsequent
  dispatch, outputs and state`, and the session as it now stands, its journal ending `queued`. It
  says the input was queued, never that it was delivered or did anything.
- **Refuses:** `ok` false, nothing sent, with `reason`: `a live requesting participant is required`;
  `session name must be between 1 and 128 bytes`; `the request's authored office has moved to
  another participant`; `no session by this name belongs to this requesting participant`; `this
  session already retains 16 inputs; let the host advance before sending more`, at
  `kMaxPendingInputs`; `input exceeds 1 MiB`; `input does not match a declared accepted schema`, or
  why the last accepted shape tried refused it; or `input was not queued (host dispatch fence
  unavailable)`.
- **Example:**
  `mail.as_role("zengine.flow").send_to_role(zengine::flow_host::kFlowHostRole, zengine::flow_host::FlowSend{"workshop", bytes});`

### `FlowInspect`

`FlowInspect` v1 asks where a session stands, and what happened after a point in its journal.

- **Takes:** `session`, one this asker runs; `after`, the last event sequence the asker already
  has, or 0 for all the journal keeps.
- **Answers:** `FlowAnswer` with `action` `inspect`, `ok`, the `project` as it now stands, `pending`,
  `first_sequence`, `last_sequence`, `dropped`, and every event the journal keeps after `after`.
- **Refuses:** `ok` false with `reason`: `a live requesting participant is required`; `session name
  must be between 1 and 128 bytes`; `the request's authored office has moved to another
  participant`; `no session by this name belongs to this requesting participant`; `event cursor
  cannot be negative`; `managed subject no longer exists`; or why the participant or its state
  could not be read.
- **Example:**
  `mail.as_role("zengine.flow").send_to_role(zengine::flow_host::kFlowHostRole, zengine::flow_host::FlowInspect{"workshop", 0});`

### `FlowStop`

`FlowStop` v1 stops the participant a session runs.

- **Takes:** `session`, one this asker runs.
- **Answers:** `FlowAnswer` with `action` `stop`, `ok`, `reason` `stopped; answer contains the final
  project snapshot`, and the final `project` and journal; the session's sender and participant are
  then unregistered.
- **Refuses:** `ok` false, nothing stopped, with `reason`: `a live requesting participant is
  required`; `session name must be between 1 and 128 bytes`; `the request's authored office has
  moved to another participant`; `no session by this name belongs to this requesting participant`;
  or `inputs still have pending dispatch; stop after they settle`.
- **Example:**
  `mail.as_role("zengine.flow").send_to_role(zengine::flow_host::kFlowHostRole, zengine::flow_host::FlowStop{"workshop"});`

### `FlowCatalog`

`FlowCatalog` v1 asks which operators the host's catalog holds.

- **Takes:** nothing.
- **Answers:** `FlowCatalogAnswer` v1 `{ok, reason, operators}`: every operator the host's live
  catalog resolves, each its identity and its descriptor's bytes.
- **Refuses:** `ok` false with `reason` `operator catalog exceeds this query's 1 MiB limit`, when the
  descriptors together pass `maker::kMaxFileBytes`.
- **Example:**
  `mail.as_role("zengine.flow").send_to_role(zengine::flow_host::kFlowHostRole, zengine::flow_host::FlowCatalog{});`

## Hears

### `FlowHostPulse`

`FlowHostPulse` v1, from the Flow host itself, on its beat: a session's inputs settled or its
journal changed, so the host tells the session's owner with `FlowChanged`. A pulse from any other
sender is ignored.

## Follow

The host says one thing unasked: `FlowChanged` v1 `{session}`, from `zengine.flow.host`, to the
office or participant that owns the session, when an input settles or its journal changes. It is
a prompt to ask again with [`FlowInspect`](#flowinspect), never word that something succeeded;
check that it came as the office `zengine.flow.host` before believing it.

## See also

- [`zengine.flow`](zengine.flow.md), the Flow pane, which asks this host.
- [Flow in an existing host](flow-runtime.md), the reference: custody, dispatch and its bounds.
- [Author a running graph in Workshop](flow.md).
- [`flow/flow-host/vocabulary.hpp`](../flow-host/vocabulary.hpp), the shapes.
