// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_KEYMAP_HPP
#define ZENGINE_WORKSHOP_KEYMAP_HPP

// ONE EXECUTABLE BINDING TRUTH (KEY-0).
//
// Before this file, Workshop's keyboard was two independently authored worlds: ~100
// executable gestures in hand-written switches, and ~20 claim sites -- band rows, headings,
// notice hints, a boot line, a cheat sheet -- each spelling the same bindings again by hand.
// KEY-R0 measured the two drifting in six places. This file is the repair's shape:
//
//   ACTION     a stable identity and a human meaning        (the declaration rows below)
//   BINDING    the gesture that requests the action         (default here + maker override)
//   EXECUTION  the owner that performs it                   (untouched -- see below)
//
// The keymap owns BINDING TRUTH and nothing else. Every dispatch site still calls its own
// existing operations; what changed is only how a gesture becomes an action identity:
//
//     switch (session_.keymap.action_for(ctx, k.scancode, k.modifiers)) {
//     case Act::kCreate: create_object(); break;            // the owner's own function
//     ...
//
// There is deliberately NO callback, NO std::function, NO command bus, NO registry object
// and NO new wire shape here. The keymap can NAME actions and gestures; it can perform
// nothing -- a remap changes how an action is requested, never who may perform it. The
// declaration rows are constexpr because that is the honest shape of what exists
// (kPanelCatalog's and kTerminalVerbs' own argument): there are no parties unknown adding
// entries, and provider-contributed declarations are deliberately out of KEY-0.
//
// EXACT MODIFIER MATCHING. An application binding matches exactly the observed modifier
// state -- `n` does not also mean `Ctrl+N`, and the accidental subset aliases the old
// per-site `held()` tests produced (Ctrl+N created, Alt+Q quit) are gone on purpose. The
// component's editable-text vocabulary keeps its own modifier grammar behind
// `TextBox::consume`; this rule governs the application keymap only.
//
// THE CONTEXT IS RESOLVED, NEVER STORED. `keyboard_context()` (screen.hpp, beside Session)
// derives the current context from live session state, spelling the routing chain ONCE --
// it replaced the two hand-kept mirrors KEY-R0 found (`editable_text_has_keyboard`,
// `paste_owner_now`), each annotated "MIRRORS THE CHAIN branch for branch". There is no
// context stack, no registration order and no focus framework: the chain's fixed written
// order, as a value.

#include "property.hpp" // Written -- the one refusal-with-reason shape this package has

#include "component/text_box.hpp"
#include "input/vocabulary.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHERE THE KEYBOARD CURRENTLY GOES -- the routing chain's branches, as values.
///
/// The first ten are the resolvable contexts: exactly the branches of
/// `WorkshopWeave::on(KeyPressed)`'s chain, with pane management split by its submode
/// because its sub-switches are genuinely different vocabularies (the arrows place a pane
/// in one and pull an edge in another). The last two are DECLARATION-ONLY activity
/// classes -- `keyboard_context()` never returns them:
///
///   kGlobal   active in every context: answered above the whole chain, the `^s`/`^o`
///             position. A focused pane does not receive these, exactly as it never
///             received the old four.
///   kNoText   active wherever no editable text has the keyboard -- `^c`-quit's measured
///             activity set (TEXT-0's gate, now a declarable fact instead of a predicate
///             mirroring the chain by hand).
enum class KeyContext : std::uint8_t {
    kCommand,
    kTerminal,
    kNaming,
    kPicker,
    kAttention,
    kContext,
    kPane,
    kDraft,
    kManageSelect,
    kManageMove,
    kManageSize,
    kManageReset,
    kGlobal,
    kNoText,
};

/// Does this context hand ordinary keys to something that takes text? The `^c` gate's one
/// question (TEXT-0), now derived from the resolved context instead of mirrored by hand.
/// A focused runtime pane counts: it receives every bare key and every character precisely
/// because it may hold editable text, and Workshop cannot see whether it currently does.
inline constexpr bool context_takes_text(KeyContext c) noexcept {
    return c == KeyContext::kTerminal || c == KeyContext::kNaming || c == KeyContext::kPane ||
           c == KeyContext::kDraft;
}

/// Is an action declared for `declared` requestable while `current` is the resolved
/// context? This is the whole activity rule; there is no priority between simultaneously
/// active rows because same-gesture collisions across intersecting contexts are refused
/// at admission (below), never resolved by order.
inline constexpr bool active_in(KeyContext declared, KeyContext current) noexcept {
    if (declared == KeyContext::kGlobal) {
        return true;
    }
    if (declared == KeyContext::kNoText) {
        return !context_takes_text(current);
    }
    return declared == current;
}

/// Can two declared contexts ever be active at the same moment? The collision question's
/// other half: two actions conflict only when a maker could press their shared gesture in
/// a state where both are requestable. Reusing one gesture across mutually exclusive
/// contexts is the working norm (`s` names a setup in command mode and sizes a pane in
/// management), and stays legal.
inline constexpr bool contexts_intersect(KeyContext a, KeyContext b) noexcept {
    if (a == b || a == KeyContext::kGlobal || b == KeyContext::kGlobal) {
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

/// One gesture as the wire reports it: a named scancode and the EXACT observed modifier
/// bits. This is the whole binding grammar -- KEY-R0's floor, deliberately: no sequences,
/// no leaders, no macros, no timing, and nothing a `KeyPressed` cannot carry.
struct Gesture {
    std::int64_t scancode = 0;
    std::int64_t modifiers = 0;

    friend constexpr bool operator==(const Gesture& a, const Gesture& b) noexcept {
        return a.scancode == b.scancode && a.modifiers == b.modifiers;
    }
};

/// THE ACTION IDENTITIES, as the code spells them. The durable spelling is the dotted
/// string on each declaration row; this enum exists so a dispatch site can `switch` on the
/// answer, and it is deliberately not persisted, not on any wire, and not stable across
/// builds -- the string is.
enum class Act : std::uint8_t {
    kNone = 0,
    // -- above every mode -------------------------------------------------------------
    kQuit,
    kSaveDocument,
    kOpenDocument,
    kTerminalToggle,
    kHotkeys,
    kAttention,
    // -- command mode ------------------------------------------------------------------
    kObjectNew,
    kObjectDelete,
    kObjectLeft,
    kObjectDown,
    kObjectUp,
    kObjectRight,
    kObjectNarrower,
    kObjectTaller,
    kObjectShorter,
    kObjectWider,
    kObjectNext,
    kInfoUp,
    kInfoDown,
    kInfoEdit,
    kWorkspaceNarrower,
    kWorkspaceWider,
    kPicker,
    kBuild,
    kBuildRealize,
    kRecipeNext,
    kRecipeBack,
    kBuildFrontier,
    kSetupName,
    kSetupRestore,
    kManage,
    kPaneTitles,
    // -- the Terminal line's controls --------------------------------------------------
    kTerminalSubmit,
    kTerminalBack,
    kTerminalUp,
    kTerminalDown,
    kTerminalComplete,
    // -- the picker --------------------------------------------------------------------
    kPickerUp,
    kPickerDown,
    kPickerChoose,
    kPickerClose,
    // -- the current-condition view -----------------------------------------------------
    kAttentionUp,
    kAttentionDown,
    kAttentionDismiss,
    kAttentionClose,
    // -- the setup-name editor's controls ----------------------------------------------
    kNamingCommit,
    kNamingCancel,
    // -- a live property draft's controls ----------------------------------------------
    kDraftCommit,
    kDraftCancel,
    // -- pane management ---------------------------------------------------------------
    kManageNext,
    kManagePrevious,
    kManageMove,
    kManageSize,
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
    kManageEdge,
    kManagePullLeft,
    kManagePullRight,
    kManagePullUp,
    kManagePullDown,
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
///
/// AN ACTION MAY OWN MORE THAN ONE ROW, and a maker's override moves ALL of an action's
/// rows to the one authored gesture. Three actions use it: `workshop.quit` (`^c` wherever
/// text cannot take the keyboard, `q` as a command), `manage.next` (tab and down are one
/// meaning), and `manage.done` (esc backs out of each of the three submodes). Two rows of
/// one action are one meaning twice requestable, so they are never a collision.
struct ActionRow {
    Act act = Act::kNone;
    const char* id = "";
    const char* label = "";
    KeyContext context = KeyContext::kCommand;
    Gesture gesture;
};

namespace scan = input::scan;
namespace mod = input::mod;

/// THE DECLARATIONS. Order inside a context group is presentation priority: the band's
/// help rows pack these left to right and cut what does not fit, so the gestures a maker
/// reaches for constantly come first.
///
/// The defaults are the shipped bindings KEY-R0 audited, with the phase's two decided
/// changes: the Terminal opener is `ctrl+t` (the old `shift+space` cannot arrive from the
/// POSIX terminal backend at all -- `ground_byte(' ')` infers Shift only on letters -- and
/// an invisible unreachable default is exactly what this file exists to end), and the
/// hotkey view is `ctrl+k` (from the measured portable free set; the conventional `?` and
/// F1 are both structurally unavailable on the console backends).
inline constexpr ActionRow kActionCatalog[] = {
    // -- above every mode -------------------------------------------------------------
    {Act::kQuit, "workshop.quit", "quit", KeyContext::kNoText, {scan::kC, mod::kCtrl}},
    {Act::kSaveDocument, "document.save", "save", KeyContext::kGlobal, {scan::kS, mod::kCtrl}},
    {Act::kOpenDocument, "document.open", "open", KeyContext::kGlobal, {scan::kO, mod::kCtrl}},
    {Act::kTerminalToggle, "workshop.terminal", "terminal", KeyContext::kGlobal,
     {scan::kT, mod::kCtrl}},
    {Act::kHotkeys, "workshop.hotkeys", "hotkeys", KeyContext::kGlobal,
     {scan::kK, mod::kCtrl}},
    // THE CURRENT-CONDITION VIEW, AND IT FOLLOWS THE KEYBOARD -- `^c`-quit's
    // class, for `^c`-quit's exact reason.
    //
    // `^a` IS THE MNEMONIC AND THE COMPONENT ALREADY OWNS IT (select all). That is not a
    // collision, it is the `kNoText` class doing the one job it exists for: the row is
    // active precisely where no editable text has the keyboard, so nothing can consume it
    // first and no text field ever loses its own gesture. `workshop.quit` shipped this
    // shape first -- `ctrl+c` is the copy chord AND the quit chord, told apart by where the
    // keys are going -- and the admission rule that refuses a component-owned
    // chord is scoped to `kGlobal` for the same reason.
    //
    // A GLOBAL WOULD HAVE COST A MNEMONIC AND BOUGHT THE TERMINAL. The measured portable
    // free set holds no chord that says "attention" (the conventional `?` and F1 are both
    // structurally unavailable on the console backends), and every free ctrl+letter echoes
    // some other bare gesture on this screen -- `^w` window, `^n` new, `^p` picker. What
    // this row gives up is reachability from inside a live text field, which is where a
    // maker is reading their own words rather than the tool's.
    //
    // `0x01` on every supported backend (input/translate.hpp), and no `posix_gap`.
    {Act::kAttention, "workshop.attention", "attention", KeyContext::kNoText,
     {scan::kA, mod::kCtrl}},
    // -- command mode ------------------------------------------------------------------
    {Act::kObjectNew, "object.new", "new", KeyContext::kCommand, {scan::kN, mod::kNone}},
    {Act::kObjectDelete, "object.delete", "delete", KeyContext::kCommand,
     {scan::kD, mod::kNone}},
    {Act::kQuit, "workshop.quit", "quit", KeyContext::kCommand, {scan::kQ, mod::kNone}},
    {Act::kObjectLeft, "object.left", "left", KeyContext::kCommand, {scan::kH, mod::kNone}},
    {Act::kObjectDown, "object.down", "down", KeyContext::kCommand, {scan::kJ, mod::kNone}},
    {Act::kObjectUp, "object.up", "up", KeyContext::kCommand, {scan::kK, mod::kNone}},
    {Act::kObjectRight, "object.right", "right", KeyContext::kCommand, {scan::kL, mod::kNone}},
    {Act::kObjectNarrower, "object.narrower", "narrower", KeyContext::kCommand,
     {scan::kH, mod::kShift}},
    {Act::kObjectTaller, "object.taller", "taller", KeyContext::kCommand,
     {scan::kJ, mod::kShift}},
    {Act::kObjectShorter, "object.shorter", "shorter", KeyContext::kCommand,
     {scan::kK, mod::kShift}},
    {Act::kObjectWider, "object.wider", "wider", KeyContext::kCommand,
     {scan::kL, mod::kShift}},
    {Act::kObjectNext, "object.next", "object", KeyContext::kCommand,
     {scan::kTab, mod::kNone}},
    {Act::kInfoEdit, "info.edit", "edit", KeyContext::kCommand,
     {scan::kReturn, mod::kNone}},
    {Act::kInfoUp, "info.up", "row up", KeyContext::kCommand, {scan::kUp, mod::kNone}},
    {Act::kInfoDown, "info.down", "row down", KeyContext::kCommand, {scan::kDown, mod::kNone}},
    {Act::kWorkspaceNarrower, "workspace.narrower", "narrow workspace", KeyContext::kCommand,
     {scan::kLeftBracket, mod::kNone}},
    {Act::kWorkspaceWider, "workspace.wider", "widen workspace", KeyContext::kCommand,
     {scan::kRightBracket, mod::kNone}},
    {Act::kPicker, "workshop.picker", "+ panel", KeyContext::kCommand, {scan::kP, mod::kNone}},
    {Act::kBuild, "builder.build", "build", KeyContext::kCommand, {scan::kB, mod::kNone}},
    {Act::kBuildRealize, "builder.build-realize", "build+realize", KeyContext::kCommand,
     {scan::kB, mod::kShift}},
    {Act::kRecipeNext, "builder.recipe", "recipe", KeyContext::kCommand,
     {scan::kC, mod::kNone}},
    {Act::kRecipeBack, "builder.recipe-back", "recipe back", KeyContext::kCommand,
     {scan::kC, mod::kShift}},
    {Act::kBuildFrontier, "builder.frontier", "frontier", KeyContext::kCommand,
     {scan::kF, mod::kNone}},
    {Act::kSetupName, "setup.name", "name/save setup", KeyContext::kCommand,
     {scan::kS, mod::kNone}},
    {Act::kSetupRestore, "setup.restore", "restore setup", KeyContext::kCommand,
     {scan::kR, mod::kNone}},
    {Act::kManage, "workshop.manage", "window", KeyContext::kCommand, {scan::kW, mod::kNone}},
    // WHAT CAN I DO WITH THIS? -- the keyboard door to the contextual-action surface
    // (CTX-0), on the subject command mode can truthfully name: the selected object, or
    // the empty room. The pointer's door is a right press, which needs no row here; this
    // row exists because a surface reachable only by a mouse button would be Workshop's
    // first gesture with no catalog identity -- exactly the drift this file ended.
    // `a` bare: portable, and free in every context that intersects kCommand (the globals
    // are all chords, kNoText holds `^c`/`^a`, and no other kCommand row spends it).
    {Act::kContextOpen, "workshop.context", "actions", KeyContext::kCommand,
     {scan::kA, mod::kNone}},
    // A PRESENTATION PREFERENCE WITH A KEY (WUX-1): whether the arrangeable panes paint
    // their title rows. Last in the command group because the band packs these in order
    // and a toggle a maker reaches for occasionally must not displace the gestures they
    // reach for constantly. `t` bare: portable (a plain letter arrives from every
    // backend), and free in every context that intersects kCommand -- the global rows are
    // all chords, kNoText holds only `^c`, and no other kCommand row spends it.
    {Act::kPaneTitles, "workshop.pane-titles", "titles", KeyContext::kCommand,
     {scan::kT, mod::kNone}},
    // -- the Terminal line's controls --------------------------------------------------
    {Act::kTerminalSubmit, "terminal.submit", "run the line", KeyContext::kTerminal,
     {scan::kReturn, mod::kNone}},
    {Act::kTerminalComplete, "terminal.complete", "what can this terminal say?",
     KeyContext::kTerminal, {scan::kTab, mod::kNone}},
    {Act::kTerminalUp, "terminal.previous", "completion up", KeyContext::kTerminal,
     {scan::kUp, mod::kNone}},
    {Act::kTerminalDown, "terminal.next", "completion down", KeyContext::kTerminal,
     {scan::kDown, mod::kNone}},
    {Act::kTerminalBack, "terminal.back", "dismiss list / clear line", KeyContext::kTerminal,
     {scan::kEscape, mod::kNone}},
    // -- the picker --------------------------------------------------------------------
    {Act::kPickerUp, "picker.up", "row up", KeyContext::kPicker, {scan::kUp, mod::kNone}},
    {Act::kPickerDown, "picker.down", "row down", KeyContext::kPicker,
     {scan::kDown, mod::kNone}},
    {Act::kPickerChoose, "picker.choose", "open or remove", KeyContext::kPicker,
     {scan::kReturn, mod::kNone}},
    {Act::kPickerClose, "picker.close", "cancel", KeyContext::kPicker,
     {scan::kEscape, mod::kNone}},
    // -- the current-condition view's own keys ------------------------------------------
    //
    // The picker's four, one purpose over. `d` rather than Return for the one gesture that
    // ACTS on a row, because Return in every other list here opens or commits and this one
    // does neither: it HIDES a presentation and changes nothing about what is true. A bare
    // letter is legal in a mode nothing in which takes text, exactly as the management
    // submodes' are.
    {Act::kAttentionUp, "attention.up", "row up", KeyContext::kAttention,
     {scan::kUp, mod::kNone}},
    {Act::kAttentionDown, "attention.down", "row down", KeyContext::kAttention,
     {scan::kDown, mod::kNone}},
    {Act::kAttentionDismiss, "attention.dismiss", "hide this one", KeyContext::kAttention,
     {scan::kD, mod::kNone}},
    {Act::kAttentionClose, "attention.close", "close", KeyContext::kAttention,
     {scan::kEscape, mod::kNone}},
    // -- the setup-name editor's controls ----------------------------------------------
    {Act::kNamingCommit, "naming.commit", "save the name", KeyContext::kNaming,
     {scan::kReturn, mod::kNone}},
    {Act::kNamingCancel, "naming.cancel", "cancel", KeyContext::kNaming,
     {scan::kEscape, mod::kNone}},
    // -- a live property draft's controls ----------------------------------------------
    {Act::kDraftCommit, "draft.commit", "commit", KeyContext::kDraft,
     {scan::kReturn, mod::kNone}},
    {Act::kDraftCancel, "draft.cancel", "cancel", KeyContext::kDraft,
     {scan::kEscape, mod::kNone}},
    // -- pane management ---------------------------------------------------------------
    {Act::kManageNext, "manage.next", "next pane", KeyContext::kManageSelect,
     {scan::kTab, mod::kNone}},
    {Act::kManageNext, "manage.next", "next pane", KeyContext::kManageSelect,
     {scan::kDown, mod::kNone}},
    {Act::kManagePrevious, "manage.previous", "previous pane", KeyContext::kManageSelect,
     {scan::kUp, mod::kNone}},
    {Act::kManageMove, "manage.move", "move", KeyContext::kManageSelect,
     {scan::kM, mod::kNone}},
    {Act::kManageSize, "manage.size", "size", KeyContext::kManageSelect,
     {scan::kS, mod::kNone}},
    {Act::kManageFront, "manage.front", "front", KeyContext::kManageSelect,
     {scan::kF, mod::kNone}},
    {Act::kManageBack, "manage.back", "back", KeyContext::kManageSelect,
     {scan::kB, mod::kNone}},
    {Act::kManageRaise, "manage.raise", "raise", KeyContext::kManageSelect,
     {scan::kR, mod::kNone}},
    {Act::kManageLower, "manage.lower", "lower", KeyContext::kManageSelect,
     {scan::kL, mod::kNone}},
    // REMOVE THE SELECTED PANE (CTX-0). Earned on its own keyboard merits, not minted for
    // the context menu: this mode's vocabulary is one-letter verbs on the selected pane,
    // and before this row a maker arranging windows could not remove one at all -- they
    // had to leave, open the picker, and find the row. `d` bare is free here (the
    // intersecting contexts are kGlobal's chords and kNoText's `^c`/`^a`).
    {Act::kManageRemove, "manage.remove", "remove", KeyContext::kManageSelect,
     {scan::kD, mod::kNone}},
    {Act::kManageReset, "manage.reset", "reset", KeyContext::kManageSelect,
     {scan::k0, mod::kNone}},
    {Act::kManageClose, "manage.close", "leave", KeyContext::kManageSelect,
     {scan::kEscape, mod::kNone}},
    {Act::kManagePlaceLeft, "manage.place-left", "place left", KeyContext::kManageMove,
     {scan::kLeft, mod::kNone}},
    {Act::kManagePlaceRight, "manage.place-right", "place right", KeyContext::kManageMove,
     {scan::kRight, mod::kNone}},
    {Act::kManagePlaceUp, "manage.place-up", "place up", KeyContext::kManageMove,
     {scan::kUp, mod::kNone}},
    {Act::kManagePlaceDown, "manage.place-down", "place down", KeyContext::kManageMove,
     {scan::kDown, mod::kNone}},
    {Act::kManageDone, "manage.done", "back", KeyContext::kManageMove,
     {scan::kEscape, mod::kNone}},
    {Act::kManageEdge, "manage.edge", "next edge", KeyContext::kManageSize,
     {scan::kTab, mod::kNone}},
    {Act::kManagePullLeft, "manage.pull-left", "pull left", KeyContext::kManageSize,
     {scan::kLeft, mod::kNone}},
    {Act::kManagePullRight, "manage.pull-right", "pull right", KeyContext::kManageSize,
     {scan::kRight, mod::kNone}},
    {Act::kManagePullUp, "manage.pull-up", "pull up", KeyContext::kManageSize,
     {scan::kUp, mod::kNone}},
    {Act::kManagePullDown, "manage.pull-down", "pull down", KeyContext::kManageSize,
     {scan::kDown, mod::kNone}},
    {Act::kManageDone, "manage.done", "back", KeyContext::kManageSize,
     {scan::kEscape, mod::kNone}},
    {Act::kManageResetPlace, "manage.reset-place", "reset place", KeyContext::kManageReset,
     {scan::kP, mod::kNone}},
    {Act::kManageResetWidth, "manage.reset-width", "reset width", KeyContext::kManageReset,
     {scan::kW, mod::kNone}},
    {Act::kManageResetHeight, "manage.reset-height", "reset height", KeyContext::kManageReset,
     {scan::kH, mod::kNone}},
    {Act::kManageResetOrder, "manage.reset-order", "reset order", KeyContext::kManageReset,
     {scan::kO, mod::kNone}},
    {Act::kManageDone, "manage.done", "back", KeyContext::kManageReset,
     {scan::kEscape, mod::kNone}},
    // -- the contextual-action surface (CTX-0) -----------------------------------------
    //
    // The picker's four, one purpose over: a list with a cursor and a gesture on the
    // selected row. `context.choose` is ONE action whose meaning the row decides -- a
    // group row descends, an action row requests (`picker.choose`'s own shape) -- and
    // `context.back` is Escape doing the appropriate smaller thing: out of an open group,
    // else out of the surface. The opener's own gesture also closes it, by `matches`,
    // like every other toggled surface here.
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
// A binding in the authored keymap file is one string: zero or more modifier words joined
// to one key name with `+` -- `ctrl+k`, `shift+h`, `[`. The key names are the named scan
// set and nothing else; punctuation keys are named by their own character because that is
// the spelling a hand editing a file reaches for. The canonical modifier order on the way
// out is ctrl, shift, alt, super; the parser accepts any order and refuses duplicates.

/// The written name of a named scancode, or nullptr for a value this grammar cannot say.
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
inline std::int64_t scancode_of_name(std::string_view name) noexcept {
    for (std::int64_t sc = 1; sc < 128; ++sc) {
        const char* word = key_name_of(sc);
        if (word != nullptr && name == word) {
            return sc;
        }
    }
    return 0;
}

/// A gesture as the FILE spells it: `ctrl+shift+z`. Total over any gesture whose scancode
/// has a name; the declaration table only holds those, and admission refuses the rest on
/// the way in.
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

/// A gesture as the SCREEN spells it -- the band's own compact voice: `^s`, `B` (the
/// capital IS shift+b, the Builder header's own long-standing spelling), `shift+space`,
/// `enter`, `esc`. One function, so a remapped binding is spelled identically on every
/// surface that names it.
inline std::string gesture_text(const Gesture& g) {
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

/// What parsing a written gesture produced: the gesture, or the refusal in words --
/// naming both what was found and what would have worked, because a maker looking at
/// their own file can fix that.
struct ParsedGesture {
    bool accepted = false;
    Gesture gesture;
    std::string refusal;
};

inline ParsedGesture parse_gesture(std::string_view text) {
    ParsedGesture out;
    if (text.empty()) {
        out.refusal = "a gesture cannot be empty";
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
/// nullptr when no such gap is known. This is STATIC knowledge about a backend's honest
/// reach (input/translate.hpp owns the details); Workshop cannot see which backend is
/// feeding it, so an authored gesture with a known gap is ACCEPTED and the gap is SAID --
/// never silently rewritten, which would be the exact silent default this family of files
/// keeps refusing. The old `shift+space` Terminal opener shipped for months inside this
/// gap with nothing saying so; this function is what that cost.
inline const char* posix_gap(const Gesture& g) noexcept {
    if ((g.modifiers & mod::kSuper) != 0) {
        return "super never arrives from a POSIX terminal";
    }
    if ((g.modifiers & mod::kAlt) != 0 && !is_posix_editing_scan(g.scancode)) {
        return "alt arrives only on the editing keys from a POSIX terminal";
    }
    if ((g.modifiers & mod::kCtrl) != 0 &&
        (g.scancode == scan::kH || g.scancode == scan::kI || g.scancode == scan::kJ ||
         g.scancode == scan::kM)) {
        return "ctrl+h/i/j/m are byte-identical to backspace/tab/newline/return on a POSIX "
               "terminal";
    }
    if ((g.modifiers & mod::kShift) != 0 && !is_letter_scan(g.scancode) &&
        !is_posix_editing_scan(g.scancode)) {
        return "shift is not observable on that key from a POSIX terminal";
    }
    return nullptr;
}

/// THE TEXT A CONSUMED PRINTABLE GESTURE'S OWN KEYSTROKE PRODUCES, or "" when none is
/// expected. This is the swallow rule's narrow correspondence (KEY-0): when the
/// application keymap consumes a gesture whose key also enters text, exactly that
/// character is owed to the gesture and must not land in whatever takes text next -- and
/// nothing else may ever be eaten, so this derives the expectation from the binding
/// instead of swallowing the next text unconditionally.
///
/// The correspondence is the same one the old hard-coded sites assumed (`" "`, `"s"`,
/// `"w"`): the key's US-layout face, matched by `same_keystroke`'s case-folding for
/// letters. A layout that produces something else leaves the flag unmatched and the
/// character through -- exactly the old sites' honest failure mode, not a new one. A
/// chord with ctrl, alt or super produces no text on any supported backend; a shifted
/// non-letter's face is the layout's own business, so nothing is claimed for it.
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

/// How much of the effective bindings the bottom band's two help rows project. A WORD in
/// the file and a closed set (WIND-2's mode-word law); `kDefault` is the file's one
/// canonical spelling of "no authored difference", and this build's default projection is
/// FULL. HIDDEN blanks the rows and does nothing else: the band's geometry is
/// `screen_of`'s and stays reserved, and no binding -- the hotkey view's included -- is
/// unbound by a maker choosing not to look at the legend.
namespace legend_mode {
inline constexpr std::int64_t kDefault = 0;
inline constexpr std::int64_t kFull = 1;
inline constexpr std::int64_t kCompact = 2;
inline constexpr std::int64_t kHidden = 3;
} // namespace legend_mode

/// One authored override row, exactly as written: an action id and a gesture, two
/// strings. This is the AUTHORED truth and it is preserved byte-for-byte in authored
/// order, whether or not this build can spend it -- the setup law's ACCEPTED clause,
/// verbatim: a well-formed reference this build cannot resolve, with all of its authored
/// intent, is not an error and must never become one. An unknown action's gesture string
/// is deliberately not judged against this build's grammar either: it is intent addressed
/// to whichever build declares the action, and the build that cannot spend a row is not
/// the build entitled to normalise it.
struct AuthoredOverride {
    std::string action;
    std::string gesture;
};

// ---- The keymap value --------------------------------------------------------------------

/// THE EFFECTIVE BINDING TRUTH: the declaration defaults plus the maker's applied
/// overrides, plus what could not be applied and is preserved. One value, read by
/// dispatch, by every help surface, and by persistence -- which is the whole phase: no
/// surface spells an executable gesture by hand any more.
///
/// It lives on the Session so the paint path -- a pure projection of what it is handed --
/// reads the SAME value dispatch reads. Defaults are in code (this struct
/// default-constructs to them); the authored file carries differences only; an absent
/// file IS this struct's default state, so deleting the file is returning to defaults.
struct Keymap {
    std::int64_t legend = legend_mode::kDefault;
    /// Every override row the maker wrote, verbatim and in authored order -- what a save
    /// writes back, so a load-save round trip edits nothing it was asked to preserve.
    std::vector<AuthoredOverride> authored;
    /// The rows this build could resolve and admit, as executable truth. DERIVED from
    /// `authored` by `apply_overrides`, never authored directly.
    std::vector<std::pair<Act, Gesture>> overrides;
    /// Accepted-with-a-caveat: the honest note about authored gestures with a known
    /// backend gap (see `posix_gap`), spoken once at load and kept nowhere else.
    std::string note;

    const Gesture* override_for(Act a) const noexcept {
        for (const std::pair<Act, Gesture>& o : overrides) {
            if (o.first == a) {
                return &o.second;
            }
        }
        return nullptr;
    }

    /// The gesture one declaration row answers to right now: the maker's override when
    /// one is authored for the row's action, the developer's default otherwise. An
    /// override moves ALL of an action's rows, which is what "quit is ctrl+q now" means.
    Gesture row_gesture(const ActionRow& row) const noexcept {
        const Gesture* o = override_for(row.act);
        return o != nullptr ? *o : row.gesture;
    }

    /// The one effective gesture of an action. For the multi-row actions this is the
    /// FIRST declared row's answer, which every current caller wants: the callers are
    /// single-row actions (the terminal toggle, the picker opener, the hotkey view).
    Gesture gesture_of(Act a) const noexcept {
        for (const ActionRow& row : kActionCatalog) {
            if (row.act == a) {
                return row_gesture(row);
            }
        }
        return Gesture{};
    }

    /// WHICH ACTION THIS GESTURE REQUESTS IN THIS CONTEXT, or kNone. Exact modifier
    /// matching, over the rows active in the resolved context. Admission's collision
    /// refusal is what makes "the first match" a non-answer: no two actions can hold one
    /// gesture in intersecting contexts, so at most one action can match.
    Act action_for(KeyContext current, std::int64_t scancode,
                   std::int64_t modifiers) const noexcept {
        const Gesture pressed{scancode, modifiers};
        for (const ActionRow& row : kActionCatalog) {
            if (active_in(row.context, current) && row_gesture(row) == pressed) {
                return row.act;
            }
        }
        return Act::kNone;
    }

    /// WHICH ABOVE-THE-MODES ACTION THIS GESTURE REQUESTS, or kNone -- `action_for`
    /// restricted to the rows DECLARED kGlobal or kNoText. The distinction matters at
    /// exactly one place: `on(KeyPressed)`'s head answers these five before any mode
    /// (and before the hotkey view's swallow), while an action's ordinary context row --
    /// `workshop.quit`'s own `q` among them -- must still travel the chain, or a modal
    /// surface could be quit through by the very key it exists to intercept.
    Act above_mode_action(KeyContext current, std::int64_t scancode,
                          std::int64_t modifiers) const noexcept {
        const Gesture pressed{scancode, modifiers};
        for (const ActionRow& row : kActionCatalog) {
            const bool above = row.context == KeyContext::kGlobal ||
                               row.context == KeyContext::kNoText;
            if (above && active_in(row.context, current) && row_gesture(row) == pressed) {
                return row.act;
            }
        }
        return Act::kNone;
    }

    /// Does this gesture spell this action's effective binding, in any context? The one
    /// consumer is the picker's "the key that opened it closes it" rule, which follows
    /// the OPENER's binding wherever the maker moved it.
    bool matches(Act a, std::int64_t scancode, std::int64_t modifiers) const noexcept {
        const Gesture pressed{scancode, modifiers};
        for (const ActionRow& row : kActionCatalog) {
            if (row.act == a && row_gesture(row) == pressed) {
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

/// Whether the component's editable-text vocabulary owns this gesture wherever text has
/// the keyboard. The keymap consults the component's own declaration rows -- the same
/// rows contextual help shows -- so the old "the TextBox vocabulary never binds ^s/^o"
/// discipline, kept in two files and checkable nowhere, is checkable here for the first
/// time. The component's key identities are its own local constants, pinned equal to
/// `input::scan`'s in the input suite; this is the same reliance `TextBox::consume`
/// itself already makes on every call.
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
///
/// The candidate is refused WHOLE on the first illegal row, and the caller's live keymap
/// is untouched (it is built into `out`, a local of the caller's). What is refused, in
/// words:
///
///   an action authored twice        two rows for one id is a contradiction, not a list
///   a gesture outside the grammar   for a KNOWN action -- this build would have to
///                                   spend it; an unknown action's gesture is preserved
///                                   unjudged (see AuthoredOverride)
///   a bare printable on a global    a bare printable cannot be global once anything on
///                                   the screen can take text -- the standing law the old
///                                   chain kept as a comment, now enforced at the door
///   a component-owned chord on a    the editing vocabulary would consume it first in
///   global                          every text context, so the global could never mean
///                                   what it says where it matters most
///   a same-context collision        two actions holding one gesture in contexts that
///                                   can be active together, named both, with the
///                                   contested gesture -- refused at admission because a
///                                   lockout must not be SAVABLE
///
/// Gestures with a known POSIX gap are ACCEPTED and noted (`Keymap::note`), never
/// rewritten: AAF-R0's ladder -- warn, explain, allow.
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
        for (const std::pair<Act, Gesture>& already : candidate.overrides) {
            if (already.first == declared->act) {
                return Written::no("`" + row.first +
                                           "` is authored twice -- one gesture per action");
            }
        }
        const ParsedGesture parsed = parse_gesture(row.second);
        if (!parsed.accepted) {
            return Written::no("`" + row.first + "`: " + parsed.refusal);
        }
        if (declared->context == KeyContext::kGlobal) {
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
    // THE COLLISION CHECK RUNS OVER THE EFFECTIVE MAP, not the authored rows alone: an
    // override can land on another action's DEFAULT as easily as on another override, and
    // both makers' files deserve the same sentence. Same-action pairs are skipped -- two
    // rows of one action are one meaning.
    for (std::size_t i = 0; i < kActionCatalogCount; ++i) {
        for (std::size_t j = i + 1; j < kActionCatalogCount; ++j) {
            const ActionRow& a = kActionCatalog[i];
            const ActionRow& b = kActionCatalog[j];
            if (a.act == b.act || !contexts_intersect(a.context, b.context)) {
                continue;
            }
            if (candidate.row_gesture(a) == candidate.row_gesture(b)) {
                return Written::no(
                    "`" + gesture_word(candidate.row_gesture(a)) + "` is authored for both `" +
                    a.id + "` and `" + b.id +
                    "`, which can be active together -- one of them must move");
            }
        }
    }
    out = std::move(candidate);
    return Written::ok();
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_KEYMAP_HPP
