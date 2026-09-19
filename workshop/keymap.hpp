// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_KEYMAP_HPP
#define ZENGINE_WORKSHOP_KEYMAP_HPP

// ONE EXECUTABLE BINDING TRUTH.
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
    // ⭐ `kTerminal` IS GONE (VD-24). It was the last context whose rows belonged to one
    // particular tool: a modal overlay that owned the keyboard whole while it was open. The
    // Terminal is a pane, so its keys are its own `PaneActions` rows and reach it through
    // `kPane` like every other pane's -- which is also why five `Act` values left with it.
    kNaming,
    // ⭐ `kPaneNaming` AND `kPicker` ARE GONE. The first was the Pane Creator's name prompt, drawn
    // inside the host's Pane Manager; the name is typed in the desktop's Pane Manager now, as that
    // pane's own draft under `kPane`. The second was the `p` picker, a mode that took the keyboard
    // whole and toggled participation; launching and closing are the desktop's rows over the
    // host's two doors (`PaneLaunchRequested`, `PaneCloseRequested`).
    // THE AUTHORING PROMPT'S CONTEXT IS GONE. It held one line a maker typed a plan row's
    // role into, for two askers in turn -- the Files pane's recipe fields, then
    // `builder.load`'s role -- and both of those panes are weaves now, each with its own
    // line inside its own room. A context with no asker is a mode nothing can enter.
    //
    // AND SO IS THE CURRENT-CONDITION VIEW'S. It sat here, in the picker's own place, and
    // owned the keyboard while it was open; the view is a PANE now and takes the keys the
    // way every pane does, under `kPane`, because a maker pressed into it.
    kContext,
    kPane,
    // ⭐ `kDraft` IS GONE: the host's Pane Manager was the last inspector whose drafts were this
    // host's, and Info's are its own weave's.
    // ⭐ `kEditor` IS GONE, AND SO IS `kNoEditor` BELOW. The source editor was the last
    // built-in that took text, and the one context whose existence a whole activity class
    // was defined against ("everywhere but the editor"). The Editor is a pane, so its keys
    // are its own `PaneActions` rows and reach it through `kPane` like every other pane's;
    // the document's save, which was "everywhere but the editor", is now "everywhere
    // nothing takes text" -- the class `workshop.quit` already had, for the same reason.
    // ⭐ `kPaneEditor` IS GONE WITH THE HOST'S PANE MANAGER. Its list is the desktop's Pane
    // Manager, its subject and rows are Info's, and its order keys were the arrangement's all along.
    kArrangePane,
    kArrangeDesk,
    kArrangeReset,
    kGlobal,
    kNoText,
    /// EVERYWHERE, UNLESS THE PANE HOLDING THE KEYBOARD DECLARED IT OWNS THIS ACTION.
    ///
    // ⭐ THIS IS WHAT `kNoEditor` BECAME (VD-26). The old class read "everywhere but the
    // source editor" and named one built-in in the host's own enum; the Editor is a pane
    // now, and the relationship it needs is the one that class was really expressing --
    // an operation the host performs on the object document, which a pane holding a
    // document of its own performs on ITS document instead, while its keys are the
    // maker's. The exclusion is DECLARED, by the pane, in `PaneActionRow::supersedes`, so
    // it survives a maker moving either row's key and the host names no pane anywhere.
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
        // EVERY CONTEXT, as a class: what takes this row away is not a mode but the
        // declaration of the pane holding the keys, which no pair of contexts can express.
        // `Keymap::row_active` is where the two halves meet.
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
    // A `kUnlessOwned` ROW MEETS EVERY OTHER ROW, because it is active in every context.
    // The collision law is about two DECLARATIONS that could both fire, and supersession
    // is not a second declaration -- it is one row standing down for one pane, judged
    // where the pane's rows are judged (`join_pane_rows`).
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
    // ⚠ `kSaveDocument` AND `kOpenDocument` WERE HERE -- `^s` and `^o` on the object document,
    // above every mode unless a pane owned them -- and retired with the document
    // (`kRetiredActions`).
    // ⚠ `kTerminalToggle` WAS HERE, ABOVE EVERY MODE (VD-22, VD-24) -- the chord that opened
    // the terminal overlay from anywhere. It retired with the overlay, for `kAttention`'s
    // reason written three lines down: the Terminal is a pane, opened from the Pane Manager.
    // ⚠ `kHotkeys` WAS HERE TOO, and left with the hotkey view it opened: the view is the
    // desktop's Hotkeys pane, over the keymap this host publishes (`KeymapShown`), and its launch
    // is the application row `desktop.hotkeys` -- which an authored `workshop.hotkeys` row names
    // now (`kRenamedActions`).
    // ⚠ `kAttention` WAS HERE, ABOVE EVERY MODE -- the chord that opened the
    // current-condition view from anywhere. The view is a pane and is opened from the
    // Pane Manager; a global that put one particular pane on the screen is exactly the
    // host-mapped route VD-22 refuses, so it retired with the overlay rather than being
    // re-pointed at a weave.
    // -- command mode ------------------------------------------------------------------
    // ⚠ THE OBJECT CANVAS'S ELEVEN WERE HERE -- `n`, `d`, `hjkl`, `shift+hjkl` and `Tab` -- and
    // retired with the canvas (`kRetiredActions`).
    // ⭐ THE THREE INSPECTOR ROWS LEFT WITH THE INFO PANEL (VD-22). `up`, `down` and `enter`
    // were command-mode rows: they moved the property cursor and opened a draft on it from
    // anywhere in Workshop, as long as Info happened to be open. Info is a weave now
    // (`Zengine/info-pane/`) and declares `info.up`, `info.down` and `info.edit` as its OWN
    // rows, so a maker presses into the pane and then edits -- and an authored override for
    // `info.edit` is applied to the pane's row wherever they moved it (WL-KEY-15). The
    // Builder's nine rows left for this reason one migration ago; these are the same nine
    // words about three keys.
    // ...and so did `[` and `]`, which refit the object canvas's workspace.
    // ⚠ `kPicker` (`p`) WAS HERE, and retired with the picker it opened (`kRetiredActions`).
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
    // ⭐ THE SOURCE EDITOR'S FOUR CONTROLS WERE HERE. `kEditorSave`, `kEditorNewline`,
    // `kEditorTab` and `kEditorDiscard` were the built-in's policy keys; the Editor pane
    // declares the same four ids as its own `PaneActionRow`s (`editor-pane/vocabulary.hpp`),
    // on the same gestures, so a maker's authored override moves with the spelling and what
    // this host compiles for them is nothing.
    // ⚠ THE HOST PANE MANAGER'S NINE KEYS AND THE PANE CREATOR'S FIVE WERE HERE. The nine retired
    // with it (`kRetiredActions`); the Creator's five are the desktop's Pane Manager's own rows now,
    // under the ids they had here, so a maker's authored override still finds them.
    // ⚠ THE TERMINAL LINE'S FIVE CONTROLS WERE HERE (VD-24). `kTerminalSubmit`,
    // `kTerminalBack`, `kTerminalUp`, `kTerminalDown` and `kTerminalComplete` were an
    // overlay's mode keys; the pane declares the same five ids as its own `PaneActionRow`s,
    // so what a maker presses is unchanged and what this host compiles for it is nothing.
    // ⚠ THE PICKER'S FOUR WERE HERE, and retired with it.
    // -- the authoring prompt (LOAD-IT) ------------------------------------------------
    // THE CURRENT-CONDITION VIEW'S FOUR ARE GONE. Three of them are the Attention pane's
    // own declared rows now, under the same ids (`attention-pane/vocabulary.hpp`), so a
    // maker's authored override still finds them; the fourth closed the overlay and a pane
    // has nothing to close.
    // -- the setup-name editor's controls ----------------------------------------------
    kNamingCommit,
    kNamingCancel,
    // ⚠ `draft.commit` AND `draft.cancel` WERE HERE, and retired with the host's last draft.
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

/// THE DECLARATIONS. Order inside a context group is presentation priority.
// WL-KEY-01, WL-KEY-06 -- agents/workshop/keyboard.md
// WL-ARR-08 -- agents/workshop/arrangement.md
// WL-CTX-05 -- agents/workshop/contextual.md
inline constexpr ActionRow kActionCatalog[] = {
    // -- above every mode -------------------------------------------------------------
    {Act::kQuit, "workshop.quit", "quit", KeyContext::kNoText, {scan::kC, mod::kCtrl}},
    // ⚠ `document.save` (`^s`) AND `document.open` (`^o`) WERE ROWS HERE, above every mode unless
    // a pane holding the keys declared a row standing in for them (`kUnlessOwned`). They acted on
    // the object document and retired with it; a pane that still names one on its own row is
    // admitted, standing in for nothing (`kRetiredActions`).
    // ⚠ `workshop.terminal` AND `workshop.hotkeys` WERE GLOBAL ROWS HERE and are the desktop's
    // launches now (`desktop.terminal`, `desktop.hotkeys`): an application's defaults belong to
    // the office that owns them (WL-DESK-01), and a maker's row for either old id is read as its
    // successor (`kRenamedActions`). Attention's `^a` (`kNoText`, beside `^c`) left the same way
    // when the current-condition view became a pane. `workshop.quit` is the one host row left
    // above every mode -- `ctrl+c` is the copy chord AND the quit chord, told apart by whether
    // the keys are going to text, which is the whole of the `kNoText` class.
    // -- command mode ------------------------------------------------------------------
    // ⚠ THE OBJECT CANVAS'S THIRTEEN COMMAND ROWS WERE HERE -- `object.new` (`n`),
    // `object.delete` (`d`), the four `object.left/down/up/right` moves (`hjkl`), their four
    // resizes (`shift+hjkl`), `object.next` (`Tab`) and `workspace.narrower/wider` (`[`, `]`) --
    // and retired with the canvas (`kRetiredActions`). Their keys are free in command mode.
    {Act::kQuit, "workshop.quit", "quit", KeyContext::kCommand, {scan::kQ, mod::kNone}},
    // ⚠ `workshop.picker` (`p`, "+ panel") WAS A ROW HERE and retired with the picker. Its two
    // duties are the desktop's Pane Manager's rows over the host's launch and close doors, and a
    // maker's row for it is kept and said at load (`kRetiredActions`) rather than read as
    // `desktop.panes`: that row is a chord answered above every mode, and the picker's bare `p`
    // moved onto it would meet the walls a row above every mode meets.
    // ⭐ THE NINE BUILD ROWS LEFT WITH THE BUILDER PANEL (VD-22). `b`, `B`, `P`, `R`, `o`,
    // `c`, `C`, `f` and `e` were command-mode rows: they acted on the Builder panel from
    // anywhere in Workshop, as long as one happened to be open. The Builder is a weave now
    // (`Zengine/builder-pane/`) and it declares those same nine ids as its OWN rows, so a
    // maker presses into the pane and then builds -- and a maker's authored override for
    // `builder.build` is applied to the pane's row wherever they moved it (WL-KEY-15).
    // Nothing does something by default from anywhere; a button, hover-to-focus, or
    // Workshop mapping a key straight to a weave's action are later UX with many options,
    // and none of them is a default now.
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
    // ...AND ONE MORE THAT ANSWERS TO NO KEY, for the same reasons. Edit Code is reached from a
    // PANE's contextual menu, on the pane a maker pointed at, and command mode cannot
    // truthfully name a pane (it names the room). It is declared so the
    // contextual row references an id and a maker's keymap file can name it; bound, it says
    // where the gesture lives rather than acting on some pane it guessed.
    {Act::kEditCode, "pane.edit-code", "edit code", KeyContext::kCommand, kNoGesture},
    // ARRANGE THE DESK: the global arrangement scope. The IDENTITY is the old
    // `workshop.manage` -- a maker's authored override for it keeps working -- and what
    // changed is the meaning's scope: it opens the desk-wide arrangement state, never a
    // pane-selection prerequisite.
    {Act::kArrangeDesk, "workshop.manage", "arrange desk", KeyContext::kCommand,
     {scan::kW, mod::kNone}},
    // WHAT CAN I DO WITH THIS? -- the keyboard door to the contextual-action surface, on the
    // subject command mode can truthfully name: the room. The pointer's door is a right press,
    // which needs no row here; this row exists because a surface reachable only by a mouse
    // button would be Workshop's first gesture with no catalog identity -- exactly the drift
    // this file ended. `a` bare: portable, and free in every context that intersects kCommand
    // (the application's rows are all chords, kNoText holds `^c`, and no other kCommand row
    // spends it).
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
    // ⭐ THE SOURCE EDITOR'S FIVE ROWS WERE HERE (VD-22, VD-25). `editor.save`,
    // `editor.newline`, `editor.tab` and `editor.discard` are the Editor pane's own
    // `PaneActionRow`s now, in the pane's namespace and on the same four gestures
    // (`editor-pane/vocabulary.hpp`); a maker's authored override moves with the spelling,
    // which is why the ids did not change. The fifth was `editor.discard`'s second row in
    // COMMAND mode -- a key that acted on the editor's document from wherever the maker was
    // standing, so that a quit refusal could name a gesture that worked there. That is
    // exactly the host-mapped route VD-22 refuses, and it retired with the rest: a maker
    // presses into the Editor and discards there, and the refusal says so.
    // ⚠ THE HOST PANE MANAGER'S ROWS WERE HERE -- its list's `up`/`down`/`choose`/`switch`, `o`
    // (open or remove) and the four order keys -- and retired with it. The Pane Creator's `n`,
    // `s`, `ctrl+d` and its name prompt's Return and Escape are the desktop's Pane Manager's own
    // rows now (`desktop-pane/vocabulary.hpp`), under the same ids.
    // ⚠ THE TERMINAL'S FIVE ROWS WERE HERE. `terminal.submit`, `terminal.complete`,
    // `terminal.previous`, `terminal.next` and `terminal.back` are the pane's own
    // `PaneActionRow`s now, in the pane's namespace and on the same five gestures
    // (`terminal-pane/vocabulary.hpp`); a maker's authored override moves with the spelling,
    // which is why the ids did not change.
    // ⚠ THE PICKER'S FOUR ROWS WERE HERE, and retired with it.
    // THE AUTHORING PROMPT'S TWO KEYS LEFT WITH ITS CONTEXT (LOAD-IT). It was a modal of
    // this host's -- one line a maker typed a plan row's role into, in a keyboard context
    // of Workshop's own -- and it served the Files pane too until that browser took its own
    // line inside its own room. The Builder pane did the same, so the last asker is gone and
    // the context with it.
    // THE CURRENT-CONDITION VIEW'S FOUR KEYS LEFT WITH ITS CONTEXT. Three are declared by
    // the Attention pane under the ids and the defaults they had here -- Up, Down and `d`,
    // spelled `attention.up`, `attention.down`, `attention.dismiss` -- so a maker who moved
    // one keeps it moved. `attention.close` and the `ctrl+a` that opened the overlay retired
    // rather than moving: a pane is removed from the desk through the close door, and nothing
    // opens one particular pane from anywhere (VD-22).
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
    // ⚠ A LIVE PROPERTY DRAFT'S TWO ROWS WERE HERE (`draft.commit`, `draft.cancel`) and retired
    // with the host's Pane Manager, the last inspector whose drafts were this host's.
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
    // key never moves a pane it is resizing (the object document's resize kept the same law
    // until it retired). The other six
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
    // The retired picker's four, one purpose over: a list with a cursor and a gesture on the
    // selected row. `context.choose` is ONE action whose meaning the row decides -- a
    // group row descends, an action row requests (`picker.choose`'s shape, while it was) -- and
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

/// A gesture as the FILE spells it: `ctrl+shift+z`. Total over any gesture whose scancode
/// has a name; the declaration table only holds those, and admission refuses the rest on
/// the way in.
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

// WL-KEY-14 -- agents/workshop/keyboard.md
// WL-DESK-08 -- agents/workshop/desktop.md
inline ParsedGesture parse_gesture(std::string_view text) {
    ParsedGesture out;
    if (text.empty()) {
        out.refusal = "a gesture cannot be empty";
        return out;
    }
    // ⭐ `none` IS A GESTURE A MAKER MAY AUTHOR, AND IT IS THE ONE THAT ANSWERS TO NO KEY.
    // Four rows shipped unbound already (`layout.rename` and its three neighbours), so a row
    // with no gesture was always a legal state of this keymap; what was missing was a way for a
    // maker to PUT a row into it. Without that, "replace or disable an application default" had
    // only half an answer -- a maker could move Escape-to-deselect onto another key, and could
    // not say that they want it gone.
    //
    // ⚠ AND IT IS A DISABLE, NOT A DELETE. The row is still declared, still listed in the
    // hotkey view, and still names its action for a later edit; what it has is `kNoGesture`,
    // which `action_for` refuses to match before it compares anything (`is_bound`). Nothing
    // reactivates a hard-wired copy behind it, because after this arc there is no hard-wired
    // copy: Escape-to-deselect is an application row like any other, and a disabled one does
    // nothing at all.
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
    // THE BRANCH THIS FILE'S PROSE STATED FIVE TIMES BEFORE THE CODE DID (BL-DEF-03): the
    // POSIX wire carries ctrl+letter as one control byte, and Shift leaves no mark on it,
    // so ctrl+shift+<letter> arrives as plain ctrl+<letter> -- the same byte, and an
    // authored binding a terminal maker could never tell from the unshifted one.
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

/// ONE ROW A PANE DECLARED, AS IT IS IN FORCE: the id the pane will be asked for, the
/// label a legend prints, and the gesture that requests it now -- the maker's authored
/// override when the file names the id, the pane's declared default otherwise.
///
/// IT HAS NO `Act` AND NO `KeyContext`, and that is the whole difference from `ActionRow`.
/// A pane row has no dispatch site in the host -- execution is the pane's, reached by
/// `PaneActionRequested` carrying `id` -- and its context is the pane's own runtime
/// handle: the row is active exactly while `keyboard_pane` resolves to that handle, which
/// is a value the routing chain already spells as `KeyContext::kPane` (screen_arrange.cpp).
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

/// ⭐ ONE APPLICATION ROW A PARTICIPATING OWNER DECLARED, AS IT IS IN FORCE -- the id the
/// declarer will be asked for, the label a legend prints, the gesture that requests it now
/// (the maker's authored override when the file names the id, the declared default
/// otherwise), and WHERE IN THE CHAIN it is answered.
///
/// IT HAS NO `Act` AND NO `KeyContext`, exactly as a `PaneRow` has neither, and for the same
/// reason: there is no dispatch site in the host -- execution belongs to the declarer, reached
/// by `AppActionRequested` carrying `id`. What it has instead of a `KeyContext` is
/// `precedence`, because an application row's scope is not a mode: it is every mode, and the
/// only question is whether it is asked BEFORE the keys cross to a pane or AFTER nothing more
/// specific claimed them (`app_precedence`, desktop_seam_vocabulary.hpp).
///
/// ⚠ `supersedes` IS NOT HERE, AND ITS ABSENCE IS THE LAW. A pane stands in for an APPLICATION
/// row by naming the row's id on its own `v2::PaneActionRow::supersedes` -- the supersession
/// travels in one direction, from the specific to the general, so an application row cannot
/// take a gesture away from a pane by declaring that it owns it.
// WL-DESK-07 -- agents/workshop/desktop.md
struct AppRow {
    std::string id;
    std::string label;
    Gesture gesture;
    std::int64_t precedence = 0; ///< `app_precedence::kAboveModes` / `kDefault`
};

/// HOW MANY APPLICATION ROWS ONE DECLARATION MAY CARRY. A pane's bound, one shape over, and
/// deliberately the same number: a declarer that needs more than this many gestures above every
/// mode is not declaring application defaults, it is claiming the keyboard.
// WL-DESK-07 -- agents/workshop/desktop.md
inline constexpr std::size_t kMaxAppActionRows = 32;

/// HOW MANY ROWS ONE PANE MAY DECLARE. The catalog's own bound, one shape over
/// (`kMaxPaneCatalogEntries`, panel.hpp): a runtime-catalog policy, deliberately its own
/// constant, bounding what a chatty provider can make this session retain and a legend
/// try to print.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline constexpr std::size_t kMaxPaneActionRows = 32;

/// THE BOUNDS ON ONE ROW'S TWO STRINGS, in BYTES -- an id is spelled in the keymap file
/// and a label on the band, and neither is a place for a paragraph.
inline constexpr std::size_t kMaxPaneActionIdLen = 64;
inline constexpr std::size_t kMaxPaneActionLabelLen = 32;

/// THE ONE SENTENCE THE COLLISION LAW SAYS, wherever it runs -- at the keymap file's
/// admission over the built-in rows, and at a pane's admission over the built-in rows and
/// its own. Two moments, one wording, so a maker reads the same refusal whichever party
/// arrived second.
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
    /// THE ROWS EVERY PANE WEAVE DECLARED, AS THEY ARE IN FORCE -- joined by
    /// `join_pane_rows` from what a pane's catalog row retains and from `authored`, and
    /// re-joined whenever either side changes. DERIVED, like `overrides`: a save writes
    /// none of it back, and a file read replaces none of it -- the loader re-joins.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    std::vector<PaneRows> panes;
    /// ⭐ THE APPLICATION ROWS THE PARTICIPATING DEFAULTS OWNER DECLARED, AS THEY ARE IN
    /// FORCE -- joined by `join_app_rows` from one office's declaration and from `authored`,
    /// and re-joined whenever either side changes. DERIVED, like `overrides` and `panes`: a
    /// save writes none of it back, and a file read replaces none of it.
    ///
    /// EMPTY IS THE HONEST DEFAULT AND NOT A FALLBACK. A Workshop whose desktop weave never
    /// loaded has no application rows, which means `Ctrl+t` does nothing and Escape sheds no
    /// selection -- and the band, the hotkey view and the backdrop all say why. There is no
    /// compiled-in copy waiting behind this vector.
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

    /// EVERY GESTURE ONE DECLARATION ROW ANSWERS TO RIGHT NOW, in authored order: the maker's
    /// overrides when the file names the row's action -- several rows for one id are one action
    /// with several keys -- and the developer's default otherwise. An override moves ALL of an
    /// action's rows, which is what "quit is ctrl+q now" means.
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

    /// HAS THE PANE HOLDING THE KEYBOARD DECLARED THAT IT OWNS THIS ACTION? (WL-KEY-15)
    /// By ID, never by gesture: a maker who moved either row's key moved neither row's
    /// meaning. A handle with no admitted rows -- every non-pane context passes one --
    /// supersedes nothing.
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

    /// WHICH PANE ACTUALLY OWNS INPUT IN THIS CONTEXT, or `kNoPaneKind` (VD-27). Workshop
    /// REMEMBERS which pane the keyboard was last pointed at, and that memory outlives the
    /// mode: a maker who opens the contextual menu over a pane is typing into the MENU, not
    /// into the pane, and a source editor that still held the memory suppressed the object
    /// document's save from a context that has always offered it -- MEASURED. Ownership is
    /// the resolved context and the remembered pane together, and this is the only place the
    /// two are combined.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    static constexpr std::int64_t owner_of(KeyContext current,
                                           std::int64_t keyboard_pane) noexcept {
        return current == KeyContext::kPane ? keyboard_pane : -1;
    }

    /// IS THIS DECLARED ROW REQUESTABLE AT THIS MOMENT? The context class says which modes
    /// it lives in; the pane that OWNS input says whether it has stood down for that pane's
    /// own row. The one answer every resolver and every view spends, so a legend cannot
    /// advertise a key the chain will not run.
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
        // A KEY THIS BUILD CANNOT NAME REQUESTS NOTHING. Without this an unnamed
        // key would match every row that declares `kNoGesture` and the first one in
        // declaration order would run -- a press with no name performing an operation.
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

    /// ⭐ WHICH APPLICATION ROW OF THIS PRECEDENCE CLASS THIS GESTURE REQUESTS, or nullptr.
    ///
    /// THE TWO GUARDS ARE `action_for`'S, FOR `action_for`'S REASONS. A key this build cannot
    /// name requests nothing -- otherwise an unnamed key would match every row a maker
    /// disabled with `none` and run the first. And a row whose id the pane holding the
    /// keyboard has declared it stands in for is not requestable while that pane has them:
    /// the same `pane_supersedes` the host's own `kUnlessOwned` rows are judged by, by ID and
    /// never by gesture, so a maker who moved either row moved neither row's meaning.
    ///
    /// ⚠ SUPERSESSION APPLIES TO BOTH CLASSES. A `kDefault` row is asked last, but "last" is
    /// not "after the pane declined it" -- the pane never tells Workshop whether it spent a
    /// key (WL-ARR-15), so the only honest exclusion is the declared one.
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

/// ACTION IDS WHOSE OWNER CHANGED, AND THE ID A KEYMAP FILE'S ROW FOR EACH IS READ AS NOW.
///
/// A maker's file is a promise about MEANING, and an owner moving did not change what these two
/// mean: `workshop.terminal` opened the Terminal and `workshop.hotkeys` opened the key list, and
/// the desktop's `desktop.terminal` and `desktop.hotkeys` do the same. So an authored row for the
/// old id is applied to the new one when the new one is not authored itself -- one row, one
/// meaning, never two rows in force -- and the load says so, naming the rename to make. The file
/// is not rewritten. `workshop.picker` is deliberately NOT here: the + panel picker toggled a
/// pane's participation, and the Pane Manager's key opens a tool; a meaning did change.
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

/// ACTION IDS THAT RETIRED WITH WHAT THEY ACTED ON, and what that was. A maker's authored row for
/// one is kept byte for byte, like any row nothing declares, and the load SAYS which ones retired
/// and with what, rather than leaving them among ids a pane may yet declare. Nothing answers them,
/// and a pane may not declare one as its own (`join_pane_rows`). A pane that names one as the row
/// it stands in for (`kOwnableDocumentSave`, `kOwnableDocumentOpen`, published before the document
/// retired) is admitted, standing in for nothing.
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

/// JOIN ONE PANE'S DECLARED ROWS INTO A KEYMAP, or say why not -- the pane's half of
/// admission, over a VALUE, so a suite can ask it with no bus.
///
/// THE LAW, IN ORDER: the row count; each row's id and label; each default gesture (an
/// unbound row carries `kUnknown` and no modifiers; any other scancode is one the file's
/// grammar can name, and the modifiers are the four this application knows); no id
/// twice, and no id that is one of Workshop's own -- the file would then name two things
/// with one row. Then the maker's authored overrides, by id, through the file's own
/// `parse_gesture`; an id authored twice or a gesture outside the grammar is refused in
/// `apply_overrides`' own words. Then THE COLLISION LAW over the effective map: every
/// built-in row that can be active while a pane holds the keys (`contexts_intersect` with
/// `kPane`: the globals -- never the no-text rows, which a text-taking pane already
/// outranks, and never another mode's), and every other row of this same pane. Another
/// pane's rows are another context and never meet these.
///
/// ATOMIC: a refusal writes nothing, so the pane's previous rows stand; acceptance
/// replaces them whole.
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
            // ⭐ ...OR AN APPLICATION ROW OF THE ABOVE-THE-MODES CLASS (WL-DESK-07). Those are
            // the rows that would otherwise take a gesture from a pane holding the keyboard,
            // so those are exactly the rows a pane must be able to stand in for. A
            // DEFAULT-class row is asked only where the keys did not cross to this pane, so
            // there is nothing there to stand in for -- and saying so is refused rather than
            // accepted-and-ignored.
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
    // THE MAKER'S OWN FILE, applied to the ids it names -- rows that were preserved as
    // unknown when the file loaded, because nobody had declared them yet (WL-KEY-06). Several
    // authored rows for one id are one action with several keys: the pane's row is repeated,
    // one entry per gesture, so dispatch answers to any of them and a legend spells the first.
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
            // ...EXCEPT THE ONES THIS PANE STANDS IN FOR. A superseded row is not
            // requestable ANYWHERE in this pane -- dispatch suppresses it for the pane, not
            // for one row of it -- so no row of this declaration can collide with it. Judging
            // that per row instead refused a pane that put `editor.save` on `ctrl+e` and
            // `editor.newline` on `ctrl+s`, which the keymap before all this accepted, and a
            // rejoin then dropped the pane's whole action set (VD-27).
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
        // ...AND AGAINST THE APPLICATION ROWS (WL-DESK-07). An application row is active in
        // every context, so it meets every pane's rows exactly as a `kUnlessOwned` host row
        // does -- and it stands down for this pane under the same declaration, by the same
        // name. A pane that wants `ctrl+t` for itself says `supersedes: "desktop.terminal"`;
        // one that merely takes it is refused, in the same sentence the file's own law says.
        for (const AppRow& row : k.app) {
            // A DEFAULT-CLASS ROW IS ASKED ONLY WHERE THE KEYS DID NOT CROSS TO THIS PANE, so
            // it and this pane's row can never both fire -- the same asymmetry `join_app_rows`
            // writes from the other side.
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

/// ⭐ JOIN THE APPLICATION ROWS ONE PARTICIPATING OWNER DECLARED (WL-DESK-07).
///
/// `join_pane_rows` one scope out, and deliberately the same shape of function: the same text
/// bounds, the same namespace rule, the same authored-override application, the same collision
/// law and the same atomic outcome. What differs is what the rows are judged AGAINST -- an
/// application row is active in every context, so it meets every host row and every pane's rows
/// rather than only the ones a pane's context intersects.
///
/// ⚠ THE PANES ARE JUDGED AGAINST THE NEW ROWS, NOT ONLY THE NEW ROWS AGAINST THE PANES. A
/// declaration arriving after a pane's would otherwise take a gesture the pane is already
/// holding and leave both live. The caller re-joins the panes after this returns (`rejoin_pane_rows`);
/// what THIS function refuses is a declaration that collides with rows already in force, so
/// both arrival orders end with one gesture meaning one thing.
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
            // A PRECEDENCE THIS BUILD CANNOT NAME IS NOT GUESSED (VD-21's "refuse rather than
            // pretend"). Defaulting it to "above every mode" would give a row written against
            // a later protocol the strongest position in the chain by accident.
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
        // ⚠ AND THE PRECEDENCE CLASS DECIDES WHO IT CAN COLLIDE WITH, which is the whole
        // reason there are two classes rather than one.
        //
        // AN ABOVE-THE-MODES ROW IS ANSWERED BEFORE EVERY HOST ROW BELOW THE FIVE and before
        // the keys cross to a pane, so it MEETS all of them: two declarations that could both
        // fire on one gesture, which is exactly what the collision law is about.
        //
        // A DEFAULT ROW MEETS NONE OF THEM, and that is a fact about the chain rather than a
        // leniency. It is asked only where the resolved context claimed nothing
        // (`action_for(...) == kNone`) and only where the keys did not cross to a pane that
        // took them -- so a host row and a default row on one gesture cannot both fire, and
        // neither can a pane's row and a default row. `context.back` on Escape and
        // `desktop.deselect` on Escape is the shipped instance of this: while the contextual
        // surface is open Escape closes it, everywhere else it puts the selection down, and
        // refusing the pair would have made the second unauthorable. (`picker.close` was the
        // first instance, and retired with the picker.)
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
