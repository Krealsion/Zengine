# Flow authoring and generation

Routed behind [AGENTS.md](../AGENTS.md). For `flow/`, also read the maker and operator owners:
[maker](maker.md), [operators](operators.md), and [packaging](packaging.md).
Public contracts: [Flow reference](../docs/reference/flow.md) and
[workbench guide](../docs/guides/flow.md).

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
- The workbench's form switch prepares a fresh local session and transfers state only. It is
  not a general manager for in-flight messages, provider migration or schema succession.
- `tests/test_flow.cpp` compares state, outputs, refusal reasons, live overlays, authority and
  admission. `tests/package` builds and loads generated code using installed packages only.
  Add new semantic boundaries to those witnesses rather than preserving a canned example alone.
