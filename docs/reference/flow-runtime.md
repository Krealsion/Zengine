# Flow in an existing host

The Flow manager runs interpreted projects on the host's existing Loom Switchboard and
operator Catalog. It creates ordinary maker participants and resolves their operators through
that live catalog. It does not create another Loom session or publish fallback primitives.

A managed interpreted participant is not an artifact loaded by the Kernel. Its concrete identity
is returned by Flow and its state is available through Flow inspection; a list of Kernel-loaded
artifacts has a different population. The standalone workbench's native generation and loading
remain available separately ([Flow reference](flow.md)).

## Conversation

The manager holds `zengine.flow.host`. Its message definitions live in
`flow-host/vocabulary.hpp`, installed through `zengine::flow` alongside the graphical pane
control shapes in `flow-pane/vocabulary.hpp`. These headers let another participant speak
the protocols. The manager implementation and pane model/view remain Workshop internals.
Each request receives one authenticated, correlated answer. `FlowChanged` is an office-authored
notification to inspect again, not an acknowledgement that an application operation succeeded.

| Request | Meaning |
|---|---|
| `FlowRun(session, project)` | Admit native Flow project bytes and create a fresh subject. Refuse an existing session or occupied definition role. |
| `FlowApply(session, definition)` | Admit native definition bytes and perform a behavior edit. Retain live state and the registered input/output contracts; require a new revision. |
| `FlowSend(session, payload)` | Admit native value bytes against a declared input and queue a fresh send to this session's subject. |
| `FlowInspect(session, after)` | Return current definition and state, retained observations newer than the event cursor, and pending dispatch count. |
| `FlowStop(session)` | Refuse while this session's input dispatch is pending; otherwise return the final project and remove its subject and operator contributions. |
| `FlowCatalog` | Describe the current host catalog using existing operator descriptors, including nested schema closure. |

`FlowAnswer` carries the request's action and session, `ok`, a reason, the subject identity,
project bytes, events, retention bounds and pending count. `FlowCatalogAnswer` carries complete
operator descriptors or an explicit refusal. Clients must check answer provenance and their
request correlation. They must check `FlowChanged`'s authored office before treating it as a
manager notification.

A successful send answer means **queued**, not delivered or successfully evaluated. Events
separately report queued input, direct dispatch or dispatch refusal, failed handler completion,
application refusal, and observed output. Input correlations connect these observations to the
request. Publication outputs are accepted only from this session's subject. A current snapshot
is another observation, not a promise that all future consequences have finished.

## Custody and authority

Personal requests scope session names to the bus-stamped requesting participant. Deliberately
office-authored requests scope them to that office, and the manager checks that the requesting
participant still holds it when the request arrives. A successor can continue an office's
sessions by speaking as that office. Merely holding an office while speaking personally does
not acquire its sessions. Other participants cannot inspect, edit, send into or stop a session
by guessing its name.

A host grants these request shapes to the manager's role only for participants allowed to
create ordinary data-authored weaves. `allow_flow_requests` spells that narrow grant. The
manager's creation authority uses the maker's normal declared emission grant; it is not a
sandbox for untrusted definitions or native operator implementations.

Each session has a fresh input participant whose send rules name only the definition's accepted
shapes and the concrete subject. The host's fenced injection acts as that participant and is
checked against its current authority. Saved payloads contain no sender, destination, reply
provenance or grants. Reusing one never reuses old authority.

## Dispatch and observation

The host adapter owns one fence per queued input. It never pumps or waits. An ordinary Timer
binding wakes its observation clock every 16 ms, retires settled fences and asks the manager to
notify the session owner. Quiet state-only triggers therefore update the pane without relying
on an output message. Hosts without a Timer can call `RuntimeHost::poll()` between their own
turns; manual inspection still reads current state and fence status.

A fence counts the synchronous dispatch descendants of one input. It does not wait for future
timers, external work or an application that has chosen not to answer. Endless feedback can
remain pending. Apply and Stop refuse while these managed input fences are open. They do not
claim global quiescence: unrelated host traffic and future external work are outside them.

Direct input delivery facts are captured from the bus at delivery rather than recovered later
from its rolling outcome journal. Output payloads are recorded by the session's ordinary
listener. This is a bounded working view; the host's Loom logger and recorder remain the homes
for broader or durable observation.

Bounds are eight sessions per manager, 16 retained input fences per session, and 128 events or
256 KiB of event payload/detail per session, whichever is reached first. An event payload above
64 KiB is omitted with an explicit marker. Old events are evicted with a dropped count and
first/last sequence bounds. Project, definition, input and catalog query data are limited to
1 MiB at admission. A host turn retires settled input slots before further requests reuse them.

## Host composition and lifetime

Construct `flow_host::RuntimeHost` from the host's existing Switchboard and Catalog, then call
`mount()`. Keep it alive while its manager, clock and sessions are used. Declare it after those
dependencies so its destructor removes its participants and contributions before the catalog
or bus dies. No Workshop rendering, pane identity or graphical interaction belongs in this
adapter.

The native manager currently owns session custody and command policy; the host adapter owns
registration, state access, behavior editing, bounded injection and teardown. A future manager
replacement must explicitly transfer its ownership records and observation relationships. No
such transfer or native-generation switch is implied by saving a project.

A fresh schema requires an explicit fresh run or the maker's authored succession machinery;
`FlowApply` never silently migrates state or changes a live participant's contract.
