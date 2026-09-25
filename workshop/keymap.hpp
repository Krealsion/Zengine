// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_KEYMAP_HPP
#define ZENGINE_WORKSHOP_KEYMAP_HPP

// One executable binding truth: the action catalog, the gesture grammar, and the keymap.
// Workshop law: agents/workshop/keyboard.md (+13 registers; agents/workshop.md routes)

#include "pane_vocabulary.hpp" // PaneActionRow -- the rows a pane weave declares
#include "property.hpp"        // Written -- the one refusal-with-reason shape this package has

#include "component/text_box.hpp"
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
    /// (`PaneActionRow::supersedes`): declared by the pane, so it survives a maker moving either
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

/// IS THIS A GESTURE A MAKER CAN PRESS?
// WL-CTX-06 -- agents/workshop/contextual.md; WL-KEY-13 -- agents/workshop/keyboard.md
inline constexpr bool is_bound(const Gesture& g) noexcept {
    return g.scancode != input::scan::kUnknown;
}

/// The declaration a row makes when the action is reachable from a surface that names it
/// and from no key at all.
// WL-KEY-13 -- agents/workshop/keyboard.md
inline constexpr Gesture kNoGesture{input::scan::kUnknown, input::mod::kNone};

/// THE ACTION IDENTITIES, as the code spells them.
// WL-KEY-01 -- agents/workshop/keyboard.md
enum class Act : std::uint8_t {
    kNone = 0,
    // -- above every mode -------------------------------------------------------------
    kQuit,
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
    kEditCode,
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
/// spelling a maker's keymap file names, so a meaning that changes keeps its id.
// WL-KEY-01, WL-KEY-06 -- agents/workshop/keyboard.md
// WL-ARR-08 -- agents/workshop/arrangement.md
// WL-CTX-05 -- agents/workshop/contextual.md
inline constexpr ActionRow kActionCatalog[] = {
    // -- above every mode -------------------------------------------------------------
    {Act::kQuit, "workshop.quit", "quit", KeyContext::kNoText, {scan::kC, mod::kCtrl}},
    // `ctrl+c` is both the copy chord and the quit chord, told apart by whether the keys go to
    // text: the whole of the `kNoText` class.
    // -- command mode ------------------------------------------------------------------
    {Act::kQuit, "workshop.quit", "quit", KeyContext::kCommand, {scan::kQ, mod::kNone}},
    // The id is the old `setup.name`, kept for the maker's file: `s` saves the live layout's desk
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
    // Arrange the desk; the id is the old `workshop.manage`, kept for the maker's file.
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
    // the maker's file. A keyboard pull is anchored at the place: a key never moves a pane it is
    // resizing.
    {Act::kManageNext, "manage.next", "next pane", KeyContext::kArrangeDesk,
     {scan::kTab, mod::kNone}},
    {Act::kManagePrevious, "manage.previous", "previous pane", KeyContext::kArrangeDesk,
     {scan::kTab, mod::kShift}},
    // Narrow to one pane: the desk's Return arranges the pane the keyboard is on.
    {Act::kArrange, "manage.arrange", "arrange", KeyContext::kArrangeDesk,
     {scan::kReturn, mod::kNone}},
    // The coarse step comes first in both scopes: the band packs in order, and `=` is what a maker
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
    {Act::kManageRemove, "manage.remove", "remove", KeyContext::kArrangePane,
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
    {Act::kManageRemove, "manage.remove", "remove", KeyContext::kArrangeDesk,
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

// ---- The written gesture grammar ---------------------------------------------------------
//
// A binding is modifier words joined to one key name with `+` (`ctrl+k`, `shift+h`, `[`);
// punctuation keys are named by their own character. Written out in the order ctrl, shift, alt,
// super; parsed in any order, with duplicates refused.

/// The written name of a named scancode, or nullptr for a value this grammar cannot say.
// WL-KEY-14 -- agents/workshop/keyboard.md
inline constexpr const char* key_name_of(std::int64_t scancode) noexcept {
    switch (scancode) {
    case scan::kA: return "a";
    case scan::kB: return "b";
    case scan::kC: return "c";
    case scan::kD: return "d";
    case scan::kE: return "e";
    case scan::kF: return "f";
    case scan::kG: return "g";
    case scan::kH: return "h";
    case scan::kI: return "i";
    case scan::kJ: return "j";
    case scan::kK: return "k";
    case scan::kL: return "l";
    case scan::kM: return "m";
    case scan::kN: return "n";
    case scan::kO: return "o";
    case scan::kP: return "p";
    case scan::kQ: return "q";
    case scan::kR: return "r";
    case scan::kS: return "s";
    case scan::kT: return "t";
    case scan::kU: return "u";
    case scan::kV: return "v";
    case scan::kW: return "w";
    case scan::kX: return "x";
    case scan::kY: return "y";
    case scan::kZ: return "z";
    case scan::k1: return "1";
    case scan::k2: return "2";
    case scan::k3: return "3";
    case scan::k4: return "4";
    case scan::k5: return "5";
    case scan::k6: return "6";
    case scan::k7: return "7";
    case scan::k8: return "8";
    case scan::k9: return "9";
    case scan::k0: return "0";
    case scan::kReturn: return "return";
    case scan::kEscape: return "escape";
    case scan::kBackspace: return "backspace";
    case scan::kTab: return "tab";
    case scan::kSpace: return "space";
    case scan::kMinus: return "-";
    case scan::kEquals: return "=";
    case scan::kLeftBracket: return "[";
    case scan::kRightBracket: return "]";
    case scan::kBackslash: return "\\";
    case scan::kSemicolon: return ";";
    case scan::kApostrophe: return "'";
    case scan::kGrave: return "`";
    case scan::kComma: return ",";
    case scan::kPeriod: return ".";
    case scan::kSlash: return "/";
    case scan::kHome: return "home";
    case scan::kDelete: return "delete";
    case scan::kEnd: return "end";
    case scan::kLeft: return "left";
    case scan::kRight: return "right";
    case scan::kDown: return "down";
    case scan::kUp: return "up";
    default: return nullptr;
    }
}

/// The scancode a written key name means, or 0 for a name outside the grammar.
// WL-KEY-14 -- agents/workshop/keyboard.md
inline std::int64_t scancode_of_name(std::string_view name) noexcept {
    for (std::int64_t sc = 1; sc < 128; ++sc) {
        const char* word = key_name_of(sc);
        if (word != nullptr && name == word) {
            return sc;
        }
    }
    return 0;
}

/// A gesture as the file spells it (`ctrl+shift+z`); total over every nameable scancode, and
/// admission refuses the rest on the way in.
// WL-KEY-14 -- agents/workshop/keyboard.md
inline std::string gesture_word(const Gesture& g) {
    std::string out;
    if ((g.modifiers & mod::kCtrl) != 0) {
        out += "ctrl+";
    }
    if ((g.modifiers & mod::kShift) != 0) {
        out += "shift+";
    }
    if ((g.modifiers & mod::kAlt) != 0) {
        out += "alt+";
    }
    if ((g.modifiers & mod::kSuper) != 0) {
        out += "super+";
    }
    const char* name = key_name_of(g.scancode);
    out += name != nullptr ? name : "?";
    return out;
}

/// Is this scancode a letter key? The one set the POSIX terminal can infer Shift on.
inline constexpr bool is_letter_scan(std::int64_t sc) noexcept {
    return sc >= scan::kA && sc <= scan::kZ;
}

/// A gesture as the SCREEN spells it -- the band's own compact voice.
// WL-KEY-02, WL-KEY-13 -- agents/workshop/keyboard.md
inline std::string gesture_text(const Gesture& g) {
    // An action that answers to no key says so: `?` would read as a key a maker cannot find, and
    // `-` is a real binding.
    if (!is_bound(g)) {
        return "unbound";
    }
    const bool shift = (g.modifiers & mod::kShift) != 0;
    const bool capital = shift && is_letter_scan(g.scancode);
    std::string out;
    if ((g.modifiers & mod::kCtrl) != 0) {
        out += "^";
    }
    if (shift && !capital) {
        out += "shift+";
    }
    if ((g.modifiers & mod::kAlt) != 0) {
        out += "alt+";
    }
    if ((g.modifiers & mod::kSuper) != 0) {
        out += "super+";
    }
    switch (g.scancode) {
    case scan::kReturn: out += "enter"; break;
    case scan::kEscape: out += "esc"; break;
    default: {
        const char* name = key_name_of(g.scancode);
        if (name == nullptr) {
            out += "?";
        } else if (capital) {
            out += static_cast<char>(name[0] - 'a' + 'A');
        } else {
            out += name;
        }
        break;
    }
    }
    return out;
}

/// What parsing a written gesture produced: the gesture, or a refusal naming what was found and
/// what would have worked.
struct ParsedGesture {
    bool accepted = false;
    Gesture gesture;
    std::string refusal;
};

// WL-KEY-14 -- agents/workshop/keyboard.md
// WL-DESK-08 -- agents/workshop/desktop.md
inline ParsedGesture parse_gesture(std::string_view text) {
    ParsedGesture out;
    if (text.empty()) {
        out.refusal = "a gesture cannot be empty";
        return out;
    }
    // `none` is a gesture a maker may author -- the one that answers to no key. A disable, not a
    // delete: the row stays declared, listed and nameable, and nothing hard-wired stands behind it.
    if (text == "none") {
        out.accepted = true;
        out.gesture = kNoGesture;
        return out;
    }
    std::int64_t mods = 0;
    std::string_view rest = text;
    while (true) {
        const std::size_t plus = rest.find('+');
        // A trailing token with no `+` after it is the key name -- including `+` itself
        // being unsayable, which is fine: no scancode names it.
        if (plus == std::string_view::npos || plus + 1 >= rest.size()) {
            break;
        }
        const std::string_view word = rest.substr(0, plus);
        std::int64_t bit = 0;
        if (word == "ctrl") {
            bit = mod::kCtrl;
        } else if (word == "shift") {
            bit = mod::kShift;
        } else if (word == "alt") {
            bit = mod::kAlt;
        } else if (word == "super") {
            bit = mod::kSuper;
        } else {
            out.refusal = "`" + std::string(word) +
                          "` is not a modifier (ctrl, shift, alt or super)";
            return out;
        }
        if ((mods & bit) != 0) {
            out.refusal = "`" + std::string(word) + "` appears twice in `" +
                          std::string(text) + "`";
            return out;
        }
        mods |= bit;
        rest = rest.substr(plus + 1);
    }
    const std::int64_t sc = scancode_of_name(rest);
    if (sc == 0) {
        out.refusal = "`" + std::string(rest) + "` is not a key this keymap can name";
        return out;
    }
    out.accepted = true;
    out.gesture = Gesture{sc, mods};
    return out;
}

// ---- Honesty about what a backend can produce --------------------------------------------

/// Is this one of the CSI editing keys the POSIX parser reads measured modifiers on?
inline constexpr bool is_posix_editing_scan(std::int64_t sc) noexcept {
    return sc == scan::kHome || sc == scan::kEnd || sc == scan::kDelete ||
           sc == scan::kLeft || sc == scan::kRight || sc == scan::kUp || sc == scan::kDown;
}

/// The measured reason a gesture cannot arrive from the POSIX terminal backend, or
/// nullptr when no such gap is known.
// WL-KEY-08 -- agents/workshop/keyboard.md
inline const char* posix_gap(const Gesture& g) noexcept {
    if ((g.modifiers & mod::kSuper) != 0) {
        return "super never arrives from a POSIX terminal";
    }
    if ((g.modifiers & mod::kAlt) != 0 && !is_posix_editing_scan(g.scancode)) {
        return "alt arrives only on the editing keys from a POSIX terminal";
    }
    // The POSIX wire carries ctrl+letter as one control byte with no mark for Shift, so
    // ctrl+shift+letter arrives as plain ctrl+letter.
    if ((g.modifiers & mod::kCtrl) != 0 && (g.modifiers & mod::kShift) != 0 &&
        is_letter_scan(g.scancode)) {
        return "ctrl+shift+letter collapses to plain ctrl+letter on a POSIX terminal";
    }
    if ((g.modifiers & mod::kCtrl) != 0 &&
        (g.scancode == scan::kH || g.scancode == scan::kI || g.scancode == scan::kJ ||
         g.scancode == scan::kM)) {
        return "ctrl+h/i/j/m are byte-identical to backspace/tab/newline/return on a POSIX "
               "terminal";
    }
    if ((g.modifiers & mod::kShift) != 0 && !is_letter_scan(g.scancode) &&
        !is_posix_editing_scan(g.scancode) && g.scancode != scan::kTab) {
        // Tab is the one non-letter whose shifted form a terminal spells on its own:
        // `ESC [ Z` is back-tab, and the translator reads it.
        return "shift is not observable on that key from a POSIX terminal";
    }
    return nullptr;
}

/// THE TEXT A CONSUMED PRINTABLE GESTURE'S OWN KEYSTROKE PRODUCES, or "" when none is
/// expected.
// WL-KEY-12 -- agents/workshop/keyboard.md
inline std::string expected_text_of(std::int64_t scancode, std::int64_t modifiers) {
    if ((modifiers & (mod::kCtrl | mod::kAlt | mod::kSuper)) != 0) {
        return std::string();
    }
    if (is_letter_scan(scancode)) {
        const char c = static_cast<char>('a' + (scancode - scan::kA));
        return std::string(1, c);
    }
    if (scancode == scan::kSpace) {
        return " ";
    }
    if ((modifiers & mod::kShift) != 0) {
        return std::string(); // a shifted digit's or punctuation's face is the layout's
    }
    switch (scancode) {
    case scan::k1: return "1";
    case scan::k2: return "2";
    case scan::k3: return "3";
    case scan::k4: return "4";
    case scan::k5: return "5";
    case scan::k6: return "6";
    case scan::k7: return "7";
    case scan::k8: return "8";
    case scan::k9: return "9";
    case scan::k0: return "0";
    case scan::kMinus: return "-";
    case scan::kEquals: return "=";
    case scan::kLeftBracket: return "[";
    case scan::kRightBracket: return "]";
    case scan::kBackslash: return "\\";
    case scan::kSemicolon: return ";";
    case scan::kApostrophe: return "'";
    case scan::kGrave: return "`";
    case scan::kComma: return ",";
    case scan::kPeriod: return ".";
    case scan::kSlash: return "/";
    default: return std::string();
    }
}

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
/// requesting it now (the maker's override, else the pane's default). No `Act` and no
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
    std::int64_t pane = -1; ///< the pane's runtime handle (`is_runtime_kind`, panel.hpp)
    std::vector<PaneRow> rows;
};

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
};

/// How many application rows one declaration may carry: a pane's bound, because a declarer
/// needing more gestures above every mode is claiming the keyboard.
// WL-DESK-07 -- agents/workshop/desktop.md
inline constexpr std::size_t kMaxAppActionRows = 32;

/// How many rows one pane may declare: its own bound on what a chatty provider can make this
/// session retain.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline constexpr std::size_t kMaxPaneActionRows = 32;

/// THE BOUNDS ON ONE ROW'S TWO STRINGS, in BYTES -- an id is spelled in the keymap file
/// and a label on the band, and neither is a place for a paragraph.
inline constexpr std::size_t kMaxPaneActionIdLen = 64;
inline constexpr std::size_t kMaxPaneActionLabelLen = 32;
static_assert(kMaxPaneActionIdLen == kMaxPaneMenuIdLen,
              "a menu row's id meets a declared action's id law, published in the pane protocol");

/// The one sentence the collision law says, at the file's admission and at a pane's, so a maker
/// reads the same refusal whichever party arrived second.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline std::string collision_sentence(const Gesture& g, std::string_view a, std::string_view b) {
    return "`" + gesture_word(g) + "` is authored for both `" + std::string(a) + "` and `" +
           std::string(b) + "`, which can be active together -- one of them must move";
}

// ---- The keymap value --------------------------------------------------------------------

/// THE EFFECTIVE BINDING TRUTH: the declaration defaults plus the maker's applied
/// overrides, plus what could not be applied and is preserved.
// WL-KEY-01, WL-KEY-07 -- agents/workshop/keyboard.md
struct Keymap {
    std::int64_t legend = legend_mode::kDefault;
    /// Every override row the maker wrote, verbatim and in authored order -- what a save
    /// writes back, so a load-save round trip edits nothing it was asked to preserve.
    // WL-KEY-07 -- agents/workshop/keyboard.md
    std::vector<AuthoredOverride> authored;
    /// The rows this build could resolve and admit, as executable truth. DERIVED from
    /// `authored` by `apply_overrides`, never authored directly.
    std::vector<std::pair<Act, Gesture>> overrides;
    /// Accepted-with-a-caveat: the honest note about authored gestures with a known
    /// backend gap (see `posix_gap`), spoken once at load and kept nowhere else.
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

    /// Every gesture one declaration row answers to now, in authored order: the maker's overrides
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
    /// remembered pane together -- a maker typing into a menu over a pane is not typing into the
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
    /// the OPENER's binding wherever the maker moved it.
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

/// Action ids whose owner changed, and the id an authored row for each is read as now: the owner
/// moving changed no meaning, so the old id's row applies to the new one unless that is authored
/// itself, and the load names the rename. `workshop.picker` is not here: its meaning changed.
// WL-KEY-06 -- agents/workshop/keyboard.md
struct RenamedAction {
    const char* was;
    const char* now;
};
inline constexpr RenamedAction kRenamedActions[] = {
    {"workshop.terminal", "desktop.terminal"},
    {"workshop.hotkeys", "desktop.hotkeys"},
};

/// The id an authored row for `was` is read as now, or nullptr.
inline const char* renamed_to(std::string_view was) noexcept {
    for (const RenamedAction& r : kRenamedActions) {
        if (was == r.was) {
            return r.now;
        }
    }
    return nullptr;
}

/// Action ids that retired with what they acted on. An authored row for one is kept byte for byte
/// and the load says what retired; nothing answers it and no pane may declare it -- though a pane
/// naming one as the row it stands in for is admitted, standing in for nothing.
// WL-KEY-06 -- agents/workshop/keyboard.md
struct RetiredAction {
    const char* id;
    const char* with;    ///< what retired and took the action with it, in a maker's words
    const char* instead; ///< what a maker reaches for now, or empty when nothing took its place
};
/// WHERE THE PICKER'S AND THE HOST PANE MANAGER'S ACTS WENT, said once each for the rows below.
inline constexpr const char* kToPaneManager =
    "the desktop's Pane Manager (`desktop.panes`) opens and closes panes";
inline constexpr const char* kToPaneManagerAndInfo =
    "the desktop's Pane Manager (`desktop.panes`) opens and closes panes; Info inspects one";
inline constexpr const char* kToArranging = "arranging a pane (`workshop.manage`) orders it";
inline constexpr const char* kToInfoRows = "Info's own rows commit and cancel its edits";

inline constexpr RetiredAction kRetiredActions[] = {
    {"document.save", "the object document", ""},
    {"document.open", "the object document", ""},
    {"object.new", "the object canvas", ""},
    {"object.delete", "the object canvas", ""},
    {"object.left", "the object canvas", ""},
    {"object.down", "the object canvas", ""},
    {"object.up", "the object canvas", ""},
    {"object.right", "the object canvas", ""},
    {"object.narrower", "the object canvas", ""},
    {"object.taller", "the object canvas", ""},
    {"object.shorter", "the object canvas", ""},
    {"object.wider", "the object canvas", ""},
    {"object.next", "the object canvas", ""},
    {"workspace.narrower", "the object canvas", ""},
    {"workspace.wider", "the object canvas", ""},
    {"workshop.picker", "the `p` picker", kToPaneManager},
    {"picker.up", "the `p` picker", kToPaneManager},
    {"picker.down", "the `p` picker", kToPaneManager},
    {"picker.choose", "the `p` picker", kToPaneManager},
    {"picker.close", "the `p` picker", kToPaneManager},
    {"pane-editor.up", "the host's Pane Manager", kToPaneManagerAndInfo},
    {"pane-editor.down", "the host's Pane Manager", kToPaneManagerAndInfo},
    {"pane-editor.choose", "the host's Pane Manager", kToPaneManagerAndInfo},
    {"pane-editor.switch", "the host's Pane Manager", kToPaneManagerAndInfo},
    {"pane-editor.open", "the host's Pane Manager", kToPaneManagerAndInfo},
    {"pane-editor.front", "the host's Pane Manager", kToArranging},
    {"pane-editor.back", "the host's Pane Manager", kToArranging},
    {"pane-editor.raise", "the host's Pane Manager", kToArranging},
    {"pane-editor.lower", "the host's Pane Manager", kToArranging},
    {"draft.commit", "the host's Pane Manager", kToInfoRows},
    {"draft.cancel", "the host's Pane Manager", kToInfoRows},
};

/// What retired with `id`, or nullptr when it is not a retired Workshop action.
inline const char* retired_with(std::string_view id) noexcept {
    for (const RetiredAction& r : kRetiredActions) {
        if (id == r.id) {
            return r.with;
        }
    }
    return nullptr;
}

/// ...AND WHAT A MAKER REACHES FOR NOW, or nullptr for an id that is not retired.
inline const char* retired_instead(std::string_view id) noexcept {
    for (const RetiredAction& r : kRetiredActions) {
        if (id == r.id) {
            return r.instead;
        }
    }
    return nullptr;
}

/// Whether the component's editable-text vocabulary owns this gesture wherever text has
/// the keyboard.
// WL-KEY-08 -- agents/workshop/keyboard.md
inline bool component_owns_gesture(const Gesture& g) noexcept {
    for (const component::EditingGesture& row : component::kEditingVocabulary) {
        if (row.scancode == g.scancode && row.modifiers == g.modifiers) {
            return true;
        }
    }
    return false;
}

/// APPLY AUTHORED OVERRIDE ROWS TO A CANDIDATE KEYMAP -- the format-independent half of
/// admission, shared by the file reader and by any suite staging overrides directly.
// WL-KEY-08 -- agents/workshop/keyboard.md
inline Written apply_overrides(
    const std::vector<std::pair<std::string, std::string>>& rows, std::int64_t legend,
    Keymap& out) {
    Keymap candidate;
    candidate.legend = legend;
    for (const std::pair<std::string, std::string>& row : rows) {
        candidate.authored.push_back(AuthoredOverride{row.first, row.second});
        const ActionRow* declared = row_of_id(row.first);
        if (declared == nullptr) {
            // Preserved with its authored intent whole -- see AuthoredOverride.
            continue;
        }
        const ParsedGesture parsed = parse_gesture(row.second);
        if (!parsed.accepted) {
            return Written::no("`" + row.first + "`: " + parsed.refusal);
        }
        // SEVERAL ROWS FOR ONE ID ARE ONE ACTION WITH SEVERAL KEYS (WL-KEY-08). What is refused
        // is a set that says nothing coherent: the same gesture twice, or `none` beside a key.
        for (const std::pair<Act, Gesture>& already : candidate.overrides) {
            if (already.first != declared->act) {
                continue;
            }
            if (already.second == parsed.gesture) {
                return Written::no("`" + row.first + "` is authored twice with `" + row.second +
                                   "` -- one row per key");
            }
            if (!is_bound(already.second) || !is_bound(parsed.gesture)) {
                return Written::no("`" + row.first +
                                   "` is authored both as `none` and as a key -- disable it "
                                   "or bind it, not both");
            }
        }
        // A kNoText row is active only where no text has the keyboard, so a bare
        // printable or an editing chord on one can never be swallowed by a field -- which
        // is why the two guards below are the global rows' alone.
        if (declared->context == KeyContext::kGlobal ||
            declared->context == KeyContext::kUnlessOwned) {
            if (parsed.gesture.modifiers == mod::kNone &&
                !expected_text_of(parsed.gesture.scancode, parsed.gesture.modifiers)
                     .empty()) {
                return Written::no(
                    "`" + row.first + "`: `" + row.second +
                    "` is a bare printable, and a bare printable cannot be global once "
                    "anything on the screen can take text");
            }
            if (component_owns_gesture(parsed.gesture)) {
                return Written::no(
                    "`" + row.first + "`: `" + row.second +
                    "` is the editing vocabulary's own gesture, which every text field "
                    "would consume first");
            }
        }
        candidate.overrides.emplace_back(declared->act, parsed.gesture);
        const char* gap = posix_gap(parsed.gesture);
        if (gap != nullptr) {
            if (!candidate.note.empty()) {
                candidate.note += "; ";
            }
            candidate.note += "`" + row.second + "` (" + row.first + "): " + gap;
        }
    }
    // The collision check runs over the effective map: an override can land on another action's
    // default. Rows of one action are one meaning.
    for (std::size_t i = 0; i < kActionCatalogCount; ++i) {
        for (std::size_t j = i + 1; j < kActionCatalogCount; ++j) {
            const ActionRow& a = kActionCatalog[i];
            const ActionRow& b = kActionCatalog[j];
            if (a.act == b.act || !contexts_intersect(a.context, b.context)) {
                continue;
            }
            // Two actions that answer to no key hold no gesture between them.
            for (const Gesture& ga : candidate.row_gestures(a)) {
                if (!is_bound(ga)) {
                    continue;
                }
                if (candidate.row_answers(b, ga)) {
                    return Written::no(collision_sentence(ga, a.id, b.id));
                }
            }
        }
    }
    out = std::move(candidate);
    return Written::ok();
}

/// THE GESTURES A MAKER'S FILE AUTHORED FOR ONE ID, in authored order, judged as a set: every
/// row parses, no gesture twice, and `none` stands alone. `moved` says the file named the id at
/// all; an id it did not name keeps its declared default. With `renamed_too`, rows written for
/// an id's OLD name are read for it when the new name is not authored (`kRenamedActions`).
// WL-KEY-08 -- agents/workshop/keyboard.md
struct AuthoredGestures {
    bool moved = false;
    std::vector<Gesture> gestures;
    Written outcome = Written::ok();
};

inline AuthoredGestures authored_gestures_for(const std::vector<AuthoredOverride>& authored,
                                              std::string_view id, bool renamed_too) {
    AuthoredGestures out;
    const auto take = [&out, id](const AuthoredOverride& o, const std::string& spelled_as) {
        const ParsedGesture parsed = parse_gesture(o.gesture);
        if (!parsed.accepted) {
            out.outcome = Written::no("`" + spelled_as + "`: " + parsed.refusal);
            return false;
        }
        for (const Gesture& already : out.gestures) {
            if (already == parsed.gesture) {
                out.outcome = Written::no("`" + std::string(id) + "` is authored twice with `" +
                                          o.gesture + "` -- one row per key");
                return false;
            }
            if (!is_bound(already) || !is_bound(parsed.gesture)) {
                out.outcome = Written::no("`" + std::string(id) +
                                          "` is authored both as `none` and as a key -- "
                                          "disable it or bind it, not both");
                return false;
            }
        }
        out.gestures.push_back(parsed.gesture);
        out.moved = true;
        return true;
    };
    for (const AuthoredOverride& o : authored) {
        if (o.action == id && !take(o, std::string(id))) {
            return out;
        }
    }
    if (!out.moved && renamed_too) {
        for (const AuthoredOverride& o : authored) {
            const char* now = renamed_to(o.action);
            if (now == nullptr || id != now) {
                continue;
            }
            if (!take(o, o.action + " (read as `" + std::string(id) + "`)")) {
                return out;
            }
        }
    }
    return out;
}

// ---- A pane's declared rows, joined under the same law ------------------------------------

/// A DECLARED ROW'S TWO STRINGS, judged the way a descriptor's are (`check_pane_text`,
/// setup.hpp): in bytes, before anything is kept, naming the field. An id is what a
/// keymap file spells and a legend never prints, so it may hold no space; a label is what
/// a legend prints, so a space is fine and a control byte is not.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline Written check_pane_action_text(const std::string& text, const char* which,
                                      std::size_t limit, bool spaces_allowed) {
    if (text.empty()) {
        return Written::no(std::string("a pane action's ") + which + " cannot be empty");
    }
    if (text.size() > limit) {
        return Written::no(std::string("a pane action's ") + which + " is at most " +
                           std::to_string(limit) + " bytes");
    }
    bool anything = false;
    for (const char c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20u || byte >= 0x7Fu) {
            return Written::no(std::string("a pane action's ") + which +
                               " must be printable ASCII");
        }
        if (byte == ' ' && !spaces_allowed) {
            return Written::no(std::string("a pane action's ") + which +
                               " cannot contain a space");
        }
        if (byte != ' ') {
            anything = true;
        }
    }
    if (!anything) {
        return Written::no(std::string("a pane action's ") + which +
                           " needs more than spaces in it");
    }
    return Written::ok();
}

/// DOES ANY ROW OF THIS ONE DECLARATION STAND IN FOR THIS HOST ACTION? The collision law's
/// question, asked of the pane rather than of a row, because that is the scope dispatch uses.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline bool superseded_here(const std::vector<PaneRow>& rows, const std::string& action_id) {
    for (const PaneRow& row : rows) {
        if (!row.supersedes.empty() && row.supersedes == action_id) {
            return true;
        }
    }
    return false;
}

/// Join one pane's declared rows into a keymap, or say why not -- over a value, so a suite asks it
/// with no bus. In order: the row count; each id and label; each default gesture (an unbound row
/// carries `kUnknown`); no id twice and none of Workshop's own; then the maker's overrides by id;
/// then the collision law against the host rows active while a pane holds the keys and this
/// pane's own rows. Atomic: a refusal writes nothing, so the previous rows stand.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline Written join_pane_rows(Keymap& k, std::int64_t pane,
                              const std::vector<v2::PaneActionRow>& declared) {
    if (declared.size() > kMaxPaneActionRows) {
        return Written::no("a pane declares at most " + std::to_string(kMaxPaneActionRows) +
                           " actions -- this one declared " + std::to_string(declared.size()));
    }
    constexpr std::int64_t kKnownModifiers = mod::kCtrl | mod::kShift | mod::kAlt | mod::kSuper;
    std::vector<PaneRow> rows;
    rows.reserve(declared.size());
    for (const v2::PaneActionRow& d : declared) {
        const Written id = check_pane_action_text(d.id, "id", kMaxPaneActionIdLen, false);
        if (!id.accepted) {
            return id;
        }
        const Written label =
            check_pane_action_text(d.label, "label", kMaxPaneActionLabelLen, true);
        if (!label.accepted) {
            return Written::no("`" + d.id + "`: " + label.refusal);
        }
        if (row_of_id(d.id) != nullptr) {
            return Written::no("`" + d.id +
                               "` is Workshop's own action id -- a pane's ids live in its "
                               "own namespace");
        }
        // ...AND SO IS ONE THAT RETIRED: a maker's authored row for it is kept, and must not
        // come to move a stranger's key because a pane borrowed the spelling.
        if (const char* with = retired_with(d.id)) {
            return Written::no("`" + d.id + "` was Workshop's own action id, retired with " +
                               with + " -- a pane's ids live in its own namespace");
        }
        for (const PaneRow& earlier : rows) {
            if (earlier.id == d.id) {
                return Written::no("`" + d.id + "` is declared twice -- one row per action");
            }
        }
        if ((d.modifiers & ~kKnownModifiers) != 0) {
            return Written::no("`" + d.id + "`: modifier bits this keymap does not know");
        }
        if (d.scancode == scan::kUnknown) {
            if (d.modifiers != mod::kNone) {
                return Written::no("`" + d.id +
                                   "`: a row with no default key cannot carry modifiers");
            }
        } else if (key_name_of(d.scancode) == nullptr) {
            return Written::no("`" + d.id + "`: scancode " + std::to_string(d.scancode) +
                               " is not a key this keymap can name");
        }
        if (!d.supersedes.empty()) {
            const ActionRow* stands_for = row_of_id(d.supersedes);
            // ...or an above-the-modes application row (WL-DESK-07), which would otherwise take a
            // gesture from a pane holding the keyboard. A default-class row is asked only where
            // the keys did not cross here, so standing in for one is refused.
            const AppRow* app_stands_for = k.app_row_of_id(d.supersedes);
            // A RETIRED ID IS STOOD IN FOR BY NOBODY: the row is the pane's own, and there is no
            // host row for it to stand down. A pane built before the retirement keeps its keys.
            if (stands_for == nullptr && app_stands_for == nullptr &&
                retired_with(d.supersedes) != nullptr) {
                rows.push_back(
                    PaneRow{d.id, d.label, Gesture{d.scancode, d.modifiers}, std::string()});
                continue;
            }
            if (stands_for == nullptr && app_stands_for == nullptr) {
                return Written::no("`" + d.id + "`: `" + d.supersedes +
                                   "` is not an action id this Workshop knows -- a row can "
                                   "only stand in for an action that exists");
            }
            if (app_stands_for != nullptr && app_stands_for->precedence != 0) {
                return Written::no("`" + d.id + "`: `" + d.supersedes +
                                   "` is answered only where the keys did not reach this "
                                   "pane, so there is nothing here to stand in for");
            }
            if (stands_for != nullptr && stands_for->context != KeyContext::kUnlessOwned) {
                return Written::no("`" + d.id + "`: `" + d.supersedes +
                                   "` is not an action a pane may own -- only the rows "
                                   "Workshop declares a pane can stand in for");
            }
        }
        rows.push_back(PaneRow{d.id, d.label, Gesture{d.scancode, d.modifiers}, d.supersedes});
    }
    // The maker's own file, applied to the ids it names: several authored rows for one id repeat
    // the pane's row once per gesture, so dispatch answers to any and a legend spells the first.
    {
        std::vector<PaneRow> widened;
        widened.reserve(rows.size());
        for (const PaneRow& row : rows) {
            const AuthoredGestures set = authored_gestures_for(k.authored, row.id, false);
            if (!set.outcome.accepted) {
                return set.outcome;
            }
            if (!set.moved) {
                widened.push_back(row);
                continue;
            }
            for (const Gesture& g : set.gestures) {
                PaneRow moved = row;
                moved.gesture = g;
                widened.push_back(std::move(moved));
            }
        }
        rows = std::move(widened);
    }
    // THE COLLISION LAW, over the effective map: the built-in rows active in a pane's
    // context, and this pane's own rows against each other. Same words as the file's.
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!is_bound(rows[i].gesture)) {
            continue;
        }
        for (const ActionRow& host : kActionCatalog) {
            if (!contexts_intersect(host.context, KeyContext::kPane)) {
                continue;
            }
            // ...except the ones this pane stands in for: a superseded row is not requestable
            // anywhere in the pane, so it is judged for the pane, never per row.
            if (superseded_here(rows, host.id)) {
                continue;
            }
            if (k.row_answers(host, rows[i].gesture)) {
                return Written::no(collision_sentence(rows[i].gesture, host.id, rows[i].id));
            }
        }
        for (std::size_t j = i + 1; j < rows.size(); ++j) {
            if (rows[j].id != rows[i].id && rows[j].gesture == rows[i].gesture) {
                return Written::no(collision_sentence(rows[i].gesture, rows[i].id, rows[j].id));
            }
        }
        // ...and against the application rows (WL-DESK-07): active in every context, they meet
        // every pane's rows as a `kUnlessOwned` host row does, and stand down only for a pane that
        // declares `supersedes`.
        for (const AppRow& row : k.app) {
            // A default-class row is asked only where the keys did not cross to this pane, so it
            // and this pane's row can never both fire.
            if (row.precedence != 0 || !is_bound(row.gesture) ||
                superseded_here(rows, row.id)) {
                continue;
            }
            if (row.gesture == rows[i].gesture) {
                return Written::no(collision_sentence(rows[i].gesture, row.id, rows[i].id));
            }
        }
    }
    for (PaneRows& p : k.panes) {
        if (p.pane == pane) {
            p.rows = std::move(rows);
            return Written::ok();
        }
    }
    k.panes.push_back(PaneRows{pane, std::move(rows)});
    return Written::ok();
}

/// Join the application rows one participating owner declared (WL-DESK-07): `join_pane_rows` one
/// scope out -- the same bounds, namespace rule, overrides, collision law and atomic outcome --
/// judged against every host row and every pane's rows in force. The caller re-joins the panes
/// after, so both arrival orders end with one gesture meaning one thing.
// WL-DESK-07 -- agents/workshop/desktop.md
inline Written join_app_rows(Keymap& k, const std::vector<AppRow>& declared) {
    if (declared.size() > kMaxAppActionRows) {
        return Written::no("an application declares at most " +
                           std::to_string(kMaxAppActionRows) + " actions -- this one declared " +
                           std::to_string(declared.size()));
    }
    std::vector<AppRow> rows;
    rows.reserve(declared.size());
    for (const AppRow& d : declared) {
        const Written id = check_pane_action_text(d.id, "id", kMaxPaneActionIdLen, false);
        if (!id.accepted) {
            return id;
        }
        const Written label =
            check_pane_action_text(d.label, "label", kMaxPaneActionLabelLen, true);
        if (!label.accepted) {
            return Written::no("`" + d.id + "`: " + label.refusal);
        }
        if (row_of_id(d.id) != nullptr) {
            return Written::no("`" + d.id +
                               "` is Workshop's own action id -- an application's ids live in "
                               "its own namespace");
        }
        for (const AppRow& earlier : rows) {
            if (earlier.id == d.id) {
                return Written::no("`" + d.id + "` is declared twice -- one row per action");
            }
        }
        if (d.precedence != 0 && d.precedence != 1) {
            // A precedence this build cannot name is refused, not guessed: defaulting it would
            // give a later protocol's row the strongest position by accident.
            return Written::no("`" + d.id + "`: precedence " + std::to_string(d.precedence) +
                               " is not one this Workshop knows (0 above the modes, 1 default)");
        }
        constexpr std::int64_t kKnown = mod::kCtrl | mod::kShift | mod::kAlt | mod::kSuper;
        if ((d.gesture.modifiers & ~kKnown) != 0) {
            return Written::no("`" + d.id + "`: modifier bits this keymap does not know");
        }
        if (d.gesture.scancode == scan::kUnknown) {
            if (d.gesture.modifiers != mod::kNone) {
                return Written::no("`" + d.id +
                                   "`: a row with no default key cannot carry modifiers");
            }
        } else if (key_name_of(d.gesture.scancode) == nullptr) {
            return Written::no("`" + d.id + "`: scancode " +
                               std::to_string(d.gesture.scancode) +
                               " is not a key this keymap can name");
        }
        rows.push_back(d);
    }
    // THE MAKER'S OWN FILE, applied to the ids it names -- including `none`, which is how a
    // maker DISABLES an application default rather than moving it (WL-DESK-07) -- and, for an id
    // that changed owners, the row written for its old id when the new one is not authored.
    {
        std::vector<AppRow> widened;
        widened.reserve(rows.size());
        for (const AppRow& row : rows) {
            const AuthoredGestures set = authored_gestures_for(k.authored, row.id, true);
            if (!set.outcome.accepted) {
                return set.outcome;
            }
            if (!set.moved) {
                widened.push_back(row);
                continue;
            }
            for (const Gesture& g : set.gestures) {
                AppRow moved = row;
                moved.gesture = g;
                widened.push_back(std::move(moved));
            }
        }
        rows = std::move(widened);
    }
    // A ROW ANSWERED ABOVE EVERY MODE MEETS EVERY TEXT FIELD, so the file's two walls for a global
    // row are its walls too: no bare printable (every field would lose that character to it), and
    // no chord the text box owns (the field would consume it first, or never see it again).
    for (const AppRow& row : rows) {
        if (row.precedence != 0 || !is_bound(row.gesture)) {
            continue;
        }
        if (row.gesture.modifiers == mod::kNone &&
            !expected_text_of(row.gesture.scancode, row.gesture.modifiers).empty()) {
            return Written::no("`" + row.id + "`: `" + gesture_word(row.gesture) +
                               "` is a bare printable, and a bare printable cannot be answered "
                               "above every mode once anything on the screen can take text");
        }
        if (component_owns_gesture(row.gesture)) {
            return Written::no("`" + row.id + "`: `" + gesture_word(row.gesture) +
                               "` is the editing vocabulary's own gesture, which every text "
                               "field would consume first");
        }
    }
    // THE COLLISION LAW, over the effective map: these rows against the host's own, against
    // each other, and against every pane's rows in force. Same words as the file's.
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!is_bound(rows[i].gesture)) {
            continue; // a disabled row collides with nothing -- it answers to no key
        }
        // The precedence class decides what a row can collide with. An above-the-modes row is
        // answered before the host rows and before the keys cross to a pane, so it meets all of
        // them. A default row meets none: it is asked only where the context claimed nothing and
        // no pane took the keys (`context.back` and `desktop.deselect` share Escape).
        if (rows[i].precedence == 0) {
            for (const ActionRow& host : kActionCatalog) {
                if (k.row_answers(host, rows[i].gesture)) {
                    return Written::no(
                        collision_sentence(rows[i].gesture, host.id, rows[i].id));
                }
            }
            for (const PaneRows& p : k.panes) {
                if (superseded_here(p.rows, rows[i].id)) {
                    continue; // that pane stands in for this row; inside it, it is not live
                }
                for (const PaneRow& pane_row : p.rows) {
                    if (pane_row.gesture == rows[i].gesture) {
                        return Written::no(
                            collision_sentence(rows[i].gesture, rows[i].id, pane_row.id));
                    }
                }
            }
        }
        // ...AND AGAINST EACH OTHER, IN THE SAME CLASS. Two application rows of one class on
        // one gesture are two declarations that could both fire, whichever class it is.
        for (std::size_t j = i + 1; j < rows.size(); ++j) {
            if (rows[j].id != rows[i].id && rows[j].precedence == rows[i].precedence &&
                rows[j].gesture == rows[i].gesture) {
                return Written::no(collision_sentence(rows[i].gesture, rows[i].id, rows[j].id));
            }
        }
    }
    k.app = std::move(rows);
    return Written::ok();
}

/// FORGET ONE PANE'S ROWS -- what a re-join under a new keymap file does with a pane
/// whose rows the file's own bindings now collide with.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline void drop_pane_rows(Keymap& k, std::int64_t pane) {
    for (std::size_t i = 0; i < k.panes.size(); ++i) {
        if (k.panes[i].pane == pane) {
            k.panes.erase(k.panes.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_KEYMAP_HPP
