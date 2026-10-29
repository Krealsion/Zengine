// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EDITOR_PANE_TRANSFERS_HPP
#define ZENGINE_EDITOR_PANE_TRANSFERS_HPP

// The Editor pane's pointer and transfers: a press, a sweep and the wheel on its canvas, the
// second button and the menu it opens, the highlight or this file's location carried out, material
// dropped in, and a dropped location opened (WL-EDIT-16..22). These are `EditorPaneWeave`'s
// members, declared in `pane.cpp`, which includes this file after the class; it compiles in no
// other place.
// Workshop law: agents/workshop/editor-transfers.md

// The pane's half of Workshop's value carry (`workshop/pane_carry.hpp`): Workshop carries owned
// bytes and interprets none of them; this pane decides what a drop means for its document
// (`editor/source-transfer/material.hpp`), and asks Workshop to approve every acquisition and every
// open for the gesture that caused it. Nothing here saves, builds, sends or runs.

#include "editor/editor-pane/common.hpp"

#include "editor/editor-pane/editor.hpp"
#include "editor/office_seam.hpp"
#include "editor/source-transfer/cpp.hpp"
#include "editor/source-transfer/material.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/persist.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace {

// ---- The pointer ---------------------------------------------------------------------

/// THE POINTER IN THE WINDOW that shows the pane's area, read onto the rows drawn there: a primary
/// press places the caret or remembers what it landed on, its motion sweeps or carries that, its
/// release or loss ends the sweep, the wheel scrolls, and a right press offers this pane's rows or
/// hands the press back. A press landed on another line than the rows show now is refused in words;
/// on its line in a drawing whose meaning moved since, it is ordinary (WL-EDIT-18).
// WL-PRESS-07 -- agents/workshop/press-chain.md
inline void EditorPaneWeave::on_window(const std::optional<ws::WindowPointer>& p,
                                       loom::Mail& mail) {
    if (!p) {
        return;
    }
    const std::int64_t gesture =
        p->phase == ws::canvas_pointer::kLost ? held_ : static_cast<std::int64_t>(p->press);
    const bool current = !p->moved && (!p->told || p->drawing == picture_);
    pointer(p->phase, p->phase == ws::canvas_pointer::kMove ? 1 : p->button, p->at, gesture,
            p->dy, p->moved, current, mail);
}

/// ONE MOMENT OF THE POINTER at a cell of the rows the pane drew: `moved` when the window drew
/// another line there, `current` when the press names the drawing the pane holds now.
inline void EditorPaneWeave::pointer(std::int64_t phase, std::int64_t button,
                                     const ws::RowCell& at, std::int64_t gesture, double dy,
                                     bool moved, bool current, loom::Mail& mail) {
    if (phase == ws::canvas_pointer::kPress && (button == 1 || button == 3) && moved) {
        held_ = 0;
        notice("nothing was pressed -- the text moved under the press; press again", true);
        say(mail);
        return;
    }
    if (phase == ws::canvas_pointer::kPress && button == 1) {
        held_ = at.shown ? gesture : 0;
        if (at.shown) {
            press_at(at.row, at.column, current, mail);
        } else if (!holding_.active) {
            end_grab();
            drag_ = Drag{};
        }
    } else if (phase == ws::canvas_pointer::kPress && button == 3) {
        right_press(at, current, mail);
    } else if (phase == ws::canvas_pointer::kMove && button == 1 && held_ != 0 &&
               gesture == held_) {
        dragged(at.row, at.column, mail);
    } else if ((phase == ws::canvas_pointer::kRelease || phase == ws::canvas_pointer::kLost) &&
               button == 1 && gesture == held_) {
        held_ = 0;
        drag_ = Drag{};
    } else if (phase == ws::canvas_pointer::kWheel) {
        wheel(dy, mail);
    }
}

/// A press names a row of this pane's room. The rows above the document are a focus statement
/// that moves nothing, and begins no sweep (WL-EDIT-16); a document row places the caret through
/// the tab geometry its row was painted with (WL-EDIT-08), the notice row left alone. On a
/// picture still `current` it may mean one thing more once the hand moves: ON the highlight it
/// remembers the selection it landed on, and on the status row this file's location
/// (WL-EDIT-19).
// WL-EDIT-19 -- agents/workshop/editor-transfers.md
inline void EditorPaneWeave::press_at(std::int64_t prow, std::int64_t pcol, bool current,
                                      loom::Mail& mail) {
    if (held_still()) {
        return;
    }
    grab_ = Grab{};
    drag_ = Drag{};
    if (!e_.open_document()) {
        return;
    }
    if (prow < chrome_rows_) {
        // STILL A FOCUS STATEMENT THAT MOVES NOTHING: the status row names this file, so a
        // press there is remembered in case the hand moves on, and changes no row.
        if (prow == 0 && status_row_) {
            grab_ = Grab{true, false, Take::Location, prow, pcol, mail.correlation(), mark_now(), {}, {}, {}};
        }
        return;
    }
    const std::size_t row = e_.first_row + static_cast<std::size_t>(prow - chrome_rows_);
    const std::size_t target =
        row < e_.buffer.line_count() ? row : e_.buffer.line_count() - 1;
    const std::int64_t column = pcol < 0 ? 0 : pcol;
    const EditorPos at{target, ws::byte_of_visual_col(e_.buffer.line(target), e_.first_col + column,
                                                      e_.buffer.tab_width())};
    if (current && row < e_.buffer.line_count() && on_highlight(at)) {
        grab_ = Grab{true, false, Take::Selection, prow, pcol, mail.correlation(), mark_now(),
                     EditorPos{e_.buffer.anchor_row(), e_.buffer.anchor_byte()},
                     EditorPos{e_.buffer.caret_row(), e_.buffer.caret_byte()}, {}};
    }
    drag_.armed = true;
    drag_.chrome_rows = chrome_rows_;
    drag_.doc_rows = doc_rows_;
    e_.buffer.place(at.row, at.byte);
    e_.follow_caret = true;
    grab_.after = mark_now();
    say(mail);
}

/// The hand moved with the press held: the selection sweep. The row is unclamped on purpose:
/// past the body's top or bottom edge the caret steps one row per motion and the follow flag
/// pulls the viewport after it, enough to sweep a selection out of the window; a negative
/// column steps leftward the same way (`EditorBuffer::drag_to`).
inline void EditorPaneWeave::dragged(std::int64_t drow, std::int64_t dcol, loom::Mail& mail) {
    if (held_still()) {
        return;
    }
    // A REMEMBERED PRESS BECOMES A CARRY ON ITS FIRST MOTION TO ANOTHER CELL, and never a
    // sweep (WL-EDIT-19): the pressed selection is put back exactly as it stood -- nothing but
    // the press's own caret moved since, or nothing is carried -- and a copy of it is taken.
    if (grab_.armed) {
        if (!grab_.started && (drow != grab_.row || dcol != grab_.column)) {
            grab_.started = true;
            if (grab_.what == Take::Location) {
                acquire(Take::Location, true, grab_.gesture, grab_.at, mail);
            } else if (e_.open_document() && grab_.after == mark_now()) {
                e_.buffer.restore_selection(grab_.anchor.row, grab_.anchor.byte, grab_.caret.row,
                                            grab_.caret.byte);
                acquire(Take::Selection, true, grab_.gesture, mark_now(), mail);
            } else {
                notice("nothing was carried -- the document changed after you pressed the highlight", true);
            }
            say(mail);
        }
        return;
    }
    if (!e_.open_document() || !drag_.armed) {
        return; // no gesture to extend: this hand took hold of nothing that selects
    }
    // THE PICTURE THE GESTURE BEGAN AGAINST, not the one composed since: the weaver's
    // hand measured this motion against the rows they could see when they pressed.
    const std::int64_t brow = drow - drag_.chrome_rows;
    std::size_t target;
    if (brow < 0) {
        target = e_.first_row > 0 ? e_.first_row - 1 : 0;
    } else if (brow >= drag_.doc_rows) {
        target = e_.first_row + static_cast<std::size_t>(drag_.doc_rows);
    } else {
        target = e_.first_row + static_cast<std::size_t>(brow);
    }
    e_.buffer.drag_to(target, dcol < 0 ? std::int64_t{-1} : e_.first_col + dcol);
    e_.follow_caret = true;
    say(mail);
}

/// THE WHEEL SCROLLS THE DOCUMENT AND MOVES NO CARET (WL-PTR-10): the notches accumulate
/// until they are worth whole lines, the window slides inside the document, and the
/// follow flag is deliberately NOT set -- the wheel's whole meaning is to look elsewhere
/// while the caret stays put. The next caret gesture brings the view back.
inline void EditorPaneWeave::wheel(double dy, loom::Mail& mail) {
    if (held_still()) {
        return;
    }
    if (!e_.open_document()) {
        return;
    }
    end_grab();
    e_.wheel_accum += dy * static_cast<double>(wheel_lines_);
    const std::int64_t lines = static_cast<std::int64_t>(e_.wheel_accum);
    e_.wheel_accum -= static_cast<double>(lines);
    if (lines == 0) {
        return;
    }
    const std::size_t rows = static_cast<std::size_t>(doc_rows_ > 0 ? doc_rows_ : 0);
    const std::size_t total = e_.buffer.line_count();
    const std::size_t furthest = total > rows ? total - rows : 0;
    std::size_t first = e_.first_row;
    if (lines > 0) {
        const std::size_t up = static_cast<std::size_t>(lines);
        first = first > up ? first - up : 0;
    } else {
        first += static_cast<std::size_t>(-lines);
    }
    if (first > furthest) {
        first = furthest;
    }
    if (first == e_.first_row) {
        return; // already at the edge: nothing moved, nothing to say again
    }
    e_.first_row = first;
    say(mail);
}

// ---- Transfers: the highlight carried out, material dropped in, a location opened ----

/// THE SECOND BUTTON, ON THE ROWS OF AN OPEN DOCUMENT, OPENS THE EDITOR'S MENU (WL-EDIT-22): on the
/// painted highlight of a picture still `current` with Extract first, on the status row with this
/// file's location first (WL-EDIT-19). One beside the rows, in an empty Editor or in one a switch
/// holds still is handed back, so the host's pane menu opens there.
// WL-EDIT-22 -- agents/workshop/editor-transfers.md
inline void EditorPaneWeave::right_press(const ws::RowCell& at, bool current, loom::Mail& mail) {
    // HELD STILL BY A SWITCH, the Editor offers nothing, and a press that changes no document
    // is handed back rather than counted as refused.
    if (holding_.active || !at.shown || !e_.open_document()) {
        (void)ws::pane_menu::pass_back(mail, pane::kEditorPaneRole, pane::kEditorPane);
        return;
    }
    end_grab();
    std::optional<Take> first;
    if (at.row < chrome_rows_) {
        if (at.row == 0 && status_row_) {
            first = Take::Location;
        }
    } else {
        const std::size_t row = e_.first_row + static_cast<std::size_t>(at.row - chrome_rows_);
        if (current && row < e_.buffer.line_count() &&
            on_highlight(EditorPos{
                row, ws::byte_of_visual_col(e_.buffer.line(row),
                                            e_.first_col + (at.column < 0 ? 0 : at.column),
                                            e_.buffer.tab_width())})) {
            first = Take::Selection;
        }
    }
    offer_menu(first, at.row, at.column, mail);
}

/// MATERIAL DROPPED ON THE DOCUMENT (WL-EDIT-17): text is inserted where it landed, or replaces
/// the selection when it landed ON the painted highlight; a location opens; a command is its
/// Terminal line, or -- in a C++ document, by a separate choice -- C++. One undo takes any
/// insertion back; nothing is saved. Refused material leaves the document, its selection and
/// its history exactly as they were. Its place in the area reads back to the row and column of
/// the rows drawn there it fell on.
inline void EditorPaneWeave::on(const ws::PaneCanvasValueDrop& drop, loom::Mail& mail) {
    if (!mail.authored_from_role(kWorkshopRole) || drop.pane != pane::kEditorPane ||
        drop.area != window_.area()) {
        return;
    }
    if (held_still()) {
        return;
    }
    end_grab();
    notice_.clear();
    const bool told = !drop.under.empty() && drop.under == window_.area();
    receive(drop.data, window_.cell_at(drop.x, drop.y),
            !window_.moved(drop.under, drop.part, drop.drawing, drop.x, drop.y),
            !told || drop.drawing == picture_, mail);
    say(mail);
}

inline void EditorPaneWeave::on(const ws::PaneMenuAnswered& answer, loom::Mail& mail) {
    if (drop_.pending && drop_.menu.pending() && answer.subject == kDropSubject) {
        const std::string chosen = drop_.menu.take(mail, answer);
        if (drop_.menu.pending()) {
            return; // not settled: not this ask's answer, or from nobody who may give it
        }
        Dropped d = std::move(drop_);
        drop_ = Dropped{};
        choose_drop(d, chosen);
        say(mail);
        return;
    }
    const std::string chosen = menu_.take(mail, answer);
    if (chosen.empty() || held_still()) {
        return; // nothing chosen, or a switch holds the Editor still: no act
    }
    // EACH ROW DOES WHAT ITS KEY DOES, through the same act (WL-EDIT-22), on the document as it
    // stood when the menu opened: the carries through `acquire`, the buffer's own rows through its
    // `consume`, the declared rows through their act. Another document, an edit or a caret moved
    // since, and the row does nothing, said, as a carry chosen then does.
    end_grab();
    if (chosen == pane::kActionExtract || chosen == pane::kActionLocation) {
        acquire(chosen == pane::kActionExtract ? Take::Selection : Take::Location, false,
                mail.correlation(), menu_mark_, mail);
    } else if (!(menu_mark_ == mark_now())) {
        notice("nothing was done -- the document changed after you pointed at it", true);
    } else {
        bool buffer = false;
        for (const BufferRow& row : kMenuBufferRows) {
            if (chosen == row.id) {
                buffer = true;
                (void)edit_by_key(row.scancode, row.modifiers, mail);
            }
        }
        if (!buffer) {
            act(chosen);
        }
    }
    say(mail);
}

/// WORKSHOP'S WORD ON WHETHER THIS GESTURE'S ACTOR MAY DO WHAT THIS PANE ASKED: carry a copy
/// (a pickup), or open a dropped location (`office::answer_operation`). An approved open is
/// settled against whether it names this document, and this document's mark now.
inline void EditorPaneWeave::on(const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
    const auto stamp = [this](Locate& l) {
        l.same_path = e_.open_document() && e_.path == l.loc.path;
        l.snapshot = mark_now();
    };
    if (office::answer_operation(answer, mail, pane::kEditorPaneRole, pane::kEditorPane, asked_,
                                 pickup_, locate_, stamp, noticing())) {
        say(mail);
    }
}

/// WORKSHOP TOOK THE COPY, OR SAID WHY NOT. The document was never touched either way.
inline void EditorPaneWeave::on(const ws::PaneCarryAnswered& answer, loom::Mail& mail) {
    if (office::settle_carry(answer, mail, pickup_, "this document is unchanged", noticing())) {
        say(mail);
    }
}

// ---- Transfer helpers ------------------------------------------------------------------

/// THE DOCUMENT AT THIS INSTANT: its generation, its bytes' revision, and the revision that
/// moves with every caret or selection change -- so a mark taken at a press says whether
/// anything at all moved since, and undo back to identical text still reads as moved.
inline Mark EditorPaneWeave::mark_now() const {
    return Mark{e_.doc_epoch, e_.buffer.content_revision(), e_.buffer.revision()};
}

/// IS THIS POSITION ON THE HIGHLIGHT? The selection's characters, and the break of every line
/// the selection runs past -- exactly the cells the pane paints as selected.
inline bool EditorPaneWeave::on_highlight(EditorPos at) const {
    if (!e_.buffer.has_selection()) {
        return false;
    }
    const EditorPos from = e_.buffer.selection_begin();
    const EditorPos to = e_.buffer.selection_end();
    return !(at < from) && at < to;
}

/// A REMEMBERED PRESS ENDS at the next act that is not its own motion: it was only a press.
inline void EditorPaneWeave::end_grab() { grab_ = Grab{}; }

/// THE EDITOR'S MENU, AS THE DOCUMENT STANDS (WL-EDIT-22): the press's own subject first, then
/// undo and redo while a step is there, cut and copy with a selection, paste, select all, the
/// carries, save, discard while there is unsaved work, and close -- each row beside the key that
/// does it now: a declared row's as the keymap holds it, the buffer's own as they are.
// WL-EDIT-22 -- agents/workshop/editor-transfers.md
inline void EditorPaneWeave::offer_menu(std::optional<Take> first, std::int64_t row,
                                        std::int64_t column, loom::Mail& mail) {
    menu_mark_ = mark_now();
    const bool selection = e_.buffer.has_selection();
    ws::pane_menu::Offer menu(pane::kEditorPane, first == Take::Selection  ? "selection"
                                                 : first == Take::Location ? "location"
                                                                           : "document");
    menu.at(row, column);
    const auto declared = [&](const char* id, const char* label) {
        menu.row(id, label, declared_key(id));
    };
    const auto buffer = [&](const BufferRow& r) { menu.row(r.id, r.label, buffer_key(r)); };
    constexpr const char* kExtract = "Extract selection to Inventory";
    constexpr const char* kLocation = "Carry this file's location";
    if (first == Take::Selection) {
        declared(pane::kActionExtract, kExtract);
    } else if (first == Take::Location) {
        declared(pane::kActionLocation, kLocation);
    }
    if (e_.buffer.can_undo()) {
        buffer(kMenuUndo);
    }
    if (e_.buffer.can_redo()) {
        buffer(kMenuRedo);
    }
    if (selection) {
        buffer(kMenuCut);
        buffer(kMenuCopy);
    }
    buffer(kMenuPaste);
    buffer(kMenuSelectAll);
    if (selection && first != Take::Selection) {
        declared(pane::kActionExtract, kExtract);
    }
    if (first != Take::Location) {
        declared(pane::kActionLocation, kLocation);
    }
    declared(pane::kActionSave, "Save source");
    if (e_.dirty()) {
        declared(pane::kActionDiscard, "Discard source edits");
    }
    declared(pane::kActionClose, "Close file");
    menu_ = menu.send(mail, pane::kEditorPaneRole);
}

inline std::string EditorPaneWeave::where_words(EditorPos p) const {
    return "L" + std::to_string(p.row + 1) + ":C" +
           std::to_string(ws::visual_col_of(e_.buffer.line(p.row), p.byte, e_.buffer.tab_width()) + 1);
}

inline std::string EditorPaneWeave::project_root() const {
    return project_known_ ? ws::persist::resolved_against(std::string(), project_dir_) : std::string();
}

/// ONE ACQUISITION, whichever hand asked -- the highlight's drag, the right-click menu, or the
/// key (WL-EDIT-19): an owned copy of what the mark still names, or a refusal in words. The
/// copy is the SELECTION'S OWN TEXT, unsaved edits included, never the file on disk and never
/// an implicit line or word; or this file's location. Then Workshop is asked to approve the
/// carry for the gesture that caused it; the carry itself follows the approval.
// WL-EDIT-19 -- agents/workshop/editor-transfers.md
inline void EditorPaneWeave::acquire(Take what, bool drag, std::uint64_t gesture, const Mark& at,
                                     loom::Mail& mail) {
    const bool selection = what == Take::Selection;
    if (!e_.open_document()) {
        notice("nothing was carried -- no source is open", true);
        return;
    }
    if (!(at == mark_now())) {
        notice(selection ? "nothing was carried -- the selection changed after you pointed at it"
                         : "nothing was carried -- the document changed after you pointed at it",
               true);
        return;
    }
    if (pickup_.stage != Pickup::Stage::Idle) {
        notice("nothing was carried -- a copy is already on its way to Workshop", true);
        return;
    }
    st::Pair pair;
    std::string label;
    std::string what_words;
    if (selection) {
        if (!e_.buffer.has_selection()) {
            notice("nothing was carried -- select text first; nothing else is carried in its place", true);
            return;
        }
        const EditorPos from = e_.buffer.selection_begin();
        const EditorPos to = e_.buffer.selection_end();
        st::SourceSelection s;
        s.editor = "the standard Editor";
        s.path = e_.path;
        s.project_root = project_root();
        s.kind = st::kCharacters;
        s.first_line = static_cast<std::int64_t>(from.row) + 1;
        s.first_column = static_cast<std::int64_t>(from.byte) + 1;
        s.end_line = static_cast<std::int64_t>(to.row) + 1;
        s.end_column = static_cast<std::int64_t>(to.byte) + 1;
        s.line_ending = e_.convention == ws::line_ending::kCRLF ? "CRLF" : "LF";
        s.unsaved = e_.dirty();
        s.captured_at_epoch_s = st::clock_now();
        const std::string text = e_.buffer.selected_text();
        pair = st::text_pair(text, s);
        const st::Lines lines = st::standard_lines(text);
        what_words = (lines.ok ? st::amount_words(lines.lines) : std::string("the selection")) +
                     " of " + file_name(e_.path);
        label = file_name(e_.path) + " " + where_words(from) + "-" + where_words(to);
    } else {
        const std::size_t row = e_.buffer.caret_row();
        st::SourceLocation loc{e_.path, static_cast<std::int64_t>(row) + 1,
                               static_cast<std::int64_t>(e_.buffer.caret_byte()) + 1};
        st::SourceLocationContext c;
        c.editor = "the standard Editor";
        c.project_root = project_root();
        c.relative = st::relative_to(e_.path, c.project_root);
        c.line_text = st::observe_line(e_.buffer.line(row));
        c.unsaved = e_.dirty();
        c.captured_at_epoch_s = st::clock_now();
        pair = st::location_pair(loc, c);
        what_words = "the location of " + file_name(e_.path) + " at line " + std::to_string(row + 1);
        label = file_name(e_.path) + ":" + std::to_string(row + 1) + " (location)";
    }
    office::begin_pickup(pair, drag, gesture, label, std::move(what_words), mail, pane::kEditorPaneRole,
                         pane::kEditorPane, asked_, pickup_, noticing());
}

/// WHY A DROP CANNOT INSERT INTO THIS DOCUMENT NOW, or empty.
inline std::string EditorPaneWeave::insertion_refusal() const {
    if (!e_.open_document()) {
        return "open a source first: a drop inserts into the open document";
    }
    if (candidate_.live) {
        return "the Editor is opening " + candidate_.path + "; drop again once it has settled";
    }
    if (paste_.awaiting) {
        return "a paste is still arriving; drop again once it has";
    }
    if (drop_.pending) {
        return "a dropped command is still waiting for your choice";
    }
    return std::string();
}

/// WHERE A DROP LANDED, read through the picture it was aimed at (the caller checked the
/// number): the character under it, or the end of the text below the last line -- and whether
/// that character is on the highlight.
inline Landing EditorPaneWeave::landing(std::int64_t prow, std::int64_t pcol) const {
    Landing l;
    const std::size_t row = e_.first_row + static_cast<std::size_t>(prow - chrome_rows_);
    if (row >= e_.buffer.line_count()) {
        const std::size_t last = e_.buffer.line_count() - 1;
        l.pos = EditorPos{last, e_.buffer.line(last).size()};
        return l;
    }
    l.pos = EditorPos{row, ws::byte_of_visual_col(e_.buffer.line(row), e_.first_col + (pcol < 0 ? 0 : pcol),
                                                  e_.buffer.tab_width())};
    l.inside = on_highlight(l.pos);
    return l;
}

inline void EditorPaneWeave::receive(const loom::Bytes& data, const ws::RowCell& drop,
                                     bool current, bool same, loom::Mail& mail) {
    const std::string bytes(data.begin(), data.end());
    st::Material m = st::read_material(bytes);
    if (m.kind == st::MaterialKind::Location) {
        open_location(m, mail);
        return;
    }
    if (m.kind == st::MaterialKind::Unsupported) {
        notice("nothing was inserted -- " + m.refusal, true);
        return;
    }
    const std::string busy = insertion_refusal();
    if (!busy.empty()) {
        notice("nothing was inserted -- " + busy, true);
        return;
    }
    if (!current) {
        notice("nothing was inserted -- the text moved under the drop; drop it again", true);
        return;
    }
    if (!drop.shown) {
        notice("nothing was inserted -- drop onto the document's text", true);
        return;
    }
    if (drop.row < chrome_rows_) {
        notice("nothing was inserted -- drop onto the document's text, not its status row", true);
        return;
    }
    const Landing at = landing(drop.row, drop.column);
    if (at.inside && !same) { // a highlight the drawing under the hand did not show there
        notice("nothing was inserted -- the selection changed under the drop; drop it again", true);
        return;
    }
    if (m.kind == st::MaterialKind::Text) {
        (void)insert_lines(m.text, at, "", false);
        return;
    }
    const st::CppDocument cpp = st::cpp_document(e_.path);
    if (cpp == st::CppDocument::No) {
        insert_command(m, at);
        return;
    }
    // IN A C++ DOCUMENT THE WEAVER CHOOSES, AND THE DROP ITSELF CHOOSES NOTHING (WL-EDIT-20): a
    // menu at the drop, continuing its gesture; nothing is inserted until a row is chosen.
    drop_.pending = true;
    drop_.material = std::move(m);
    drop_.at = at;
    drop_.mark = mark_now();
    drop_.menu = ws::pane_menu::Offer(pane::kEditorPane, kDropSubject)
                     .at(drop.row, drop.column)
                     .row(kInsertLine, "Insert its Terminal line")
                     .row(kInsertCpp, cpp == st::CppDocument::Yes ? "Generate C++ that builds it"
                                                                  : "Generate C++ (this .h is C++)")
                     .send(mail, pane::kEditorPaneRole);
    if (!drop_.menu.pending()) {
        drop_ = Dropped{};
        notice("nothing was inserted -- the choice for the dropped command could not be offered", true);
        return;
    }
    notice("choose how the dropped " + drop_.material.what + " goes in -- nothing is inserted until you do", false);
}

/// TEXT INTO THE DOCUMENT AS ONE UNDOABLE EDIT, or a refusal that leaves it untouched. `select`
/// leaves the insertion selected (generated code, for review).
inline bool EditorPaneWeave::insert_lines(const std::string& text, const Landing& at,
                                          const std::string& note, bool select) {
    const st::Lines lines = st::standard_lines(text);
    if (!lines.ok) {
        notice("nothing was inserted -- " + lines.refusal, true);
        return false;
    }
    std::size_t size = text.size();
    for (const std::string& l : e_.buffer.lines()) {
        size += l.size() + 2;
    }
    if (size > ws::kMaxSourceBytes) {
        notice("nothing was inserted -- the document would outgrow what the standard Editor holds", true);
        return false;
    }
    const bool replacing = at.inside && e_.buffer.has_selection();
    const EditorPos start = replacing ? e_.buffer.selection_begin() : at.pos;
    e_.buffer.insert_at(at.pos, lines.lines, at.inside);
    if (select) {
        e_.buffer.restore_selection(start.row, start.byte, e_.buffer.caret_row(), e_.buffer.caret_byte());
    }
    e_.follow_caret = true;
    const std::string what = note.empty() ? st::amount_words(lines.lines) : note;
    notice((replacing ? "replaced the highlighted selection with " : "inserted ") + what + " at " +
               where_words(start) + " -- ctrl+z takes it back; nothing was saved",
           false);
    return true;
}

inline void EditorPaneWeave::insert_command(const st::Material& m, const Landing& at) {
    const st::TerminalLine t = st::terminal_line(*m.command, m.address);
    if (!t.ok) {
        notice("nothing was inserted -- " + m.what + ": " + t.refusal, true);
        return;
    }
    if (!insert_lines(t.line, at, "the Terminal line for " + m.what, false)) {
        return; // refused: its sentence stands
    }
    notice(notice_ + office::terminal_line_after(t, m), !t.missing.empty());
}

inline void EditorPaneWeave::choose_drop(const Dropped& d, const std::string& chosen) {
    if (chosen.empty()) {
        notice("the dropped " + d.material.what + " was not inserted", false);
        return;
    }
    if (!(d.mark == mark_now())) {
        notice("nothing was inserted -- the document changed after the drop; drop it again", true);
        return;
    }
    if (chosen == kInsertLine) {
        insert_command(d.material, d.at);
    } else if (chosen == kInsertCpp) {
        insert_cpp(d.material, d.at);
    }
}

/// C++ THAT BUILDS THE DROPPED COMMAND (WL-EDIT-20), inserted as ONE undoable edit and left
/// selected: the insertion is the preview. Its includes are named, never written in.
// WL-EDIT-20 -- agents/workshop/editor-transfers.md
inline void EditorPaneWeave::insert_cpp(const st::Material& m, const Landing& at) {
    const st::GeneratedCpp g = st::cpp_value_function(*m.command, e_.buffer.lines());
    if (!g.ok) {
        notice("no C++ was generated -- " + g.refusal, true);
        return;
    }
    // GENERATED CODE IS WHOLE LINES: it goes in before the line the drop landed on and ends in
    // a line break, so it never joins the text on either side of it.
    Landing whole = at;
    whole.pos.byte = 0;
    whole.inside = false;
    if (!insert_lines(st::join_lf(g.lines) + "\n", whole, "C++ for " + m.what, true)) {
        return; // refused: its sentence stands
    }
    notice(office::generated_cpp_words(g, m.what, ", selected for review") +
               " -- ctrl+z removes it; nothing was sent, saved or built",
           !g.holes.empty());
}

/// A DROPPED LOCATION: ask Workshop whether this gesture's actor may open a file, then ask the
/// opening office as this Editor's office (WL-EDIT-21). A locator is never inserted as text.
// WL-EDIT-21 -- agents/workshop/editor-transfers.md
inline void EditorPaneWeave::open_location(const st::Material& m, loom::Mail& mail) {
    const std::string refused = office::location_refusal(m.location, locate_);
    if (!refused.empty()) {
        notice(refused, true);
        return;
    }
    office::ask_location(m, mail, pane::kEditorPaneRole, pane::kEditorPane, asked_, locate_, noticing());
}

/// THE OPEN CAME TO SOMETHING. A refusal is said with the location's own context; a success
/// places the caret only where the location says, in that same document, with nothing moved
/// since it was shown, and on a line that still reads as it did when saved.
inline void EditorPaneWeave::settle_location(const SourceOpened& said) {
    Locate l = std::move(locate_);
    locate_ = Locate{};
    const office::Roots roots = office::roots_of(l.ctx, project_root());
    if (!said.accepted) {
        notice(office::open_refused_words(l.loc.path, said.refusal, roots), true);
        return;
    }
    if (!e_.open_document() || e_.path != l.loc.path) {
        notice("the location's file was shown, but the Editor holds another document now -- the caret was not moved", false);
        return;
    }
    const std::string opened = office::opened_words(shown_path(), roots);
    if (l.loc.line <= 0) {
        notice(opened, false);
        return;
    }
    const bool untouched = l.same_path
                               ? e_.doc_epoch == l.snapshot.epoch && e_.buffer.revision() == l.snapshot.revision
                               : e_.doc_epoch == installed_epoch_ && e_.buffer.revision() == installed_revision_;
    if (!untouched) {
        notice(opened + " -- you moved in it before the location arrived, so the caret stays where you put it", false);
        return;
    }
    const std::size_t row = static_cast<std::size_t>(l.loc.line - 1);
    if (row >= e_.buffer.line_count()) {
        notice(opened + " -- it has no line " + std::to_string(l.loc.line) + " now, so the caret was not moved", false);
        return;
    }
    if (l.ctx && !st::still_reads(e_.buffer.line(row), l.ctx->line_text)) {
        notice(opened + " -- line " + std::to_string(l.loc.line) +
                   " no longer reads as it did when the location was saved, so the caret was not moved",
               false);
        return;
    }
    e_.buffer.place(row, l.loc.column > 0 ? static_cast<std::size_t>(l.loc.column - 1) : 0);
    e_.follow_caret = true;
    notice(opened + " at line " + std::to_string(l.loc.line), false);
}

} // namespace

#endif // ZENGINE_EDITOR_PANE_TRANSFERS_HPP
