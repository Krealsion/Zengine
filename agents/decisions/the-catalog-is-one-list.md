# The catalog is one list, and a live offer joins it whole or not at all

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [catalog](../workshop/catalog.md).

**Context.** The picker's population was two arrays walked twice, the compile-time kinds and the
panes offices had offered this run, and two loops are how a painter, a cursor bound and a
selection come to disagree about which row index means what. A live offer arrives from a party
this build has never met, in a message whose every field is a stranger's bytes; an office Loom
preserves is not an office this application will hold, and the substrate imposes no bound on a
role's length. A provider that could name the picker's first two rows, or buy the top of the list
by choosing a name, would make a presentation a thing a message could rewrite.

**Decision.** One vocabulary of kinds: the compile-time rows of `kPanelCatalog`, in its order,
and session-local handles minted from `kFirstRuntimeKind` up, told apart by one test. Admission
judges a descriptor whole, the stamped office first and as a view, then the two keys by the setup
file's own law, then the name and the summary, and only then copies, so an offer wrong in its
fourth field leaves nothing of its first three behind; a refresh keeps its handle, and an invalid
refresh keeps the last accepted descriptor. A runtime offer may not shadow a built-in, and two
offices offering one key are two panes. The catalog holds at most thirty-two panes, built-ins
included, in first-accepted-offer order, never sorted, and nothing holds a pointer into it.

**Alternatives considered.**
- *Tried: an owned string as the office check's argument* — the copy became the precondition of
  the check that decides whether the copy is allowed; the check takes a view, pinned by case
  `"an office longer than the key bound is delivered whole and admitted by nobody"`.
- *Argued: aliasing the catalog bound to `kMaxSetupPanes`* — refused in the constant's own words:
  one bounds what a file may name and is a promise to saved bytes, the other bounds what live
  offers may make this session retain, and spelling one as the other would let a change to either
  silently move the other.
- *Argued: sorting the runtime rows by role, name or arrival* — refused: a maker who opens
  Workshop twice with the same providers sees the same list in the same order, and a provider
  cannot buy itself the top of the list; pinned by case `"the runtime catalog is beside the
  compile-time one and never inside it"`.

**Consequences.** Thirty-two against the built-ins leaves thirty distinct runtime references,
four times the tallest picker this composition can show, and bounds what a chatty or malicious
provider can make this session hold to a few kilobytes. `Occupancy` carries a `std::string` copy
rather than a pointer into a row that may move. The setup a maker saves holds the two strings of
a reference and never a row of the catalog; a fresh Workshop starts with an empty runtime catalog
and earns every row again from a live offer.

**Laws supported.** [WL-CAT-01](../workshop/catalog.md), [WL-CAT-03](../workshop/catalog.md),
[WL-CAT-04](../workshop/catalog.md), [WL-CAT-05](../workshop/catalog.md).
