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

// ⭐ A VERDICT ON AN `AppActions` DECLARATION IS THE PANE PROTOCOL'S `ActionsJudged`, answered to
// the declaration, and a later withdrawal is `ActionsWithdrawn` (workshop/pane_vocabulary.hpp):
// one pair of shapes for both declaration surfaces, so BL-WORK-04 is not owed twice. An earlier
// `ActionsRefused` on this seam named no attempt and no declaration; it was never published.

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

/// TAKE THIS PANE OFF THE DESK -- deliberate participation removal, the one duty of the retired
/// picker's toggle that launching does not do.
///
/// ⚠ CLOSING IS NOT UNLOADING, AND IT IS NOT THE OTHER HALF OF A TOGGLE. The pane's row leaves
/// the live desk and its presentation leaves the screen; its provider stays loaded and keeps
/// everything it holds -- an Editor's unsaved source, a Terminal's history -- so a launch finds
/// it as it was. A pane that is not on the desk is refused in words: a close never opens one.
struct PaneCloseRequested {
    std::string office;
    std::string pane;
    ZEN_SHAPE(PaneCloseRequested, 1, ZEN_FIELD(office), ZEN_FIELD(pane));
};

/// WHAT THE CLOSE CAME TO -- addressed back to whoever asked.
struct PaneCloseAnswered {
    std::string office;
    std::string pane;
    bool closed = false; ///< it was on the desk, and now is not
    std::string refusal; ///< empty iff it was closed
    ZEN_SHAPE(PaneCloseAnswered, 1, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(closed),
              ZEN_FIELD(refusal));
};

/// SHOW THIS PANE IF IT IS HIDDEN, HIDE IT IF IT IS SHOWN -- a strict visibility toggle, judged
/// by the host against the desk AS IT IS when the request is handled: on the desk (seated or
/// waiting for room) -> closed; not on the desk -> opened and focused. Asked by a presenter
/// whose own copy of the inventory may be a reading behind (a queued toggle, a stale snapshot);
/// the host's answer says which of the two it did, so a toggle never silently means the other.
struct PaneToggleRequested {
    std::string office;
    std::string pane;
    ZEN_SHAPE(PaneToggleRequested, 1, ZEN_FIELD(office), ZEN_FIELD(pane));
};

/// WHAT THE TOGGLE CAME TO: exactly one of `opened` (and then focused) or `closed`, or a refusal
/// in the launch or close door's own words.
struct PaneToggleAnswered {
    std::string office;
    std::string pane;
    bool opened = false;
    bool closed = false;
    std::string refusal;
    ZEN_SHAPE(PaneToggleAnswered, 1, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(opened),
              ZEN_FIELD(closed), ZEN_FIELD(refusal));
};

// ---- Editing a binding ----------------------------------------------------------------------

/// THE OPERATIONS AN EDIT MAY ASK FOR, on one action id's AUTHORED rows. `set` makes the gesture
/// the id's only key; `add` appends it (authoring the declared default beside it when the file
/// said nothing yet); `remove` takes it out, and removing the last leaves the id DISABLED --
/// never a silent fall-back to a default the maker just took away; `disable` binds `none`;
/// `reset` removes every authored row so the declared default stands. `set_spelled`,
/// `add_spelled` and `remove_spelled` carry the gesture as `text`, in the keymap file's own
/// grammar -- for a chord no pane can capture (one answered above every mode), and for a key a
/// presenter knows only by the spelling the host showed it.
namespace keymap_edit {
inline constexpr std::int64_t kSet = 1;
inline constexpr std::int64_t kAdd = 2;
inline constexpr std::int64_t kRemove = 3;
inline constexpr std::int64_t kDisable = 4;
inline constexpr std::int64_t kReset = 5;
inline constexpr std::int64_t kSetSpelled = 6;
inline constexpr std::int64_t kAddSpelled = 7;
inline constexpr std::int64_t kRemoveSpelled = 8;
} // namespace keymap_edit

/// CHANGE HOW ONE ACTION IS REQUESTED. Asked by an office (the Hotkeys pane, or any presenter);
/// the host builds the CANDIDATE authored list and judges it exactly as a file is judged at load
/// -- grammar, the walls, the collision law over the host, application and every pane's rows in
/// force -- and a conflict anywhere refuses the edit with that law's own sentence, changing
/// nothing: not the live map, not the file, not any pane's rows. An accepted edit is applied
/// live, then written; the answer tells the two apart.
struct KeymapEditRequested {
    std::string action;         ///< the durable id a keymap file names
    std::int64_t op = 0;        ///< one of `keymap_edit`
    std::int64_t scancode = 0;  ///< `input::scan`'s space, for set/add/remove
    std::int64_t modifiers = 0; ///< `input::mod`'s bitmask
    std::string text;           ///< the spelled gesture, for set_spelled/add_spelled
    ZEN_SHAPE(KeymapEditRequested, 1, ZEN_FIELD(action), ZEN_FIELD(op), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(text));
};

/// WHAT THE EDIT CAME TO, answered to the asker on the delivery that asked. `accepted`: the
/// candidate passed and is the live map now (`applied`). `written`: the file holds it; when it
/// does not, `file_refusal` says why -- an isolated run, a file refused at load, a write that
/// failed, or a file changed by another hand since this host read it -- and the live change
/// stands for this run only. `sentence` is what the band says, written to stand alone.
struct KeymapEditAnswered {
    std::string action;
    bool accepted = false;
    std::string refusal;
    bool applied = false;
    bool written = false;
    std::string file_refusal;
    std::string sentence;
    ZEN_SHAPE(KeymapEditAnswered, 1, ZEN_FIELD(action), ZEN_FIELD(accepted), ZEN_FIELD(refusal),
              ZEN_FIELD(applied), ZEN_FIELD(written), ZEN_FIELD(file_refusal),
              ZEN_FIELD(sentence));
};

// ---- The maker's own pane, through the host's doors ------------------------------------------

/// THE PANE CREATOR'S THREE ACTS ON THE ONE OPEN DEFINITION (WL-MAKER-08): make a pane under a
/// name and put it on the desk, write the open definition to its pane file, or put it back to
/// what that file holds.
namespace maker_pane_act {
inline constexpr std::int64_t kCreate = 1;
inline constexpr std::int64_t kSave = 2;
inline constexpr std::int64_t kDiscard = 3;
} // namespace maker_pane_act

/// THE ACTION IDS A PRESENTER OF THE CREATOR DECLARES FOR THE THREE, spelled here because two
/// parties say them: the presenter declares them as its own rows, and the host names their keys
/// in the sentences it says about the maker's pane (a quit it refuses, a save with no pane). They
/// are the ids the host's Pane Manager declared, so a maker's authored override still finds them.
inline constexpr const char* kCreatorNewId = "pane-creator.new";
inline constexpr const char* kCreatorSaveId = "pane-creator.save";
inline constexpr const char* kCreatorDiscardId = "pane-creator.discard";
/// ...AND THE NAME LINE'S TWO, which the presenter declares while a name is being typed.
inline constexpr const char* kCreatorNameId = "pane-creator.name";
inline constexpr const char* kCreatorCancelId = "pane-creator.cancel";

/// ASK THE HOST FOR ONE OF THE CREATOR'S ACTS. `name` is read for `kCreate` and ignored otherwise.
///
/// ⚠ THE DEFINITION IS THE HOST'S, AND SO IS EVERY REFUSAL. A presenter holds the line a name is
/// typed into and the keys; the host holds the one open definition, judges the name, refuses a
/// new pane over unsaved edits and a save with no file, and says what it did in its own words.
struct MakerPaneRequested {
    std::int64_t act = 0;
    std::string name;
    ZEN_SHAPE(MakerPaneRequested, 1, ZEN_FIELD(act), ZEN_FIELD(name));
};

/// WHAT THE ACT CAME TO -- addressed back to whoever asked, in the sentence the band says.
///
/// ⚠ IT ANSWERS ONE ASK, UNDER THAT ASK'S NUMBER. A presenter reads it against its record of the
/// act it is waiting on, never against the latest number it has sent: its pastes and its launches
/// are asks too, and a newer one is no verdict on this (WL-MAKER-14).
struct MakerPaneAnswered {
    std::int64_t act = 0;
    bool accepted = false; ///< the host did what was asked (a discard with nothing to discard too)
    std::string said;      ///< the host's own sentence: what was done, or why not
    ZEN_SHAPE(MakerPaneAnswered, 1, ZEN_FIELD(act), ZEN_FIELD(accepted), ZEN_FIELD(said));
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
/// the state a maker needs explained rather than a row that silently disappears. `pending` is
/// the run still owing it: the plan row that loads its office has not settled, so the pane is
/// not here YET -- a state, not the verdict `available` false would otherwise read as.
struct InventoryPane {
    std::string office;
    std::string pane;
    std::string name;    ///< the two lines a maker reads, as the offering provider wrote them
    std::string summary; ///
    bool open = false;      ///< participating in the live desk
    bool available = false; ///< some holder of `office` currently offers this pane
    bool waiting = false;   ///< authored open, but this screen has no room to seat it
    bool pending = false;   ///< not offered YET: the plan row loading `office` has not settled
    ZEN_SHAPE(InventoryPane, 1, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(name),
              ZEN_FIELD(summary), ZEN_FIELD(open), ZEN_FIELD(available), ZEN_FIELD(waiting),
              ZEN_FIELD(pending));
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

/// TELL ME THE INVENTORY AS IT IS NOW -- asked by a presenter that has just arrived (a new or a
/// replaced incarnation), and answered with `PaneInventory` to that incarnation alone.
///
/// ⚠ WHY A QUESTION AND NOT ONLY THE PUBLICATION. The publication is said when the reading
/// CHANGES; a presenter that arrives while nothing changes -- a desktop reloaded in place, the
/// inventory exactly as it was -- would otherwise wait for an unrelated change to learn a fact
/// the owner already holds. Asked as an office, answered once, and the answer is a reading of the
/// one inventory, not a second copy of it.
struct PaneInventoryRequested {
    ZEN_SHAPE(PaneInventoryRequested, 1);
};

// ---- The effective keymap, said out loud ----------------------------------------------------

/// ONE BINDING AS IT IS IN FORCE: which action, what a legend calls it, the gesture that
/// requests it now in the keymap file's own spelling (empty when it answers to no key), and
/// where in the chain it is answered. `authored` says the maker's keymap file moved or
/// disabled it; `remappable` is false only for the text box's own keys, listed for discovery.
struct ShownBinding {
    std::string group;   ///< where it is answered: "above every mode", a mode's name, a pane
    std::string id;      ///< the durable id a keymap file names
    std::string label;
    std::string gesture; ///< e.g. `ctrl+t`; empty when the row answers to no key
    bool authored = false;
    bool remappable = true;
    ZEN_SHAPE(ShownBinding, 1, ZEN_FIELD(group), ZEN_FIELD(id), ZEN_FIELD(label),
              ZEN_FIELD(gesture), ZEN_FIELD(authored), ZEN_FIELD(remappable));
};

/// THE ONE BINDING TRUTH, AS A VALUE -- what dispatch reads, projected for a presenter: every
/// host row, every application row, every pane's rows in force, and the text box's own keys.
///
/// ⚠ A PRESENTER KEEPS NO CATALOG OF ITS OWN. A floor that printed `ctrl+t` because that is the
/// declared default would go on printing it after a maker moved or disabled the row; the
/// desktop reads its own rows' gestures here, as the host resolved them.
///
/// Published `to_any` when it changes and answered to a presenter that asks (`KeymapRequested`),
/// on `PaneInventory`'s terms. `file` is the keymap file this run reads; `word` is what reading
/// it came to (empty when there was nothing to say).
struct KeymapShown {
    std::vector<ShownBinding> rows;
    std::string file;
    std::string word;
    ZEN_SHAPE(KeymapShown, 1, ZEN_FIELD(rows), ZEN_FIELD(file), ZEN_FIELD(word));
};

/// TELL ME THE KEYMAP AS IT IS NOW -- `PaneInventoryRequested`'s question, one owner over.
struct KeymapRequested {
    ZEN_SHAPE(KeymapRequested, 1);
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
