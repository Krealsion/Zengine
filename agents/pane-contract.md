# Agent law — The pane contract (router)

Routed behind [`../AGENTS.md`](../AGENTS.md), for tasks touching `pane/`: the contract between a
pane and the host that arranges it -- what crosses, which way, and what each side may trust. The
law lives in the registers under [`pane-contract/`](pane-contract/): one law per `##`, a
`PANE-<AREA>-<NN>` id that is permanent, a `LAW` of one line, `MEANS`, `DOES NOT MEAN` and a `WHY`
naming one decision record under `decisions/`, and its proof, naming the owner identifiers and the
exact witness cases, under `### <id>` in the appendix that ends the register, `## Proven by`.
Disagreements follow
[intent, evidence, and architectural fit](../AGENTS.md#intent-evidence-and-architectural-fit), not
an automatic ranking of tests over law. The whole of the law in one screen is
`grep -h '^LAW' agents/pane-contract/*.md`.

Substrate law is the Loom's and applies whole: a pane is an ordinary participating weave, and
whom it trusts is Loom's role authorship. A pane's law states the pane's half and what any host
owes it; what Workshop does as the host -- its catalog, its keys, its menus, its desk -- is
Workshop's law, routed by [`workshop.md`](workshop.md). The prose of both halves, and of the shipped
tools that arrive through the seam, is [`panes.md`](panes.md) until it is a register.

## Where the law is

| the task touches… | read |
|---|---|
| the offer, the area, the keys, a declared row's id, a menu's answer, the quit's answer | [seam](pane-contract/seam.md) `PANE-SEAM` |

**Where the code is.** `pane/vocabulary.hpp` (the shapes a pane and its host exchange),
`pane/drawing.hpp` and `pane/canvas_drawing.hpp` (the helpers a pane draws with, and its area's
name), `pane/facets.hpp` (the facets a pane may be seated on), `pane/menu.hpp` (a requester's record
of its menu), `pane/settings.hpp` and `pane/settings_taken.hpp` (the settings a pane is handed),
`pane/operation.hpp` and `pane/carry.hpp` (approval and carry), `pane/presenter_vocabulary.hpp` (the
presenter's seam). The library is `zengine::pane` ([`pane/AGENTS.md`](../pane/AGENTS.md)).

**Where a case goes.** With the pane or the host whose half it witnesses: the seam's own suite is
`workshop_panes_seam` (`tests/test_workshop_panes_seam.cpp`), and a pane's half lives in its
suite, `workshop_panes_<part>`.

## Ongoing rules

The seven rules of [`workshop.md`](workshop.md#ongoing-rules) apply to these registers unchanged,
with the family spelled `PANE` and the pointer `// PANE-… -- agents/pane-contract/<file>`.
