// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESKTOP_PANE_MANUAL_VIEW_HPP
#define ZENGINE_DESKTOP_PANE_MANUAL_VIEW_HPP

// The Manual: the manual of the pane whose `?` a weaver pressed, asked of that pane's office under
// a number of this image's own and shown beside the keys in force, in a text area: wrapped to the
// pane's width, scrolled, its text selected and copied.
// It keeps the words only while its pane is on the desk; the weave routes its messages here.
// Workshop law: agents/workshop/desktop-presenting.md

#include "workshop/desktop-pane/desktop_common.hpp"
#include "workshop/desktop-pane/manual_reader.hpp"
#include "workshop/desktop-pane/vocabulary.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "component/row_map.hpp"
#include "component/text_area.hpp"
#include "graphics/elements/text_face.hpp"
#include "graphics/role.hpp"
#include "input/vocabulary.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/weave.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::desktop_pane {

class ManualView {
public:
    /// THE MANUAL'S ROWS: the lines up and down, its two ends, the copy, everything selected, and
    /// Escape, which closes it.
    static std::vector<ws::v3::PaneActionRow> manual_rows() {
        using ws::v3::PaneActionRow;
        return {PaneActionRow{kActionManualUp, "line up", input::scan::kUp, input::mod::kNone},
                PaneActionRow{kActionManualDown, "line down", input::scan::kDown,
                              input::mod::kNone},
                PaneActionRow{kActionManualTop, "top", input::scan::kHome, input::mod::kNone},
                PaneActionRow{kActionManualBottom, "bottom", input::scan::kEnd, input::mod::kNone},
                PaneActionRow{kActionManualCopy, "copy", input::scan::kC, input::mod::kCtrl},
                PaneActionRow{kActionManualAll, "select all", input::scan::kA, input::mod::kCtrl},
                PaneActionRow{kActionManualClose, "close", input::scan::kEscape, input::mod::kNone,
                              {},
                              {ws::cause_of<ws::PaneCloseRequested>(
                                  ws::to_pane(kDesktopRole, kManualPane))}}};
    }

    // ---- Its rooms ----------------------------------------------------------------------------

    /// THE PROSE ROOM WORKSHOP GRANTED IT, kept for a host that grants no canvas.


    /// ...AND ITS OWN CANVAS, whose lattice is the room it composes for while it holds one.


    const PaneCanvas& canvas() const { return manual_canvas_; }
    /// ...for the office to hand the room a graphics window gives the pane.
    PaneCanvas& canvas() { return manual_canvas_; }


    // ---- Asking for a pane's words ------------------------------------------------------------

    /// A PANE'S `?`, or the manual key while a pane holds the keys: the Manual is shown (or
    /// focused) and that pane is asked for its whole manual, under a number of this image's own,
    /// so only its answer is shown. The keys come from the keymap the host last said.
    void show_manual(const std::string& office, const std::string& pane_key, bool keys_heard,
                     const ws::KeymapShown& keymap, loom::Mail& mail, std::uint64_t& launches) {
        const bool itself = office == kDesktopRole && pane_key == kManualPane;
        if (!itself) {
            manual_ = ManualShown{};
            manual_.office = office;
            manual_.pane = pane_key;
            if (office.empty()) {
                manual_.word = "no pane was named: press the ? at the top right of a pane, or "
                               "the manual key while a pane holds your keys";
            } else {
                manual_.word = "asking " + office + " for its manual";
                const std::uint64_t asked = ++manual_asks_;
                const loom::Ticket sent =
                    mail.as_role(kDesktopRole)
                        .send_to_role(office, ws::PaneDocumentRequested{pane_key, std::string()},
                                      asked);
                if (sent.valid()) {
                    manual_.asked = asked;
                    manual_.sent = sent;
                } else {
                    manual_.word = "the ask for the manual of " + office + " was not sent";
                }
            }
        }
        launch(mail, kDesktopRole, kManualPane, launches);
        say_manual(keys_heard, keymap, mail);
    }

    /// THE MANUAL A PANE ANSWERED, to this image's latest ask and no older one: a weaver who
    /// pressed another `?` since is shown that pane's, and this answer is about one no longer
    /// asked about.
    void document_shown(const ws::PaneDocumentShown& shown, bool keys_heard,
                        const ws::KeymapShown& keymap, loom::Mail& mail) {
        if (!mail.answers_ask() || manual_.asked == 0 || mail.correlation() != manual_.asked) {
            return;
        }
        manual_.asked = 0;
        manual_.text.clear();
        if (!shown.refusal.empty()) {
            manual_.word = shown.refusal;
            manual_.words.reset();
        } else {
            manual_.word.clear();
            manual_.words = shown;
        }
        say_manual(keys_heard, keymap, mail);
    }

    /// ...AND A MANUAL ASKED OF AN OFFICE THAT COULD NOT BE REACHED: said, not waited on. True
    /// when the refusal named that ask.
    bool ask_refused(const loom::DispatchRefused& refused, bool keys_heard,
                     const ws::KeymapShown& keymap, loom::Mail& mail) {
        if (!refused_ask(manual_.asked != 0, manual_.sent, manual_.asked, refused, mail,
                         ws::PaneDocumentRequested::zen_name,
                         ws::PaneDocumentRequested::zen_version, manual_.office.c_str())) {
            return false;
        }
        manual_.asked = 0;
        manual_.word =
            "the manual of " + manual_.office + " could not be asked (" + refused.reason + ")";
        say_manual(keys_heard, keymap, mail);
        return true;
    }

    /// ...AND ITS WORDS GONE, when its pane leaves the desk.
    void forget_manual() { manual_ = ManualShown{}; }

    /// THE INVENTORY AS THE HOST SAID IT: nothing keeps a pane's manual past the `?` that showed
    /// it, so the words go once the Manual's own pane is not open on the desk.
    void inventory_said(const std::vector<ws::InventoryPane>& panes) {
        bool open = false;
        for (const ws::InventoryPane& p : panes) {
            open = open || (p.office == kDesktopRole && p.pane == kManualPane && p.open);
        }
        if (!open) {
            forget_manual();
        }
    }

    // ---- The keys and the wheel ---------------------------------------------------------------

    void manual_action(const std::string& id, bool keys_heard, const ws::KeymapShown& keymap,
                       loom::Mail& mail, std::uint64_t& launches) {
        if (id == kActionManualUp) {
            manual_scroll(-1);
        } else if (id == kActionManualDown) {
            manual_scroll(+1);
        } else if (id == kActionManualTop) {
            manual_.text.scroll_home();
        } else if (id == kActionManualBottom) {
            manual_scroll(kFurthest);
        } else if (id == kActionManualCopy) {
            if (manual_.text.has_selection()) {
                mail.publish(graphics::ClipboardCopy{manual_.text.selected_text()});
            }
            return;
        } else if (id == kActionManualAll) {
            manual_.text.select_all();
        } else if (id == kActionManualClose) {
            close(mail, kDesktopRole, kManualPane, launches);
            return;
        } else {
            return;
        }
        say_manual(keys_heard, keymap, mail);
    }

    /// THE WHEEL SCROLLS THE TEXT a line a notch: freely in a window that sets type, a notch's
    /// part a line's part; in a room of cells a whole line, fractions carried (Powers' convention).
    void wheel(double dy, bool keys_heard, const ws::KeymapShown& keymap, loom::Mail& mail) {
        if (!manual_canvas_.room().cells) {
            const auto units = static_cast<std::int64_t>(
                std::llround(-dy * static_cast<double>(component::TextArea::kUnitsPerLine)));
            if (units != 0) {
                const graphics::TextAreaGeometry g = geometry();
                manual_.text.scroll_by(units, g.columns, g.shown);
                say_manual(keys_heard, keymap, mail);
            }
            return;
        }
        const std::int64_t notches = take_notches(manual_.wheel, dy);
        if (notches != 0) {
            manual_scroll(-notches);
            say_manual(keys_heard, keymap, mail);
        }
    }

    /// A HAND ON THE MANUAL: a primary press on its text puts the caret there and starts a
    /// selection, Shift extending one; a move while held extends it; a press or a drag on the bar
    /// moves the text there; the release ends either. True when the moment was the text's or the
    /// bar's.
    bool pointer(const ws::WindowPointer& p, bool keys_heard, const ws::KeymapShown& keymap,
                 loom::Mail& mail) {
        if (p.phase == ws::canvas_pointer::kRelease || p.phase == ws::canvas_pointer::kLost) {
            const bool was = manual_.held || manual_.barred;
            manual_.held = false;
            manual_.barred = false;
            return was;
        }
        const bool press = p.phase == ws::canvas_pointer::kPress && p.button == 1;
        const bool drag = p.phase == ws::canvas_pointer::kMove && p.press != 0 &&
                          (manual_.held || manual_.barred);
        if (!press && !drag) {
            return false;
        }
        const bool fresh = press && (p.modifiers & input::mod::kShift) == 0;
        const ws::RowsRoom room = manual_canvas_.room();
        if (!room.cells) {
            const graphics::TextAreaGeometry g = geometry();
            const std::int64_t y = p.y - room.text.line; // the heading stands above the text
            if ((press && graphics::text_area_on_bar(g, p.x, y)) || (drag && manual_.barred)) {
                manual_.barred = true;
                manual_.text.scroll_to(graphics::text_area_bar_scroll(g, y), g.lines, g.shown);
                say_manual(keys_heard, keymap, mail);
                return true;
            }
            if (press && y < 0) {
                return false;
            }
            if (const auto at = graphics::text_area_place_at(manual_.text, g, p.x, y)) {
                select_to(*at, fresh);
                say_manual(keys_heard, keymap, mail);
                return true;
            }
            return false;
        }
        const component::TextArea::Cells cells =
            manual_.text.cells(body_rows(), manual_canvas_.columns());
        if (const auto at = manual_.text.place_at(cells, p.at.row - 1, p.at.column)) {
            select_to(*at, fresh);
            say_manual(keys_heard, keymap, mail);
            return true;
        }
        return false;
    }

    // ---- What it shows ------------------------------------------------------------------------

    /// THE MANUAL AS ITS PANE SHOWS IT: which pane it is about, then its text: in a room of cells
    /// its lines from the scroll's, a marker for the lines above and below, and its selection; in
    /// a window that sets type the area's face over those rows (`RowsTextArea`).
    void say_manual(bool keys_heard, const ws::KeymapShown& keymap, loom::Mail& mail) {
        const std::int64_t room_rows = manual_canvas_.rows();
        const std::int64_t room_columns = manual_canvas_.columns();
        if (!manual_canvas_.granted() || room_rows <= 0 || room_columns <= 0) {
            return;
        }
        std::vector<ws::TextRow> out;
        const auto push = [&out, room_rows, room_columns](std::string text, std::int64_t role) {
            if (static_cast<std::int64_t>(out.size()) < room_rows) {
                out.push_back(
                    ws::TextRow{fit(std::move(text), room_columns), role});
            }
        };
        if (manual_.office.empty() && manual_.word.empty()) {
            push("MANUAL -- press the ? at the top right of a pane to read its manual here",
                 graphics::role::kMuted);
            publish_manual(mail, std::move(out));
            return;
        }
        push("MANUAL -- " + manual_.office + (manual_.pane.empty() ? "" : "/" + manual_.pane),
             graphics::role::kAccent);
        if (!manual_.words || !keys_heard) {
            push(manual_.words ? std::string("waiting for the keys in force") : manual_.word,
                 manual_.asked != 0 || manual_.words ? graphics::role::kMuted
                                                     : graphics::role::kAlert);
            publish_manual(mail, std::move(out));
            return;
        }
        manual_reader::ManualText text = manual_reader::manual_text(*manual_.words, keymap);
        manual_.text.set(std::move(text.text), std::move(text.looks));
        const std::int64_t body = body_rows();
        const component::TextArea::Cells cells = manual_.text.cells(body, room_columns);
        const auto first = static_cast<std::int64_t>(out.size());
        ws::RowsCaret caret;
        for (std::size_t i = 0; i < cells.rows.size(); ++i) {
            const component::TextArea::CellRow& row = cells.rows[i];
            const std::int64_t at = first + static_cast<std::int64_t>(i);
            if (row.selection_end > row.selection_begin) {
                if (caret.sel_begin_row == ws::kNoSelection) {
                    caret.sel_begin_row = at;
                    caret.sel_begin_col = row.selection_begin;
                }
                caret.sel_end_row = at;
                caret.sel_end_col = row.selection_end;
            }
            push(row.text, row.marker ? graphics::role::kMuted : row.look.role);
        }
        std::vector<ws::RowsTextArea> areas;
        areas.push_back(
            ws::RowsTextArea{first, body, manual_.text, kManualTextPart, kManualBarPart});
        publish_manual(mail, std::move(out), caret, std::move(areas));
    }

private:
    /// As far as the text goes: the bottom's scroll, held inside the text by the area.
    static constexpr std::int64_t kFurthest = std::int64_t{1} << 30;

    /// THE ROWS THE TEXT STANDS ON: the room less the heading.
    std::int64_t body_rows() const { return std::max<std::int64_t>(manual_canvas_.rows() - 1, 1); }

    /// Where the text stands in a window that sets type: under the heading, the room's width.
    graphics::TextAreaGeometry geometry() const {
        const ws::RowsRoom room = manual_canvas_.room();
        return graphics::text_area_geometry(manual_.text, room.width, body_rows() * room.text.line,
                                            room.text);
    }

    /// THE TEXT MOVES BY `by` LINES, held inside it: a line of the cells, or a line's height in a
    /// window.
    void manual_scroll(std::int64_t by) {
        if (manual_canvas_.room().cells) {
            manual_.text.scroll_lines(by, manual_canvas_.columns(), body_rows());
            return;
        }
        const graphics::TextAreaGeometry g = geometry();
        const std::int64_t units =
            std::clamp<std::int64_t>(by, -kFurthest, kFurthest) * component::TextArea::kUnitsPerLine;
        manual_.text.scroll_by(units, g.columns, g.shown);
    }

    /// A press or a drag reaching `at`: a fresh press puts the caret there, anything else extends.
    void select_to(std::size_t at, bool fresh) {
        if (fresh) {
            manual_.text.press(at);
            manual_.held = true;
        } else {
            manual_.text.drag_to(at);
        }
    }

    /// The Manual's rows, drawn in its area with its selection and its text area. A press on a row
    /// means nothing; the text and the bar hear the hand (`pointer`).
    void publish_manual(loom::Mail& mail, std::vector<ws::TextRow> rows,
                        const ws::RowsCaret& caret = {}, std::vector<ws::RowsTextArea> areas = {}) {
        manual_map_.begin();
        (void)manual_map_.settle();
        manual_canvas_.publish_rows(mail, std::move(rows), {}, caret, 0, std::move(areas));
    }

    // ---- State not in the shape ----------------------------------------------------------

    /// THE MANUAL: the pane it is about, the ask it waits on, the words that pane answered and
    /// where its window stands -- all of it gone when its pane leaves the desk, and none of it in
    /// `DesktopState`, so a reloaded image shows no words it did not ask for.
    struct ManualShown {
        std::string office, pane;
        std::uint64_t asked = 0;  ///< the number of the ask not yet answered; 0 when none
        loom::Ticket sent{};      ///< ...and its queued attempt, which a refusal names
        std::optional<ws::PaneDocumentShown> words;
        std::string word;         ///< what it says where it has no words: asking, or a refusal
        component::TextArea text; ///< the words as the area holds them: scroll and selection
        bool held = false;        ///< a press on the text is held: a move extends the selection
        bool barred = false;      ///< ...or one on the bar: a move scrolls
        double wheel = 0.0;
    };
    ManualShown manual_;
    std::uint64_t manual_asks_ = 0;
    PaneCanvas manual_canvas_{kManualPane, kDesktopRole};
    component::RowMap<std::int64_t> manual_map_;
};

} // namespace zengine::desktop_pane

#endif // ZENGINE_DESKTOP_PANE_MANUAL_VIEW_HPP
