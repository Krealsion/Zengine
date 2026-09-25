// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP

// The pane protocol: every shape a weave and Workshop exchange to offer, present and drive a
// pane (docs/reference/workshop-panes.md). No shape names a provider: the provider is the office
// Loom stamped on the message, `mail.authored_role()`, which no payload can write. Workshop sends
// each gesture and asks nothing back; a shape a pane does not accept is refused at Loom's gate.

#include "surface/vocabulary.hpp"

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
    std::string summary; ///< one line, so a maker can tell what they are opening
    ZEN_SHAPE(PaneOffered, 1, ZEN_FIELD(pane), ZEN_FIELD(name), ZEN_FIELD(summary));
};

namespace v2 {
/// Initial comfort in body text rows/columns. Zero/zero retains the v1 fallback.
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

/// Workshop -> provider: the prose rows and columns granted to the pane's body -- never cells,
/// pixels or a rectangle. Sent when the pane opens, on a valid re-offer, and when the fit changes.
struct PaneRoom {
    std::string pane;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    ZEN_SHAPE(PaneRoom, 1, ZEN_FIELD(pane), ZEN_FIELD(rows), ZEN_FIELD(columns));
};

/// Provider -> Workshop: the pane's rows, inside the room granted. Content over the budget is
/// refused whole, never truncated.
struct PaneContent {
    std::string pane;
    std::vector<surface::SurfaceTextRow> rows;
    ZEN_SHAPE(PaneContent, 1, ZEN_FIELD(pane), ZEN_FIELD(rows));
};

/// Content naming the generation of the subject it projects. Admitted only if not older than the
/// generation held, so a queued projection of a replaced document cannot repaint its successor.
/// A new version rather than a field: a published `(name, version)` is frozen (Loom GATE-04).
namespace v2 {

struct PaneContent {
    std::string pane;
    std::vector<surface::SurfaceTextRow> rows;
    std::int64_t generation = 0; ///< the subject's generation these rows project
    ZEN_SHAPE(PaneContent, 2, ZEN_FIELD(pane), ZEN_FIELD(rows), ZEN_FIELD(generation));
};

} // namespace v2

/// Workshop -> provider: a primary press inside the pane's granted room, as a place in the
/// `PaneRoom` lattice (row 0 is the body's first prose row, under Workshop's header row). A press
/// that names no row is consumed and not sent; no pixel, cell or rectangle ever crosses.
struct PanePressed {
    std::string pane;
    std::int64_t row = 0;
    std::int64_t column = 0;
    ZEN_SHAPE(PanePressed, 1, ZEN_FIELD(pane), ZEN_FIELD(row), ZEN_FIELD(column));
};

/// The same press, and whether ordinary keys were reaching this pane just before it: a routing
/// fact read before the press moved anything, never a command or a focus notice. Sent instead of
/// v1, only to a holder that accepts it; a v1 press states no routing fact.
namespace v2 {

struct PanePressed {
    std::string pane;
    std::int64_t row = 0;
    std::int64_t column = 0;
    bool keys_went_here = false; ///< ordinary keys reached this pane just before this press
    ZEN_SHAPE(PanePressed, 2, ZEN_FIELD(pane), ZEN_FIELD(row), ZEN_FIELD(column),
              ZEN_FIELD(keys_went_here));
};

} // namespace v2

/// Workshop -> provider: a key went down while this pane held the keyboard. `scancode` and
/// `modifiers` are `zengine::input`'s own numbers, unchanged (input/vocabulary.hpp is not
/// included, so a pane that takes no keys does not depend on it); no platform event, key name,
/// repeat or release crosses. Holding the keyboard means the maker pressed into this pane last --
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

/// Workshop -> provider: the wheel turned over this pane's body -- `input::PointerWheel`'s
/// notches, unconverted (+1.0 per notch away from the maker). It follows the pointer, not the
/// keyboard, and names no place: what a notch is worth is the pane's.
struct PaneWheel {
    std::string pane;
    double dx = 0.0; ///< horizontal notches, +1.0 per notch to the right
    double dy = 0.0; ///< vertical notches, +1.0 per notch away from the maker
    ZEN_SHAPE(PaneWheel, 1, ZEN_FIELD(pane), ZEN_FIELD(dx), ZEN_FIELD(dy));
};

/// Two retired host actions a pane may still name in `supersedes`: admitted standing in for
/// nothing, so a provider built against this header loads on either side of the retirement.
inline constexpr const char* kOwnableDocumentSave = "document.save";
inline constexpr const char* kOwnableDocumentOpen = "document.open";

/// One action a pane declares: an id in its own namespace -- the spelling a maker's keymap file
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
    /// file moved either row; a retired id stands in for nothing; any other is refused.
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
    std::string refusal;          ///< why, in the maker's words
    ZEN_SHAPE(ActionsWithdrawn, 1, ZEN_FIELD(pane), ZEN_FIELD(declaration), ZEN_FIELD(refusal));
};

/// Workshop -> provider: the maker pressed the gesture one of this pane's rows answers to, as the
/// resolved id after the maker's keymap -- sent instead of `PaneKey`, the keystroke's character
/// swallowed. A keystroke matching no row crosses as `PaneKey` and `PaneTextInput`.
struct PaneActionRequested {
    std::string pane;
    std::string id; ///< one of the ids this pane declared, as it declared it
    ZEN_SHAPE(PaneActionRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(id));
};

/// Provider -> Workshop: where this pane's caret is and what it has selected, in `PanePressed`'s
/// lattice -- a second sentence beside the rows, said only by a pane that has a caret. Judged
/// against the content last accepted and refused whole if it names a row that content lacks,
/// which leaves no caret; `row == kNoCaret` says there is none. It asks for nothing.
struct PaneCaret {
    std::string pane;
    /// The caret's prose row in the granted body; `surface::kNoCaret` (-1) says there is none.
    std::int64_t row = -1;
    std::int64_t column = 0; ///< ...and the prose column it sits BEFORE
    /// The selection, in the same lattice; `surface::kNoSelection` (-1) says there is none.
    std::int64_t sel_begin_row = -1;
    std::int64_t sel_begin_col = 0; ///< inclusive, a caret-like position
    std::int64_t sel_end_row = -1;  ///< reading-order end row
    std::int64_t sel_end_col = 0;   ///< exclusive, a caret-like position
    ZEN_SHAPE(PaneCaret, 1, ZEN_FIELD(pane), ZEN_FIELD(row), ZEN_FIELD(column),
              ZEN_FIELD(sel_begin_row), ZEN_FIELD(sel_begin_col), ZEN_FIELD(sel_end_row),
              ZEN_FIELD(sel_end_col));
};

/// `PaneCaret` naming its generation, for `v2::PaneContent`'s reason and under its rule.
namespace v2 {

struct PaneCaret {
    std::string pane;
    std::int64_t row = -1;
    std::int64_t column = 0;
    std::int64_t sel_begin_row = -1;
    std::int64_t sel_begin_col = 0;
    std::int64_t sel_end_row = -1;
    std::int64_t sel_end_col = 0;
    std::int64_t generation = 0;
    ZEN_SHAPE(PaneCaret, 2, ZEN_FIELD(pane), ZEN_FIELD(row), ZEN_FIELD(column),
              ZEN_FIELD(sel_begin_row), ZEN_FIELD(sel_begin_col), ZEN_FIELD(sel_end_row),
              ZEN_FIELD(sel_end_col), ZEN_FIELD(generation));
};

} // namespace v2

/// Workshop -> provider: the hand moved with the button down after a press that named a row of
/// this pane, in the same lattice, resolved against the body now and deliberately unclamped --
/// what past the edge means is the pane's. A pane in front takes nothing. No release is sent:
/// each drag is a whole sentence, and the record ends on release or when the room is lost.
struct PaneDragged {
    std::string pane;
    std::int64_t row = 0;    ///< a prose row of the granted BODY; may be < 0 or >= rows
    std::int64_t column = 0; ///< a prose column of the same region; may be < 0 or >= columns
    ZEN_SHAPE(PaneDragged, 1, ZEN_FIELD(pane), ZEN_FIELD(row), ZEN_FIELD(column));
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
/// maker's latest gesture. A pane that never says it keeps Escape.
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

// ---- The second button, a picture's number, and a menu a pane asks the host to present ----
//
// Every word a pane says about a gesture echoes that gesture's correlation; a stale, zero or
// spent one moves nothing. A press names the picture the medium had been handed when it was read
// (`v3`), and a pane acts only on its current picture. Not closed: the medium's own latency after
// it handled a canvas, and a press the platform buffered before the input beat read it.

/// Workshop -> provider: button 2 or 3 went down or up in this pane's room, or a hold the host
/// could not keep ended (`lost`). Sent only to a holder that accepts it; the press is consumed by
/// delivery, and the release is the pressing pane's, unclamped. Button 1 stays `PanePressed`.
struct PaneButton {
    std::string pane;
    std::int64_t button = 0;   ///< 2 (middle) or 3 (right)
    bool pressed = false;      ///< down, or up
    std::int64_t row = 0;      ///< the granted lattice (a press); unclamped (a release)
    std::int64_t column = 0;
    bool lost = false;         ///< a release the host sent because no hand could: owner loss, arbitration
    std::int64_t picture = 0;  ///< the picture the medium held at the press; 0 = unnumbered
    ZEN_SHAPE(PaneButton, 1, ZEN_FIELD(pane), ZEN_FIELD(button), ZEN_FIELD(pressed),
              ZEN_FIELD(row), ZEN_FIELD(column), ZEN_FIELD(lost), ZEN_FIELD(picture));
};

/// Provider -> Workshop: that press was not mine -- open your own surface for my pane. Echoes the
/// press's correlation; the host's pane menu opens once, while the press is the latest act.
struct PanePassRequested {
    std::string pane;
    ZEN_SHAPE(PanePassRequested, 1, ZEN_FIELD(pane));
};

/// Provider -> Workshop: give my pane the keys, because a menu choice asked for an edit. Echoes
/// the choice's correlation and is granted only while that choice is the maker's latest act.
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

/// Presentation bounds on one menu; the id bound is a declared action's (`kMaxPaneActionIdLen`),
/// published here so a presenter built from this header alone can judge.
inline constexpr std::size_t kMaxPaneMenuRows = 32;
inline constexpr std::size_t kMaxPaneMenuIdLen = 64;
inline constexpr std::size_t kMaxPaneMenuLabelLen = 64;

/// Provider -> Workshop: present these rows beside this place in my room and say which was
/// chosen. Echoes the gesture's correlation; judged where the menu would open and granted to the
/// presenter (`presenter_vocabulary.hpp`). `subject` is the pane's own word, echoed in the answer.
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

namespace v3 {

/// CONTENT THAT NUMBERS ITS PICTURE. `generation` is v2's (0 for a pane with no subject
/// generation); `picture` is the pane's own number for this row-to-meaning map, kept across a
/// repaint that moves no row and changed by one that does. Admitted under v2's generation rule;
/// the picture is recorded and echoed, never judged, by the host.
struct PaneContent {
    std::string pane;
    std::vector<surface::SurfaceTextRow> rows;
    std::int64_t generation = 0;
    std::int64_t picture = 0;
    ZEN_SHAPE(PaneContent, 3, ZEN_FIELD(pane), ZEN_FIELD(rows), ZEN_FIELD(generation),
              ZEN_FIELD(picture));
};

/// v2's press, naming the picture the medium had been handed for this pane when the press was
/// read (0 when the pane never numbered one). Sent instead of v2/v1 exactly to a holder with this
/// door, `v2::PanePressed`'s rule.
struct PanePressed {
    std::string pane;
    std::int64_t row = 0;
    std::int64_t column = 0;
    bool keys_went_here = false;
    std::int64_t picture = 0;
    ZEN_SHAPE(PanePressed, 3, ZEN_FIELD(pane), ZEN_FIELD(row), ZEN_FIELD(column),
              ZEN_FIELD(keys_went_here), ZEN_FIELD(picture));
};

} // namespace v3

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
