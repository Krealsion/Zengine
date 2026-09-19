# Flow authoring and native generation

**Reference.** `zengine::flow` exports the authoring, persistence, graph and generation headers.
`zengine::maker` exports maker definitions, the interpreted runtime and its existing succession
helpers. Both consume Loom's schemas and operator meaning; Flow introduces no new message bus,
type system or authority policy. The [workbench guide](../guides/flow.md) walks through usage.

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
instance through the Kernel. General cross-form succession and graphical node editing remain
separate consumers of these surfaces.

`recover_cpp` supports exactly the current generator format and unchanged generated source.
It compares complete regenerated bytes, rather than trusting a copied marker or treating a
retained definition as an inverse parser for arbitrary C++. Loaded artifacts can describe their
definition; that description is not cryptographic evidence that handwritten native code agrees
with it. Native code is trusted under the host's ordinary execution boundary.
