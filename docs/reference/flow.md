# Flow authoring and native generation

**Reference.** `zengine::flow` exports the authoring, persistence, graph and generation headers.
`zengine::maker` exports maker definitions, the interpreted runtime and its existing succession
helpers. Both consume Loom's schemas and operator meaning; Flow introduces no new message bus,
type system or authority policy. The [workbench guide](../guides/flow.md) covers the standalone
tool; [graphical authoring](../workshop/flow.md) covers Workshop. Workshop runs projects through
its [existing-host manager](flow-runtime.md), rather than opening a nested Loom session.
The installed `flow-host/vocabulary.hpp` and `flow-pane/vocabulary.hpp` expose their control
messages through `zengine::flow`; their runtime and presentation implementations remain internal.

## Authoring and persistence

`flow::Draft` uses `op::Builder` for typed node references and `maker::admit_definition` for
complete definitions. A draft does not alter a running weave. `flow::Project` pairs a maker
definition with a value admitted at its state schema. `make_project` validates both.

`project_bytes` wraps their existing native byte representations in `zengine.flow.Project v1`
with `definition:Bytes` and `state:Bytes`. `save_project` writes that one envelope through the
maker file writer, then replaces the previous save. A failed write preserves the previous file.
Projects use the maker file reader's 1 MiB limit and assume a single writer per save path.
The save represents state at the call, not an input queue, provider catalog or process checkpoint.

`definition_json` admits a Loom debug-codec projection before interpreting its fields.
`graph_lines` and `graph_svg` inspect a definition without evaluating its operators.

## Graph drafts and workspaces

`flow/graph_edit.hpp` provides `GraphDraft`: schema fields, message triggers, ordered
operator nodes, typed bindings and the selected state result. Operator port descriptions
are observations from a supplied palette, never retained implementations. Live admission
and evaluation resolve the actual host catalog again. A connection must match its target
port, and node references must point backwards in execution order. Removing a node used
elsewhere, or the chosen result while other nodes remain, requires an explicit reconnect.

`flow/workspace.hpp` provides `Workspace`, its byte codecs and explicit file save/open
functions. A workspace retains a project, node positions, viewport, reusable examples
and unfinished value forms. Layout does not change executable graph meaning. Empty
triggers are stored as draft metadata and restored to the graph; unwired ports retain
their missing input names. Neither makes an unfinished graph executable. `problems`
provides node/port diagnostics before Run or Apply, followed by the existing admission
and host checks. Saving a draft does not run or apply it.

[Message drafts](message-drafts.md) own the typed values and named reusable library.
Workspace forms retain their exact schema and editing context independently of that
library. When an unlaunched schema changes, its old form remains a historical draft;
the new shape starts separately. Retention does not register conflicting old and new
shapes with the runtime. An unfinished state form is distinct from the admitted initial
state: only an explicit Keep state changes the latter.

Workspace saves use the same 1 MiB and single-writer-per-path discipline as project
saves. The complete candidate is validated for reopening before replacement. A workspace
contains no live session custody, message queue, sender identity or grant. Starting or
sending after reopening is a fresh host action.

## Workshop control and reload

The installed `flow-pane/vocabulary.hpp` declares `flow_pane::FlowEdit`, whose wire shape is
`FlowEdit v1` with `action:Text`
and `arguments:List<Text>`, addressed to `zengine.flow`. Arguments are separate strings, not
shell text. `FlowEdited v1` answers with `ok:Bool`, `reason:Text`, `workspace:Bytes` and
`problems:List<Text>`. The workspace uses the codec above. The `describe` action summarizes
the edit grammar; `list-erase(row,index)` removes an item from the selected form's list.

The pane refuses every `FlowEdit` request while a graphical dialog is open, including
`describe`. Confirm or cancel through that dialog before issuing another command. A failed
semantic edit keeps the previous model. For Run, Apply, Send, Stop, Catalog and Inspect, an
accepted command queues a host-manager request; its `ok` is not the runtime outcome. The pane
observes the later answer or dispatch refusal through the
[Flow host protocol](flow-runtime.md). The host's normal message grants still apply.

`FlowPaneState` is the pane's reload snapshot, separate from the workspace file. It retains
the workspace, save path, dirty flag, page, deliberate-initial-state flag, and an open dialog's
action, fields, selected field, caret and selection anchor. On activation the replacement
reconnects to the existing session and reacquires graphical grants. Pending asks, hit maps,
held gestures and process custody are not restored. A deliberate initial-state edit remains
protected from newly observed live state until a matching successful Run answer is accepted. Unconfirmed dialog text is
not included in an ordinary workspace save. An orderly quit refuses an open dialog or unsaved
work until the maker resolves it.

The graphical renderer uses the [pane canvas protocol](workshop-panes.md#optional-pane-local-canvas).
It draws rectangular nodes and orthogonal wires with fixed-size printable-ASCII labels; display
substitution and ellipses leave stored values intact. Zoom scales node positions and widths
from 50% to 200%, while glyph size and port-row height remain fixed. Content is clipped before
the canvas budgets are spent. A view that still exceeds a budget becomes an explicit recovery
picture, with save/export and zoom controls, rather than a partially drawn graph. Pointer hit
testing uses the room's device grain and the picture identified by Workshop's input fence.

## What generation means

`generate_cpp` admits the input definition, emits explicit C++ statements for every node and
binding in each trigger, and retains its canonical definition bytes. The generated handler does
not pass a graph to the interpreter. Its `flow::Step` helpers pack one operator invocation at a
time; they never walk a graph. Schemas and state still use `loom::Schema` and `loom::Value`.
Generation is a change in representation, not a claim that this implementation is faster.

The generated artifact exposes three surfaces:

| surface | purpose |
|---|---|
| ordinary Loom weave ABI | state, accepted/emitted schemas, inspection and message handling |
| Zengine operator-consumer ABI | the host's live outer-trigger lookup, offered for each instance |
| `zengine_flow_artifact`, ABI 1 | retained definition and compiled trigger entry points |

`flow::CompiledModule` opens the artifact and mounts its compiled triggers as native
`op::OperatorDef` contributions under the definition's existing provider identity. The generated
weave spends its outer trigger through that catalog. Each compiled trigger spends its nodes
through the same catalog. Overlaying or unmounting either layer therefore remains observable.
The original definition remains available through `CompiledModule::definition()` for inspection.

The module adapts the existing operator ABI to a numbered view of the definition's identities.
These numbers index immutable authoring metadata, never a cached provider or callable. Every
description and evaluation still resolves the full original name at spend. This also preserves
operator names containing NUL without changing the existing string-based operator ABI.

## Host integration and lifetime

The host owns one catalog, declares which operator providers it trusts, and keeps that catalog
alive longer than its modules and weaves. After `CompiledModule::open(catalog, path)`, check
`module->mount()`. Hold `module->offer()` around exactly one `Kernel::load` or reload so the
new instance copies its own operator table. Keep the module alive until all instances that
received its offer are destroyed. Unmount its provider explicitly when retiring its rules.

`CompiledModule::open` opens native code; it is a trusted host action, not a deferred permission
request. Apply any host file-admission policy before calling it. The Kernel separately admits
the ordinary weave manifest and applies the host-authored grant at load. A generated artifact
loaded without a Flow module offer refuses construction. It never invents a local operator
catalog. As with `OperatorOffer`, offers are synchronous; overlapping offers to one image are
unsupported.

The generated-project CMake file uses `find_package(zengine)` and
`loom_weave_build_contract`. No Zengine or Loom source tree is required. In-tree generated
fixtures use `zengine_weave` and carry the repository's sanitizer options.

## Preserved behavior and limits

Both forms share `maker::Runtime` for message dispatch, inspection, state writes and emissions:

- Nodes run in authored order, even if a node's result is unused. Each spend checks the authored
  input/output signature IDs against the live operator. Additional arguments beyond the live
  port count remain ignored, matching the existing evaluator.
- An evaluation failure leaves state unchanged. The outer output gate checks the selected
  answer against the target state field before write-back.
- Each successful trigger writes one named field, then emits from the new state. A failed emit
  keeps that write and continues later emits. Publication still spends the weave's grant.
- All seven Loom kinds, optional absence and nested schema closure retain their maker meaning.
  Emitted schemas participate in admission before any listener exists.
- Ordinary diagnostics honor the incoming reply address and correlation. Outgoing `reply_to`
  is empty, so the bus-stamped sender is the return address. `Adopt` retains the authenticated
  `Bus::answer` path. Unarmed ceremony messages remain refused.
- A native composition can propagate an existing refusal through `op::Refusal`; `Catalog`
  preserves its reason. Ordinary exceptions still get the native implementation-failure diagnostic.

Generation covers admitted definitions, including ones that cannot be mounted: admission is
not operator availability or successful registration. For example, an empty provider is still
refused at mount. No optimizer removes nodes or freezes primitive implementations.

`apply_behaviour_edit` operates on the interpreted form, preserving its registered schema
contracts and state. The workbench returns to interpreted form for an edit, then generates and
builds again for native code. Compiled conversion metadata and its migration operator are
retained, but the existing in-process `begin_schema_edit` helper does not arm an exported native
instance through the Kernel. General cross-form succession remains a separate integration
concern. Workshop graphical editing uses the interpreted host-manager path; it does not implicitly compile or load native code.

`recover_cpp` supports exactly the current generator format and unchanged generated source.
It compares complete regenerated bytes, rather than trusting a copied marker or treating a
retained definition as an inverse parser for arbitrary C++. Loaded artifacts can describe their
definition; that description is not cryptographic evidence that handwritten native code agrees
with it. Native code is trusted under the host's ordinary execution boundary.
