# zengine.connections -- the Connections pane

**Manual.** The pane that shows the other Loom hosts connected to this Workshop, as the guest door
reports them. It is a *weave*: a piece of Workshop that is loaded, replaced and spoken to by
messages, built as `zengine-connections-pane`. Other weaves reach it by its *office*, the name it
answers to, `zengine.connections`, and it offers one pane, `connections`. The connections themselves
are the guest door's, [`zengine.guests`](zengine.guests.md).

## About

Each connection is said as the door reports it: the door's number for it, the name Workshop
established for it (with what the peer claimed beside it, when the two differ), where it stands, its
session's weave, where it came from and, when refused, why. Beneath a connection a guests-file row
admitted, the pane says the file's version and what the row's powers do not reach on this host, and
the row's role and doors when it names them.

The pane derives nothing and decides nothing. `awaiting-decision` is a fact the door reports, not a
question the pane asks you: it admits no one, grants nothing and declares no commands. A press on a
row does nothing; a right press opens Workshop's own menu for the pane.

## Use

Open it from the Pane Manager, <!-- key desktop.panes -->`ctrl+p`<!-- /key -->. In a Workshop
launched without a guests file it says so, `CONNECTIONS -- this Workshop accepts no guests`; launched
with `--guests <file>` it lists each host as it connects, changes and goes, `nobody is connected`
until one does. A connection that ends is shown `closed` once, then dropped.

## Follow

The pane's rows follow the door's inventory, `GuestConnections` v3 from `zengine.guests`: asked once
when the pane goes live, then taken whole at each publication. A word about connections from anyone
else is not taken. An observer follows the door instead of reading the pane
([Follow](zengine.guests.md#follow)); a guest asks for its own row there.

## Shows

### `heading`

How many connections, where Workshop listens, and the host the guests file names, `CONNECTIONS -- 2
connections at 127.0.0.1:4242 -- a development host`; `CONNECTIONS (waiting for the guest door)`
before the door has said anything; `CONNECTIONS -- this Workshop accepts no guests` and `launch with
--guests <file> to listen` when it listens for none.

### `connection`

One connection: `#1 agent (claims 'agent-from-elsewhere')  admitted  weave 20  from 127.0.0.1:5000`;
a name only claimed is `'stranger' (unverified)`, and no name `(unnamed)`; a refusal follows ` -- `.
An admitted connection stands in the pane's ink, one awaiting a decision in the alert colour, and
the rest muted.

### `version`

Beneath a connection a guests-file row admitted: the file's version and what the row's powers do not
reach here, in the door's words, `version 1 -- not here: builds; file writes`, or `version 2 -- all
its powers reach here`.

### `reach`

Beneath that, when the row names a role or doors: its reach in the door's words, `role tutor: may
capture, guide; reads zengine.builder-pane/*; points within zengine.builder-pane/*`.

### `more`

When the pane has no room for the rest, `... 3 more`: a connection is said whole, with the lines
beneath it, or counted here.

### `refused`

`refused 2, shed 1 (all time)`: the connections refused and shed since launch, when there were any
and room is left for the line.

## Files

The pane reads no file. The guests file is the door's ([Files](zengine.guests.md#files)).

## See also

- [`zengine.guests`](zengine.guests.md), the door whose inventory this is.
- [Drive Workshop from another host](external-host.md): say who may connect, and link a host.
