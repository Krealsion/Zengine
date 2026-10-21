// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_DESKTOP_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_DESKTOP_SEAM_VOCABULARY_HPP

// The desktop seam: what crosses between Workshop and the weave that owns the application's
// defaults -- the gestures it answers above every pane, launching and closing (WL-DESK-01). The
// desktop decides which requests to make and when; the host decides whether they can be
// performed. A desktop that failed to load leaves no defaults, never a process a weaver cannot
// leave.

#include "workshop/pane_vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The office the application-defaults weave holds: where the host sends `AppActionRequested`,
/// and the one durable name a replacement desktop claims.
inline constexpr const char* kDesktopRole = "zengine.desktop";

/// THE APPLICATION ROW THAT SHOWS A PANE'S MANUAL, as the desktop declares it: a pane's `?` asks
/// it about that pane, and the `?` is drawn only while some row by this id is declared. Spelled in
/// the host too, for that reason, and a suite pins the two spellings against each other.
// WL-CHROME-09 -- agents/workshop/chrome.md
inline constexpr const char* kManualAction = "desktop.manual";

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

/// Version 2 of the application's declaration says what each press may cause, as a pane's row
/// does (`v3::PaneActionRow::causes`); Workshop widens v1 with none and holds them with the rows.
namespace v2 {

struct AppActionRow {
    std::string id;
    std::string label;
    std::int64_t scancode = 0;
    std::int64_t modifiers = 0;
    std::int64_t precedence = app_precedence::kAboveModes;
    std::vector<ActionCause> causes = {}; ///< at most `kMaxActionCauses`, none twice
    ZEN_SHAPE(AppActionRow, 2, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(precedence), ZEN_FIELD(causes));
};

struct AppActions {
    std::vector<AppActionRow> rows;
    ZEN_SHAPE(AppActions, 2, ZEN_FIELD(rows));
};

} // namespace v2

/// A weaver pressed the gesture a declared application row answers to: the resolved id, sent
/// instead of that keystroke, its character swallowed. `office` and `pane` name the pane the act
/// was performed on -- the pane holding the keys when a key asked it, the pane whose `?` was
/// pressed -- and are both empty when it was performed on none.
struct AppActionRequested {
    std::string id; ///< one of the ids this office declared, as it declared it
    std::string office;
    std::string pane;
    ZEN_SHAPE(AppActionRequested, 2, ZEN_FIELD(id), ZEN_FIELD(office), ZEN_FIELD(pane));
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

/// Put down whatever the weaver has picked up: the host act the desktop's Escape row asks for. The
/// selection is the host's; the desktop owns when. The ask echoes, in Loom's envelope, the number
/// minted for the keystroke that requested the row, so a stale or zero echo acts on nothing.
struct DeselectRequested {
    ZEN_SHAPE(DeselectRequested, 1);
};

// ---- The inventory the launcher presents ----------------------------------------------------

/// One pane as the one inventory has it. `open` is the weaver's authored participation,
/// `available` whether anything holds the office that offers it now, `pending` a run still owing
/// it -- separate facts, so an unavailable pane is explained rather than vanishing.
struct InventoryPane {
    std::string office;
    std::string pane;
    std::string name;    ///< the two lines a weaver reads, as the offering provider wrote them
    std::string summary; ///
    bool open = false;      ///< participating in the live desk
    bool available = false; ///< some holder of `office` currently offers this pane
    bool pending = false;   ///< not offered YET: the plan row loading `office` has not settled
    ZEN_SHAPE(InventoryPane, 2, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(name),
              ZEN_FIELD(summary), ZEN_FIELD(open), ZEN_FIELD(available), ZEN_FIELD(pending));
};

/// The whole inventory, published whenever it changes and replaced whole, never merged: a reading
/// of `inventory_rows`, not a second inventory (WL-DESK-04). Published to any, because which
/// weave presents it is the load plan's business. Its version moves with `InventoryPane`'s, whose
/// content id it carries.
struct PaneInventory {
    std::vector<InventoryPane> panes;
    ZEN_SHAPE(PaneInventory, 2, ZEN_FIELD(panes));
};

/// Ask for the inventory as it is now, answered to the asking incarnation alone -- for a presenter
/// that arrives while nothing changes.
struct PaneInventoryRequested {
    ZEN_SHAPE(PaneInventoryRequested, 1);
};

// ---- The effective keymap, said out loud ----------------------------------------------------

/// One binding as it is in force: where it is answered, whose it is, its action and label, its
/// gesture in the keymap file's spelling (empty for none) and as the Hotkeys view spells it, and
/// what a press of it may cause. `office`, `pane` and `id` are its address: a pane's row names its
/// office and pane, an application row its office and no pane, Workshop's own row Workshop's office;
/// the text box's own keys name neither. `remappable` is false only for the text box's own keys.
struct ShownBinding {
    std::string group;   ///< where it is answered: "above every mode", a mode's name, a pane
    std::string office;  ///< the office that declared it; empty for the text box's keys
    std::string pane;    ///< the pane, for a pane's row; empty otherwise
    std::string id;      ///< the durable id a keymap file names
    std::string label;
    std::string gesture; ///< e.g. `ctrl+t`; empty when the row answers to no key
    std::string shown;   ///< ...as the Hotkeys view spells it, `ctrl+t` or `^t`; empty with it
    bool authored = false;
    bool remappable = true;
    std::vector<ActionCause> causes = {}; ///< as its declarer declared them
    std::string enters = {};              ///< the mode a press leaves its pane in, or empty
    ZEN_SHAPE(ShownBinding, 3, ZEN_FIELD(group), ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(id),
              ZEN_FIELD(label), ZEN_FIELD(gesture), ZEN_FIELD(shown), ZEN_FIELD(authored),
              ZEN_FIELD(remappable), ZEN_FIELD(causes), ZEN_FIELD(enters));
};

/// One pane's modes as its declaration in force says them: the mode its rows are in force for,
/// and what each other mode's rows may cause.
struct ShownModes {
    std::string office;
    std::string pane;
    std::string mode;
    std::vector<ModeCauses> elsewhere;
    ZEN_SHAPE(ShownModes, 1, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(mode),
              ZEN_FIELD(elsewhere));
};

/// The one binding truth as a value, projected for a presenter, which keeps no catalog of its own:
/// a floor printing a declared default would go on printing it after the weaver moved the row.
/// Published when it changes and answered on request, like `PaneInventory`. `moved` is each key
/// the weaver's file names for an id no row in force answers to (a pane's other mode, a pane not
/// open), `gesture` empty for `none`, its address the id alone; `spelling` says how every `shown`
/// spells, `full` or `short`; `modes` is each pane's modes, for every pane that declared one.
struct KeymapShown {
    std::vector<ShownBinding> rows;
    std::string file;
    std::string word;
    std::vector<ShownBinding> moved;
    std::string spelling;
    std::vector<ShownModes> modes = {};
    ZEN_SHAPE(KeymapShown, 3, ZEN_FIELD(rows), ZEN_FIELD(file), ZEN_FIELD(word),
              ZEN_FIELD(moved), ZEN_FIELD(spelling), ZEN_FIELD(modes));
};

/// Ask for the keymap as it is now.
struct KeymapRequested {
    ZEN_SHAPE(KeymapRequested, 1);
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_DESKTOP_SEAM_VOCABULARY_HPP
