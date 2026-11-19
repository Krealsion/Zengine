# Flow authoring and generation

Routed behind [AGENTS.md](../AGENTS.md). For `flow/`, also read the maker and operator owners:
[maker](maker.md), [operators](operators.md), and [packaging](packaging.md).
Public contracts: [Flow reference](../flow/docs/flow-reference.md),
[standalone workbench](../flow/docs/flow-guide.md), and
[Workshop authoring](../flow/docs/flow.md). For `flow/flow-host/`, read
[host integration](flow-host.md); for `flow/flow-pane/`, also read [panes](panes.md) and
[message drafts](message-drafts.md).

- Definitions, schemas, admission and graph meaning retain their existing owners. Keep the
  authoring package independent of Workshop and external-host test-session infrastructure.
- `maker::Runtime` owns shared dispatch/state/emission behavior.
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
- Generation refuses a fold in words and writes nothing: only the evaluator spends an operator
  reference, and a generated loop would spend its body outside the evaluation's budget.
  `write_generated` is the one writer of a generated project, for the workbench and the pane.
- Emits stay the
  participant's own publications in its namespace (MW-DEF-08); an empty trigger keeps none, so an
  emit on one is refused rather than lost at save.
- The standalone workbench owns its local Loom session. Its form switch prepares a fresh
  session and transfers state only. Workshop instead uses `flow_host::RuntimeHost` over
  the existing host bus/catalog. Never embed the standalone session in the pane.
- A message shape is authored through `flow/shape.hpp` alone: `GraphDraft`, the pane's model,
  the workbench and the View Builder name, field and type a shape there, so no tool keeps rules
  of its own. A new rule for a shape is written there once.
- `GraphDraft` keeps maker graph meaning; node positions and the viewport are workspace
  presentation metadata. Empty triggers and unwired ports may be saved as unfinished work
  without relaxing maker admission or live operator checks.
- `Workspace` owns the graph, layout, named examples and retained forms, and where each When
  and Then card was put (`DockPlace`, since version 2 of its file), and each trigger whose answer
  was deleted with none chosen since (`unanswered`, version 3). Versions 1 and 2 are read still,
  and listed for the census as `flow::v1::WorkspaceFile` and `flow::v2::WorkspaceFile`.
  The right-press menus' edits are `GraphDraft`
  operations, each taking what it breaks with it (`prune_emits`, `reads_of`); a message declared
  elsewhere is never renamed or reshaped (`owns`).
  Historical form
  shapes are separate observations, not competing definitions published in a host registry.
  A changed shape never silently overwrites authored values. Initial-state edits stay
  distinct from incomplete form contents and observed running state.
- `flow_pane::Model` owns semantic edit commands for graphical and message callers.
  A refused command leaves its previous model intact. Form edits update retained workspace
  data and dirty state; reopening restores active context by exact schema identity.
- A graphical dialog is an unfinished edit owned by its current interaction context. All
  `FlowEdit` requests are refused while it is open; failure to confirm preserves its text.
  While one is open the pane declares Return as its `dialog-confirm` row, so a confirm arrives
  under a gesture; a writing verb's confirm (save, generate, export, library save) asks Workshop
  for class `write` before any byte, and a refusal keeps the dialog and its text.
  Pane reload retains the page, dialog text/caret/selection, dirty flag and deliberate-state
  flag separately from the saved workspace, and the Events as they read: the state the running
  rule was run with, as the host's text for it, read back against the definition the first
  answer after the reload names, and the earlier runs' Events lines, the last
  `Model::kMaxEarlier`. New observations cannot overwrite an initial-state edit awaiting Run.
  Grants, hit maps, held gestures and pending asks are reacquired or ended.
- Canvas pictures clip invisible primitives before spending the shared budgets. A still-dense
  view becomes an explicit recovery picture with no graph hit regions; it never silently drops
  visible nodes. ASCII display substitution changes no values.
  Saved layout uses the authored grid; `GridProjection` maps it to the room's measured text
  advance and padded row height.
  Shared canvas text clipping owns glyph bounds, caret and selection; native text keeps the
  pane's ground beneath it.
  Pointer hit tests spend the room's device grain, and an interaction spends the
  pictured meaning or is refused. Every room change immediately ends held gestures and
  invalidates old picture maps, including a change of text metrics.
- The pane carries a declared message's shape out, with every shape it nests
  (`flow::shape::carried`), from a press's drag or a right press on its row in Messages. A
  dropped description is read with the shapes it carries and nothing else
  (`flow::shape::described`), never the pane's catalog or a registry; one naming a shape it does
  not carry declares nothing, and its page and notice say why in Loom's words. It carries no
  value and no operator out.
- The pane consumes the shared canvas seam and host-manager messages. It owns no private
  catalog, fallback primitives, nested host, native-code loading policy or bus pumping.
- The pane finds powers through the discovery door (`flow/flow-pane/find.hpp`) and never through
  `FlowCatalog`, which supplies the graph's port signatures alone. It asks only for offered
  powers, so a participant's own trigger body is never Flow's to offer, and with a port selected
  only for what yields the port's type in Loom's spelling, and with a wire in hand from an answer
  or a field (`held_wire`) only for what takes the wire's type, the preview adding a step wired
  from it at a port of that type (`ports_taking`). It shows rows as the door answered
  them: it matches nothing again and classifies nothing. A row is known by its kind and its
  identity together, since a form and an operator may share a name.
  A preview evaluates nothing. Add takes an operator reference (the row's
  identity and two content ids): `add-node` places a step at the end or before a node, and
  `add-node-into` before a port's node and into that port, one renumbering edit; a stale
  reference is refused against the graph's ports, never re-bound.
  `GraphDraft::node_ports` is the one answer to a node's ports, a fold's derived from its body by
  `op::fold_ports`.
- `tests/test_flow.cpp` compares state, outputs, refusal reasons, live overlays, authority and
  admission. `tests/package` builds and loads generated code using installed packages only.
  `tests/test_flow_graph.cpp` covers incomplete graph persistence, retained forms, schema
  changes, command atomicity and the cards' places with the files version 1 wrote.
  `tests/test_flow_view.cpp` covers canvas clipping, dense views, long text, independent measured
  axes, device-grain hit testing, the rail's grouping and preview, the rule's sentence, the Next
  line's walk, the journal read one line a message, the tally drawn in light and in cells, a card
  chosen and put, what makes two presses one double click, a step named by its operator and no
  internal word drawn, the rail's words broken to two lines, the sentence and the graph lighting
  each other, what resting tells, what the last message changed lit, and the kinds a kept field is
  chosen from.
  `tests/test_flow_pane.cpp` covers modal commands, native text dragging,
  reload preservation, the right press handed back, the empty press and the pan, the chosen step's
  x and Delete, the When and Then cards moved, the double clicks that add, the box at a port, the
  editors in place, the names offered as a message is named, the rail narrowed by a wire in hand,
  the pointer resting with no button held, a wire that does not fit,
  finding through the door
  and the pane in a graphics window; host behavior is witnessed under [host integration](flow-host.md).
  Add new semantic boundaries to those witnesses rather than preserving a canned example alone.
