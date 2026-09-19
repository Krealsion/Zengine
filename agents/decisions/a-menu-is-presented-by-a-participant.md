# A pane's menu is presented by a participant

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws
it supports are in [pane-menu](../workshop/pane-menu.md).

**Context.** A pane's own menu was presented by the host: `ContextMenu::foreign` held the rows,
the cursor, the office and the request's number, and the host's key and press handlers walked
it and answered. It worked and could not be replaced, and two reviews found what that cost: a
click's own release defeated the keyboard grab its choice asked for, a raw click queued behind
new content chose the row that moved in, and a reloaded desktop acted on its predecessor's menu
although the code said it discarded one. The selected direction is an ordinary replaceable
presentation participant behind a narrow host connection, with Hotkeys and the Pane Manager
keeping their own operations. Fixed hosting machinery need not move.

**Decision.** Three parties, and no question with two owners. The REQUESTER asks Workshop for a
menu as today and keeps, in its image and never in reload-kept state, a record of that ask
(`pane_menu::Asked`): a choice counts only from the presenter's office, under this image's own
pending number, once, about the pane and subject asked. WORKSHOP keeps input custody and
display: it judges the ask where the menu would open, grants one menu at a time to whoever holds
`zengine.presenter` (`MenuGranted`), forwards the maker's acts numbered as acts, draws the lines
the presenter shows inside the room it granted, withdraws the menu when custody moves, and
records a choice as the continuation of the act the presenter names. It answers a requester only
when nothing was presented or no presenter can answer. The PRESENTER owns what can be presented,
how it reads, what a key or press means, when it ends and with what outcome, and it answers the
requester itself. The shipped policies, local to these consumers: a reloaded requester cancels
its predecessor's menus (no record crosses, and the host withdraws the menu when the pane is
offered again); a presenter reloaded by an image keeping `HeldMenu` hands the open menu over; a
presenter that leaves, or a holder that does not carry the menu, ends it answered by the host; a
subject that moved is judged by the requester when the choice arrives.

**Alternatives considered.**
- *Tried: the host presents (`ContextMenu::foreign`)* — replaced. Nothing could hold the office
  instead, and its answer path claimed a discard it never implemented: the review's reload probe
  opened Alpha's menu, reloaded the real desktop, chose, and Alpha opened.
- *Tried: counting a button's release as an act* — refused. The review's batched click chose
  Modify on the press and advanced the count on the release before the keyboard request
  arrived; the request was refused as late. A release completes its press; the fixed case and a
  matrix over capture, typed spelling and delivery pin it, and a newer key still defeats a grab.
- *Tried: forgetting the asker when the menu leaves the screen, and settling a refusal by office*
  — replaced. The review unloaded the presenter, closed the pane, and the ask stayed pending:
  the withdrawal's refusal found nothing to settle. A pane asking under its press's number before
  hearing the press, with the presenter killed and revived around the older withdrawal, saw that
  refusal end the NEWER menu. The host now keeps a withdrawn menu's asker and attempt span until
  its own fence has come round twice, and a refusal settles only the menu its attempt names.
- *Tried: one word for both endings (`MenuClosed{menu, false, 0}`)* — replaced. A presenter said
  it for an offer it refused, whose requester it had answered, and for a menu it did not hold,
  whose requester nobody had; the host closed the popup on both and could tell them apart in
  neither. An independent probe loaded a fresh image while a press withdrew the open menu: the
  withdrawal was delivered, the image ignored it, and the ask stayed pending — reproduced on
  both shipped images, and on the numbered one as well. `MenuReturned` is the second word now.
  `MenuClosed` means answered, and is what lets the host stop keeping who asked; a give-back
  means unanswered, and the host settles that requester, open or withdrawn, once.
- *Argued: read every unchosen close as unanswered* — refused: a refused offer's requester was
  already answered by the presenter, so it would hear a second and contrary word about its ask.
- *Argued: the presenter acknowledges each withdrawal, or the host answers every one* — refused:
  an acknowledgement a silent presenter never sends keeps its record forever, and the host
  answering would take the answer from the party that decides it. No timeout either.
- *Argued: the requester asks the presenter, which asks the host* — refused: the presenter would
  vouch for the requester's identity to the host, and the ask would be judged a hop later.
  Loom authenticates the requester where it asks, which is the host.
- *Argued: the host relays the presenter's result* — refused: the host would keep the answer
  and the lifetime record, and the presenter would be a renderer. The party that decides the
  outcome signs it; the host answers only what no presenter can.
- *Argued: the host's own chrome, room and tab menus through the presenter* — deferred. They are
  the management route that must work with no presenter, a broken one, or none loaded.
- *Argued: the pending ask in `DesktopState`* — refused for the desktop: a successor would act on
  a menu about rows its predecessor was showing. A pane that wants transfer carries it on purpose.

**Consequences.** A Workshop without a presenter refuses a pane's menu in words and keeps every
host route. An interaction a holder cannot carry ends somewhere: the seam has a word for it, and
the two shipped images say it alike. It adds no completion guarantee — a delivered withdrawal an
image simply never answers is still that presenter's silence. A presenter's lines are judged like a pane's rows and a presenter that overflows its
room loses the menu. The seam is installed, so a stranger can build a presenter as well as a
requester, and the numbered example replaces the shipped one both from a plan row and by a live
reload that keeps the open menu. A press on a presented menu names the picture the medium held,
through the same fence a pane's press does.

**Laws supported.** [WL-CTX-10](../workshop/pane-menu.md).
