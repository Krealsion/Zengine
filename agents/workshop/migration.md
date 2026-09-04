# Workshop law — migration

Register `WL-MIG`: yesterday's session belongs to a conversion. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md). The conversion convention itself is
[`../operators.md`](../operators.md).

## WL-MIG-01 — The session reader knows one shape

LAW — The session reader carries one format number and no second; the retired shapes live in their own history header, shipped as `zengine-workshop-session-history`, an operator provider.

PROVEN BY — `workshop/session_persist.hpp` `kFormatVersion`, `loaded_from`;
`workshop/session_history.hpp` `conversions`; `workshop/session_migration_provider.cpp`
`conversions`; `workshop/CMakeLists.txt` `zengine-workshop-session-history`;
`tests/test_workshop_persistence.cpp` case `"MIG-0/SC-8: the session reader owns no historical
shape and no conversion"`, case `"MIG-0/SC-7: the shipped artifact supplies exactly the
conventional edges"`.
WHY — `agents/decisions/yesterday-belongs-to-a-conversion.md`

## WL-MIG-02 — A retired struct is copied verbatim, and every edge is direct

LAW — A retired shape is copied verbatim and keeps the wire identity an old file's bytes claim, every historical id is pinned with its provenance, and the catalog reads current off the reader's schema.

MEANS
- `v1` to `v6` composes `session_v1_to_v3`, `v3_to_v4`, `v4_to_v5` and `v5_to_v6` in C++;
- the catalog holds no `v1` to `v3` or `v3` to `v4` edge, so there is no chain to walk.

PROVEN BY — `workshop/session_history.hpp` `v1`, `v2`, `v3`, `v4`, `v5`, `conversions`,
`session_v1_to_v3`, `session_v3_to_v4`, `session_v4_to_v5`, `session_v5_to_v6`,
`v3::WorkshopSession`, `v4::WorkshopSession`, `v5::WorkshopSession`, `session_v2_to_v3`,
`session_v1_to_v6`, `desk_v2_to_v3`; `tests/test_workshop_persistence.cpp` case `"WUX-10/SC-3: a
retired shape's wire identity is the identity it was written at"`, case `"WUX-10/SC-4: three
DIRECT edges, and no chain to walk even if one wanted to"`, case `"MIG-0/SC-7: a conversion owns
yesterday's semantics and does not rewrite history"`.
WHY — `agents/decisions/yesterday-belongs-to-a-conversion.md`

## WL-MIG-03 — Version 6 moved the number without moving a field

LAW — A version-5 desk with no Layouts row had the layout surface anyway; a version-6 desk with none took it off; `desk_v5_to_v6` materializes the row in both copies of a desk.

MEANS
- it converts a link's `known` too, because that value is compared for exact equality;
- it does not touch the `known` of an unassociated layout;
- a desk already at `kMaxSetupPanes` refuses rather than dropping either fact.

PROVEN BY — `workshop/session_history.hpp` `session_v5_to_v6`, `desk_v5_to_v6`,
`v5::WorkshopSession`, `names_layouts`; `workshop/session_persist.hpp` `WorkshopSession`;
`workshop/setup.hpp` `kMaxSetupPanes`; `tests/test_workshop_persistence.cpp` case `"WUX-12/SC-11:
a real pre-WUX-12 session comes back with nothing lost"`, case `"WUX-12/SC-11: an explicit
historical row is preserved, never duplicated"`, case `"WUX-12: a full desk refuses the conversion
rather than losing either fact"`.
WHY — `agents/decisions/yesterday-belongs-to-a-conversion.md`

## WL-MIG-04 — A retained old branch would compile, so a source tripwire forbids it

LAW — A retained v4 or v5 branch in the reader would admit and behave for every file that does not depend on the distinction, so the persistence suite forbids the retired tokens in the reader.

PROVEN BY — `workshop/session_persist.hpp` `kFormatVersion`;
`tests/test_workshop_persistence.cpp` case `"MIG-0/SC-8: the session reader owns no historical
shape and no conversion"`.
WHY — `agents/decisions/yesterday-belongs-to-a-conversion.md`

## WL-MIG-05 — Old vintages are one layout, live at zero, related to nothing

LAW — A v1/v2/v3 session is exactly one layout at position zero (`absent_placement`'s argument, one field over), and every historical layout is related to nothing (`absent_link`).

MEANS
- nothing is inferred from `--setup`: the reader may not decide what the maker never wrote.

PROVEN BY — `workshop/session_history.hpp` `absent_placement`, `absent_link`,
`session_v1_to_v3`, `session_v3_to_v4`, `session_v4_to_v5`; `tests/test_workshop_persistence.cpp`
case `"WUX-10/SC-5: a version-3 session becomes exactly one layout, live at zero"`, case
`"WUX-10/SC-5: all three vintages arrive as one layout at position zero"`, case `"WUX-11/SC-15: a
version-4 session opens with its run whole and every link none"`.
WHY — `agents/decisions/yesterday-belongs-to-a-conversion.md`

## WL-MIG-06 — The reader's whole knowledge of history is one arm

LAW — This shape's name at another version is a historical claim, and the reader asks `op::migrate` for one live direct edge to `schema_of<WorkshopSession>()`.

MEANS
- the next format move changes `kFormatVersion` and authors edges in the provider.

DOES NOT MEAN
- that a rung may be added to the reader: that is the thing this seam exists to prevent.

PROVEN BY — `workshop/session_persist.hpp` `op::migrate`, `schema_of`, `kFormatVersion`,
`could_not_convert`, `from_text`; `tests/test_workshop_persistence.cpp` case `"MIG-0/SC-5: nothing
but a historical claim of THIS shape asks for a conversion"`, case `"MIG-0/SC-14: a current
session bypasses conversion entirely"`, case `"MIG-0/SC-5: an old session with no conversion live
refuses and changes nothing"`.
WHY — `agents/decisions/yesterday-belongs-to-a-conversion.md`

## WL-MIG-07 — A conversion cannot skip a check

LAW — This format's whole law is one function with two callers — off the gate, and out of a conversion's answer — so no conversion skips a check; the format word crosses untouched, the version cannot.

PROVEN BY — `workshop/session_persist.hpp` `current_in`, `forged_version`,
`WorkshopSession::format_version`; `workshop/session_history.hpp` `mismatched_version`;
`tests/test_workshop_persistence.cpp` case `"MIG-0: an old session's OWN law still runs -- the
conversion skips no check"`, case `"MIG-0: a current-version file whose own field says otherwise
is a forgery"`, case `"WUX-0 D/MIG-0: an unreadable session names its version by NUMBER"`.
WHY — `agents/decisions/yesterday-belongs-to-a-conversion.md`

## WL-MIG-08 — The catalog reaches the reader as a reading, never a power

LAW — The conversion catalog reaches the reader as a read-only pointer the host wires — null is ordinary, and what every fixture gets — and nothing a holder can do mounts, loads or realizes anything.

MEANS
- an old file's version claim is a lookup key that reaches no load door.

PROVEN BY — `workshop/weave.hpp` `HostContext::conversions`; `workshop/session_persist.hpp`
`op::migrate`, `from_text`, `load_file`; `tests/test_workshop_persistence.cpp` case `"MIG-0/SC-6:
with the conversion mounted, the desk comes back through the weave"`, case `"MIG-0/SC-11:
unmounting the artifact takes the conversion with it"`.
WHY — `agents/decisions/yesterday-belongs-to-a-conversion.md`

## WL-MIG-09 — The ordering is authored plan order, and nothing else

LAW — A provider-only row runs inside `PlanExecutor::begin()`; the first weave row opens a conversation; the session is read from `on(SurfaceReady)`, so a conversion row placed above is live in time.

MEANS
- both shipped plans name it second; there is no retry, no pending posture and no demand-load.

PROVEN BY — `workshop/load_execute.hpp` `PlanExecutor`, `PlanExecutor::begin`;
`workshop/graphical-load-plan.json` `zengine-workshop-session-history`;
`tests/test_workshop_load.cpp` case `"BOOT-0: a plan of provider-only rows finishes inside
begin(), turning nothing"`, case `"the shipped default plan is a legal plan, and it is the
terminal arrangement"`.
WHY — `agents/decisions/yesterday-belongs-to-a-conversion.md`

## WL-MIG-10 — Reading never rewrites

LAW — A converted session is in-memory and the file first changes at the ordinary close-time save, which writes the current shape; a converter is needed only while yesterday's bytes exist.

PROVEN BY — `workshop/weave_session.cpp` `save_last_session`, `restore_last_session`;
`tests/test_workshop_persistence.cpp` case `"MIG-0/SC-13: reading an old session does not rewrite
it; the next close does"`, case `"WUX-12/SC-11: the maker sees no loss, and the next run spends no
conversion"`.
WHY — `agents/decisions/yesterday-belongs-to-a-conversion.md`

## Do not assume

- That `session_persist` still reads old sessions, or that an old file gets what it asks for:
  one version is admitted; an older file opens exactly when a conversion is mounted
  (WL-MIG-01, WL-MIG-06).
- That a session format move implies a desk format move: the session is 6 and the desk is 3
  (WL-MIG-03).
