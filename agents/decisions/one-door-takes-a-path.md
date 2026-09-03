# One door takes a path

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [editor](../workshop/editor.md).

**Context.** The editor's only door was the Builder's chosen recipe: a maker could not open a
file they could see (`5a302ae`, "Workshop: choose what to edit -- the Files pane, one editor
door, one meaning for a relative source"). `EditorState::recipe` was write-only provenance.

**Decision.** `open_source(path, mail)` is the one door and it takes a path: normalize against
the project, same-path reveal, dirty refusal, bounded read, `source_in`, trial-seat, install with
`doc_epoch++` and a viewport reset, focus and sentence. Files hands it a row's path;
`edit_source` keeps only the Builder half — which recipe, the `cmake_target` refusal, the host's
answer over the completed catalog. Identity is a normalized spelling, not a filesystem object.
Where the overlay stack has no slot left the door refuses and names the remedy.

**Alternatives considered.**
- *Canonicalizing paths* — rejected: Windows case-folding and hard links remain named
  residuals, and claiming otherwise would need a filesystem question on every open.
- *Keeping acquisition provenance on `EditorState`* — removed with the factoring: the editor
  owns the document it has open, not the reason somebody asked for it.
- *A second door for the browser* — rejected; pinned by case `"EDIT-1: Builder and the browser
  open ONE document, however the path is spelled"`.

**Consequences.** `a.cpp` and `./a.cpp` cannot become two documents, and two referrers cannot
disagree about which file the dirty refusal is protecting. `HostContext::recipe_source` is a
function spent at the gesture and stored nowhere. A third referrer is a call, not a policy.

**Laws supported.** [WL-EDIT-05](../workshop/editor.md), [WL-EDIT-06](../workshop/editor.md),
[WL-EDIT-13](../workshop/editor.md).
