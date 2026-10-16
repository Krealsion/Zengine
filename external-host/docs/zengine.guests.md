# zengine.guests -- the guest door

**Manual.** The guest door: the one participant in Workshop that listens for other Loom hosts,
admits each by the guests file you wrote, and keeps the inventory of who is connected. It answers to
the office `zengine.guests` (an office is the name a weave answers to). Workshop mounts it when it
is launched with `--guests <file>`, and only then; it is not a file you load. Its pane is
[`zengine.connections`](zengine.connections.md). How to write the guests file and link a host to
it: [Drive Workshop from another host](external-host.md).

## About

The door holds Workshop's listener, the guests file's rows, the inventory of connections and, for
each session it admitted, the row that admitted it. A peer says a name when it connects, its
*claim*; the door *establishes* the name of the row whose credential it presented, and a peer that
presents none of them is refused. A row written `"admit": "ask"` waits `awaiting-decision`, acting on
nothing, until the host decides.

An admitted session is an ordinary participant on Workshop's bus, under a grant no wider than its
row's powers. The door grants nothing itself: the bus checks every send a guest makes against that
grant. Its own answers are to the asker alone, and a guest is never told another row's powers.

When a connection ends, the door closes the input session its guest held, on the guest's behalf,
lets go of the ask for the plan the guest was holding, and the observation relay beside it forgets
the guest's subscriptions. The door does its work on a beat of its own from the Timer.

## Use

Launch Workshop with `--guests <file>`; it says where it listens and which weave holds the door,
`zengine-workshop - guests: listening on 127.0.0.1:7654 for 1 guest(s) named in guests.json (door:
weave #12; the Connections pane lists them)`.
Then any admitted session may ask for its own row
([`GuestRowDescribedRequested`](#guestrowdescribedrequested)) and where the plan is
([`GuestPlanRequested`](#guestplanrequested)); a row with `inspect` may ask for the inventory
([`GuestConnectionsRequested`](#guestconnectionsrequested)), as Workshop's own participants do. A
guest without the power is refused by the bus, before the door hears it. Workshop's office may ask
what an admitted session's row lets it do ([`ActorScopeRequested`](#actorscoperequested)).

## Commands

### `GuestConnectionsRequested`

`GuestConnectionsRequested` v1, v2 and v3 ask for the inventory as it stands now.

- **Takes:** nothing; the version asked is the version answered.
- **Answers:** `GuestConnections`, to the asker alone: `listen`, where Workshop listens (empty when
  it does not); each connection's number, its state (`awaiting-hello`, `awaiting-decision`,
  `admitted`, `refused` or `closed`), the name it claimed and the name established, the far address,
  its session's weave and its refusal; and `refused` and `shed`, counted since launch. v2 adds the
  admitting row's powers, the file's `host` and `version`, and its losses, what those powers do not
  reach on this host; v3 adds the row's role and its scopes, its reach in words. To a guest, v2 and v3
  say the powers, losses, role and scopes of its own row alone.
- **Refuses:** nothing.
- **Example:** the Connections pane asks it at v3 when it goes live.

### `GuestRowDescribedRequested`

`GuestRowDescribedRequested` v1 and v2 ask for the asker's own row.

- **Takes:** nothing.
- **Answers:** `GuestRowDescribed`, at the version asked: the row the door admitted the asking
  session under, never one found by name -- its `name`, its powers (`may`), its `observe` list, each
  `{producer, shape, version}`, and the file's `host`; never its credential, and nothing of another
  row. Two rows of one name stay apart. v2 adds the file's `version`, the row's `role`, whether it
  reads and points by pane (`by_door`), and its `capture` and `guide` doors as the file wrote them.
- **Refuses:** `only a session this door admitted has a row to describe`.
- **Example:** `desk/read` asks it first, to learn which panes its row reads.

### `GuestPlanRequested`

`GuestPlanRequested` v1 asks where Workshop's load plan is. The door listens before the plan is
realized, so a guest can be admitted while rows that declare shapes are still to come, and a shape
such a row declares is refused `unknown schema: <name> v<n>` until it comes: the words a shape this
run never declares is refused in. The answer tells the two apart.

- **Takes:** `wait`: true holds the answer until the plan is at rest; false answers at once.
- **Answers:** `GuestPlanSaid`: `state`, `loading` while rows are still to come, `complete` (every row
  settled, an optional one perhaps stepped over), `waiting` (on a row the weaver has to build) or
  `refused`; `at_rest`; and the rows it names, `waiting_on` and the optional rows stepped over
  (`unavailable`), said only to a row with `capture` or `inspect`, which already reads rows' names,
  and to Workshop's own participants. To any other row they are empty.
- **Refuses:** `this host wired no reading of its load plan`; past the asks it holds at once, `the
  guest door already holds <n> asks for the plan at rest; ask again without wait, or once one is
  answered`; and `the bus could not hold this ask for the plan at rest; ask again without wait`. A
  held ask whose guest goes is let go, unanswered.
- **Example:** `workshop/connections` with `wait_plan` asks `{"wait": true}` before its first read.

### `ActorScopeRequested`

`ActorScopeRequested` v1 asks what one sender may do, for a message an owner received directly,
past any gesture: the question Workshop answers at `zengine.workshop`, judged here from the rows
this door admitted. Only Workshop's office asks it here; the door answers no one else.

- **Takes:** `session`, the sender's weave; `pane`; `classes`, the action classes the act is
  (`build`, `write`, `open`); `id`, the act.
- **Answers:** `ActorScopeJudged{allowed, refusal}`, to the asker alone: a session this door
  admitted is judged by the row it was admitted under, even after its connection has gone, and the
  file's `host`; any other participant, the weaver's own included, is not narrowed. Nothing is
  remembered.
- **Refuses:** `the question names no action class`, and each class's refusal: `` guest '<name>'
  may not build here: its row has no `build`, ... ``, `this is a weaver's host: only the weaver's
  hand writes a file here, ...`, `this Workshop knows no action class '<class>'`.
- **Example:** asked as `zengine.workshop` about a guest on a weaver's host, `{session: 14, pane:
  "inventory", classes: ["write"], id: "toolbox.save"}` answers `allowed` false and the write's
  refusal.

## Hears

### `TimerFired`

`TimerFired` v1, from the Timer: the door's beat, `zengine.guests.service`. On each, the door admits,
refuses and ends connections, answers the asks for the plan once it is at rest, and publishes the
inventory when a connection came, changed or went. Shape: [`timer/vocabulary.hpp`](../../timer/vocabulary.hpp).

### `TimerReady`

`TimerReady` v1, from the Timer: it is live, so the door asks for its beat again. Shape:
[`timer/vocabulary.hpp`](../../timer/vocabulary.hpp).

### `TimerResolution`

`TimerResolution` v1, from the Timer: what it made of the door's ask for its beat. Shape:
[`timer/vocabulary.hpp`](../../timer/vocabulary.hpp).

## Follow

The door publishes the inventory from `zengine.guests`, `GuestConnections` v1, v2 and v3 together,
on each beat on which a connection came, changed or went, and once on its first beat, empty or not.
A connection that closed is said `closed` once, then dropped. The publications carry every row's
powers and losses, so they are for Workshop's own participants: no guest observes them, and the
relay refuses a guest that asks, `guest '<name>' may not observe GuestConnections v<n>: it says every
connection to this host; ask for your own row with GuestRowDescribedRequested`. To follow: ask the
inventory, then take each publication whole, in place of what you hold.

## Files

**The guests file**, named by `--guests <file>` and read once, at launch: a changed row applies at
the next launch. A file Workshop refuses stops it before anything is mounted, `zengine-workshop -
guests: <the reason>`, then `nothing was mounted and nothing was loaded.` It may name a loopback
listener alone: `guests file '<path>': listen must be loopback (127.0.0.1:<port>)`. When it names a
`port_file`, the port the door listens on is written there before
the plan is realized. What a row holds, its powers, the file's versions and hosts:
[Say who may connect](external-host.md#1-say-who-may-connect).

## See also

- [`zengine.connections`](zengine.connections.md), the pane that shows the inventory.
- [Drive Workshop from another host](external-host.md), and
  [author and check a recipe through an ELH](elh-recipe-journey.md).
- The shapes: [`workshop/guest_seam_vocabulary.hpp`](../../workshop/guest_seam_vocabulary.hpp).
