// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESKTOP_PANE_MANUAL_VIEW_HPP
#define ZENGINE_DESKTOP_PANE_MANUAL_VIEW_HPP

// The Manual: the manual of the pane whose `?` a weaver pressed, asked of that pane's office under
// a number of this image's own and shown beside the keys in force, through a window of its lines.
// It keeps the words only while its pane is on the desk; the weave routes its messages here.
// Workshop law: agents/workshop/desktop-presenting.md

#include "workshop/desktop-pane/desktop_common.hpp"
#include "workshop/desktop-pane/manual_reader.hpp"
#include "workshop/desktop-pane/vocabulary.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "component/row_map.hpp"
#include "graphics/role.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/weave.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::desktop_pane {

class ManualView {
public:
    /// THE MANUAL'S ROWS: the lines up and down, its two ends, and Escape, which closes it.
    static std::vector<ws::v3::PaneActionRow> manual_rows() {
        using ws::v3::PaneActionRow;
        return {PaneActionRow{kActionManualUp, "line up", input::scan::kUp, input::mod::kNone},
                PaneActionRow{kActionManualDown, "line down", input::scan::kDown,
                              input::mod::kNone},
                PaneActionRow{kActionManualTop, "top", input::scan::kHome, input::mod::kNone},
                PaneActionRow{kActionManualBottom, "bottom", input::scan::kEnd, input::mod::kNone},
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
        manual_.first = 0;
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
            manual_.first = 0;
        } else if (id == kActionManualBottom) {
            manual_scroll(static_cast<std::int64_t>(manual_.lines));
        } else if (id == kActionManualClose) {
            close(mail, kDesktopRole, kManualPane, launches);
            return;
        } else {
            return;
        }
        say_manual(keys_heard, keymap, mail);
    }

    /// THE WHEEL MOVES THE WINDOW, one line per notch, fractions carried (Powers' convention).
    void wheel(double dy, bool keys_heard, const ws::KeymapShown& keymap, loom::Mail& mail) {
        const std::int64_t notches = take_notches(manual_.wheel, dy);
        if (notches != 0) {
            manual_scroll(-notches);
            say_manual(keys_heard, keymap, mail);
        }
    }

    // ---- What it shows ------------------------------------------------------------------------

    /// THE MANUAL AS ITS PANE SHOWS IT: which pane it is about, then its lines from the window's
    /// first, and a line that says how many more there are below.
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
        const std::vector<ws::TextRow> lines =
            manual_reader::manual_lines(*manual_.words, keymap, room_columns);
        manual_.lines = lines.size();
        manual_scroll(0);
        const std::size_t end = std::min(lines.size(), manual_.first + manual_window());
        for (std::size_t i = manual_.first; i < end; ++i) {
            push(lines[i].text, lines[i].role);
        }
        if (end < lines.size()) {
            push("  v " + std::to_string(lines.size() - end) + " more lines below",
                 graphics::role::kMuted);
        }
        publish_manual(mail, std::move(out));
    }

private:
    /// THE WINDOW'S LINES: the room less the heading and the line that says what is below.
    std::size_t manual_window() const {
        return static_cast<std::size_t>(std::max<std::int64_t>(manual_canvas_.rows() - 2, 1));
    }

    /// THE WINDOW MOVES BY `by` LINES, held inside the manual's lines.
    void manual_scroll(std::int64_t by) {
        const std::int64_t last = std::max<std::int64_t>(
            static_cast<std::int64_t>(manual_.lines) - static_cast<std::int64_t>(manual_window()),
            0);
        manual_.first = static_cast<std::size_t>(
            std::clamp<std::int64_t>(static_cast<std::int64_t>(manual_.first) + by, 0, last));
    }

    /// The Manual's rows, drawn in its area. A press there means nothing.
    void publish_manual(loom::Mail& mail, std::vector<ws::TextRow> rows) {
        manual_map_.begin();
        (void)manual_map_.settle();
        manual_canvas_.publish_rows(mail, std::move(rows));
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
        std::size_t first = 0;    ///< the window's first line
        std::size_t lines = 0;    ///< ...and how many lines it last composed
        double wheel = 0.0;
    };
    ManualShown manual_;
    std::uint64_t manual_asks_ = 0;
    PaneCanvas manual_canvas_{kManualPane, kDesktopRole};
    component::RowMap<std::int64_t> manual_map_;
};

} // namespace zengine::desktop_pane

#endif // ZENGINE_DESKTOP_PANE_MANUAL_VIEW_HPP
