# Flow host integration

Routed for `flow-host/`; read [Flow](flow.md), [maker](maker.md), and
[operators](operators.md). Public contract: [Flow host](../docs/reference/flow-runtime.md).

- `RuntimeHost` references the host's existing Switchboard and Catalog. It owns no Kernel,
  private bus, fallback catalog, rendering or file policy. Destroy it before those dependencies.
- `Manager` owns session custody and command policy. Its adapter owns native registration,
  snapshots, behavior editing, bounded input injection, observation and teardown. Bootstrap
  composes those owners; it does not implement Flow behavior.
- Personal session keys use stamped sender identity. Office keys require deliberate office
  authorship and the current holder at dispatch. A guessed name or personal speech by an office
  holder does not acquire office custody.
- Saved bytes are values only. Each send uses the session's current participant and concrete,
  shape-scoped grant; no saved sender, target, grant or answer provenance is restored.
- `FlowAnswer.ok` on Send proves queueing. Direct delivery, handler failure, application refusal,
  output and current state remain distinct observations. Correlations identify; they do not
  authenticate. Answers use the answer door; change notices carry the manager's authored office.
- No private pumping or waiting. Existing Timer bindings drive `poll`; bare hosts may call it
  between turns. Input fences cover synchronous dispatch ancestry only, never future application
  completion. Keep this limit in Apply/Stop and UI language.
- Capture direct input fates at delivery, before the rolling outcome journal can evict them.
  Retention loss and oversized payload omission are explicit. The bounded event view is not a
  replacement for Loom's logger or recorder.
- Interpreted subjects are ordinary native maker registrations, not Kernel-loaded artifacts.
  Keep live operator resolution and existing maker admission/behavior-edit semantics unchanged.
- `tests/test_flow_runtime.cpp` witnesses same-host resolution, correlated observations,
  custody, state-preserving edits, refusal, retention, stop/reopen and Timer-driven notification.
