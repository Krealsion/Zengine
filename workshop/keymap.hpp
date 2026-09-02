// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_KEYMAP_HPP
#define ZENGINE_WORKSHOP_KEYMAP_HPP

// ONE EXECUTABLE BINDING TRUTH.
// Workshop law: agents/workshop/keyboard.md (+13 registers; agents/workshop.md routes)

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
// WL-KEY-03, WL-KEY-05 -- agents/workshop/keyboard.md
enum class KeyContext : std::uint8_t {
    kCommand,
    kTerminal,
    kNaming,
    /// THE PANE CREATOR'S NAME PROMPT: one line a maker types a new pane's name into.
    // WL-MAKER-11 -- agents/workshop/maker-pane.md
    kPaneNaming,
    kPicker,
    kAttention,
    kContext,
    kPane,
    kDraft,
    kEditor,
    kFiles,
    kPaneEditor,
    kArrangePane,
    kArrangeDesk,
    kArrangeReset,
    kGlobal,
    kNoText,
    kNoEditor,
};

/// Does this context hand ordinary keys to something that takes text?
// WL-EDIT-04 -- agents/workshop/editor.md
// WL-FOCUS-09 -- agents/workshop/focus.md
// WL-KEY-03 -- agents/workshop/keyboard.md
inline constexpr bool context_takes_text(KeyContext c) noexcept {
    return c == KeyContext::kTerminal || c == KeyContext::kNaming ||
           c == KeyContext::kPaneNaming || c == KeyContext::kPane || c == KeyContext::kDraft ||
           c == KeyContext::kEditor;
}

/// Is an action declared for `declared` requestable while `current` is the resolved
/// context?
// WL-CTX-06 -- agents/workshop/contextual.md; WL-KEY-08 -- agents/workshop/keyboard.md
inline constexpr bool active_in(KeyContext declared, KeyContext current) noexcept {
    if (declared == KeyContext::kGlobal) {
        return true;
    }
    if (declared == KeyContext::kNoText) {
        return !context_takes_text(current);
    }
    if (declared == KeyContext::kNoEditor) {
        return current != KeyContext::kEditor;
    }
    return declared == current;
}

/// Can two declared contexts ever be active at the same moment?
// WL-KEY-06, WL-KEY-08 -- agents/workshop/keyboard.md
inline constexpr bool contexts_intersect(KeyContext a, KeyContext b) noexcept {
    if (a == b || a == KeyContext::kGlobal || b == KeyContext::kGlobal) {
        return true;
    }
    if (a == KeyContext::kNoText) {
        return b == KeyContext::kNoEditor || !context_takes_text(b);
    }
    if (b == KeyContext::kNoText) {
        return a == KeyContext::kNoEditor || !context_takes_text(a);
    }
    if (a == KeyContext::kNoEditor) {
        return b != KeyContext::kEditor;
    }
    if (b == KeyContext::kNoEditor) {
        return a != KeyContext::kEditor;
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
    kEditSource,
    // -- the source editor's controls --------------------------------------------------
    kEditorSave,
    kEditorNewline,
    kEditorTab,
    kEditorDiscard,
    // -- the project browser's controls ------------------------------------------------
    kFilesUp,
    kFilesDown,
    kFilesOpen,
    kFilesParent,
    kFilesRefresh,
    kFilesUseRecipes,
    kFilesMark,
    kFilesNextMark,
    kFilesPreviousMark,
    // -- the Pane Editor's keys -------------------------------------------------------
    kPaneEditorUp,
    kPaneEditorDown,
    kPaneEditorChoose,
    kPaneEditorSwitch,
    kPaneEditorOpen,
    kPaneEditorFront,
    kPaneEditorBack,
    kPaneEditorRaise,
    kPaneEditorLower,
    // -- the Pane Creator: a pane made inside the Pane Manager ------------------------
    kPaneCreatorNew,
    kPaneCreatorSave,
    kPaneCreatorDiscard,
    kPaneNamingCommit,
    kPaneNamingCancel,
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
// WL-KEY-01, WL-KEY-06 -- agents/workshop/keyboard.md; WL-ATTN-10 -- agents/workshop/attention.md
struct ActionRow {
    Act act = Act::kNone;
    const char* id = "";
    const char* label = "";
    KeyContext context = KeyContext::kCommand;
    Gesture gesture;
};

namespace scan = input::scan;
namespace mod = input::mod;

/// THE DECLARATIONS. Order inside a context group is presentation priority.
// WL-KEY-01, WL-KEY-06 -- agents/workshop/keyboard.md
// WL-ARR-08 -- agents/workshop/arrangement.md
// WL-CTX-05 -- agents/workshop/contextual.md
inline constexpr ActionRow kActionCatalog[] = {
    // -- above every mode -------------------------------------------------------------
    {Act::kQuit, "workshop.quit", "quit", KeyContext::kNoText, {scan::kC, mod::kCtrl}},
    // `^s` FOLLOWS THE KEYBOARD. The document's save is answered above every mode
    // EXCEPT the source editor -- there the same physical chord is `editor.save`'s, the
    // editor's own row below, because a maker with their hands in source who presses the
    // one save chord every editor teaches must not write the OBJECT document instead.
    // Two meanings are two action identities with complementary declared activity
    // (`kNoEditor` / `kEditor`), never one identity with a branch -- so a maker may
    // remap either without touching the other, and the collision law sees no collision
    // because there is none: no state has both rows active.
    {Act::kSaveDocument, "document.save", "save", KeyContext::kNoEditor,
     {scan::kS, mod::kCtrl}},
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
    // SAVING A SETUP STOPPED NAMING A LAYOUT, and the IDENTITY is deliberately the
    // old `setup.name` -- a maker's authored override for it keeps working, exactly as
    // `workshop.manage` kept working when arrangement changed what it opens. What moved is the
    // meaning: `s` writes the live layout's desk to its associated artifact (or, with no
    // association, to the host's configured setup path, establishing the association on
    // success) and it no longer opens the name editor. Renaming is `layout.rename` below.
    {Act::kSetupSave, "setup.name", "save setup", KeyContext::kCommand,
     {scan::kS, mod::kNone}},
    {Act::kSetupRestore, "setup.restore", "restore setup", KeyContext::kCommand,
     {scan::kR, mod::kNone}},
    // THE LAYOUT SHELF: step along the run of desk arrangements this Workshop is
    // holding, add one, drop one. Beside the two setup rows because they are the same
    // family -- a layout IS a setup, and these four are what makes the plural reachable.
    //
    // FOUR BARE PRINTABLES, WHICH IS LEGAL HERE FOR THE ARRANGEMENT SCOPES' OWN REASON:
    // nothing in command mode takes text, so a letter cannot be swallowed by a buffer, and
    // every one of them arrives from BOTH backends as itself. That last clause is the whole
    // selection criterion and it is narrow: the POSIX wire carries an unshifted printable
    // and a SHIFTED LETTER, and nothing else in this family -- `<`, `>` and `+` reach
    // `terminal_byte_scancode` as bytes it cannot name (a shifted punctuation key is not a
    // scancode plus Shift there), and ctrl+shift+letter cannot be said at all. So the
    // conventional spellings for these four gestures are exactly the ones a terminal maker
    // could not press, and these are their unshifted neighbours.
    //
    // `,` AND `.` ARE THE RUN'S TWO DIRECTIONS -- adjacent keys wearing `<` and `>`, walking
    // a run that is itself horizontal, and free in every context that intersects kCommand
    // (the globals are chords, kNoText holds `^c`/`^a`, and no other kCommand row spends
    // either).
    //
    // `=` IS THE KEY WEARING `+`. It means grow inside the two arrangement scopes, which do
    // not intersect this one -- reusing a gesture across mutually exclusive contexts is the
    // working norm here (`w`) -- and adding a layout is not destructive, so a bare key is
    // the right price for it.
    //
    // REMOVAL IS THE ONE CHORD, AND THE ASYMMETRY IS THE POINT. `^w` is what every
    // application with tabs means by "close this one", it is free in every context that
    // intersects kCommand, it is not a chord the TextBox owns (`kEditingVocabulary`), and
    // `posix_gap` passes it. What it is NOT is a bare letter: discarding a layout cannot be
    // undone -- this application has no undo, and the arrangement is gone with the value --
    // so it may not be one slipped keystroke away in the mode where every other bare letter
    // does something harmless. `x` was the obvious mnemonic and is deliberately refused:
    // An early phase bound it to "close the Builder" and a later one took that back on purpose, so
    // a maker's hand may still mean the panel by it, and the worst outcome for a key with a
    // half-remembered meaning is a new destructive one.
    {Act::kLayoutNext, "layout.next", "next layout", KeyContext::kCommand,
     {scan::kPeriod, mod::kNone}},
    {Act::kLayoutPrevious, "layout.previous", "previous layout", KeyContext::kCommand,
     {scan::kComma, mod::kNone}},
    {Act::kLayoutNew, "layout.new", "new layout", KeyContext::kCommand,
     {scan::kEquals, mod::kNone}},
    {Act::kLayoutRemove, "layout.remove", "remove layout", KeyContext::kCommand,
     {scan::kW, mod::kCtrl}},
    //...AND FOUR THAT ANSWER TO NO KEY. Rename, duplicate and the two reorder
    // steps are reached from a tab's contextual menu -- and rename also from a double-click
    // on the tab, which is where a maker's hand goes first. They are DECLARED here anyway,
    // because a contextual row references a `kActionCatalog` id and because a maker may
    // bind any of them in their own keymap file; what they do not have is a shipped
    // gesture.
    //
    // WHY NOT A DEFAULT. The criterion beside `layout.next` above is the whole answer:
    // command mode's free set is bare printables and plain ctrl chords that BOTH backends
    // deliver, `<`/`>`/`+` are bytes the POSIX wire cannot name, and ctrl+shift+letter
    // cannot be said at all. Four more of that set spent on operations a maker reaches by
    // pointing would be four gestures taken from whatever asks next -- and a chord chosen
    // for symmetry rather than for use is the unreachable default the keymap exists to end.
    {Act::kLayoutRename, "layout.rename", "rename layout", KeyContext::kCommand, kNoGesture},
    {Act::kLayoutDuplicate, "layout.duplicate", "duplicate layout", KeyContext::kCommand,
     kNoGesture},
    {Act::kLayoutMoveLeft, "layout.move-left", "move layout left", KeyContext::kCommand,
     kNoGesture},
    {Act::kLayoutMoveRight, "layout.move-right", "move layout right", KeyContext::kCommand,
     kNoGesture},
    // ARRANGE THE DESK: the global arrangement scope. The IDENTITY is the old
    // `workshop.manage` -- a maker's authored override for it keeps working -- and what
    // changed is the meaning's scope: it opens the desk-wide arrangement state, never a
    // pane-selection prerequisite.
    {Act::kArrangeDesk, "workshop.manage", "arrange desk", KeyContext::kCommand,
     {scan::kW, mod::kNone}},
    // WHAT CAN I DO WITH THIS? -- the keyboard door to the contextual-action surface
    //, on the subject command mode can truthfully name: the selected object, or
    // the empty room. The pointer's door is a right press, which needs no row here; this
    // row exists because a surface reachable only by a mouse button would be Workshop's
    // first gesture with no catalog identity -- exactly the drift this file ended.
    // `a` bare: portable, and free in every context that intersects kCommand (the globals
    // are all chords, kNoText holds `^c`/`^a`, and no other kCommand row spends it).
    {Act::kContextOpen, "workshop.context", "actions", KeyContext::kCommand,
     {scan::kA, mod::kNone}},
    // A PRESENTATION PREFERENCE WITH A KEY: whether the arrangeable panes paint
    // their title rows. Last in the command group because the band packs these in order
    // and a toggle a maker reaches for occasionally must not displace the gestures they
    // reach for constantly. `t` bare: portable (a plain letter arrives from every
    // backend), and free in every context that intersects kCommand -- the global rows are
    // all chords, kNoText holds only `^c`, and no other kCommand row spends it.
    {Act::kPaneTitles, "workshop.pane-titles", "titles", KeyContext::kCommand,
     {scan::kT, mod::kNone}},
    // EDIT THE SOURCE THE BUILDER'S CHOSEN RECIPE NAMES -- the Builder-owned door into
    // the source editor. `e` bare: portable, free in every context that intersects
    // kCommand, and the recipe it opens is exactly the row `builder.build` would build,
    // so the two gestures cannot come to mean different recipes. With no Builder panel
    // open it does nothing, exactly as `b` does.
    {Act::kEditSource, "builder.edit-source", "edit source", KeyContext::kCommand,
     {scan::kE, mod::kNone}},
    // -- the source editor's controls --------------------------------------------------
    //
    // THE EDITOR'S POLICY KEYS, beside its component-shaped mechanics (which live in
    // `EditorBuffer::consume` and are deliberately not remappable -- editor.hpp's own
    // vocabulary table shows them). Save is the chord that follows the keyboard (see
    // `document.save` above); newline and tab are the two keys whose MEANING is the
    // editor's rather than the buffer's mechanics, declared here so no executable
    // gesture is a hand-written literal; discard is the one deliberate door out of an
    // unsaved buffer, with a second row in command mode so the quit refusal can name a
    // gesture that works where the maker is standing. Its default is a PLAIN ctrl
    // chord on purpose: the POSIX wire cannot carry ctrl+shift+letter at all (0x04 is
    // 0x04), so a hard chord would be a door a terminal maker cannot open -- and the
    // soft one is honest to bind because the discard is itself an undoable edit.
    {Act::kEditorSave, "editor.save", "save source", KeyContext::kEditor,
     {scan::kS, mod::kCtrl}},
    {Act::kEditorNewline, "editor.newline", "newline", KeyContext::kEditor,
     {scan::kReturn, mod::kNone}},
    {Act::kEditorTab, "editor.tab", "insert tab", KeyContext::kEditor,
     {scan::kTab, mod::kNone}},
    {Act::kEditorDiscard, "editor.discard", "discard source edits", KeyContext::kEditor,
     {scan::kD, mod::kCtrl}},
    {Act::kEditorDiscard, "editor.discard", "discard source edits", KeyContext::kCommand,
     {scan::kD, mod::kCtrl}},
    // -- the project browser's controls ------------------------------------------------
    //
    // NINE VERBS, ALL SAYABLE ON EVERY BACKEND THIS APPLICATION SHIPS. Up, Down, Return
    // and Backspace are plain named keys the POSIX terminal wire carries as themselves,
    // and `r` is a bare letter, which is legal here for the arrangement scopes' reason:
    // nothing in this context takes text, so a letter cannot be swallowed by a buffer.
    // There is deliberately no ctrl+shift+letter anywhere -- the POSIX wire cannot say
    // one at all (measured), so binding one would ship a door a terminal maker
    // could not open.
    //
    // BACKSPACE MEANS PARENT, AND THERE IS NO `..` ROW FOR IT TO PRESS. Going up is the
    // lexical parent of where the browser is standing; at a filesystem root a path has no
    // parent and the gesture says so. That is the only boundary left here, and it is the
    // filesystem's rather than the project's.
    //
    // SIX AND THE SIXTH IS THE FIRST THING THIS BROWSER DOES THAT IS NOT
    // ABOUT LOOKING. `u` is a bare letter for `r`'s reason exactly -- nothing in this
    // context takes text -- and it is free in EVERY context this build declares, so no
    // remap was needed to make room for it. It is a Files row rather than a contextual
    // menu row because the contextual surface names three subject kinds (a pane, a
    // document object, the room) and a browser ROW is none of them: minting a fourth
    // subject to carry one action would widen a declaration protocol for a gesture the
    // keymap already knows how to say.
    {Act::kFilesUp, "files.up", "row up", KeyContext::kFiles, {scan::kUp, mod::kNone}},
    {Act::kFilesDown, "files.down", "row down", KeyContext::kFiles, {scan::kDown, mod::kNone}},
    {Act::kFilesOpen, "files.open", "enter or edit", KeyContext::kFiles,
     {scan::kReturn, mod::kNone}},
    {Act::kFilesParent, "files.parent", "up a directory", KeyContext::kFiles,
     {scan::kBackspace, mod::kNone}},
    {Act::kFilesRefresh, "files.refresh", "look again", KeyContext::kFiles,
     {scan::kR, mod::kNone}},
    {Act::kFilesUseRecipes, "files.use-recipes", "use as recipes", KeyContext::kFiles,
     {scan::kU, mod::kNone}},
    //...AND THREE MORE WHICH ARE ABOUT PLACES RATHER THAN ABOUT ROWS. Once
    // the browser can leave the directory Workshop was launched in, "get me back there" and
    // "get me back to the other one" are gestures a maker needs and had no way to ask for.
    //
    // `m` / `n` / `shift+n` ARE BARE LETTERS FOR `r`'s REASON EXACTLY: nothing in this
    // context takes text, so a letter cannot be swallowed by a buffer, and all three are
    // free in every context that intersects `kFiles` (the globals are chords, `kNoText`
    // holds `^c`/`^a`, `kNoEditor` holds `^s`, and no other `kFiles` row spends them).
    // The next/previous PAIR is `builder.recipe`/`builder.recipe-back`'s shape one context
    // over -- a letter and its shifted self -- and shift on a LETTER is the one shifted form
    // the POSIX wire carries (`posix_gap`), so neither ships inside a gap.
    //
    // THEY COME LAST because the band packs these in declaration order: the four navigation
    // verbs are what a maker reaches for constantly, and a gesture used a few times a
    // session must not displace one used a few times a minute.
    {Act::kFilesMark, "files.mark", "mark this place", KeyContext::kFiles,
     {scan::kM, mod::kNone}},
    {Act::kFilesNextMark, "files.next-mark", "next mark", KeyContext::kFiles,
     {scan::kN, mod::kNone}},
    {Act::kFilesPreviousMark, "files.previous-mark", "previous mark", KeyContext::kFiles,
     {scan::kN, mod::kShift}},
    // -- the Terminal line's controls --------------------------------------------------
    // THE PANE EDITOR'S KEYS: a list with a cursor and one gesture on the row it
    // is on, in the Files pane's own shape. `up`/`down` step whichever list the keys are
    // in, `switch` moves them between the PANES list and the subject's rows, and `choose`
    // is the one Return: on a pane row it makes that pane the SUBJECT, on an editable row
    // it opens a draft. The four ORDER keys and `open` spend the arrangement's and the
    // picker's own doors on the subject -- the letters are the arrangement scope's, so a
    // maker who learned `f` there does not learn a second word here. None of these takes
    // text: the draft a row opens is `kDraft`'s, exactly as the Info panel's is.
    {Act::kPaneEditorUp, "pane-editor.up", "row up", KeyContext::kPaneEditor,
     {scan::kUp, mod::kNone}},
    {Act::kPaneEditorDown, "pane-editor.down", "row down", KeyContext::kPaneEditor,
     {scan::kDown, mod::kNone}},
    {Act::kPaneEditorChoose, "pane-editor.choose", "subject or edit", KeyContext::kPaneEditor,
     {scan::kReturn, mod::kNone}},
    {Act::kPaneEditorSwitch, "pane-editor.switch", "panes / rows", KeyContext::kPaneEditor,
     {scan::kTab, mod::kNone}},
    {Act::kPaneEditorOpen, "pane-editor.open", "open or remove", KeyContext::kPaneEditor,
     {scan::kO, mod::kNone}},
    {Act::kPaneEditorFront, "pane-editor.front", "front", KeyContext::kPaneEditor,
     {scan::kF, mod::kNone}},
    {Act::kPaneEditorBack, "pane-editor.back", "back", KeyContext::kPaneEditor,
     {scan::kB, mod::kNone}},
    {Act::kPaneEditorRaise, "pane-editor.raise", "raise", KeyContext::kPaneEditor,
     {scan::kR, mod::kNone}},
    {Act::kPaneEditorLower, "pane-editor.lower", "lower", KeyContext::kPaneEditor,
     {scan::kL, mod::kNone}},
    // THE PANE CREATOR'S KEYS, inside the Pane Manager: `new` opens the name prompt for a
    // pane made of authored data; `save` writes the open definition to its project file;
    // `discard` puts it back to what that file holds -- the source editor's own pair, and
    // the two doors the quit refusal names. The identities carry the creator's own word
    // because they are the creator's acts and not the manager's; the letters are the ones
    // this context had free, and `s` says "save" here exactly as it does in command mode.
    // The discard chord is a plain ctrl+letter for the editor's reason: the POSIX wire
    // cannot say ctrl+shift+letter at all.
    {Act::kPaneCreatorNew, "pane-creator.new", "new pane", KeyContext::kPaneEditor,
     {scan::kN, mod::kNone}},
    {Act::kPaneCreatorSave, "pane-creator.save", "save pane", KeyContext::kPaneEditor,
     {scan::kS, mod::kNone}},
    {Act::kPaneCreatorDiscard, "pane-creator.discard", "discard pane edits",
     KeyContext::kPaneEditor, {scan::kD, mod::kCtrl}},
    // ...and the name prompt's own two keys, in its own context, so the legend over a
    // pane being named says what the keys do there.
    {Act::kPaneNamingCommit, "pane-creator.name", "make the pane", KeyContext::kPaneNaming,
     {scan::kReturn, mod::kNone}},
    {Act::kPaneNamingCancel, "pane-creator.cancel", "cancel", KeyContext::kPaneNaming,
     {scan::kEscape, mod::kNone}},
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
    // letter is legal in a mode nothing in which takes text, exactly as the arrangement
    // scopes' are.
    {Act::kAttentionUp, "attention.up", "row up", KeyContext::kAttention,
     {scan::kUp, mod::kNone}},
    {Act::kAttentionDown, "attention.down", "row down", KeyContext::kAttention,
     {scan::kDown, mod::kNone}},
    {Act::kAttentionDismiss, "attention.dismiss", "hide this one", KeyContext::kAttention,
     {scan::kD, mod::kNone}},
    {Act::kAttentionClose, "attention.close", "close", KeyContext::kAttention,
     {scan::kEscape, mod::kNone}},
    // -- the layout-name editor's controls ---------------------------------------------
    //
    // THE IDENTITIES ARE THE OLD ONES AND THE MEANING NARROWED: this editor
    // renamed a setup and wrote its file in one gesture, and it now renames the layout and
    // writes nothing at all. `naming.commit` is still the key that finishes it, so an
    // authored override keeps working; the LABEL is what stopped being true.
    {Act::kNamingCommit, "naming.commit", "rename", KeyContext::kNaming,
     {scan::kReturn, mod::kNone}},
    {Act::kNamingCancel, "naming.cancel", "cancel", KeyContext::kNaming,
     {scan::kEscape, mod::kNone}},
    // -- a live property draft's controls ----------------------------------------------
    {Act::kDraftCommit, "draft.commit", "commit", KeyContext::kDraft,
     {scan::kReturn, mod::kNone}},
    {Act::kDraftCancel, "draft.cancel", "cancel", KeyContext::kDraft,
     {scan::kEscape, mod::kNone}},
    // -- arranging panes ---------------------------------------------------------------
    //
    // ONE VOCABULARY, TWO SCOPES. Moving and resizing a pane are one maker intent --
    // arrange it -- so the old move/size submodes are gone and their gestures live
    // side by side: arrows place, shift+arrows pull an extent (the document's own
    // `hjkl` / `shift+hjkl` family, said with the keys a pane already used). Every
    // action shared by the two scopes owns a row in each, so one maker override moves
    // both. The IDENTITIES keep the `manage.` prefix on purpose: the ids are the
    // durable spelling a maker's keymap file holds, and this transition preserves
    // authored intent (`manage.move`, `manage.size` and `manage.edge` are RETIRED --
    // an authored row naming one is preserved as an unknown id, byte-for-byte,
    // exactly as the admission has always treated ids it cannot spend).
    //
    // A KEYBOARD PULL IS ANCHORED AT THE PLACE: `pull-right` widens and `pull-left`
    // narrows by moving the RIGHT edge, `pull-down`/`pull-up` the bottom one -- so a
    // key never moves a pane it is resizing, `doc::resize`'s own law. The other six
    // anchors remain the pointer's: every edge and corner of the pane is a handle.
    {Act::kManageNext, "manage.next", "next pane", KeyContext::kArrangeDesk,
     {scan::kTab, mod::kNone}},
    {Act::kManagePrevious, "manage.previous", "previous pane", KeyContext::kArrangeDesk,
     {scan::kTab, mod::kShift}},
    // NARROW TO ONE PANE: the desk's Return binds the arrangement to the pane the
    // keyboard is on -- the same act the pane context menu's `arrange` row performs on
    // the pointed pane, which is what earns this action its key (no action receives a
    // key merely so a menu has something to print; this one has a job in this scope).
    {Act::kArrange, "manage.arrange", "arrange", KeyContext::kArrangeDesk,
     {scan::kReturn, mod::kNone}},
    // THE COARSE STEP COMES FIRST IN BOTH SCOPES, and that is this file's own
    // priority rule spent deliberately: order inside a context group is what the band's
    // legend packs left to right and cuts from the right. `=` is the gesture a maker on a
    // shipped desk reaches for before any other -- it is the one that turns a pane they
    // can see into a pane they can work in -- so a legend that had room for the four fine
    // place keys and not for this one would be advertising the wrong half. `=` and `-` are
    // the two keys a hand already reads as bigger and smaller, plain printable ASCII (so a
    // POSIX terminal can say them, which `ctrl+shift+<letter>` cannot), and neither was
    // bound in either arranging scope. Both spend `kCoarseStepCells` on BOTH axes through
    // the same bottom-right-anchored proposal the shifted arrows take -- one owner, one
    // clamping law, and a pane that never moves under a key that resizes it.
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

/// A gesture as the SCREEN spells it -- the band's own compact voice.
// WL-KEY-02, WL-KEY-13 -- agents/workshop/keyboard.md
inline std::string gesture_text(const Gesture& g) {
    // AN ACTION THAT ANSWERS TO NO KEY SAYS SO. `key_name_of` has no name for
    // `kUnknown` and the fall-through below spells it `?`, which in a two-column legend
    // reads as a key a maker cannot find rather than as one that is not there. `-` is a
    // real binding on this keyboard, so a dash would be worse than the question mark.
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
/// nullptr when no such gap is known.
// WL-KEY-08 -- agents/workshop/keyboard.md
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

// ---- The keymap value --------------------------------------------------------------------

/// THE EFFECTIVE BINDING TRUTH: the declaration defaults plus the maker's applied
/// overrides, plus what could not be applied and is preserved.
// WL-KEY-01, WL-KEY-02, WL-KEY-07 -- agents/workshop/keyboard.md
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

    /// WHICH ACTION THIS GESTURE REQUESTS IN THIS CONTEXT, or kNone.
    // WL-KEY-04, WL-KEY-08 -- agents/workshop/keyboard.md
    Act action_for(KeyContext current, std::int64_t scancode,
                   std::int64_t modifiers) const noexcept {
        const Gesture pressed{scancode, modifiers};
        // A KEY THIS BUILD CANNOT NAME REQUESTS NOTHING. Without this an unnamed
        // key would match every row that declares `kNoGesture` and the first one in
        // declaration order would run -- a press with no name performing an operation.
        if (!is_bound(pressed)) {
            return Act::kNone;
        }
        for (const ActionRow& row : kActionCatalog) {
            if (active_in(row.context, current) && row_gesture(row) == pressed) {
                return row.act;
            }
        }
        return Act::kNone;
    }

    /// WHICH ABOVE-THE-MODES ACTION THIS GESTURE REQUESTS, or kNone -- `action_for`
    /// restricted to the rows DECLARED kGlobal, kNoText or kNoEditor.
    // WL-FOCUS-06 -- agents/workshop/focus.md; WL-KEY-05 -- agents/workshop/keyboard.md
    Act above_mode_action(KeyContext current, std::int64_t scancode,
                          std::int64_t modifiers) const noexcept {
        const Gesture pressed{scancode, modifiers};
        if (!is_bound(pressed)) {
            return Act::kNone; // `action_for`'s rule, for `action_for`'s reason
        }
        for (const ActionRow& row : kActionCatalog) {
            const bool above = row.context == KeyContext::kGlobal ||
                               row.context == KeyContext::kNoText ||
                               row.context == KeyContext::kNoEditor;
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
        if (!is_bound(pressed)) {
            return false; // `action_for`'s rule, for `action_for`'s reason
        }
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
        // A kNoEditor row is above every mode BUT the editor, so it is active inside
        // every ordinary text context -- the two refusals that keep a global honest
        // there apply to it identically.
        if (declared->context == KeyContext::kGlobal ||
            declared->context == KeyContext::kNoEditor) {
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
            // TWO ACTIONS THAT ANSWER TO NO KEY ARE NOT TWO ACTIONS HOLDING ONE GESTURE
            //. Without this, every keymap file would be refused the moment a
            // second `kNoGesture` row was declared, naming a clash that cannot be pressed.
            if (!is_bound(candidate.row_gesture(a))) {
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
