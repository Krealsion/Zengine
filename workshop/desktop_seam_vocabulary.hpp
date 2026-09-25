// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_DESKTOP_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_DESKTOP_SEAM_VOCABULARY_HPP

// The desktop seam: what crosses between Workshop and the weave that owns the application's
// defaults -- the gestures it answers above every pane, launching and closing, and what stands in
// the empty room (WL-DESK-01). The desktop decides which requests to make and when; the host
// decides whether they can be performed. A desktop that failed to load leaves no defaults and no
// backdrop, never a process a maker cannot leave.

#include "surface/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The office the application-defaults weave holds: where the host sends `AppActionRequested`,
/// and the one durable name a replacement desktop claims.
inline constexpr const char* kDesktopRole = "zengine.desktop";

// ---- Application actions -------------------------------------------------------------------

/// Where in the routing chain an application row is answered: exactly two places (WL-DESK-07).
namespace app_precedence {
/// Above every mode and ahead of the keys crossing to a pane -- still superseded by name when a
/// pane holding the keyboard declares it owns the gesture (`v2::PaneActionRow::supersedes`).
inline constexpr std::int64_t kAboveModes = 0;
/// Last, and only where nothing more specific owned the gesture (WL-DESK-02).
inline constexpr std::int64_t kDefault = 1;
} // namespace app_precedence

/// One application action as the desktop declares it: a pane row's four fields plus `precedence`.
/// What it does is the declarer's, reached by `AppActionRequested`.
struct AppActionRow {
    std::string id;             ///< the durable action id, in the DECLARING office's namespace
    std::string label;          ///< what a legend prints beside the key
    std::int64_t scancode = 0;  ///< `zengine::input::scan`'s space; `kUnknown` = no default
    std::int64_t modifiers = 0; ///< `zengine::input::mod`'s bitmask
    std::int64_t precedence = app_precedence::kAboveModes; ///< one of `app_precedence`
    ZEN_SHAPE(AppActionRow, 1, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(precedence));
};

/// The application actions one office declares, judged whole under its stamp by the collision law
/// a pane's rows meet, and atomic both ways. Only the office the host listens to is admitted: two
/// parties owning what the application does above every mode would be duplicate authority.
struct AppActions {
    std::vector<AppActionRow> rows;
    ZEN_SHAPE(AppActions, 1, ZEN_FIELD(rows));
};

/// A maker pressed the gesture a declared application row answers to: the resolved id, sent
/// instead of that keystroke, its character swallowed.
struct AppActionRequested {
    std::string id; ///< one of the ids this office declared, as it declared it
    ZEN_SHAPE(AppActionRequested, 1, ZEN_FIELD(id));
};

// ---- Launching -----------------------------------------------------------------------------

/// Open this pane, or focus it if it is already on the desk (WL-DESK-03). `office` and `pane` are
/// a `PaneRef`'s two halves. A launch never toggles and never loads: a pane no provider offers
/// is refused, saying which of the two reasons applies.
struct PaneLaunchRequested {
    std::string office;
    std::string pane;
    ZEN_SHAPE(PaneLaunchRequested, 1, ZEN_FIELD(office), ZEN_FIELD(pane));
};

/// What the launch came to: `opened` and `focused` are two facts; a refusal is neither.
struct PaneLaunchAnswered {
    std::string office;
    std::string pane;
    bool opened = false;  ///< it was not participating, and now is
    bool focused = false; ///< the keyboard and the selection are on it now
    std::string refusal;  ///< empty iff the request was performed
    ZEN_SHAPE(PaneLaunchAnswered, 1, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(opened),
              ZEN_FIELD(focused), ZEN_FIELD(refusal));
};

/// Take this pane off the desk (WL-DESK-12). Closing unloads nothing: the provider keeps all it
/// holds, so a launch finds it as it was. A pane not on the desk is refused.
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

/// Show this pane if hidden, hide it if shown -- judged by the host against the desk as it is when
/// handled, never by the asker's reading, and answered with which it did (WL-DESK-13).
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

/// The operations an edit may ask for on one action id's authored rows. Removing the last row
/// leaves the id disabled, never a silent fall-back to a default; `reset` lets the declared
/// default stand. The `_spelled` forms carry the gesture as `text`, in the keymap file's grammar.
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

/// Change how one action is requested. The host judges the candidate list exactly as a file is
/// judged at load; any conflict refuses the edit with that law's sentence, changing nothing. An
/// accepted edit is applied live, then written.
struct KeymapEditRequested {
    std::string action;         ///< the durable id a keymap file names
    std::int64_t op = 0;        ///< one of `keymap_edit`
    std::int64_t scancode = 0;  ///< `input::scan`'s space, for set/add/remove
    std::int64_t modifiers = 0; ///< `input::mod`'s bitmask
    std::string text;           ///< the spelled gesture, for set_spelled/add_spelled
    ZEN_SHAPE(KeymapEditRequested, 1, ZEN_FIELD(action), ZEN_FIELD(op), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(text));
};

/// What the edit came to: `accepted` and `applied` (live now), `written` (in the file), or why not
/// in `file_refusal` -- then the live change stands for this run only. `sentence` stands alone.
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

/// The Pane Creator's three acts on the one open definition (WL-MAKER-08).
namespace maker_pane_act {
inline constexpr std::int64_t kCreate = 1;
inline constexpr std::int64_t kSave = 2;
inline constexpr std::int64_t kDiscard = 3;
} // namespace maker_pane_act

/// The action ids a presenter of the Creator declares, spelled here because the host names their
/// keys too; they are the ids a maker's authored override already finds.
inline constexpr const char* kCreatorNewId = "pane-creator.new";
inline constexpr const char* kCreatorSaveId = "pane-creator.save";
inline constexpr const char* kCreatorDiscardId = "pane-creator.discard";
/// ...and the name line's two, declared while a name is being typed.
inline constexpr const char* kCreatorNameId = "pane-creator.name";
inline constexpr const char* kCreatorCancelId = "pane-creator.cancel";

/// Ask the host for one of the Creator's acts (`name` read for `kCreate` only). The definition is
/// the host's, and so is every refusal.
struct MakerPaneRequested {
    std::int64_t act = 0;
    std::string name;
    ZEN_SHAPE(MakerPaneRequested, 1, ZEN_FIELD(act), ZEN_FIELD(name));
};

/// What the act came to, answering one ask under that ask's number; a presenter reads it against
/// the act it is waiting on, never its latest number (WL-MAKER-14).
struct MakerPaneAnswered {
    std::int64_t act = 0;
    bool accepted = false; ///< the host did what was asked (a discard with nothing to discard too)
    std::string said;      ///< the host's own sentence: what was done, or why not
    ZEN_SHAPE(MakerPaneAnswered, 1, ZEN_FIELD(act), ZEN_FIELD(accepted), ZEN_FIELD(said));
};

/// Put down whatever the maker has picked up: the host act the desktop's Escape row asks for. The
/// selection is the host's; the desktop owns when. The ask echoes, in Loom's envelope, the number
/// minted for the keystroke that requested the row, so a stale or zero echo acts on nothing.
struct DeselectRequested {
    ZEN_SHAPE(DeselectRequested, 1);
};

// ---- The inventory the launcher presents ----------------------------------------------------

/// One pane as the one inventory has it. `open` is the maker's authored participation,
/// `available` whether anything holds the office that offers it now, `pending` a run still owing
/// it -- separate facts, so an unavailable pane is explained rather than vanishing.
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

/// The whole inventory, published whenever it changes and replaced whole, never merged: a reading
/// of `inventory_rows`, not a second inventory (WL-DESK-04). Published to any, because which
/// weave presents it is the load plan's business.
struct PaneInventory {
    std::vector<InventoryPane> panes;
    ZEN_SHAPE(PaneInventory, 1, ZEN_FIELD(panes));
};

/// Ask for the inventory as it is now, answered to the asking incarnation alone -- for a presenter
/// that arrives while nothing changes.
struct PaneInventoryRequested {
    ZEN_SHAPE(PaneInventoryRequested, 1);
};

// ---- The effective keymap, said out loud ----------------------------------------------------

/// One binding as it is in force: action, label, gesture in the keymap file's spelling (empty for
/// none) and where it is answered. `remappable` is false only for the text box's own keys.
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

/// The one binding truth as a value, projected for a presenter, which keeps no catalog of its own:
/// a floor printing a declared default would go on printing it after the maker moved the row.
/// Published when it changes and answered on request, like `PaneInventory`.
struct KeymapShown {
    std::vector<ShownBinding> rows;
    std::string file;
    std::string word;
    ZEN_SHAPE(KeymapShown, 1, ZEN_FIELD(rows), ZEN_FIELD(file), ZEN_FIELD(word));
};

/// Ask for the keymap as it is now.
struct KeymapRequested {
    ZEN_SHAPE(KeymapRequested, 1);
};

// ---- The backdrop ---------------------------------------------------------------------------

/// What stands in the empty room: rows the host paints behind every pane (WL-DESK-05). Not a pane
/// and taking no input -- the desktop owns the words, the host the wall. Refused whole past
/// `kMaxBackdropRows`.
struct DesktopFace {
    std::vector<surface::SurfaceTextRow> rows;
    ZEN_SHAPE(DesktopFace, 1, ZEN_FIELD(rows));
};

/// How many rows a backdrop may say: its own bound on what a desktop can make the host retain.
inline constexpr std::size_t kMaxBackdropRows = 64;

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_DESKTOP_SEAM_VOCABULARY_HPP
