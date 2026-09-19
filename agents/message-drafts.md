# Reusable typed drafts

Routed behind [AGENTS.md](../AGENTS.md). Public contract:
[message drafts](../docs/reference/message-drafts.md). Public headers also follow
[packaging](packaging.md); evidence follows [verification](verification.md).

- `message_draft::Draft` edits Loom `Value` against its immutable original `Schema`.
  Missing required fields are valid draft state; every other gate error refuses an
  edit transactionally. Actual use spends the original schema's complete admission.
- `Path` carries field names and indices structurally. `path_label` is presentation,
  never a grammar: field names may contain the punctuation the label uses.
- Scalar forms spend Loom's lexer and composer. Bytes have no text spelling here;
  containers use declared schema/type references. No second scalar parser or type
  system belongs in the package.
- Absence, false, empty text and empty containers are distinct. Nothing fills an
  unauthored scalar. `unset` removes content; a frontend wanting disabled contents
  or invalid unfinished text owns that presentation state separately.
- Nested values copied into or out of a draft are isolated by the editing/snapshot
  doors. Ordinary `value()`/`get()` inspections obey Loom's immutable-reader rule.
- `Library` is a data library, not a schema catalog for a running host. Presets carry
  title, schema closure and draft content; no destination, sender, grants,
  correlation or reply provenance is persisted. A send is the caller's fresh act.
- Schema agreement includes complete closures. Registry claims on roots alone do
  not establish that their referenced components agree. The optional current
  registry check reads existing knowledge without publishing into it.
- The optional persistence projection changes presence requirements only, and is
  private to decoding draft data. Never register it as the actual runtime schema.
  The original descriptors stay authoritative across save/reopen.
- Persistence retains the existing maker file writer's single-writer-per-path
  discipline and size limit. Validate the whole candidate before replacement.
- `tests/test_message_draft.cpp` covers presence, scalar meaning, nested editing,
  copy isolation, partial persistence, conflicts, corruption and failed saves.
  `tests/package/message_draft_consumer.cpp` exercises the installed boundary.
