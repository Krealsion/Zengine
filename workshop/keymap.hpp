// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_KEYMAP_HPP
#define ZENGINE_WORKSHOP_KEYMAP_HPP

// One executable binding truth: the action catalog, the gesture grammar, and the keymap.
// Workshop law: agents/workshop/keyboard.md (+7 registers; agents/workshop.md routes)

#include "pane_vocabulary.hpp" // ActionCause and an action id's bound

#include "input/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHERE THE KEYBOARD CURRENTLY GOES -- the routing chain's branches, as values.
// WL-KEY-03, WL-KEY-05 -- agents/workshop/keyboard.md
enum class KeyContext : std::uint8_t {
    kCommand,
    kNaming,
    kContext,
    kPane,
    kArrangePane,
    kArrangeDesk,
    kArrangeReset,
    kGlobal,
    kNoText,
    /// Everywhere, unless the pane holding the keyboard declared it owns this action
    /// (`PaneActionRow::supersedes`): declared by the pane, so it survives a weaver moving either
    /// row's key.
    kUnlessOwned,
};

/// Does this context hand ordinary keys to something that takes text?
// WL-FOCUS-09 -- agents/workshop/focus.md
// WL-KEY-03 -- agents/workshop/keyboard.md
inline constexpr bool context_takes_text(KeyContext c) noexcept {
    return c == KeyContext::kNaming || c == KeyContext::kPane;
}

/// Is an action declared for `declared` requestable while `current` is the resolved
/// context?
// WL-CTX-06 -- agents/workshop/contextual.md
inline constexpr bool active_in(KeyContext declared, KeyContext current) noexcept {
    if (declared == KeyContext::kGlobal) {
        return true;
    }
    if (declared == KeyContext::kNoText) {
        return !context_takes_text(current);
    }
    if (declared == KeyContext::kUnlessOwned) {
        // Every context as a class: what takes the row away is the keyboard pane's declaration,
        // judged in `Keymap::row_active`.
        return true;
    }
    return declared == current;
}

/// Can two declared contexts ever be active at the same moment?
// WL-KEY-06, WL-KEY-08 -- agents/workshop/keyboard.md
inline constexpr bool contexts_intersect(KeyContext a, KeyContext b) noexcept {
    if (a == b || a == KeyContext::kGlobal || b == KeyContext::kGlobal) {
        return true;
    }
    // A `kUnlessOwned` row meets every other row: supersession is one row standing down for one
    // pane, judged where the pane's rows are (`join_pane_rows`).
    if (a == KeyContext::kUnlessOwned || b == KeyContext::kUnlessOwned) {
        return true;
    }
    if (a == KeyContext::kNoText) {
        return !context_takes_text(b);
    }
    if (b == KeyContext::kNoText) {
        return !context_takes_text(a);
    }
    return false;
}

/// One gesture as the wire reports it: a named scancode and the EXACT observed modifier bits.
// WL-KEY-04 -- agents/workshop/keyboard.md
struct Gesture {
    std::int64_t scancode = 0;
    std::int64_t modifiers = 0;

    friend constexpr bool operator==(const Gesture& a, const Gesture& b) noexcept {
        return a.scancode == b.scancode && a.modifiers == b.modifiers;
    }
};

/// IS THIS A GESTURE A WEAVER CAN PRESS?
// WL-CTX-06 -- agents/workshop/contextual.md; WL-KEY-13 -- agents/workshop/key-spelling.md
inline constexpr bool is_bound(const Gesture& g) noexcept {
    return g.scancode != input::scan::kUnknown;
}

/// The declaration a row makes when the action is reachable from a surface that names it
/// and from no key at all.
// WL-KEY-13 -- agents/workshop/key-spelling.md
inline constexpr Gesture kNoGesture{input::scan::kUnknown, input::mod::kNone};

/// THE ACTION IDENTITIES, as the code spells them.
// WL-KEY-01 -- agents/workshop/keyboard.md
enum class Act : std::uint8_t {
    kNone = 0,
    // -- above every mode -------------------------------------------------------------
    kQuit,
    kFocusLeave,
    // -- command mode ------------------------------------------------------------------
    kSetupSave,
    kSetupRestore,
    kLayoutNext,
    kLayoutPrevious,
    kLayoutNew,
    kLayoutRemove,
    kLayoutRename,
    kLayoutDuplicate,
    kLayoutMoveLeft,
    kLayoutMoveRight,
    kArrangeDesk,
    kPaneTitles,
    kBandKeys,
    kHotkeysKeys,
    kEditCode,
    kFocusNext,
    kFocusPrevious,
    kFocusLeft,
    kFocusRight,
    kFocusUp,
    kFocusDown,
    kFocusEnter,
    // -- the setup-name editor's controls ----------------------------------------------
    kNamingCommit,
    kNamingCancel,
    // -- arranging panes ---------------------------------------------------------------
    kManageNext,
    kManagePrevious,
    kArrange,
    kManageFront,
    kManageBack,
    kManageRaise,
    kManageLower,
    kManageRemove,
    kManageReset,
    kManageClose,
    kManageDone,
    kManagePlaceLeft,
    kManagePlaceRight,
    kManagePlaceUp,
    kManagePlaceDown,
    kManagePullLeft,
    kManagePullRight,
    kManagePullUp,
    kManagePullDown,
    kManageGrow,
    kManageShrink,
    kManageResetPlace,
    kManageResetWidth,
    kManageResetHeight,
    kManageResetOrder,
    // -- the contextual-action surface -------------------------------------------------
    kContextOpen,
    kContextUp,
    kContextDown,
    kContextChoose,
    kContextBack,
};

/// One declaration row: the identity, the human meaning, where it is requestable, and the
/// developer's default gesture. Exactly what the common consumers need and nothing more --
/// no callback, no availability flag, no ordering weight.
// WL-KEY-01 -- agents/workshop/keyboard.md; WL-ATTN-10 -- agents/workshop/attention.md
struct ActionRow {
    Act act = Act::kNone;
    const char* id = "";
    const char* label = "";
    KeyContext context = KeyContext::kCommand;
    Gesture gesture;
};

namespace scan = input::scan;
namespace mod = input::mod;

/// The declarations; order inside a context group is presentation priority. An id is the durable
/// spelling a weaver's keymap file names, so a meaning that changes keeps its id.
// WL-KEY-01, WL-KEY-06 -- agents/workshop/keyboard.md
// WL-ARR-08 -- agents/workshop/arrangement.md
// WL-CTX-05 -- agents/workshop/contextual.md
inline constexpr ActionRow kActionCatalog[] = {
    // -- above every mode -------------------------------------------------------------
    {Act::kQuit, "workshop.quit", "quit", KeyContext::kNoText, {scan::kC, mod::kCtrl}},
    // `ctrl+c` is both the copy chord and the quit chord, told apart by whether the keys go to
    // text: the whole of the `kNoText` class.
    // The keys back to Workshop from any pane or mode, whatever the pane keeps Escape for: the one
    // host row of the `kGlobal` class, so no pane may declare its chord (WL-KEY-15).
    {Act::kFocusLeave, "focus.leave", "leave pane", KeyContext::kGlobal, {scan::kQ, mod::kCtrl}},
    // -- command mode ------------------------------------------------------------------
    {Act::kQuit, "workshop.quit", "quit", KeyContext::kCommand, {scan::kQ, mod::kNone}},
    // The id is the old `setup.name`, kept for the weaver's file: `s` saves the live layout's desk
    // to its artifact, and renaming is `layout.rename`.
    {Act::kSetupSave, "setup.name", "save setup", KeyContext::kCommand,
     {scan::kS, mod::kNone}},
    {Act::kSetupRestore, "setup.restore", "restore setup", KeyContext::kCommand,
     {scan::kR, mod::kNone}},
    // The layout shelf, on bare keys both backends deliver (the POSIX wire carries only an
    // unshifted printable or a shifted letter; agents/decisions/one-binding-truth.md): `,` and `.`
    // walk the run, `=` adds. Removal is the one chord, `^w`: discarding a layout cannot be
    // undone, so it is never one slipped bare key away.
    {Act::kLayoutNext, "layout.next", "next layout", KeyContext::kCommand,
     {scan::kPeriod, mod::kNone}},
    {Act::kLayoutPrevious, "layout.previous", "previous layout", KeyContext::kCommand,
     {scan::kComma, mod::kNone}},
    {Act::kLayoutNew, "layout.new", "new layout", KeyContext::kCommand,
     {scan::kEquals, mod::kNone}},
    {Act::kLayoutRemove, "layout.remove", "remove layout", KeyContext::kCommand,
     {scan::kW, mod::kCtrl}},
    // ...and four that answer to no key, reached from a tab's menu (rename also by double-click):
    // declared so a menu row and a keymap file can name them.
    {Act::kLayoutRename, "layout.rename", "rename layout", KeyContext::kCommand, kNoGesture},
    {Act::kLayoutDuplicate, "layout.duplicate", "duplicate layout", KeyContext::kCommand,
     kNoGesture},
    {Act::kLayoutMoveLeft, "layout.move-left", "move layout left", KeyContext::kCommand,
     kNoGesture},
    {Act::kLayoutMoveRight, "layout.move-right", "move layout right", KeyContext::kCommand,
     kNoGesture},
    // ...and Edit Code, reached from a pane's menu: command mode cannot name a pane.
    {Act::kEditCode, "pane.edit-code", "edit code", KeyContext::kCommand, kNoGesture},
    // Arrange the desk; the id is the old `workshop.manage`, kept for the weaver's file.
    {Act::kArrangeDesk, "workshop.manage", "arrange desk", KeyContext::kCommand,
     {scan::kW, mod::kNone}},
    // The keyboard door to the contextual surface, on the one subject command mode can name: the
    // room. `a` is portable and free in every context that meets kCommand.
    {Act::kContextOpen, "workshop.context", "actions", KeyContext::kCommand,
     {scan::kA, mod::kNone}},
    // Whether the arrangeable panes paint their title rows; last in the group, because the band
    // packs in order and an occasional toggle must not displace constant gestures.
    {Act::kPaneTitles, "workshop.pane-titles", "titles", KeyContext::kCommand,
     {scan::kT, mod::kNone}},
    // ...and how the band and the Hotkeys view each spell a key: two more presentation
    // preferences, toggled from a pane's Settings, and from command mode once a weaver binds them.
    {Act::kBandKeys, "workshop.band-keys", "band key spelling", KeyContext::kCommand, kNoGesture},
    {Act::kHotkeysKeys, "workshop.hotkeys-keys", "Hotkeys key spelling", KeyContext::kCommand,
     kNoGesture},
    // ...and the keyboard's way to a pane: a selection moved in reading order or by direction,
    // and entered. Moving it gives no pane the keys; only `focus.enter` does.
    {Act::kFocusNext, "focus.next", "next pane", KeyContext::kCommand, {scan::kTab, mod::kNone}},
    {Act::kFocusPrevious, "focus.previous", "previous pane", KeyContext::kCommand,
     {scan::kTab, mod::kShift}},
    {Act::kFocusLeft, "focus.left", "pane left", KeyContext::kCommand, {scan::kLeft, mod::kNone}},
    {Act::kFocusRight, "focus.right", "pane right", KeyContext::kCommand,
     {scan::kRight, mod::kNone}},
    {Act::kFocusUp, "focus.up", "pane up", KeyContext::kCommand, {scan::kUp, mod::kNone}},
    {Act::kFocusDown, "focus.down", "pane down", KeyContext::kCommand, {scan::kDown, mod::kNone}},
    {Act::kFocusEnter, "focus.enter", "enter pane", KeyContext::kCommand,
     {scan::kReturn, mod::kNone}},
    // -- the layout-name editor's controls ---------------------------------------------
    // The ids are the old ones (`naming.commit` still finishes it); the editor now renames the
    // layout and writes nothing.
    {Act::kNamingCommit, "naming.commit", "rename", KeyContext::kNaming,
     {scan::kReturn, mod::kNone}},
    {Act::kNamingCancel, "naming.cancel", "cancel", KeyContext::kNaming,
     {scan::kEscape, mod::kNone}},
    // -- arranging panes ---------------------------------------------------------------
    // One vocabulary, two scopes: arrows place, shift+arrows pull an extent, and every action the
    // scopes share owns a row in each, so one override moves both. The `manage.` ids are kept for
    // the weaver's file. A keyboard pull is anchored at the place: a key never moves a pane it is
    // resizing.
    {Act::kManageNext, "manage.next", "next pane", KeyContext::kArrangeDesk,
     {scan::kTab, mod::kNone}},
    {Act::kManagePrevious, "manage.previous", "previous pane", KeyContext::kArrangeDesk,
     {scan::kTab, mod::kShift}},
    // Narrow to one pane: the desk's Return arranges the pane the keyboard is on.
    {Act::kArrange, "manage.arrange", "arrange", KeyContext::kArrangeDesk,
     {scan::kReturn, mod::kNone}},
    // The coarse step comes first in both scopes: the band packs in order, and `=` is what a weaver
    // on a shipped desk reaches for first. `=` and `-` read as bigger and smaller and a POSIX
    // terminal can say them; both spend `kCoarseStepCells` through the anchored proposal.
    {Act::kManageGrow, "manage.grow", "grow", KeyContext::kArrangePane,
     {scan::kEquals, mod::kNone}},
    {Act::kManageShrink, "manage.shrink", "shrink", KeyContext::kArrangePane,
     {scan::kMinus, mod::kNone}},
    {Act::kManagePlaceLeft, "manage.place-left", "place left", KeyContext::kArrangePane,
     {scan::kLeft, mod::kNone}},
    {Act::kManagePlaceRight, "manage.place-right", "place right", KeyContext::kArrangePane,
     {scan::kRight, mod::kNone}},
    {Act::kManagePlaceUp, "manage.place-up", "place up", KeyContext::kArrangePane,
     {scan::kUp, mod::kNone}},
    {Act::kManagePlaceDown, "manage.place-down", "place down", KeyContext::kArrangePane,
     {scan::kDown, mod::kNone}},
    {Act::kManagePullLeft, "manage.pull-left", "narrower", KeyContext::kArrangePane,
     {scan::kLeft, mod::kShift}},
    {Act::kManagePullRight, "manage.pull-right", "wider", KeyContext::kArrangePane,
     {scan::kRight, mod::kShift}},
    {Act::kManagePullUp, "manage.pull-up", "shorter", KeyContext::kArrangePane,
     {scan::kUp, mod::kShift}},
    {Act::kManagePullDown, "manage.pull-down", "taller", KeyContext::kArrangePane,
     {scan::kDown, mod::kShift}},
    {Act::kManageFront, "manage.front", "front", KeyContext::kArrangePane,
     {scan::kF, mod::kNone}},
    {Act::kManageBack, "manage.back", "back", KeyContext::kArrangePane,
     {scan::kB, mod::kNone}},
    {Act::kManageRaise, "manage.raise", "raise", KeyContext::kArrangePane,
     {scan::kR, mod::kNone}},
    {Act::kManageLower, "manage.lower", "lower", KeyContext::kArrangePane,
     {scan::kL, mod::kNone}},
    {Act::kManageRemove, "manage.remove", "hide pane", KeyContext::kArrangePane,
     {scan::kD, mod::kNone}},
    {Act::kManageReset, "manage.reset", "reset", KeyContext::kArrangePane,
     {scan::k0, mod::kNone}},
    {Act::kManageClose, "manage.close", "leave", KeyContext::kArrangePane,
     {scan::kEscape, mod::kNone}},
    {Act::kManageGrow, "manage.grow", "grow", KeyContext::kArrangeDesk,
     {scan::kEquals, mod::kNone}},
    {Act::kManageShrink, "manage.shrink", "shrink", KeyContext::kArrangeDesk,
     {scan::kMinus, mod::kNone}},
    {Act::kManagePlaceLeft, "manage.place-left", "place left", KeyContext::kArrangeDesk,
     {scan::kLeft, mod::kNone}},
    {Act::kManagePlaceRight, "manage.place-right", "place right", KeyContext::kArrangeDesk,
     {scan::kRight, mod::kNone}},
    {Act::kManagePlaceUp, "manage.place-up", "place up", KeyContext::kArrangeDesk,
     {scan::kUp, mod::kNone}},
    {Act::kManagePlaceDown, "manage.place-down", "place down", KeyContext::kArrangeDesk,
     {scan::kDown, mod::kNone}},
    {Act::kManagePullLeft, "manage.pull-left", "narrower", KeyContext::kArrangeDesk,
     {scan::kLeft, mod::kShift}},
    {Act::kManagePullRight, "manage.pull-right", "wider", KeyContext::kArrangeDesk,
     {scan::kRight, mod::kShift}},
    {Act::kManagePullUp, "manage.pull-up", "shorter", KeyContext::kArrangeDesk,
     {scan::kUp, mod::kShift}},
    {Act::kManagePullDown, "manage.pull-down", "taller", KeyContext::kArrangeDesk,
     {scan::kDown, mod::kShift}},
    {Act::kManageFront, "manage.front", "front", KeyContext::kArrangeDesk,
     {scan::kF, mod::kNone}},
    {Act::kManageBack, "manage.back", "back", KeyContext::kArrangeDesk,
     {scan::kB, mod::kNone}},
    {Act::kManageRaise, "manage.raise", "raise", KeyContext::kArrangeDesk,
     {scan::kR, mod::kNone}},
    {Act::kManageLower, "manage.lower", "lower", KeyContext::kArrangeDesk,
     {scan::kL, mod::kNone}},
    {Act::kManageRemove, "manage.remove", "hide pane", KeyContext::kArrangeDesk,
     {scan::kD, mod::kNone}},
    {Act::kManageReset, "manage.reset", "reset", KeyContext::kArrangeDesk,
     {scan::k0, mod::kNone}},
    {Act::kManageClose, "manage.close", "leave", KeyContext::kArrangeDesk,
     {scan::kEscape, mod::kNone}},
    {Act::kManageResetPlace, "manage.reset-place", "reset place", KeyContext::kArrangeReset,
     {scan::kP, mod::kNone}},
    {Act::kManageResetWidth, "manage.reset-width", "reset width", KeyContext::kArrangeReset,
     {scan::kW, mod::kNone}},
    {Act::kManageResetHeight, "manage.reset-height", "reset height",
     KeyContext::kArrangeReset, {scan::kH, mod::kNone}},
    {Act::kManageResetOrder, "manage.reset-order", "reset order", KeyContext::kArrangeReset,
     {scan::kO, mod::kNone}},
    {Act::kManageDone, "manage.done", "back", KeyContext::kArrangeReset,
     {scan::kEscape, mod::kNone}},
    // -- the contextual-action surface -------------------------------------------------
    // `context.choose` is one action whose meaning the row decides (a group descends, an action
    // requests); `context.back` leaves a group, else the surface; the opener's gesture closes it.
    {Act::kContextUp, "context.up", "row up", KeyContext::kContext,
     {scan::kUp, mod::kNone}},
    {Act::kContextDown, "context.down", "row down", KeyContext::kContext,
     {scan::kDown, mod::kNone}},
    {Act::kContextChoose, "context.choose", "choose", KeyContext::kContext,
     {scan::kReturn, mod::kNone}},
    {Act::kContextBack, "context.back", "back", KeyContext::kContext,
     {scan::kEscape, mod::kNone}},
};

inline constexpr std::size_t kActionCatalogCount = sizeof(kActionCatalog) / sizeof(kActionCatalog[0]);

} // namespace zengine::workshop

// The written gesture grammar and what a backend cannot produce: a part of this header,
// which the keymap value and its joins spell and parse with.
#include "gesture_grammar.hpp"

namespace zengine::workshop {

// ---- The legend preference ---------------------------------------------------------------

/// How much of the effective bindings the bottom band's two help rows project.
// WL-KEY-09 -- agents/workshop/keyboard.md
namespace legend_mode {
inline constexpr std::int64_t kDefault = 0;
inline constexpr std::int64_t kFull = 1;
inline constexpr std::int64_t kCompact = 2;
inline constexpr std::int64_t kHidden = 3;
} // namespace legend_mode

/// One authored override row, exactly as written: an action id and a gesture, two
/// strings.
// WL-KEY-06, WL-KEY-08 -- agents/workshop/keyboard.md
struct AuthoredOverride {
    std::string action;
    std::string gesture;
};

// ---- A pane's rows, joined ---------------------------------------------------------------

/// One row a pane declared, as in force: the id the pane is asked for, its label, and the gesture
/// requesting it now (the weaver's override, else the pane's default). No `Act` and no
/// `KeyContext`: execution is the pane's (`PaneActionRequested`), and the row is active exactly
/// while `keyboard_pane` resolves to its handle.
// WL-KEY-15 -- agents/workshop/keyboard.md
struct PaneRow {
    std::string id;
    std::string label;
    Gesture gesture;
    /// The `kUnlessOwned` action id this row stands in for while its pane holds the
    /// keyboard, or empty (`PaneActionRow::supersedes`, WL-KEY-15).
    std::string supersedes;
};

/// THE ROWS OF ONE PANE, keyed by the runtime handle Workshop minted for it -- the
/// integer that stands where a built-in row's `KeyContext` stands, so two panes declaring
/// one bare key are two contexts that never meet, and no enum value is minted per pane.
// WL-KEY-15 -- agents/workshop/keyboard.md
struct PaneRows {
    std::int64_t pane = -1; ///< the pane's runtime handle (`is_runtime_kind`, panes.hpp)
    std::vector<PaneRow> rows;
    /// A `gap_sentence` for each bound row whose gesture the terminal cannot produce, derived at
    /// every join from the rows in force, so a re-join neither drops nor repeats one.
    // WL-KEY-08 -- agents/workshop/keyboard.md
    std::vector<std::string> gaps;
};

/// The namespace of the application's own rows' ids, the desktop's (`desktop.terminal`).
inline constexpr const char* kApplicationIdPrefix = "desktop.";

/// One application row a participating owner declared, as in force: id, label, gesture now, and
/// where in the chain it is answered (`app_precedence`). No `Act` or `KeyContext`: execution is
/// the declarer's. No `supersedes`, by law: supersession runs from the specific to the general,
/// so an application row cannot take a gesture from a pane.
// WL-DESK-07 -- agents/workshop/desktop.md
struct AppRow {
    std::string id;
    std::string label;
    Gesture gesture;
    std::int64_t precedence = 0; ///< `app_precedence::kAboveModes` / `kDefault`
    /// What a press may cause, as the declarer said it (`v2::AppActionRow::causes`).
    // WL-CAUSE-01 -- agents/workshop/action-causes.md
    std::vector<ActionCause> causes = {};
};

/// How many application rows one declaration may carry. The rows are a weaver's own shortcuts
/// beside the desktop's few, and two rows never share a gesture, so the keyboard bounds them; this
/// only keeps one declaration finite, with room for every hotkey Inventory may switch on (512).
// WL-DESK-07 -- agents/workshop/desktop.md
inline constexpr std::size_t kMaxAppActionRows = 1024;

/// How many rows one pane may declare: its own bound on what a chatty provider can make this
/// session retain, with room for a row per command hotkey Inventory keeps (512) beside its own.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline constexpr std::size_t kMaxPaneActionRows = 1024;

static_assert(kMaxPaneActionIdLen == kMaxPaneMenuIdLen,
              "a menu row's id meets a declared action's id law, published in the pane protocol");

/// The one sentence the collision law says, at the file's admission and at a pane's, so a weaver
/// reads the same refusal whichever party arrived second.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline std::string collision_sentence(const Gesture& g, std::string_view a, std::string_view b) {
    return "`" + gesture_word(g) + "` is authored for both `" + std::string(a) + "` and `" +
           std::string(b) + "`, which can be active together -- one of them must move";
}

// ---- The keymap value --------------------------------------------------------------------

/// THE EFFECTIVE BINDING TRUTH: the declaration defaults plus the weaver's applied
/// overrides, plus what could not be applied and is preserved.
// WL-KEY-01, WL-KEY-07 -- agents/workshop/keyboard.md
struct Keymap {
    std::int64_t legend = legend_mode::kDefault;
    /// Every override row the weaver wrote, verbatim and in authored order -- what a save
    /// writes back, so a load-save round trip edits nothing it was asked to preserve.
    // WL-KEY-07 -- agents/workshop/keyboard.md
    std::vector<AuthoredOverride> authored;
    /// The rows this build could resolve and admit, as executable truth. DERIVED from
    /// `authored` by `apply_overrides`, never authored directly.
    std::vector<std::pair<Act, Gesture>> overrides;
    /// Accepted-with-a-caveat: the honest note about authored gestures with a known
    /// backend gap (see `posix_gap`), spoken once at load and kept nowhere else; a pane's rows
    /// keep their own (`PaneRows::gaps`).
    std::string note;
    /// The rows every pane declared, as in force: derived by `join_pane_rows`, re-joined when
    /// either side changes, and never saved.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    std::vector<PaneRows> panes;
    /// The application rows the defaults owner declared, as in force: derived by `join_app_rows`,
    /// never saved. Empty is the honest default: a Workshop whose desktop never loaded has none,
    /// and nothing compiled in stands behind them.
    // WL-DESK-07 -- agents/workshop/desktop.md
    std::vector<AppRow> app;

    /// The rows one pane holds in force, or nullptr for a pane that declared none.
    const PaneRows* pane_rows(std::int64_t pane) const noexcept {
        for (const PaneRows& p : panes) {
            if (p.pane == pane) {
                return &p;
            }
        }
        return nullptr;
    }

    /// WHICH OF THIS PANE'S ROWS THIS GESTURE REQUESTS, or nullptr -- `action_for` one
    /// context over, with the same guard: a key this build cannot name requests nothing.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    const PaneRow* pane_action_for(std::int64_t pane, std::int64_t scancode,
                                   std::int64_t modifiers) const noexcept {
        const Gesture pressed{scancode, modifiers};
        if (!is_bound(pressed)) {
            return nullptr;
        }
        const PaneRows* rows = pane_rows(pane);
        if (rows == nullptr) {
            return nullptr;
        }
        for (const PaneRow& row : rows->rows) {
            if (row.gesture == pressed) {
                return &row;
            }
        }
        return nullptr;
    }

    const Gesture* override_for(Act a) const noexcept {
        for (const std::pair<Act, Gesture>& o : overrides) {
            if (o.first == a) {
                return &o.second;
            }
        }
        return nullptr;
    }

    /// Every gesture one declaration row answers to now, in authored order: the weaver's overrides
    /// when the file names the action, else the default. An override moves all of an action's rows.
    // WL-KEY-08 -- agents/workshop/keyboard.md
    std::vector<Gesture> row_gestures(const ActionRow& row) const {
        std::vector<Gesture> out;
        for (const std::pair<Act, Gesture>& o : overrides) {
            if (o.first == row.act) {
                out.push_back(o.second);
            }
        }
        if (out.empty()) {
            out.push_back(row.gesture);
        }
        return out;
    }

    /// The FIRST gesture a row answers to -- what a legend spells and a sentence names. Every
    /// other key of the set is a key too (`row_answers`); the legend simply prints one.
    Gesture row_gesture(const ActionRow& row) const noexcept {
        const Gesture* o = override_for(row.act);
        return o != nullptr ? *o : row.gesture;
    }

    /// Does this row answer to this gesture -- any member of its set, not only the first.
    bool row_answers(const ActionRow& row, const Gesture& pressed) const noexcept {
        bool any = false;
        for (const std::pair<Act, Gesture>& o : overrides) {
            if (o.first == row.act) {
                any = true;
                if (o.second == pressed) {
                    return true;
                }
            }
        }
        return !any && row.gesture == pressed;
    }

    /// The one effective gesture of an action. For the multi-row actions this is the
    /// FIRST declared row's answer, which every current caller wants.
    Gesture gesture_of(Act a) const noexcept {
        for (const ActionRow& row : kActionCatalog) {
            if (row.act == a) {
                return row_gesture(row);
            }
        }
        return Gesture{};
    }

    /// Has the pane holding the keyboard declared it owns this action? By id, never by gesture:
    /// moving a key moves no meaning. A handle with no admitted rows supersedes nothing.
    bool pane_supersedes(std::int64_t pane, const std::string& action_id) const noexcept {
        const PaneRows* rows = pane_rows(pane);
        if (rows == nullptr) {
            return false;
        }
        for (const PaneRow& row : rows->rows) {
            if (!row.supersedes.empty() && row.supersedes == action_id) {
                return true;
            }
        }
        return false;
    }

    /// Which pane owns input in this context, or `kNoPaneKind`: the resolved context and the
    /// remembered pane together -- a weaver typing into a menu over a pane is not typing into the
    /// pane. The only place the two combine.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    static constexpr std::int64_t owner_of(KeyContext current,
                                           std::int64_t keyboard_pane) noexcept {
        return current == KeyContext::kPane ? keyboard_pane : -1;
    }

    /// Is this declared row requestable now? The context class says which modes it lives in; the
    /// pane owning input says whether it stood down. The one answer every resolver and view spends.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    bool row_active(const ActionRow& row, KeyContext current,
                    std::int64_t keyboard_pane) const noexcept {
        if (!active_in(row.context, current)) {
            return false;
        }
        if (row.context != KeyContext::kUnlessOwned) {
            return true;
        }
        return !pane_supersedes(owner_of(current, keyboard_pane), row.id);
    }

    /// WHICH ACTION THIS GESTURE REQUESTS IN THIS CONTEXT, or kNone. `keyboard_pane` is the
    /// runtime handle of the pane holding the keys, where one does (`kNoPaneKind` otherwise).
    // WL-KEY-04 -- agents/workshop/keyboard.md
    Act action_for(KeyContext current, std::int64_t scancode, std::int64_t modifiers,
                   std::int64_t keyboard_pane = -1) const noexcept {
        const Gesture pressed{scancode, modifiers};
        // A key this build cannot name requests nothing; otherwise it would match every
        // `kNoGesture` row.
        if (!is_bound(pressed)) {
            return Act::kNone;
        }
        for (const ActionRow& row : kActionCatalog) {
            if (row_active(row, current, keyboard_pane) && row_answers(row, pressed)) {
                return row.act;
            }
        }
        return Act::kNone;
    }

    /// WHICH ABOVE-THE-MODES ACTION THIS GESTURE REQUESTS, or kNone -- `action_for`
    /// restricted to the rows DECLARED kGlobal, kNoText or kUnlessOwned.
    // WL-FOCUS-06 -- agents/workshop/focus.md; WL-KEY-05 -- agents/workshop/keyboard.md
    Act above_mode_action(KeyContext current, std::int64_t scancode, std::int64_t modifiers,
                          std::int64_t keyboard_pane = -1) const noexcept {
        const Gesture pressed{scancode, modifiers};
        if (!is_bound(pressed)) {
            return Act::kNone; // `action_for`'s rule, for `action_for`'s reason
        }
        for (const ActionRow& row : kActionCatalog) {
            const bool above = row.context == KeyContext::kGlobal ||
                               row.context == KeyContext::kNoText ||
                               row.context == KeyContext::kUnlessOwned;
            if (above && row_active(row, current, keyboard_pane) &&
                row_answers(row, pressed)) {
                return row.act;
            }
        }
        return Act::kNone;
    }

    /// Which application row of this precedence class this gesture requests, or nullptr, under
    /// `action_for`'s two guards: an unnamed key requests nothing, and a row the keyboard pane
    /// stands in for is not requestable -- in both classes, since a pane never says whether it
    /// spent a key (WL-ARR-15).
    // WL-DESK-07 -- agents/workshop/desktop.md
    const AppRow* app_action_for(std::int64_t precedence, KeyContext current,
                                 std::int64_t scancode, std::int64_t modifiers,
                                 std::int64_t keyboard_pane = -1) const noexcept {
        const Gesture pressed{scancode, modifiers};
        if (!is_bound(pressed)) {
            return nullptr;
        }
        for (const AppRow& row : app) {
            if (row.precedence != precedence || row.gesture != pressed) {
                continue;
            }
            if (pane_supersedes(owner_of(current, keyboard_pane), row.id)) {
                continue;
            }
            return &row;
        }
        return nullptr;
    }

    /// Is this application row requestable at this moment? The legend's half of the question
    /// above, so a band cannot advertise a key the chain will not run.
    // WL-DESK-07 -- agents/workshop/desktop.md
    bool app_row_active(const AppRow& row, KeyContext current,
                        std::int64_t keyboard_pane) const noexcept {
        return is_bound(row.gesture) &&
               !pane_supersedes(owner_of(current, keyboard_pane), row.id);
    }

    /// The application row an id names, or nullptr.
    const AppRow* app_row_of_id(std::string_view id) const noexcept {
        for (const AppRow& row : app) {
            if (row.id == id) {
                return &row;
            }
        }
        return nullptr;
    }

    /// Does this gesture spell this action's effective binding, in any context? The one
    /// consumer is the contextual surface's "the key that opened it closes it" rule, which follows
    /// the OPENER's binding wherever the weaver moved it.
    bool matches(Act a, std::int64_t scancode, std::int64_t modifiers) const noexcept {
        const Gesture pressed{scancode, modifiers};
        if (!is_bound(pressed)) {
            return false; // `action_for`'s rule, for `action_for`'s reason
        }
        for (const ActionRow& row : kActionCatalog) {
            if (row.act == a && row_answers(row, pressed)) {
                return true;
            }
        }
        return false;
    }

    /// The legend the band projects: the authored word, with `default` meaning this
    /// build's own answer, which is FULL.
    std::int64_t resolved_legend() const noexcept {
        return legend == legend_mode::kDefault ? legend_mode::kFull : legend;
    }
};

/// The declaration row an id names, or nullptr. The FIRST row: an id's rows agree on
/// everything an override needs (the act), so one answer serves.
inline const ActionRow* row_of_id(std::string_view id) noexcept {
    for (const ActionRow& row : kActionCatalog) {
        if (id == row.id) {
            return &row;
        }
    }
    return nullptr;
}

} // namespace zengine::workshop

// Override admission, and a pane's and an application's rows joined: a part of this
// header, which needs the whole keymap value before it.
#include "keymap_join.hpp"

#endif // ZENGINE_WORKSHOP_KEYMAP_HPP
