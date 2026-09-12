# Workshop law — managed opening

Register `WL-OPEN`: one open operation, coordinated by the opening manager, published jointly
by the Editor and the desk. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). Document custody is [`editor.md`](editor.md); what crosses
the seam is the protocol's law in [`../panes.md`](../panes.md); the substrate's law is Loom's
joint-publication reference page, never restated here.

## WL-OPEN-01 — One manager owns the open operation, and nothing else

LAW — The native `OpeningManager` owns one open intent, its exact participants, stage, attempts, supersession and outcome; the Editor owns the document, Workshop the presentation, Loom the publication.

MEANS
- the manager holds no document, no rows, no room, no focus and no pointer into either owner;
- it asks the desk for a trial and an admission, the Editor for a preparation, and commits;
- Files and the Builder ask `zengine.opening`; `zengine.editor` relays with the asker's right.

DOES NOT MEAN
- that ordinary panes acquire this ceremony: a pane's reveal is still its own ask.

PROVEN BY — `workshop/opening.hpp` `OpeningManager`, `OpeningState`;
`workshop/open_seam_vocabulary.hpp` `kOpeningRole`, `PresentationTrialRequested`,
`PrepareSourceRequested`, `PresentationAdmitRequested`; `workshop/weave_opening.cpp` `ask`,
`answers_flight`;
`workshop/workshop.cpp` `mount_in_office`; `files/files.cpp` `open`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W70: a managed open has one commitment
-- the published claims, the pane's reads, its snapshot and the desk agree at it, and A is
current before it"`; `tests/test_workshop_panes_files.cpp` case `"FILES-WEAVE: Return on a
source opens it in the Editor, through the one door"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-OPEN-02 — Publication and application are two facts, and `accepted` is both

LAW — `commit_joint` publishes both claims in one protected step; `SourceOpened.accepted` means published AND applied by both owners, read from the bus's record; Declined, Failed and Lost name the owner.

MEANS
- pending application stays pending: the manager answers only from `zen.JointApplied`'s record;
- after publication a failed application rolls nothing back and claims nothing preserved;
- Declined is a functioning owner's answer; Failed holds the owner until reloaded or removed.

PROVEN BY — `workshop/weave_opening.cpp` `on(PresentationAdmitted)`, `on(JointApplied)`,
`settle`, `unapplied_words`; `workshop/weave_managed.cpp` `on_claim_published`;
`editor-pane/pane.cpp` `on_claim_published`, `activate`; `tests/test_workshop_panes_editor.cpp`
case `"EDIT-W78: a loaded owner that cannot apply the published claim is held, named, and
reloaded"`, case `"EDIT-W80: the real desk, shown a presentation it holds no trial for,
answers that it did not apply it -- Declined, not held, named, and re-claiming its own
truth"`, case `"EDIT-W79: the real Editor, held behind a publication its image could not
apply, is reloaded into the normal image -- the successor keeps A, and the record says B was
never applied"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-OPEN-03 — Before the commitment A is current, and admitted input invalidates B

LAW — Until the commitment A stays readable, editable and current at every door; input routed to the pane or applied to A moves a claim and aborts the preparation; nothing is held, dropped or retargeted.

MEANS
- a caret key counts: the desk's `routed` count moves for every input it routes to the pane;
- a refused open preserves A and its intervening work with no setup or focus change;
- a paste still arriving refuses the open through either door; its answer lands in A.

DOES NOT MEAN
- that the 256-gesture hold returns under a larger cap: there is no hold.

PROVEN BY — `workshop/weave_managed.cpp` `note_routed`, `derive_presentation`,
`mirror_presentation`; `workshop/weave_external.cpp` `external_key`, `external_text`;
`editor-pane/pane.cpp` `claim_document`, `after_delivery`, `judge_source`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W71: legitimate A input while B is being
arranged is admitted to A, and B is refused without moving the desk"`, case `"EDIT-W72: more
than 256 ordinary events across an opening, from three producers, are admitted in order with
nothing held and nothing dropped"`, case `"EDIT-W58: a clipboard answer refuses the open
wherever it lands, and A keeps its paste"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-OPEN-04 — One live opening; a newer request supersedes a preparation, never a commitment

LAW — A newer request cancels a Preparing flight and tells its requester it was superseded; a committed flight awaiting application is not superseded, and the newer request is refused in words.

MEANS
- there is no queue of intents and no retry: the refused requester asks again;
- a resize, setup change or owner replacement before the commitment aborts it at the bus.

PROVEN BY — `workshop/weave_opening.cpp` `on(OpenSourceRequested)`, `on(JointEnded)`,
`refusal_of`; `tests/test_workshop_panes_editor.cpp` case `"EDIT-W73: a competing open
through the OLD door while B is being arranged supersedes it, the stale preparation cannot
commit, and a later setup change survives"`, case `"EDIT-W67: room lost before the
commitment refuses the open, and nothing is authored or moved"`, case `"EDIT-W74: a real
reload or removal of the Editor at queued intervals of an open cannot commit a stale
preparation, keeps custody, and reclaims what the operation held"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-OPEN-05 — Delivered silence stays pending, bounded and inspectable

LAW — No timeout, retry or forced recovery: an owner that never answers leaves the one flight pending, its operation, requester, stage, awaited office and attempt readable in `OpeningState` and on the desk.

MEANS
- queue emptiness and elapsed time prove nothing; the next request supersedes it;
- unrelated panes and the picker keep working while it stands.

PROVEN BY — `workshop/opening.hpp` `OpeningState::stage`, `OpeningState::awaiting`,
`OpeningState::attempt`; `workshop/weave_opening.cpp` `progress`;
`workshop/weave_managed.cpp` `on(ManagedOpenProgress)`; `tests/test_workshop_panes_editor.cpp`
case `"EDIT-W75: a silent or failed preparation stays pending and inspectable, a lost
terminal answer undoes nothing, and no forged authority or attempt decides anything"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-OPEN-06 — Records are released when consumed; at most one Failed record is retained

LAW — The manager releases each record it consumed and retains at most one, a commitment an owner could not apply, for its repair's late word; every newer terminal outcome that takes the result retires it.

MEANS
- a repair's late word about a retired record consults nothing and decides nothing;
- releasing a record does not lift the claimant's hold and does not repair it;
- a repair issues no second terminal answer to the old requester.

PROVEN BY — `workshop/opening.hpp` `OpeningState::retained`; `workshop/weave_opening.cpp`
`settle`, `retire`, `release_retained`, `on(JointApplied)`;
`tests/test_workshop_panes_opening.cpp` case `"OPEN-W1: a newer immediate refusal retires the
older retained repair record -- the repair's late word cannot overwrite it, the claimant's
hold is untouched, and the freed slot lets the next open take"`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W81: the real manager's outcome survives
an unrelated coordination begun on the same bus before its application notice was consumed"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-OPEN-07 — A refusal is Loom's word about an exact attempt, or the manager's immediate one

LAW — Each requester keeps its ticket and clears only the ask whose exact attempt Loom's authenticated `zen.DispatchRefused` names; nothing queued is refused at once; forged and late notices settle nothing.

MEANS
- no document-only fallback, and no standalone mode inferred from a refusal;
- a manager with no Editor to bind refuses at once, in words: never silence;
- handler failure and delivered silence are not dispatch refusals.

PROVEN BY — `files/files.cpp` `on(DispatchRefused)`, `Ask::attempt`; `builder-pane/pane.cpp`
`on(DispatchRefused)`, `edit_source`; `editor-pane/pane.cpp` `on(DispatchRefused)`, `Relay`;
`workshop/weave_opening.cpp` `on(DispatchRefused)`; `tests/test_workshop_panes_files.cpp`
case `"FILES-WEAVE: an open refused at dispatch is said by that exact attempt, and a fresh
attempt takes once an opening office is present"`, case `"FILES-WEAVE: a forged refusal
naming the pane's own live attempt settles nothing, and the open completes"`;
`tests/test_workshop_panes_builder.cpp` case `"BLD-WEAVE: each of e's two asks refused at
dispatch is said by that attempt and stage, and a fresh e takes once the office is
present"`, case `"BLD-WEAVE: a forged refusal naming the pane's own live attempt settles
nothing at either stage, and the open completes"`; `tests/test_workshop_panes_editor.cpp`
case `"EDIT-W77: the old door still opens and shows, or refuses truthfully, by a kept answer
right"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-OPEN-08 — The authority is the host's, minted for the manager's exact incarnation

LAW — The host mints `JointAuthority` for the mounted manager over the two offices; it names that incarnation, a replaced manager's retained copy is refused `NotOperator`, and only the host re-mints.

MEANS
- no operation id, role spelling, correlation or payload is authority;
- manager hot reload is not supported; the obligation is stated for a host that ever does.

PROVEN BY — `workshop/workshop.cpp` `mint_joint_authority`, `kOpeningRole`;
`workshop/opening.hpp` `OpeningManager::set_authority`; `tests/test_workshop_panes_opening.cpp`
case `"OPEN-W2: the
manager's authority names its exact incarnation -- a swapped manager's retained capability is
refused in words, and the host authorizes the successor by minting again"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-OPEN-09 — The host's turn attributes a native showing failure from the record alone

LAW — A native owner's showing that throws is recorded by Loom and re-raised at the host's turn; `serve_until_idle` names the held owner from the turn's last tap fact and serves on; other throws propagate.

MEANS
- the desk says which owner is held; the repair is that owner's reload or removal;
- an owner held since an earlier turn explains no later exception.

PROVEN BY — `workshop/host_pump.hpp` `serve_until_idle`, `ServedTurn`;
`workshop/host_pump.cpp` `TurnTap`, `LastFact`; `tests/test_workshop_panes_opening.cpp` case
`"OPEN-W3: the host's turn attributes a native showing failure from Loom's record and serves
on, and an exception the record does not explain propagates"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## Do not assume

- That a refused open through the Editor's own office ever installed a document alone: the
  relay is refused in words or answered by the manager (WL-OPEN-07).
- That a live SDL or terminal run was measured for every law here; the register's witnesses
  are model rigs over the real weaves, and the live media are the phase's witness runs.
