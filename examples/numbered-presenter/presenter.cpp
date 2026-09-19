// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Numbered Presenter: a second presenter of a pane's offered menu -- the ordinary replacement of
// the one Workshop ships (`menu-presenter/`). A load-plan row loads it under `zengine.presenter`
// instead of the shipped artifact, or a reload puts its image in the shipped one's place, and every
// pane's menu is presented its way from then on. The panes that ask for menus do not change: what
// a row means, which subject it is about and what choosing it does stay theirs. The seam, and who
// owns what across it, is `workshop/presenter_vocabulary.hpp`.
// Walkthrough: docs/workshop/panes.md ("Replacing the menu presenter").
//
// WHAT IT DOES DIFFERENTLY, which is the point of replacing one:
//
//   the lines     every row is numbered, "> 3 label" under the cursor, so a row can be named by
//                 its number; a room too small for the menu windows it like the shipped one
//   the keys      a digit 1-9 chooses that row at once; up and down WRAP at the ends; choose
//                 chooses the cursor's row; back dismisses
//   the mouse     a press on a row only puts the cursor there; the RELEASE on that same row
//                 chooses it (a hand that slides off before letting go chooses nothing); a press
//                 outside dismisses
//
// WHAT IT KEEPS: the same accepted set and the same reload state (`HeldMenu`), so an open menu
// crosses a reload between the two presenters in either direction -- shown again the new way,
// answered under the same number -- and the same answer discipline: exactly once, chosen,
// dismissed, withdrawn in the host's words, or refused. An act or a withdrawal for a menu this
// image does not hold it answers not at all: it gives the interaction back to the host, which
// knows who asked and settles them.
//
// It is one source file using only headers the installed Zengine and Loom packages publish, so a
// single-source recipe builds it with these links:
//     zengine::pane, zengine::activation, zengine::component, zengine::input, loom::switchboard
// and a load-plan row loads it under the role "zengine.presenter".

#include "workshop/pane_vocabulary.hpp"
#include "workshop/presenter_vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/list_window.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace ws = zengine::workshop;
namespace surface = zengine::surface;
namespace component = zengine::component;
namespace scan = zengine::input::scan;

constexpr const char* kWorkshopRole = "zengine.workshop";

// ---- CHANGE THIS FIRST -------------------------------------------------------------------------
//
// How one row reads: its number (1-9; a row past nine has none), then its label, with the cursor's
// row marked. The width is the room's; an undrawable byte of a label is shown as a space.
std::string line_for(std::size_t row, const std::string& label, bool here, std::int64_t columns) {
    std::string text = here ? "> " : "  ";
    text += row < 9 ? std::to_string(row + 1) + " " : std::string("  ");
    for (const char c : label) {
        const unsigned char byte = static_cast<unsigned char>(c);
        text += byte < 0x20u || byte >= 0x7Fu ? ' ' : c;
    }
    if (static_cast<std::int64_t>(text.size()) > columns) {
        text.resize(static_cast<std::size_t>(columns));
    }
    return text;
}

/// THE ROW A BARE DIGIT NAMES, or -1.
std::int64_t digit_row(std::int64_t scancode, std::int64_t modifiers) {
    if (modifiers != zengine::input::mod::kNone) {
        return -1;
    }
    const std::int64_t digits[] = {scan::k1, scan::k2, scan::k3, scan::k4, scan::k5,
                                   scan::k6, scan::k7, scan::k8, scan::k9};
    for (std::int64_t i = 0; i < 9; ++i) {
        if (digits[i] == scancode) {
            return i;
        }
    }
    return -1;
}

const char* refusal_of(const ws::MenuGranted& g) {
    if (g.rows.empty()) {
        return "nothing to present -- the request offered no rows";
    }
    if (g.rows.size() > ws::kMaxPaneMenuRows) {
        return "too many rows to present";
    }
    for (const ws::PaneMenuRow& r : g.rows) {
        if (r.id.empty() || r.id.size() > ws::kMaxPaneMenuIdLen ||
            r.label.size() > ws::kMaxPaneMenuLabelLen) {
            return "a row's id is empty or too long, or its label is too long";
        }
    }
    if (g.room_rows <= 0 || g.room_columns <= 4) {
        return "no room to present a menu here";
    }
    return nullptr;
}

class NumberedPresenter
    : public loom::WeaveBase<NumberedPresenter, ws::HeldMenu,
                             loom::Accept<loom::Activated, ws::MenuGranted, ws::MenuInput,
                                          ws::MenuWithdrawn>,
                             loom::Emit<ws::MenuShown, ws::MenuClosed, ws::MenuReturned,
                                        ws::PresenterReady, ws::PaneMenuAnswered>> {
public:
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        // WHAT THIS IMAGE CARRIES: after a reload in another presenter's place, that one's open
        // menu -- which is shown again, numbered, and answered by this image.
        (void)mail.as_role(ws::kPresenterRole)
            .send_to_role(kWorkshopRole, ws::PresenterReady{state_.menu});
        if (state_.menu != 0) {
            show(mail);
        }
    }

    void on(const ws::MenuGranted& g, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || g.menu <= 0) {
            return;
        }
        if (state_.menu != 0 && state_.menu != g.menu) {
            answer(mail, false, std::string(), "replaced by a newer menu");
            state_ = ws::HeldMenu{};
        }
        if (const char* refused = refusal_of(g)) {
            (void)mail.as_role(ws::kPresenterRole)
                .send_to_role(kWorkshopRole, ws::MenuClosed{g.menu, false, 0});
            (void)mail.as_role(ws::kPresenterRole)
                .send_to_role(g.office,
                              ws::PaneMenuAnswered{g.pane, g.subject, false, std::string(),
                                                   refused},
                              mail.correlation());
            return;
        }
        state_ = ws::HeldMenu{g.menu,
                              static_cast<std::int64_t>(mail.correlation()),
                              g.office,
                              g.pane,
                              g.subject,
                              g.rows,
                              0,
                              g.room_rows,
                              g.room_columns};
        first_ = 0;
        pressed_row_ = -1;
        show(mail);
    }

    void on(const ws::MenuInput& in, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || in.menu <= 0) {
            return;
        }
        if (in.menu != state_.menu) {
            // A MENU THIS IMAGE DOES NOT HOLD: given back, never left unanswered.
            give_back(in.menu, mail);
            return;
        }
        switch (in.kind) {
        case ws::menu_input::kKey: key(in, mail); break;
        case ws::menu_input::kPress: press(in, mail); break;
        case ws::menu_input::kRelease: release(in, mail); break;
        case ws::menu_input::kOutside: close(mail, false, std::string(), "dismissed", in.input); break;
        default: break;
        }
    }

    /// ...AND THE HOST'S CANCELLATION OF ONE IT DOES NOT HOLD IS GIVEN BACK TOO: the same rule
    /// as an act it cannot carry, because both are one interaction's, and this image keeps the
    /// shipped presenter's answer discipline exactly.
    void on(const ws::MenuWithdrawn& w, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || w.menu <= 0) {
            return;
        }
        if (w.menu != state_.menu) {
            give_back(w.menu, mail);
            return;
        }
        answer(mail, false, std::string(), w.why);
        state_ = ws::HeldMenu{};
    }

private:
    /// AN INTERACTION THIS IMAGE CANNOT CARRY, HANDED BACK. It holds no such menu, so it has said
    /// nothing to that requester and never will -- and the host, which knows who asked, settles
    /// it. Never `MenuClosed`: that is this image's word that it finished the work and answered.
    void give_back(std::int64_t menu, loom::Mail& mail) {
        (void)mail.as_role(ws::kPresenterRole)
            .send_to_role(kWorkshopRole,
                          ws::MenuReturned{menu, "this image does not hold that menu"});
    }

    void key(const ws::MenuInput& in, loom::Mail& mail) {
        const std::int64_t count = static_cast<std::int64_t>(state_.rows.size());
        const std::int64_t named = digit_row(in.scancode, in.modifiers);
        if (named >= 0) {
            if (named < count) {
                choose(mail, named, in.input);
            }
            return;
        }
        switch (in.verb) {
        case ws::menu_verb::kUp:
            state_.cursor = state_.cursor > 0 ? state_.cursor - 1 : count - 1; // wraps
            show(mail);
            break;
        case ws::menu_verb::kDown:
            state_.cursor = state_.cursor + 1 < count ? state_.cursor + 1 : 0; // wraps
            show(mail);
            break;
        case ws::menu_verb::kChoose: choose(mail, state_.cursor, in.input); break;
        case ws::menu_verb::kBack: close(mail, false, std::string(), "dismissed", in.input); break;
        default: break;
        }
    }

    /// A PRESS PUTS THE CURSOR ON THE ROW AND WAITS FOR ITS RELEASE.
    void press(const ws::MenuInput& in, loom::Mail& mail) {
        pressed_row_ = -1;
        if (in.button != 1 || in.picture != picture_) {
            return; // aimed at lines this image has since replaced: nothing is armed
        }
        const std::int64_t row = row_at_line(in.line);
        if (row < 0) {
            return;
        }
        pressed_row_ = row;
        pressed_input_ = in.input;
        if (state_.cursor != row) {
            state_.cursor = row;
            show(mail);
        }
    }

    /// ...AND THE RELEASE ON THAT SAME ROW CHOOSES IT, as the act the press began: the release
    /// carries its press's number, so the choice continues that one act.
    void release(const ws::MenuInput& in, loom::Mail& mail) {
        const std::int64_t armed = pressed_row_;
        pressed_row_ = -1;
        if (armed < 0 || in.button != 1 || in.input != pressed_input_) {
            return;
        }
        if (row_at_line(in.line) == armed) {
            choose(mail, armed, in.input);
        }
    }

    void choose(loom::Mail& mail, std::int64_t row, std::int64_t input) {
        if (row < 0 || row >= static_cast<std::int64_t>(state_.rows.size())) {
            return;
        }
        close(mail, true, state_.rows[static_cast<std::size_t>(row)].id, std::string(), input);
    }

    void close(loom::Mail& mail, bool chosen, const std::string& id, const std::string& why,
               std::int64_t input) {
        (void)mail.as_role(ws::kPresenterRole)
            .send_to_role(kWorkshopRole, ws::MenuClosed{state_.menu, chosen, input});
        answer(mail, chosen, id, why);
        state_ = ws::HeldMenu{};
        pressed_row_ = -1;
    }

    void answer(loom::Mail& mail, bool chosen, const std::string& id, const std::string& why) {
        (void)mail.as_role(ws::kPresenterRole)
            .send_to_role(state_.office,
                          ws::PaneMenuAnswered{state_.pane, state_.subject, chosen,
                                               chosen ? id : std::string(),
                                               chosen ? std::string() : why},
                          static_cast<std::uint64_t>(state_.correlation));
    }

    std::string fitted(std::string text) const {
        if (static_cast<std::int64_t>(text.size()) > state_.room_columns) {
            text.resize(static_cast<std::size_t>(state_.room_columns));
        }
        return text;
    }

    component::ListWindow window() const {
        return component::cursor_window(state_.rows.size(),
                                        static_cast<std::size_t>(state_.cursor), first_,
                                        static_cast<std::size_t>(state_.room_rows));
    }

    std::int64_t row_at_line(std::int64_t line) const {
        const component::ListWindow w = window();
        const std::int64_t offset = line - (w.before > 0 && w.markers > 0 ? 1 : 0);
        if (line < 0 || offset < 0 || offset >= static_cast<std::int64_t>(w.count)) {
            return -1;
        }
        return static_cast<std::int64_t>(w.first) + offset;
    }

    void show(loom::Mail& mail) {
        const component::ListWindow w = window();
        const bool earlier = w.before > 0 && w.markers > 0;
        const bool more = w.after > 0 && w.markers > (earlier ? 1u : 0u);
        if (!shown_ || w.first != first_ || w.count != shown_count_ || earlier != shown_earlier_ ||
            state_.menu != shown_menu_) {
            ++picture_;
        }
        shown_ = true;
        first_ = w.first;
        shown_count_ = w.count;
        shown_earlier_ = earlier;
        shown_menu_ = state_.menu;
        ws::MenuShown said;
        said.menu = state_.menu;
        said.picture = picture_;
        if (earlier) {
            said.lines.push_back(surface::SurfaceTextRow{
                fitted("  ^ " + std::to_string(w.before) + " above"), surface::role::kMuted});
        }
        for (std::size_t i = w.first; i < w.end(); ++i) {
            const bool here = static_cast<std::int64_t>(i) == state_.cursor;
            said.lines.push_back(
                surface::SurfaceTextRow{line_for(i, state_.rows[i].label, here, state_.room_columns),
                                        here ? surface::role::kAccent : surface::role::kFill});
        }
        if (more) {
            said.lines.push_back(surface::SurfaceTextRow{
                fitted("  v " + std::to_string(w.after) + " below"), surface::role::kMuted});
        }
        (void)mail.as_role(ws::kPresenterRole).send_to_role(kWorkshopRole, said);
    }

    // NOT STATE: this image's layout, and a press it armed -- a successor lays the menu out afresh
    // and arms nothing a hand began under another image.
    std::size_t first_ = 0;
    std::int64_t picture_ = 0;
    bool shown_ = false;
    std::size_t shown_count_ = 0;
    bool shown_earlier_ = false;
    std::int64_t shown_menu_ = 0;
    std::int64_t pressed_row_ = -1;
    std::int64_t pressed_input_ = 0;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(NumberedPresenter)
