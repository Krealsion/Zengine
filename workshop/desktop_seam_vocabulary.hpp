// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_DESKTOP_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_DESKTOP_SEAM_VOCABULARY_HPP

// ============================================================================================
// THE DESKTOP SEAM -- what crosses between Workshop and the weave that owns the application's
// default behaviour: which gestures the application answers above every pane, what a launch
// request means, and what stands in the empty room.
// ============================================================================================
//
// WHY THERE IS A SEAM HERE AT ALL. Until this arc, "what Ctrl+t does", "what Escape does when
// nothing more specific owns it" and "what a maker sees in the empty workspace" were three
// facts compiled into the host: a global row in `kActionCatalog`, a hard-coded line at the end
// of `on(KeyPressed)`, and a rectangle document nobody could replace. None of them is a fact
// about ROOM, FOCUS, REALIZATION or ROOTS -- the four the host keeps (VD-19) -- so none of them
// had to be the host's. They are an application's policy, and this seam is where a weave holds
// it.
//
// WHAT THE HOST STILL DECIDES, AND WHY THAT IS NOT A LOOPHOLE. A launch request names a pane;
// the host resolves it against the ONE inventory (`inventory_rows`), seats it in the room it
// owns, and answers. The desktop cannot open a pane the catalog does not hold, cannot cause an
// artifact to load, and gains no authority by asking -- exactly as a plan row asks for
// authority and confers none (VD-21). What the desktop owns is WHICH requests to make and WHEN;
// what the host owns is whether they can be performed.
//
// ⚠ AND A DESKTOP IS NOT THE FIXED ROOT. The host's authority, its recovery and its ability to
// exit belong to the process, not to this weave. A Workshop whose desktop refused to load is a
// Workshop with no application defaults and no backdrop -- which is a diagnosable state a
// console can report, not a process a maker cannot leave.

#include "surface/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// THE OFFICE THE APPLICATION-DEFAULTS WEAVE HOLDS. Spelled here because the HOST must know
/// which role to send `AppActionRequested` to, and because a stranger writing a replacement
/// desktop needs one durable name to claim.
inline constexpr const char* kDesktopRole = "zengine.desktop";

// ---- Application actions -------------------------------------------------------------------

/// WHERE IN THE ROUTING CHAIN AN APPLICATION ROW IS ANSWERED. Two values, and the fact that
/// there are exactly two is the law rather than an implementation convenience.
///
/// This is what "explicit global shortcuts have DECLARED precedence" means here: a row says
/// which of the two classes it is in, and a maker reading the hotkey view is told. Neither
/// class is "a key no pane answered" -- an unanswered key is silence, and silence settles
/// nothing (WL-ARR-15).
namespace app_precedence {
/// ABOVE EVERY MODE, and ahead of the keys crossing to a pane. What a launch binding needs:
/// `Ctrl+t` must open or focus the Terminal while a maker's hands are in Neovim, or it is not
/// a launch binding at all -- it is a key that works when nothing is happening.
///
/// ⚠ AND IT IS STILL SUPERSEDABLE BY NAME. A pane that must own the gesture says so on its own
/// row (`v2::PaneActionRow::supersedes`), and then the application row is not requestable while
/// that pane holds the keyboard. That is the accepted `kUnlessOwned` mechanism, unchanged and
/// reused: declared, by name, surviving a maker moving either row.
inline constexpr std::int64_t kAboveModes = 0;
/// LAST, AND ONLY WHERE NOTHING MORE SPECIFIC OWNED THE GESTURE. What Escape-to-deselect needs:
/// every mode, overlay and draft answers Escape with a row of its own, and a focused pane's
/// Escape is the pane's -- especially Neovim's. This class is answered exactly where the host's
/// hard-coded deselect line used to sit, so the ORDER did not change when the OWNER did.
inline constexpr std::int64_t kDefault = 1;
} // namespace app_precedence

/// ONE APPLICATION ACTION, AS THE DESKTOP DECLARES IT.
///
/// The four fields a pane row has, plus `precedence`. There is deliberately no callback, no
/// availability flag and no target: what the row DOES is the declarer's, reached by
/// `AppActionRequested` carrying the id, exactly as a pane's row is.
struct AppActionRow {
    std::string id;             ///< the durable action id, in the DECLARING office's namespace
    std::string label;          ///< what a legend prints beside the key
    std::int64_t scancode = 0;  ///< `zengine::input::scan`'s space; `kUnknown` = no default
    std::int64_t modifiers = 0; ///< `zengine::input::mod`'s bitmask
    std::int64_t precedence = app_precedence::kAboveModes; ///< one of `app_precedence`
    ZEN_SHAPE(AppActionRow, 1, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(precedence));
};

/// THE APPLICATION ACTIONS ONE OFFICE DECLARES -- judged whole, under the office Loom stamped
/// on it, by the same collision law a pane's rows meet.
///
/// ATOMIC BOTH WAYS, and the rule is `PaneActions`' rule: a declaration that fails any law
/// joins nothing and leaves the previously admitted rows exactly as they were; one that passes
/// REPLACES them. An empty `rows` is a legal declaration of nothing -- which is how a
/// replacement desktop ships with no defaults at all.
///
/// ⚠ ONLY THE OFFICE THE HOST WAS TOLD TO LISTEN TO IS ADMITTED. `kDesktopRole` is not a
/// password: it is an address, and the host's policy (the Weaver, set by the maker) decides who
/// may hold it. A second office declaring application rows is refused by name, because two
/// parties owning "what the application does above every mode" is the duplicate-authority
/// defect this seam exists to avoid.
struct AppActions {
    std::vector<AppActionRow> rows;
    ZEN_SHAPE(AppActions, 1, ZEN_FIELD(rows));
};

/// A MAKER PRESSED THE GESTURE ONE OF THE DECLARED APPLICATION ROWS ANSWERS TO -- and this is
/// the RESOLVED id, after the maker's keymap file moved the row wherever they authored it.
///
/// SENT INSTEAD OF WHATEVER THAT KEYSTROKE WOULD OTHERWISE HAVE BEEN, never beside it, and the
/// character it would have produced is swallowed exactly as the host's own printable triggers
/// are. Workshop sends it and asks nothing back, for `PaneKey`'s reason exactly.
struct AppActionRequested {
    std::string id; ///< one of the ids this office declared, as it declared it
    ZEN_SHAPE(AppActionRequested, 1, ZEN_FIELD(id));
};

/// ⭐ WORKSHOP REFUSED A DECLARATION, AND THE DECLARER IS TOLD (BL-WORK-04).
///
/// THE GAP THIS CLOSES, IN THE FOUNDER'S WORDS: "It should be able to know, so it can do so
/// gracefully." Before this shape, a provider whose `PaneActions` lost the collision law was
/// told nothing at all: Workshop named the pane and the collision on its own band, retained the
/// last accepted rows, and the provider went on believing its keys were live.
///
/// ⚠ THIS IS APPLICATION REJECTION AFTER DELIVERY, NOT LOOM DISPATCH REFUSAL. The shape
/// arrived, was decoded and was judged; `zen.DispatchRefused` answers a different question (was
/// it admitted to the bus at all) and neither substitutes for the other. A provider that hears
/// nothing was ACCEPTED -- silence here is the accept, because the refusal is the exception.
///
/// ⚠ AND IT MANDATES NOTHING. Workshop owns the fact; the provider owns its recovery policy. It
/// may re-declare on different gestures, drop the rows, tell the maker in its own room, or do
/// nothing at all. Workshop does not ask it to abandon a mode, discard a draft or choose
/// different bindings, and does not retry.
struct ActionsRefused {
    /// The pane key whose rows were refused, in the declaring office's namespace -- or EMPTY
    /// for an `AppActions` declaration, which names no pane.
    std::string pane;
    /// Workshop's own sentence, the one a maker reads on the band: which id, which gesture, and
    /// what it collided with. The declaration's own words, not a code.
    std::string refusal;
    ZEN_SHAPE(ActionsRefused, 1, ZEN_FIELD(pane), ZEN_FIELD(refusal));
};

// ---- Launching -----------------------------------------------------------------------------

/// OPEN THIS PANE, OR PUT THE MAKER IN IT IF IT IS ALREADY THERE.
///
/// `office` and `pane` are a `PaneRef`'s two durable halves -- the role whose holder offers the
/// pane, and the pane key in that office's namespace. They are the setup file's own two keys,
/// so a launcher names a pane exactly as a saved desk does.
///
/// ⚠ LAUNCHING IS NOT TOGGLING, and the difference is the whole point of the shape's name. A
/// request for a pane that is already on the desk FOCUSES it: it is selected and given the
/// keyboard. It does not close it, does not remove it from the setup, and above all does not
/// unload its provider -- a maker who presses the Terminal key twice wanted the Terminal twice.
///
/// ⚠ AND IT LOADS NOTHING. A pane no provider has offered is not launchable, and the answer
/// says which of the two honest reasons applies: the catalog has no such pane, or it has one
/// whose provider is not currently there. Neither is a reason to go and find an artifact.
struct PaneLaunchRequested {
    std::string office;
    std::string pane;
    ZEN_SHAPE(PaneLaunchRequested, 1, ZEN_FIELD(office), ZEN_FIELD(pane));
};

/// WHAT THE LAUNCH CAME TO -- addressed back to whoever asked.
///
/// `opened` and `focused` are two facts and not one: a pane that was closed and is now on the
/// desk was OPENED, one that was already there was FOCUSED, and a request that could not be
/// performed is neither with `refusal` saying why in the host's own words.
struct PaneLaunchAnswered {
    std::string office;
    std::string pane;
    bool opened = false;  ///< it was not participating, and now is
    bool focused = false; ///< the keyboard and the selection are on it now
    std::string refusal;  ///< empty iff the request was performed
    ZEN_SHAPE(PaneLaunchAnswered, 1, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(opened),
              ZEN_FIELD(focused), ZEN_FIELD(refusal));
};

/// PUT DOWN WHATEVER THE MAKER HAS PICKED UP -- the host operation the shipped desktop asks for
/// when its Escape row is requested, and the first member of the "ask the host to do the
/// application-default thing" family.
///
/// ⚠ WHY THE DESKTOP DOES NOT JUST DO IT. The selection is `Panels::selected` and it is the
/// host's: a pane is selected, arranged, covered and given the keys by the party that owns the
/// room. What the desktop owns is WHEN putting it down happens -- on which gesture, in which
/// precedence class, or not at all. So the desktop owns the policy and the host owns the act,
/// which is the same split `PaneLaunchRequested` makes one operation over.
///
/// ⚠ AND IT MUST ECHO THE ASK IT ANSWERS. `answering` is carried in Loom's envelope, not in
/// this shape: it is the number this host minted for the one keystroke that requested the
/// desktop's row, and an answer that echoes nothing (zero) or echoes a keystroke that is over
/// acts on nothing. That is `PaneEscapeUnspent`'s accepted correlation repair, unchanged, moved
/// to the party that now owns the policy -- because the failure it prevents is unchanged too: a
/// second Escape leaves the maker's latest gesture all over again, and the first Escape's reply
/// must not be spent on it.
///
/// THE REST OF THE FAMILY IS NAMED AND NOT BUILT (a configuration seam): which gesture selects,
/// which panes are selectable at all, which take the keyboard, whether a pane intercepts the
/// pointer or is clicked through, and where a launched pane is placed. Each is a distinct
/// choice with a distinct owner; this journey needed one of them.
struct DeselectRequested {
    ZEN_SHAPE(DeselectRequested, 1);
};

// ---- The inventory the launcher presents ----------------------------------------------------

/// ONE PANE, AS THE ONE INVENTORY HAS IT.
///
/// ⚠ `open` AND `available` ARE DIFFERENT QUESTIONS AND THE SEAM KEEPS THEM APART. `open` is
/// the maker's own authored participation -- whether this pane is on the desk they are looking
/// at. `available` is whether anything currently holds the office that offers it. A pane can be
/// authored-open and unavailable (its provider refused to load, or went away), which is exactly
/// the state a maker needs explained rather than a row that silently disappears.
struct InventoryPane {
    std::string office;
    std::string pane;
    std::string name;    ///< the two lines a maker reads, as the offering provider wrote them
    std::string summary; ///
    bool open = false;      ///< participating in the live desk
    bool available = false; ///< some holder of `office` currently offers this pane
    bool waiting = false;   ///< authored open, but this screen has no room to seat it
    ZEN_SHAPE(InventoryPane, 1, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(name),
              ZEN_FIELD(summary), ZEN_FIELD(open), ZEN_FIELD(available), ZEN_FIELD(waiting));
};

/// THE WHOLE INVENTORY, SAID BY THE HOST WHENEVER IT CHANGES.
///
/// PUBLISHED `to_any` AND NOT ADDRESSED, for `StandingConditions`' reason: which weave presents
/// this is the load plan's business, and a host that addressed one would be a host with a
/// launcher compiled into it again.
///
/// ⚠ REPLACED WHOLE, NEVER MERGED. A row that stopped being returned stopped being in the
/// inventory. Half of a previous reading and half of this one would be a picture of a moment
/// that never existed.
///
/// ⚠ AND THIS IS NOT A SECOND INVENTORY. It is `inventory_rows`' answer, said out loud. The
/// host holds the one population; this shape is a reading of it, and a presenter that kept a
/// copy and edited it would be the second owner the arc exists to avoid.
struct PaneInventory {
    std::vector<InventoryPane> panes;
    ZEN_SHAPE(PaneInventory, 1, ZEN_FIELD(panes));
};

// ---- The backdrop ---------------------------------------------------------------------------

/// WHAT STANDS IN THE EMPTY ROOM -- the rows Workshop paints in the workspace, behind every
/// pane, where the prototype object canvas used to draw its rectangles.
///
/// ⚠ IT IS NOT A PANE, AND THAT IS DELIBERATE. A pane is seated, ordered, covered, arranged and
/// removable; the room's floor is none of those. What a maker sees when their desk is empty is
/// a property OF THE ROOM, and the room is the host's -- so the host paints it, from rows the
/// desktop weave says. The desktop owns the words; Workshop owns the wall.
///
/// ⚠ AND IT TAKES NO INPUT. There is no press shape, no key shape and no room grant for the
/// backdrop: a press in the empty workspace means what it has always meant (the selection is
/// put down), and the desktop's gestures are its declared application rows. A backdrop that
/// answered presses would be a pane that had skipped seating.
///
/// Rows are refused whole rather than truncated if they exceed `kMaxBackdropRows`, for
/// `PaneContent`'s reason one surface over.
struct DesktopFace {
    std::vector<surface::SurfaceTextRow> rows;
    ZEN_SHAPE(DesktopFace, 1, ZEN_FIELD(rows));
};

/// HOW MANY ROWS A BACKDROP MAY SAY. A room-floor policy, deliberately its own constant:
/// it bounds what a chatty desktop can make the host retain and paint.
inline constexpr std::size_t kMaxBackdropRows = 64;

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_DESKTOP_SEAM_VOCABULARY_HPP
