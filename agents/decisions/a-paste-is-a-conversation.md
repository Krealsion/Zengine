# A paste is a conversation

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [text-box](../workshop/text-box.md).

**Context.** The clipboard crosses the Skin seam as a pair of shapes and every text-holding
participant mirrors a copy (`235141b`). A paste then read the mirror, which is stale against
the platform's current value, and a focus or mode change between a request and its answer
could have redirected clipboard text into another box (`cc50da1`, "Stage the paste races a
mutation must redden"; `5463b64`, "Read the clipboard only when a maker asks to paste it").

**Decision.** `Session::clipboard` is one in-process mirror — written on copy and cut, said to
the process once around the chain, filled by copies heard from elsewhere without counting them
as writes, never persisted, watching no system clipboard. A paste is a request: `consume()`'s
Ctrl+V bumps `paste_requests`, Workshop names the asking draft (`paste_owner_now`), asks the
Skin's role through `loom::AskBook` (capacity 4, refuse-new), and applies the answer only if the
same owner still holds the same `draft_epoch` — for a property row also the same object and
label. Anything else discards the payload whole. A medium that answers `readable=false` falls
back to the mirror. The editor pins `doc_epoch` and `buffer.revision()` as well.

**Alternatives considered.**
- *Pasting from the mirror* — replaced; pinned by case `"QR-11: paste reads the platform
  current, not the mirror stale"`.
- *Relocating a late answer to the moved caret in the editor* — rejected: a document that
  merely moved gets `paste again`; pinned by case `"EDIT-0: a late paste answer may not land at
  a caret that has since moved"`.
- *A second ask book* — rejected (`52a6c51`, "Spend Loom's asker book instead of keeping a
  second one").
- *Persisting the clipboard* — rejected: the session keeps the desk, never the work in
  progress.
- *Pretending an unanswerability notice* — none: an ask with nobody at the Skin's role stays
  open, bounded by the book.

**Consequences.** An unsolicited clipboard text enters no box and no mirror. A `Row::resume`
draft keeps its epoch, so an extent change mid-flight does not orphan its paste. Copy-here,
paste-there stays true on a terminal through the mirror with no platform claim. The Composer, a
provider, holds the same conversation itself with its own book.

**Laws supported.** [WL-EDIT-11](../workshop/editor.md), [WL-TEXT-08](../workshop/text-box.md),
[WL-TEXT-09](../workshop/text-box.md), [WL-TEXT-10](../workshop/text-box.md).
