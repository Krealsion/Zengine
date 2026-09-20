# Flow authoring and generation

Routed behind [AGENTS.md](../AGENTS.md). For `flow/`, also read the maker and operator owners:
[maker](maker.md), [operators](operators.md), and [packaging](packaging.md).
Public contracts: [Flow reference](../docs/reference/flow.md),
[standalone workbench](../docs/guides/flow.md), and
[Workshop authoring](../docs/workshop/flow.md). For `flow-host/`, read
[host integration](flow-host.md); for `flow-pane/`, also read [panes](panes.md) and
[message drafts](message-drafts.md).

- Definitions, schemas, admission and graph meaning retain their existing owners. Keep the
  authoring package independent of Workshop and external-host test-session infrastructure.
- `maker::Runtime` owns shared dispatch/state/emission behavior. The interpreted subclass spends
  a catalog composition; the generated weave spends the compiled contribution in that catalog.
- The native representation must keep the outer trigger replaceable as well as its leaves.
  Every node runs in order, including unused nodes after the selected result. No purity-based
  optimization follows from an operator's bus-free argument surface.
- `IndexedHost` indices name authored identities only. They must resolve through the current
  catalog at every description/evaluation, preserve full names and never hold resolved providers.
- `CompiledModule` owns a native image share. Its catalog outlives it; it outlives native weaves
  holding its offered context. Every load/reload gets an instance offer. Opening it executes
  native initialization, so host file policy precedes opening rather than following inspection.
- C++ recovery claims equivalence only after complete deterministic regeneration matches the
  source. Never accept only a marker/hash while ignoring handwritten changes.
- The standalone workbench owns its local Loom session. Its form switch prepares a fresh
  session and transfers state only. Workshop instead uses `flow_host::RuntimeHost` over
  the existing host bus/catalog. Never embed the standalone session in the pane.
- `GraphDraft` keeps maker graph meaning; node positions and the viewport are workspace
  presentation metadata. Empty triggers and unwired ports may be saved as unfinished work
  without relaxing maker admission or live operator checks.
- `Workspace` owns the graph, layout, named examples and retained forms. Historical form
  shapes are separate observations, not competing definitions published in a host registry.
  A changed shape never silently overwrites authored values. Initial-state edits stay
  distinct from incomplete form contents and observed running state.
- `flow_pane::Model` owns semantic edit commands for graphical and message callers.
  A refused command leaves its previous model intact. Form edits update retained workspace
  data and dirty state; reopening restores active context by exact schema identity.
- A graphical dialog is an unfinished edit owned by its current interaction context. All
  `FlowEdit` requests are refused while it is open; failure to confirm preserves its text.
  Pane reload retains the page, dialog text/caret/selection, dirty flag and deliberate-state
  flag separately from the saved workspace. New observations cannot overwrite an initial-state
  edit awaiting Run. Grants, hit maps, held gestures and pending asks are reacquired or ended.
- Canvas pictures clip invisible primitives before spending the shared budgets. A still-dense
  view becomes an explicit recovery picture with no graph hit regions; it never silently drops
  visible nodes. Ellipses mark shortened text. ASCII display substitution changes no values.
  Saved layout uses the authored grid; `GridProjection` maps it to the room's measured text
  advance and padded row height. Pointer deltas invert those axes before applying graph zoom.
  Shared canvas text clipping owns glyph bounds, caret and selection; native text keeps the
  pane's ground beneath it. Zoom scales node positions and widths, not glyph size or port-row
  height. Pointer hit tests spend the room's device grain, and an interaction spends the
  pictured meaning or is refused. Every room change immediately ends held gestures and
  invalidates old picture maps, including a change of text metrics.
- The pane consumes the shared canvas seam and host-manager messages. It owns no private
  catalog, fallback primitives, nested host, native-code loading policy or bus pumping.
- `tests/test_flow.cpp` compares state, outputs, refusal reasons, live overlays, authority and
  admission. `tests/package` builds and loads generated code using installed packages only.
  `tests/test_flow_graph.cpp` covers incomplete graph persistence, retained forms, schema
  changes and command atomicity. `tests/test_flow_view.cpp` covers canvas clipping, dense views,
  long text, independent measured axes and device-grain hit testing. `tests/test_flow_pane.cpp`
  covers modal commands, native text dragging and reload preservation; host behavior is witnessed
  under [host integration](flow-host.md).
  Add new semantic boundaries to those witnesses rather than preserving a canned example alone.
