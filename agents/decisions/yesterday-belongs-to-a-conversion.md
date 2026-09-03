# Yesterday belongs to a conversion

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [migration](../workshop/migration.md).

**Context.** The session reader carried three shapes and three roads — the version it writes and
two older ones kept beside it — every one compiled into every Workshop, and the next format
change was going to add a fourth (`0cffe92`, "Let the session reader forget every shape but the
one it admits"). The format then moved three times in a week: 3 → 4, 4 → 5, 5 → 6.

**Decision.** The reader knows one shape and carries `kFormatVersion` and no second number. The
retired shapes live in `workshop/session_history.hpp`, shipped as
`zengine-workshop-session-history`, an ordinary operator provider. A retired struct is copied
verbatim and keeps the wire identity an old file's bytes claim; every edge is direct and the
catalog reads `current` off the reader's own schema. The reader's whole knowledge of history is
one arm asking `op::migrate`. A conversion cannot skip a check. The catalog reaches the reader as
a reading, never a power. The ordering is authored plan order. Reading never rewrites. Old
vintages are one layout, live at zero, related to nothing. Version 6 moved the number without
moving a field.

**Alternatives considered.**
- *Retained old readers beside the current one* — retired; the setup file keeps its version-2
  reader deliberately, because a setup is a named artifact with no session to ride.
- *A retained version-5 branch in the current reader* — it would compile, admit and behave for
  every file that does not depend on the distinction, so only the source tripwire catches it;
  pinned by case `"MIG-0/SC-8: the session reader owns no historical shape and no conversion"`.
- *Chained edges or a searched route* — rejected: a searched route is a result no participant
  authored; pinned by case `"WUX-10/SC-4: three DIRECT edges, and no chain to walk even if one
  wanted to"`.
- *A reordered field in a retired struct* — measured to strand every file: content ids are
  pinned for every vintage, v4's `0xb621c9f3616c7bb1` measured off `a39795e` and v5's
  `0x6f5b0dfc72bfa501` read off a file the live witness left behind.
- *Inferring plurality or an association for an old file* — refused: defaulted, not inferred;
  inventing an association from `--setup` would be the reader deciding what the maker never
  wrote down.
- *Converting the live desk but not a link's `known`* — rejected: it would tell every maker
  their desk had drifted because of an upgrade they did not make.
- *Dropping a fact for a desk at `kMaxSetupPanes`* — refused: the file survives for a build that
  can say more.
- *A retry, a pending posture or a demand-load for the conversion row* — refused: row order buys
  it, and a case pins the shipped plans' order.

**Consequences.** Each format move cost the reader one number and one shape; the edges renamed
themselves with no string edited, no plan row added and `operator/migration.hpp` unchanged. The
forbidden-token list names v4 and v5. `HostContext::conversions` is null for every fixture. A
version claim is a lookup key that reaches no load door; the file first changes at the ordinary
close-time save.

**Laws supported.** [WL-MIG-01](../workshop/migration.md), [WL-MIG-02](../workshop/migration.md),
[WL-MIG-03](../workshop/migration.md), [WL-MIG-04](../workshop/migration.md),
[WL-MIG-05](../workshop/migration.md), [WL-MIG-06](../workshop/migration.md),
[WL-MIG-07](../workshop/migration.md), [WL-MIG-08](../workshop/migration.md),
[WL-MIG-09](../workshop/migration.md), [WL-MIG-10](../workshop/migration.md).
