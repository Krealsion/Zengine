# Workshop law — the catalog

Register `WL-CAT`: the panel kinds, the built-in rows, and the runtime offers admitted beside
them. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md). What crosses
the pane seam is the protocol's law, in [`../panes.md`](../panes.md); a reference's two keys
are the setup file's law (WL-SETUP-10).

## WL-CAT-01 — A panel kind is one integer from one closed vocabulary

LAW — A panel kind is a plain integer: the compile-time kinds are `kPanelCatalog`'s rows, in its order, and a runtime handle is minted from `kFirstRuntimeKind` up; nothing else is a kind.

MEANS
- `is_runtime_kind` is the one test that tells a session-local handle from a compile-time kind;
- the kinds are deliberately unalike, and nothing in the vocabulary tells the built-ins apart.

PROVEN BY — `workshop/panel.hpp` `panel`, `kPanelCatalog`, `kPanelKinds`, `kFirstRuntimeKind`,
`is_runtime_kind`; `tests/test_workshop_panes_seam.cpp` case `"the runtime catalog is beside the
compile-time one and never inside it"`, case `"an unknown runtime reference never becomes the
Builder"`.
WHY — `agents/decisions/the-catalog-is-one-list.md`

## WL-CAT-02 — A descriptor's prose is judged whole, in bytes, before it is kept

LAW — A runtime pane's name and summary meet one owner: present, more than spaces, no control byte, at most `kMaxPaneNameLen` and `kMaxPaneSummaryLen` bytes; the refusal names the field and says bytes.

MEANS
- a name that rendered as nothing would leave a picker row a maker cannot tell from a blank line;
- nothing here counts a code point, a grapheme or a cell.

PROVEN BY — `workshop/setup.hpp` `check_pane_text`, `kMaxPaneNameLen`, `kMaxPaneSummaryLen`;
`tests/test_workshop_panes_seam.cpp` case `"a descriptor's name and summary are bounded, and a
refusal keeps nothing"`.
WHY — `agents/decisions/a-name-is-judged-in-bytes.md`

## WL-CAT-03 — Admission is atomic both ways, and a built-in cannot be shadowed

LAW — An offer is judged whole under the office Loom stamped on it: an invalid first offer adds nothing, an invalid refresh keeps the last accepted descriptor, and a refresh keeps its handle.

MEANS
- a built-in offered by whoever holds `zengine.workshop` is refused as a forgery, by name;
- two offices offering one pane key are two panes and two handles, and neither can move the other;
- an offer with no stamped office is refused and retains nothing.

PROVEN BY — `workshop/setup.hpp` `admit_pane_offer`, `Admission`;
`tests/test_workshop_panes_seam.cpp` case `"a valid offer is admitted under the office that
stamped it"`, case `"an offer with no stamped office is refused, and retains nothing"`, case `"a
runtime offer cannot shadow a built-in pane"`, case `"re-offering one reference refreshes it in
place and grows nothing"`, case `"two offices offering one pane key stay two panes, and neither
can move the other"`.
WHY — `agents/decisions/the-catalog-is-one-list.md`

## WL-CAT-04 — The catalog holds at most `kMaxPaneCatalogEntries` panes, built-ins included

LAW — Live offers may make this session hold at most `kMaxPaneCatalogEntries` distinct panes, built-ins counted; the one past the bound is refused visibly and changes nothing; a refresh is still allowed.

MEANS
- the bound is a runtime-catalog policy and deliberately not an alias of `kMaxSetupPanes`;
- it bounds what a chatty or malicious provider can make this session retain.

PROVEN BY — `workshop/panel.hpp` `kMaxPaneCatalogEntries`; `tests/test_workshop_panes_seam.cpp`
case `"the combined catalog stops at thirty-two entries, built-ins included"`, case `"a picker
population larger than its rows is windowed, not truncated"`.
WHY — `agents/decisions/the-catalog-is-one-list.md`

## WL-CAT-05 — Runtime rows keep first-accepted-offer order, and nothing points into them

LAW — The runtime catalog is first-accepted-offer order and is never sorted; the combined picker walks the compile-time rows and then these; and no consumer holds a pointer into `entries`.

MEANS
- a provider cannot buy the top of the list by choosing a name;
- a later offer may grow the vector, so a row is looked up by handle or by reference when needed.

PROVEN BY — `workshop/panel.hpp` `RuntimeCatalog`, `RuntimeCatalog::entries`,
`RuntimeCatalog::next_kind`; `workshop/setup.hpp` `combined_catalog`;
`tests/test_workshop_panes_seam.cpp` case `"the runtime catalog is beside the compile-time one
and never inside it"`, case `"re-offering one reference refreshes it in place and grows
nothing"`.
WHY — `agents/decisions/the-catalog-is-one-list.md`
