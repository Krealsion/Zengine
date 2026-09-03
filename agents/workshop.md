# Agent law — Workshop (router)

Routed behind [`../AGENTS.md`](../AGENTS.md), for tasks touching `workshop/` and `component/`.
Workshop's law lives in the registers under [`workshop/`](workshop/): one law per `##`, a
`WL-<AREA>-<NN>` id that is permanent, a `LAW` of one line, `MEANS`, `DOES NOT MEAN`, a
`PROVEN BY` naming the owner identifiers and the exact witness cases, and a `WHY` naming one
decision record under `decisions/`. Authority when they disagree: tests > code > register >
decision record. The whole of the law in one screen is `grep -h '^LAW' agents/workshop/*.md`.

What crosses the pane seam is the protocol's law, in [`panes.md`](panes.md); the Surface
vocabulary is [`surface.md`](surface.md). Workshop's registers hold Workshop's side only.

## Where the law is

| the task touches… | read |
|---|---|
| the composition in cells, the reserved column, the fine lattice, the unit a face reports | [geometry](workshop/geometry.md) `WL-GEO` · [chrome](workshop/chrome.md) `WL-CHROME` |
| the setup file, a pane's default and the maker's override, places and slots, the seven states, the plane sequence, the selection lift, arranging a pane | [setup-file](workshop/setup-file.md) `WL-SETUP` · [panes-and-windows](workshop/panes-and-windows.md) `WL-PANE` · [planes](workshop/planes.md) `WL-FRONT` · [arrangement](workshop/arrangement.md) `WL-ARR` |
| the panel catalog: the kinds, the built-in rows, a live offer's admission and its bounds | [catalog](workshop/catalog.md) `WL-CAT` |
| a press, a double-click, reading past a fitted row, what a routing bool means, the pointer order | [pointer](workshop/pointer.md) `WL-PTR` · [press-chain](workshop/press-chain.md) `WL-PRESS` |
| a hotkey, the keymap file, the hotkey view, where the keys go | [keyboard](workshop/keyboard.md) `WL-KEY` · [focus](workshop/focus.md) `WL-FOCUS` |
| an editable line, the TextBox, the clipboard, a paste | [text-box](workshop/text-box.md) `WL-TEXT` |
| the Info panel's body, its controls, its grounds | [info-body](workshop/info-body.md) `WL-INFO` · [info-controls](workshop/info-controls.md) `WL-CTRL` |
| semantic text in a panel, the Builder's rows, the foot band, a name on material | [regions](workshop/regions.md) `WL-RGN` |
| the source editor, the project anchor and recipes, the Files pane, paths, marks, roots | [editor](workshop/editor.md) `WL-EDIT` · [project](workshop/project.md) `WL-PROJ` · [files](workshop/files.md) `WL-FILES` |
| the contextual surface, a condition versus an utterance | [contextual](workshop/contextual.md) `WL-CTX` · [attention](workshop/attention.md) `WL-ATTN` |
| several desks, the tab run, the durable files and the session, an old session's conversion | [layouts](workshop/layouts.md) `WL-LAYOUT` · [tab-run](workshop/tab-run.md) `WL-TAB` · [session](workshop/session.md) `WL-SESSION` · [migration](workshop/migration.md) `WL-MIG` |
| the Pane Manager, a pane a maker made from data | [pane-manager](workshop/pane-manager.md) `WL-PED` · [maker-pane](workshop/maker-pane.md) `WL-MAKER` |
| the maker's document, its operations and its file | [document](workshop/document.md) · [document-file](workshop/document-file.md) `WL-DOC` |
| the Terminal overlay, its pane, its completion | [terminal](workshop/terminal.md) `WL-TERM` |

**Where a case goes.** Workshop's tests are seven suites, one per area — document, screen,
panels, panes, persistence, load, editor — and a new case belongs to the one whose subject it
proves; [`verification.md`](verification.md) names them.

## Ongoing rules

1. Routed law documents are registers in the `timer-laws` form: one law per `##`, LAW one line,
   MEANS at most 3, DOES NOT MEAN at most 2, PROVEN BY naming owner identifiers and exact witness
   cases, WHY naming one decision record. Each register at most 16 KB; this router at most 8 KB.
2. A new law is a new entry under its owner. Never a bullet appended to a neighbouring entry.
3. Law text lives in the register only. Source carries a one-line pointer,
   `// WL-… -- agents/workshop/<file>`; rationale lives in a decision record. A comment block
   that argues is a decision record in the wrong place.
4. A phase that edits a `TEST_CASE` named in any PROVEN BY re-verifies every law naming it, in
   the same commit, and lists the ids re-verified in the commit message. The evidence trail is
   Git history.
5. Tests > code > register > decision. Fix downward, never upward: a register entry that
   contradicts a passing test is the thing that is wrong.
6. Law ids are permanent; a retired law keeps its number and one line.
7. `witness: none` is written where it is true and repeated under the register's `## Do not
   assume`; a law witnessed except one clause writes `UNWITNESSED — <clause>` after PROVEN BY,
   and that debt is counted, repeated and reciprocated the same way. Lowering the count of
   witnessed laws to make a deletion pass is the thing this rule forbids.

## Do not assume

- That a tag inside a `TEST_CASE` literal is a citation. It is a fossil. Retiring the convention
  means no new tags; rename a test only when touching it for another reason, and treat the rename
  as a register edit. What a retired tag covered is found with `git log -S'<TAG>'`; tags are not
  reintroduced into registers, decision records or source comments.
- That docking exists — it is still absent and still refused.
- That a seam law is stated here. A room grant, a pressed row, a key or a wheel crossing to a
  provider is the protocol's law; a Workshop register states only Workshop's conformance.
- That a law without a witness is hidden. Each register lists its own under `## Do not assume`.
