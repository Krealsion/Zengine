// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP

// The pane protocol: every shape a weave and Workshop exchange to offer, present and drive a
// pane (workshop/docs/workshop-panes.md). No shape names a provider: the provider is the office
// Loom stamped on the message, `mail.authored_role()`, which no payload can write. Workshop sends
// each gesture and asks nothing back; a shape a pane does not accept is refused at Loom's gate.

#include "graphics/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// Workshop -> everyone: who has panes? It carries nothing; a field would be a filter.
struct PaneCatalogRequested {
    ZEN_SHAPE(PaneCatalogRequested, 1);
};

/// Provider -> Workshop: one pane it can present -- its key in the office's own namespace, and
/// the two lines the Pane Manager lists.
struct PaneOffered {
    std::string pane;    ///< the durable pane key, in the AUTHORING office's namespace
    std::string name;    ///< what the Pane Manager lists
    std::string summary; ///< one line, so a weaver can tell what they are opening
    ZEN_SHAPE(PaneOffered, 1, ZEN_FIELD(pane), ZEN_FIELD(name), ZEN_FIELD(summary));
};

/// The most body rows, and the most body columns, a pane may ask for (`v2::PaneOffered`).
inline constexpr std::int64_t kMaxPaneComfort = 512;

namespace v2 {
/// Initial comfort in body text rows/columns, each 1..`kMaxPaneComfort`. Zero/zero retains the
/// v1 fallback.
/// A refresh does not replace the first accepted preference for this pane identity.
struct PaneOffered {
    std::string pane;
    std::string name;
    std::string summary;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    ZEN_SHAPE(PaneOffered, 2, ZEN_FIELD(pane), ZEN_FIELD(name), ZEN_FIELD(summary),
              ZEN_FIELD(rows), ZEN_FIELD(columns));
};
} // namespace v2

/// The widest, and the tallest, canvas body a pane may ask for, in canvas pixels
/// (`v3::PaneOffered`): the text comfort's bound, in pixels.
inline constexpr std::int64_t kMaxPaneBodyPx = kMaxPaneComfort * graphics::kCellPx;

namespace v3 {
/// A canvas pane's initial body: `width` by `height` canvas pixels, each 1..`kMaxPaneBodyPx`, and
/// `text_rows` rows of the medium's own text beneath them (0..`kMaxPaneComfort`), so a picture is
/// granted exactly its size whatever the medium sets type at. Zero/zero retains the v1 fallback.
/// A refresh does not replace the first accepted preference for this pane identity.
struct PaneOffered {
    std::string pane;
    std::string name;
    std::string summary;
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::int64_t text_rows = 0;
    ZEN_SHAPE(PaneOffered, 3, ZEN_FIELD(pane), ZEN_FIELD(name), ZEN_FIELD(summary),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(text_rows));
};
} // namespace v3

/// Workshop -> provider: a key went down while this pane held the keyboard. `scancode` and
/// `modifiers` are `zengine::input`'s own numbers, unchanged (input/vocabulary.hpp is not
/// included, so a pane that takes no keys does not depend on it); no platform event, key name,
/// repeat or release crosses. Holding the keyboard means the weaver pressed into this pane last --
/// not a capture or a lease -- and chords the keymap answers above every mode never arrive here.
struct PaneKey {
    std::string pane;
    std::int64_t scancode = 0;  ///< `zengine::input::scan`'s space, unchanged
    std::int64_t modifiers = 0; ///< `zengine::input::mod`'s bitmask, unchanged
    ZEN_SHAPE(PaneKey, 1, ZEN_FIELD(pane), ZEN_FIELD(scancode), ZEN_FIELD(modifiers));
};

/// Workshop -> provider: the text the platform made of a keystroke, as UTF-8 -- the only truthful
/// route to a character. A shape of its own because a key may make no text and text may come
/// with no key; a keystroke that makes both sends both, key first. No selection, IME or clipboard.
struct PaneTextInput {
    std::string pane;
    std::string text;
    ZEN_SHAPE(PaneTextInput, 1, ZEN_FIELD(pane), ZEN_FIELD(text));
};

/// Two retired host actions a pane may still name in `supersedes`: admitted standing in for
/// nothing, so a provider built against this header loads on either side of the retirement.
inline constexpr const char* kOwnableDocumentSave = "document.save";
inline constexpr const char* kOwnableDocumentOpen = "document.open";

/// THE BOUNDS ON ONE ROW'S TWO STRINGS, in BYTES -- an id is spelled in the keymap file and a
/// label on the band, and neither is a place for a paragraph. Workshop refuses a declaration past
/// either; they are published here so a pane can judge its own rows before it declares them.
inline constexpr std::size_t kMaxPaneActionIdLen = 64;
inline constexpr std::size_t kMaxPaneActionLabelLen = 32;

/// One action a pane declares: an id in its own namespace -- the spelling a weaver's keymap file
/// names, so renaming it breaks their overrides -- a label, and a default gesture as `PaneKey`'s
/// two numbers (`kUnknown` = none). A default must be one the keymap file can spell, or admission
/// refuses it; Workshop judges the id and never interprets it, and no key name crosses.
struct PaneActionRow {
    std::string id;             ///< the durable action id, in the PANE's own namespace
    std::string label;          ///< what a legend prints beside the key
    std::int64_t scancode = 0;  ///< `zengine::input::scan`'s space; `kUnknown` = no default
    std::int64_t modifiers = 0; ///< `zengine::input::mod`'s bitmask
    ZEN_SHAPE(PaneActionRow, 1, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers));
};

/// Provider -> Workshop: the actions one pane declares beside its offer, judged whole under the
/// office stamp. A declaration that fails joins nothing and keeps the previous rows; one that
/// passes replaces them. Declaring rows points no keyboard at the pane.
struct PaneActions {
    std::string pane;                ///< the pane key, in the AUTHORING office's namespace
    std::vector<PaneActionRow> rows; ///< at most `kMaxPaneActionRows` (workshop/keymap.hpp)
    ZEN_SHAPE(PaneActions, 1, ZEN_FIELD(pane), ZEN_FIELD(rows));
};

/// Version 2 of the declaration adds `supersedes`. A version, not a field: a published
/// `(name, version)` is frozen (Loom GATE-04), so v1 is unchanged and a pane built against it
/// still loads. Workshop joins both into one row set; a later statement replaces a pane's rows.
namespace v2 {

/// `PaneActionRow` v1's four fields, plus the one this version exists for.
struct PaneActionRow {
    std::string id;             ///< the durable action id, in the PANE's own namespace
    std::string label;          ///< what a legend prints beside the key
    std::int64_t scancode = 0;  ///< `zengine::input::scan`'s space; `kUnknown` = no default
    std::int64_t modifiers = 0; ///< `zengine::input::mod`'s bitmask
    /// One of Workshop's own action ids this row stands in for while the pane owns the keyboard,
    /// or empty (WL-KEY-15). An application row (WL-DESK-07) stays superseded wherever a keymap
    /// file moved either row; a retired id stands in for nothing, and an application row no weave
    /// declares now, `desktop.` and its name, for nothing until one does; any other is refused.
    std::string supersedes;
    ZEN_SHAPE(PaneActionRow, 2, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(supersedes));
};

/// `PaneActions` v1's statement, over v2 rows. Every law of the v1 shape applies unchanged:
/// judged whole under the office stamp, atomic both ways, and a replacement rather than a merge.
struct PaneActions {
    std::string pane;
    std::vector<PaneActionRow> rows;
    ZEN_SHAPE(PaneActions, 2, ZEN_FIELD(pane), ZEN_FIELD(rows));
};

} // namespace v2

// ---- What a press may cause ----------------------------------------------------------------

/// ONE SHAPE A PRESS OF AN ACTION MAY SEND, to anyone, directly or after an answer it waits on,
/// beside the pane protocol a pane speaks to Workshop about itself (its offer, rows, content,
/// caret, menus and the approval it asks before an act). `subject` says what the send is about:
/// empty, the pane's own state; `on <prefix>`, the part a press lands on, named `<prefix><s>`;
/// `to <office>/<pane>`, always that one pane. No field of the message is declared.
struct ActionCause {
    std::string shape;        ///< the shape's name, as Loom knows it
    std::int64_t version = 0; ///< its version, 1 or more
    std::string subject;      ///< "", "on <prefix>" or "to <office>/<pane>"
    ZEN_SHAPE(ActionCause, 1, ZEN_FIELD(shape), ZEN_FIELD(version), ZEN_FIELD(subject));
};

/// What the rows of ONE OTHER MODE of a pane may cause: never its rows, which only the mode in
/// force declares, so a reader can tell "another mode can" from "nothing here can".
struct ModeCauses {
    std::string mode; ///< the mode's name, as the pane's census and manual name it
    std::vector<ActionCause> causes;
    ZEN_SHAPE(ModeCauses, 1, ZEN_FIELD(mode), ZEN_FIELD(causes));
};

/// The most causes one row may declare, and one other mode's entry; the most other modes; the
/// longest mode name and shape name (a subject's bound is `kMaxCauseSubjectLen`, beside the parts).
inline constexpr std::size_t kMaxActionCauses = 16;
inline constexpr std::size_t kMaxModeCauses = 64;
inline constexpr std::size_t kMaxPaneModes = 16;
inline constexpr std::size_t kMaxPaneModeLen = 32;
inline constexpr std::size_t kMaxCauseShapeLen = 128;

/// Version 3 of the declaration says what each press may cause, and the modes: the mode these rows
/// are in force for, the mode a row's press leaves the pane in, and what each other mode's rows may
/// cause. Workshop widens v1 and v2 with none of them and holds the three where it holds the rows.
namespace v3 {

/// `PaneActionRow` v2's five fields, the shapes a press may send, and the mode it enters.
struct PaneActionRow {
    std::string id;             ///< the durable action id, in the PANE's own namespace
    std::string label;          ///< what a legend prints beside the key
    std::int64_t scancode = 0;  ///< `zengine::input::scan`'s space; `kUnknown` = no default
    std::int64_t modifiers = 0; ///< `zengine::input::mod`'s bitmask
    std::string supersedes = {};               ///< as v2's
    std::vector<ActionCause> causes = {};      ///< at most `kMaxActionCauses`, none twice
    std::string enters = {};                   ///< a mode `elsewhere` names, or empty: no change
    ZEN_SHAPE(PaneActionRow, 3, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(supersedes), ZEN_FIELD(causes), ZEN_FIELD(enters));
};

/// The rows of the mode in force, its name, and each other mode's causes, judged whole by the laws
/// of v1 and v2 and replaced whole.
struct PaneActions {
    std::string pane;
    std::string mode;                       ///< the mode these rows are in force for; may be empty
    std::vector<PaneActionRow> rows;
    std::vector<ModeCauses> elsewhere = {}; ///< at most `kMaxPaneModes`, each mode once
    ZEN_SHAPE(PaneActions, 3, ZEN_FIELD(pane), ZEN_FIELD(mode), ZEN_FIELD(rows),
              ZEN_FIELD(elsewhere));
};

} // namespace v3

/// Version 4 of the declaration lets a row name a key for a POSIX terminal beside its default:
/// where the keys come from a POSIX terminal it stands in place of the default, and everywhere
/// else the default stands (WL-KEY-08). Workshop widens v1 to v3 with none.
namespace v4 {

/// `PaneActionRow` v3's seven fields, and a key for a POSIX terminal.
struct PaneActionRow {
    std::string id;             ///< the durable action id, in the PANE's own namespace
    std::string label;          ///< what a legend prints beside the key
    std::int64_t scancode = 0;  ///< `zengine::input::scan`'s space; `kUnknown` = no default
    std::int64_t modifiers = 0; ///< `zengine::input::mod`'s bitmask
    std::string supersedes = {};          ///< as v2's
    std::vector<ActionCause> causes = {}; ///< as v3's
    std::string enters = {};              ///< as v3's
    /// The key in force in place of the default where the keys come from a POSIX terminal, or
    /// `kUnknown` for none: a key that terminal delivers, on a row with a default.
    std::int64_t posix_scancode = 0;
    std::int64_t posix_modifiers = 0; ///< its modifiers, as `modifiers`
    ZEN_SHAPE(PaneActionRow, 4, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(supersedes), ZEN_FIELD(causes), ZEN_FIELD(enters),
              ZEN_FIELD(posix_scancode), ZEN_FIELD(posix_modifiers));
};

/// The rows of the mode in force, its name and each other mode's causes, as v3 says them, over
/// v4 rows, judged and replaced whole by the laws of v1 to v3.
struct PaneActions {
    std::string pane;
    std::string mode;                       ///< as v3's
    std::vector<PaneActionRow> rows;
    std::vector<ModeCauses> elsewhere = {}; ///< as v3's
    ZEN_SHAPE(PaneActions, 4, ZEN_FIELD(pane), ZEN_FIELD(mode), ZEN_FIELD(rows),
              ZEN_FIELD(elsewhere));
};

} // namespace v4

/// Workshop -> provider: its verdict on one action declaration, as Loom's answer to that
/// delivery, so it reaches only the incarnation that declared (Loom ANS-03). `declaration`
/// numbers an accepted one and is never reused; it mandates nothing, and silence is no verdict.
struct ActionsJudged {
    std::string pane;             ///< the pane key declared; empty for application rows
    bool accepted = false;
    std::int64_t declaration = 0; ///< Workshop's number for it when accepted; 0 when refused
    std::string refusal;          ///< Workshop's own sentence; empty exactly when accepted
    ZEN_SHAPE(ActionsJudged, 1, ZEN_FIELD(pane), ZEN_FIELD(accepted), ZEN_FIELD(declaration),
              ZEN_FIELD(refusal));
};

/// Workshop -> provider: an accepted declaration left the keymap. It names the number, so a
/// successor holding the office can tell it is not about its own rows; nothing is retained.
struct ActionsWithdrawn {
    std::string pane;             ///< as the declaration named it; empty for application rows
    std::int64_t declaration = 0; ///< the number `ActionsJudged` gave it
    std::string refusal;          ///< why, in the weaver's words
    ZEN_SHAPE(ActionsWithdrawn, 1, ZEN_FIELD(pane), ZEN_FIELD(declaration), ZEN_FIELD(refusal));
};

/// Workshop -> provider: the weaver pressed the gesture one of this pane's rows answers to, as the
/// resolved id after the weaver's keymap -- sent instead of `PaneKey`, the keystroke's character
/// swallowed. A keystroke matching no row crosses as `PaneKey` and `PaneTextInput`.
struct PaneActionRequested {
    std::string pane;
    std::string id; ///< one of the ids this pane declared, as it declared it
    ZEN_SHAPE(PaneActionRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(id));
};

/// Provider -> Workshop, about a pane its office offered: seat it now, select it and point the
/// keys at it -- an ask, sent when the pane's own act needs nothing more. Judged through the
/// launch door's trial and written whole in the answering delivery, or refused with nothing
/// moved. Not an offer, a room grant, a reservation or focus authority; the notice line names
/// the asking pane every time.
struct PaneRevealRequested {
    std::string pane;
    ZEN_SHAPE(PaneRevealRequested, 1, ZEN_FIELD(pane));
};

/// Workshop's answer on the delivery that asked: `seated` (on the desk, selected, keys pointed
/// at it, all written before this answer), or the launch door's refusal with nothing moved.
struct PaneRevealAnswered {
    std::string pane;
    bool seated = false;
    std::string refusal;
    ZEN_SHAPE(PaneRevealAnswered, 1, ZEN_FIELD(pane), ZEN_FIELD(seated), ZEN_FIELD(refusal));
};

/// Provider -> Workshop: the Escape Workshop sent this pane was unspent, so Workshop's own last
/// meaning (putting the pane down) may run. Sent under the correlation that Escape arrived on,
/// and honoured only while the pane is still selected with the keys and that Escape is still the
/// weaver's latest gesture. A pane that never says it keeps Escape.
struct PaneEscapeUnspent {
    std::string pane;
    ZEN_SHAPE(PaneEscapeUnspent, 1, ZEN_FIELD(pane));
};

/// Workshop -> everyone, published as its office: may this Workshop end? It waits for as many
/// answers as Loom counted accepters. A pane that accepts it MUST answer (`PaneQuitAnswered`)
/// truthfully, on the delivery that asked; a pending operation that could change the answer is a
/// refusal. Gestures are held until the last answer and replayed if the quit is refused; a
/// delivery Loom refuses refuses the quit.
struct PaneQuitRequested {
    ZEN_SHAPE(PaneQuitRequested, 1);
};

/// One pane's answer: `permitted`, or its own sentence naming what an orderly end would lose.
struct PaneQuitAnswered {
    std::string pane;
    bool permitted = false;
    std::string refusal;
    ZEN_SHAPE(PaneQuitAnswered, 1, ZEN_FIELD(pane), ZEN_FIELD(permitted), ZEN_FIELD(refusal));
};

// ---- The keys a pane holds, a press handed back, and a menu a pane asks the host to present ----
//
// Every word a pane says about a gesture echoes that gesture's correlation -- for a press, the
// number the graphics window that showed the pane gave it; a stale, zero or spent one moves
// nothing. A press is read against the drawing the pane last sent the window: no number names
// which drawing the window showed when it read the press.

/// Workshop -> the office of a pane a graphics window shows in an area of its own, as Workshop's
/// office: the keys now reach the pane (`here`), or no longer do, said each time that changes. A
/// press the window gives the pane is read against the last of these it heard before that press:
/// whether the keys were already the pane's when it was made.
struct PaneKeys {
    std::string pane;
    bool here = false;
    ZEN_SHAPE(PaneKeys, 1, ZEN_FIELD(pane), ZEN_FIELD(here));
};

/// Provider -> Workshop: that press was not mine -- open your own surface for my pane. Echoes the
/// window's number for the press; the host's pane menu opens once, where the press was made, while
/// the press is the latest act.
struct PanePassRequested {
    std::string pane;
    ZEN_SHAPE(PanePassRequested, 1, ZEN_FIELD(pane));
};

/// Provider -> Workshop: give my pane the keys, because a menu choice asked for an edit. Echoes
/// the choice's correlation and is granted only while that choice is the weaver's latest act.
struct PaneKeyboardRequested {
    std::string pane;
    ZEN_SHAPE(PaneKeyboardRequested, 1, ZEN_FIELD(pane));
};

/// One row a pane offers for presentation: an id (what comes back chosen) and a label. A sentence
/// the presenter shows and returns, never an operation the host performs.
struct PaneMenuRow {
    std::string id;
    std::string label;
    ZEN_SHAPE(PaneMenuRow, 1, ZEN_FIELD(id), ZEN_FIELD(label));
};

/// THE MARK A ROW'S LABEL ENDS WITH WHEN CHOOSING IT OPENS ANOTHER MENU, the host's groups and a
/// pane's rows alike (`pane_menu::Offer::submenu`); a row that asks for more input ends in "...".
inline constexpr const char* kSubmenuMark = " >";

/// THE MARK BEFORE THE KEY THAT DOES WHAT A ROW DOES, in its label after its words
/// (`pane_menu::Offer::row` with a key): a presenter shows the key apart, beside the words. A
/// tab, which no label draws, so a presenter that knows no key shows a blank there.
inline constexpr char kMenuKeyMark = '\t';

/// Presentation bounds on one menu; the id bound is a declared action's (`kMaxPaneActionIdLen`),
/// published here so a presenter built from this header alone can judge.
inline constexpr std::size_t kMaxPaneMenuRows = 32;
inline constexpr std::size_t kMaxPaneMenuIdLen = 64;
inline constexpr std::size_t kMaxPaneMenuLabelLen = 64;

/// Provider -> Workshop: present these rows beside this place in my area -- a row and a column of
/// the window's text there, from the area's corner -- and say which was chosen. Echoes the
/// gesture's correlation; judged where the menu would open and granted to the presenter
/// (`presenter_vocabulary.hpp`). A menu continuing a press opens where the press was made.
/// `subject` is the pane's own word, echoed in the answer.
struct PaneMenuRequested {
    std::string pane;
    std::string subject;
    std::int64_t row = 0;
    std::int64_t column = 0;
    std::vector<PaneMenuRow> rows;
    ZEN_SHAPE(PaneMenuRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(subject), ZEN_FIELD(row),
              ZEN_FIELD(column), ZEN_FIELD(rows));
};

/// THE OFFICE THAT PRESENTS A PANE'S MENU AND ANSWERS IT. A requester authenticates a choice as
/// authored from this office; Workshop grants menus to whoever holds it.
inline constexpr const char* kPresenterRole = "zengine.presenter";

/// What the menu came to: `chosen` with the row's `id`, or not (dismissed, replaced, closed,
/// refused). Exactly once per ask, from the presenter -- or from Workshop for an ask it refused or
/// a presenter that left. A choice is a fact about a gesture, not an authority.
struct PaneMenuAnswered {
    std::string pane;
    std::string subject;
    bool chosen = false;
    std::string id;
    std::string refusal;
    ZEN_SHAPE(PaneMenuAnswered, 1, ZEN_FIELD(pane), ZEN_FIELD(subject), ZEN_FIELD(chosen),
              ZEN_FIELD(id), ZEN_FIELD(refusal));
};

/// Provider -> Workshop: open the host's own pane menu for `office`/`target` -- the management
/// route for a covered, closed or press-consuming pane. Echoes the menu answer's correlation;
/// refused for a pair the inventory does not name.
struct PaneManageRequested {
    std::string pane;
    std::string office;
    std::string target;
    ZEN_SHAPE(PaneManageRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(office), ZEN_FIELD(target));
};

// ---- The parts a pane names --------------------------------------------------------------
//
// A pane names the parts a weaver acts on -- a row, a control inside one, an element of its
// drawing -- with names of its own that it keeps across its redraws, so an agent finds a part by
// what it is rather than by where it falls: the action its drawing's record asks
// (`graphics::ElementRecord`), which the window that shows the pane's area says in its reading
// (`graphics::AreaShown`). A name is the pane's: nothing interprets it.

/// The most parts one drawing may name, and the longest a name may be.
inline constexpr std::size_t kMaxPaneParts = 2048;
inline constexpr std::size_t kMaxPanePartNameLen = 256;
/// The longest subject a cause may name: `on ` or `to ` and a part's prefix, or a pane.
inline constexpr std::size_t kMaxCauseSubjectLen = 3 + kMaxPanePartNameLen;


} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
