// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Numbered Presenter: an ordinary replacement for the presenter Workshop ships
// (`workshop/menu-presenter/`), loaded under `zengine.presenter` in its place; the seam is
// `workshop/presenter_vocabulary.hpp`, what it does differently its walkthrough's
// (workshop/docs/panes.md, "Replacing the menu presenter"). It draws the menu in the area granted
// and hears the pointer there. One source file, built with zengine::pane, zengine::graphics, zengine::activation,
// zengine::component, zengine::input, loom::switchboard, and loaded by a load-plan row.

#include "workshop/pane_vocabulary.hpp"
#include "workshop/presenter_vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/list_window.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace ws = zengine::workshop;
namespace gfx = zengine::graphics;
namespace component = zengine::component;
namespace scan = zengine::input::scan;

constexpr const char* kWorkshopRole = ws::pane_menu::kWorkshopRole;

// ---- CHANGE THIS FIRST -------------------------------------------------------------------------
//
// How one row reads: its number (1-9; a row past nine has none), then its label, with the cursor's
// row marked. The host's standard rows are numbered on from the requester's, with no rule between.
// The width is the room's; an undrawable byte of a label is shown as a space.
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

/// WHAT THE OFFER MAY BE: empty when it can be shown. A line spends four columns, "> 1 ", before
/// a row's label.
std::string refusal_of(const ws::MenuGranted& g) { return ws::menu_refusal_of(g, 4); }

class NumberedPresenter
    : public loom::WeaveBase<
          NumberedPresenter, ws::HeldMenu,
          loom::Accept<loom::Activated, ws::MenuGranted, ws::MenuInput, ws::MenuWithdrawn,
                       gfx::AreaRoom, gfx::DrawnRefused, gfx::AreaButtonChanged>,
          loom::Emit<ws::MenuShown, ws::MenuClosed, ws::MenuReturned, ws::PresenterReady,
                     ws::PaneMenuAnswered, gfx::Drawn, gfx::RoomAsked>> {
public:
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        // WHAT THIS IMAGE CARRIES: after a reload in another presenter's place, that one's open
        // menu -- which is shown again, numbered, where it stands, and answered by this image.
        ws::announce_presenter_ready(mail, state_);
        drawing_.stand_in(state_.area);
        if (state_.menu != 0) {
            show(mail);
            drawing_.ask_room(mail);
        }
    }

    void on(const ws::MenuGranted& g, loom::Mail& mail) {
        if (ws::accept_menu_grant(g, mail, state_, refusal_of(g))) {
            pictures_.first = 0;
            pressed_row_ = -1;
            drawing_.stand_in(state_.area);
            show(mail);
        }
    }

    void on(const ws::MenuInput& in, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || in.menu <= 0) {
            return;
        }
        if (in.menu != state_.menu) {
            // A MENU THIS IMAGE DOES NOT HOLD: the interaction is given back -- this image
            // cannot carry it now, which is not a claim that nobody ever answered it.
            give_back(in.menu, mail);
            return;
        }
        switch (in.kind) {
        case ws::menu_input::kKey: key(in, mail); break;
        case ws::menu_input::kOutside: close(mail, false, std::string(), "dismissed", in.input); break;
        default: break;
        }
    }

    /// ...AND THE HOST'S CANCELLATION OF ONE IT DOES NOT HOLD IS GIVEN BACK TOO: this image keeps
    /// the shipped presenter's answer discipline exactly, timing included, by the same helper.
    void on(const ws::MenuWithdrawn& w, loom::Mail& mail) {
        ws::settle_menu_withdrawn(w, mail, state_);
        drawing_.stand_in(state_.area);
    }

    void on(const gfx::AreaRoom& r, loom::Mail& mail) {
        if (state_.menu != 0 && drawing_.room(r, mail)) {
            show(mail);
        }
    }

    void on(const gfx::DrawnRefused& refused, loom::Mail& mail) {
        (void)drawing_.refused(refused, mail);
    }

    /// A PRESS PUTS THE CURSOR ON THE ROW AND WAITS FOR ITS RELEASE; THE RELEASE ON THAT SAME ROW
    /// CHOOSES IT, as the act the press began: both carry the press's number, and the choice is
    /// said under it. The next press arms afresh. It accepts what the shipped presenter accepts,
    /// so either may be reloaded in the other's place.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        if (state_.menu == 0 || b.button != 1) {
            return;
        }
        const std::int64_t line = drawing_.line_at(b.area, b.x, b.y, mail);
        const std::int64_t row = line >= 0 ? row_at_line(line) : -1;
        if (b.pressed) {
            pressed_row_ = -1;
            if (row < 0) {
                return;
            }
            pressed_row_ = row;
            pressed_press_ = mail.correlation();
            if (state_.cursor != row) {
                state_.cursor = row;
                show(mail);
            }
            return;
        }
        const std::int64_t armed = pressed_row_;
        pressed_row_ = -1;
        if (armed >= 0 && mail.correlation() == pressed_press_ && row == armed) {
            choose(mail, armed, 0, pressed_press_);
        }
    }

private:
    /// AN INTERACTION THIS IMAGE CANNOT CARRY, HANDED BACK (`workshop::give_back_menu`).
    void give_back(std::int64_t menu, loom::Mail& mail) { ws::give_back_menu(mail, menu); }

    /// EVERY ROW SHOWN: the requester's, then the host's standard rows.
    std::int64_t count() const {
        return static_cast<std::int64_t>(state_.rows.size() + state_.standard.size());
    }
    /// Row `row` of that run, and whether it is a standard row (the host's to spend).
    const ws::PaneMenuRow& row_of(std::int64_t row, bool& standard) const {
        const std::size_t r = static_cast<std::size_t>(row);
        standard = r >= state_.rows.size();
        return standard ? state_.standard[r - state_.rows.size()] : state_.rows[r];
    }

    void key(const ws::MenuInput& in, loom::Mail& mail) {
        const std::int64_t count = this->count();
        const std::int64_t named = digit_row(in.scancode, in.modifiers);
        if (named >= 0) {
            if (named < count) {
                choose(mail, named, in.input, 0);
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
        case ws::menu_verb::kChoose: choose(mail, state_.cursor, in.input, 0); break;
        case ws::menu_verb::kBack: close(mail, false, std::string(), "dismissed", in.input); break;
        default: break;
        }
    }

    /// CHOOSE A ROW, as act `input` of Workshop's or the window's press `press`.
    void choose(loom::Mail& mail, std::int64_t row, std::int64_t input, std::uint64_t press) {
        if (row < 0 || row >= count()) {
            return;
        }
        bool standard = false;
        const ws::PaneMenuRow& chosen = row_of(row, standard);
        if (standard) {
            close(mail, false, std::string(), "a standard row was chosen", input, chosen.id, press);
            return;
        }
        close(mail, true, chosen.id, std::string(), input, std::string(), press);
    }

    void close(loom::Mail& mail, bool chosen, const std::string& id, const std::string& why,
               std::int64_t input, const std::string& standard = std::string(),
               std::uint64_t press = 0) {
        ws::close_held_menu(mail, state_, chosen, id, why, input, standard, press);
        pressed_row_ = -1;
        drawing_.stand_in(state_.area);
    }

    std::string fitted(std::string text) const {
        if (static_cast<std::int64_t>(text.size()) > state_.room_columns) {
            text.resize(static_cast<std::size_t>(state_.room_columns));
        }
        return text;
    }

    component::ListWindow window() const {
        return component::cursor_window(static_cast<std::size_t>(count()),
                                        static_cast<std::size_t>(state_.cursor), pictures_.first,
                                        static_cast<std::size_t>(state_.room_rows));
    }

    std::int64_t row_at_line(std::int64_t line) const {
        const component::ListWindow w = window();
        const std::int64_t offset = line - (component::earlier_marker(w) > 0 ? 1 : 0);
        if (line < 0 || offset < 0 || offset >= static_cast<std::int64_t>(w.count)) {
            return -1;
        }
        return static_cast<std::int64_t>(w.first) + offset;
    }

    /// THE MENU SHOWN: its extent said to Workshop, which sizes its area to it, and its lines drawn
    /// there, naming none.
    void show(loom::Mail& mail) {
        const component::ListWindow w = window();
        const std::size_t earlier = component::earlier_marker(w);
        const std::size_t more = component::more_marker(w);
        std::vector<ws::MenuLine> lines;
        if (earlier > 0) {
            lines.push_back(ws::MenuLine{fitted("  ^ " + std::to_string(earlier) + " above"),
                                         gfx::role::kMuted, std::string()});
        }
        for (std::size_t i = w.first; i < w.end(); ++i) {
            const bool here = static_cast<std::int64_t>(i) == state_.cursor;
            bool standard = false;
            const ws::PaneMenuRow& row = row_of(static_cast<std::int64_t>(i), standard);
            lines.push_back(ws::MenuLine{line_for(i, row.label, here, state_.room_columns),
                                         here ? gfx::role::kAccent : gfx::role::kFill,
                                         std::string()});
        }
        if (more > 0) {
            lines.push_back(ws::MenuLine{fitted("  v " + std::to_string(more) + " below"),
                                         gfx::role::kMuted, std::string()});
        }
        std::int64_t width = 1;
        for (const ws::MenuLine& line : lines) {
            width = static_cast<std::int64_t>(line.text.size()) > width
                        ? static_cast<std::int64_t>(line.text.size())
                        : width;
        }
        ws::MenuShown said;
        said.menu = state_.menu;
        said.picture = pictures_.shown(w.first, w.count, earlier > 0, state_.menu);
        said.area = state_.area;
        said.rows = static_cast<std::int64_t>(lines.size());
        said.columns = width;
        (void)mail.as_role(ws::kPresenterRole).send_to_role(kWorkshopRole, said);
        drawing_.draw(lines, mail);
    }

    // NOT STATE: this image's layout and drawing, and a press it armed -- a successor lays the
    // menu out afresh and arms nothing a hand began under another image.
    ws::MenuPictureTracker pictures_;
    ws::MenuDrawing drawing_;
    std::int64_t pressed_row_ = -1;
    std::uint64_t pressed_press_ = 0;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(NumberedPresenter)
