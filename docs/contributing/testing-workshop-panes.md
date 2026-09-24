# Testing a loaded Workshop pane

Use the real pane artifact when the claim depends on routing, permissions, layout or replies.
[`tests/workshop_support.hpp`](../../tests/workshop_support.hpp) supplies `PaneRig`, shared by
the `workshop_panes` suite. The test that owns the behavior supplies its actor and any controlled
peer. Search the test sources for `InventoryStory` for an example of input from a participant
with explicit grants. Its defaults are a fixture choice, not Workshop's default user authority.

## Arrange the same boundary you intend to prove

| Your case needs... | Arrange... | Otherwise you may be testing... |
|---|---|---|
| A guest gesture | An admitted input session and an actor granted the exact operation the gesture requests | A host-root action that bypasses the actor decision |
| A slow or reordered reply | A real requester and a controlled responding weave holding `mail.defer_answer()` | An ordinary message that never earned answer authority |
| A placed pane | The authored setup plus the normal setup/layout reconciliation | A changed setup object with stale runtime rectangles |
| A click that closes its target | Both pointer positions measured before delivering the press | A fixture trying to measure a pane that the press already removed |
| A particular test case | The runner's listed selection and actual executed population | A filter that selected nothing or several unintended cases |

### Permissions belong to the arranged actor

Inspect the fixture's actual grant construction. For example, the Inventory story's default
permission mask does not include the separate `PaneValueCarryRequested` rule used to acquire a
field from Info. A loaded pane's own send grant does not supply the input actor's missing grant.
Add the specific capability for a positive case and deliberately withhold it for the negative
case. An integer mask is a helper convention; the shape, version and target are the capability.
See [external-host powers](../workshop/external-host.md) for real guest admission.

### Delay a reply without changing what a reply means

Send the request from a registered participant. In the responding weave's handler, retain its
deferred-answer capability, then answer from a later delivery to that same responder using
`loom::answer_deferred`. Assert the capability was valid before arranging the race. A test-root
request with no participant to answer cannot provide that capability; assigning a matching
correlation to an ordinary message does not make it an answer.

The test root may trigger the responder's later delivery; the deferred capability still owns the
original conversation. Keep the actors' grants explicit, including replies. Do not change the
production provenance check to accommodate an incorrectly arranged test. Loom's
[message reference](https://github.com/Krealsion/Loom/blob/main/docs/reference/messaging.md)
owns these guarantees. `PokeDescribe` is answered by the substrate for an ordinary weave, so a
slow source is a raw `loom::Weave` that defers it (`DeferredSource` in
[`tests/test_workshop_info_views.cpp`](../../tests/test_workshop_info_views.cpp)).

### Put a message between a request and its answer

The bus is FIFO, and `pump_pending()` delivers exactly the backlog present when it was called.
Stop the turn after the delivery that sent the request (an observer that calls `bus.stop()`, then
`pump_pending()` turns), enqueue the interference, then drain. `InventoryStory::until_delivered`
in [`tests/inventory_story.hpp`](../../tests/inventory_story.hpp) does this for one injected
event. Helpers that drain (`act`, `key`, `click`) spend whatever is queued, so build the order
this way rather than by sleeping. A maker's later gesture always reaches Workshop after the
request, so only an event without a gesture can land in between: a reset request, a provider
reload, an owner that loses its door.

Two facts of the runtime shape such cases. A reload through `PaneRig::enqueue_reload` revives
the new image at the same `WeaveId`, so a check that compares holder ids cannot see it. For
Loom's own dispatch refusal, hold the owner's role for an interval with an office that lacks the
door (`ScriptedViews::doorless` in the Info views suite); the send is queued and refused as
`NotAccepted`. `PaneRig::load` and `unload` mount a seat whose state a realized plan has already
published, so beside `run_plan` use `enqueue_reload`, or a seat with a state of its own
(`unload_then_load` in the Info views suite).

### Separate authored placement from the last arranged picture

`PaneRig::extent` reports a medium's `SurfaceExtent`; it is not a force-layout command.
`adopt_screen` ignores an unchanged normalized extent and face metrics, so reporting the same
dimensions after directly editing `session().setup.active` will not apply that setup.
For a maker-facing layout test, use the ordinary setup or pane operation. A fixture that directly
arranges authored state can report a genuinely changed extent and then restore the intended one
to trigger reconciliation. Verify the resulting rectangle before measuring input. This is a
fixture setup technique, not an interaction a user should have to perform.

Measure press and release before dispatching a press that may close its pane. If the story
deliberately moves a target during a gesture, record that motion explicitly instead of having a
coordinate helper silently retarget the release. Keep cells, subcells and device pixels distinct;
the pane's current room and shared hit geometry determine where the control is painted.

### Select the cases you think you selected

Doctest treats commas in `--test-case` as filter separators, even when the shell quotes the whole
argument. Prefer a distinctive comma-free pattern and inspect `--list-test-cases` with the same
filter before relying on a focused result. A list is discovery, not an executed test; report the
actual run's population and exit code. The [official lane](build-and-test.md) remains the delivery
check for a complete repository green.

## Test what happens after the first outcome

A useful operation test follows a delay or refusal with the maker's next action:

- Edit or drop a field while a replacing read is pending, then release the reply. Prove the
  chosen adoption policy preserves accepted edits, or that the conflicting edit was visibly refused.
- Refuse permission, refuse at the owner, or refuse delivery; then issue a different valid
  operation. Prove the old operation's purpose cannot mislabel or consume the new answer.
- Cancel an observation before approval and during a read, then reuse the view. Prove late
  replies cannot restart observation or affect the replacement subject.

These are test-design obligations for the behavior under change, not a claim that every existing
pane implements the same policy. The [pane authoring guide](../guides/make-a-workshop-tool.md#ask-another-weave-and-recognize-its-answer)
explains the separate decisions of recognizing a reply and applying its result.
