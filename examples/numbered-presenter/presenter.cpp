// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Numbered Presenter: an ordinary replacement for the presenter of a pane's offered menu that
// Workshop ships (`workshop/menu-presenter/`), loaded under `zengine.presenter` in its place; the seam is
// `workshop/presenter_vocabulary.hpp`, and what it does differently is its walkthrough's:
// workshop/docs/panes.md ("Replacing the menu presenter"). One source file, built by a
// single-source recipe with zengine::pane, zengine::activation, zengine::component,
// zengine::input, loom::switchboard, and loaded by a load-plan row under that role.

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

namespace {

namespace ws = zengine::workshop;
namespace surface = zengine::surface;
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
        ws::announce_presenter_ready(mail, state_);
        if (state_.menu != 0) {
            show(mail);
        }
    }

    void on(const ws::MenuGranted& g, loom::Mail& mail) {
        if (ws::accept_menu_grant(g, mail, state_, refusal_of(g))) {
            pictures_.first = 0;
            pressed_row_ = -1;
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
        case ws::menu_input::kPress: press(in, mail); break;
        case ws::menu_input::kRelease: release(in, mail); break;
        case ws::menu_input::kOutside: close(mail, false, std::string(), "dismissed", in.input); break;
        default: break;
        }
    }

    /// ...AND THE HOST'S CANCELLATION OF ONE IT DOES NOT HOLD IS GIVEN BACK TOO: this image keeps
    /// the shipped presenter's answer discipline exactly, timing included, by the same helper.
    void on(const ws::MenuWithdrawn& w, loom::Mail& mail) {
        ws::settle_menu_withdrawn(w, mail, state_);
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
        if (in.button != 1 || !pictures_.matches(in)) {
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
        if (row < 0 || row >= count()) {
            return;
        }
        bool standard = false;
        const ws::PaneMenuRow& chosen = row_of(row, standard);
        if (standard) {
            close(mail, false, std::string(), "a standard row was chosen", input, chosen.id);
            return;
        }
        close(mail, true, chosen.id, std::string(), input);
    }

    void close(loom::Mail& mail, bool chosen, const std::string& id, const std::string& why,
               std::int64_t input, const std::string& standard = std::string()) {
        ws::close_held_menu(mail, state_, chosen, id, why, input, standard);
        pressed_row_ = -1;
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

    void show(loom::Mail& mail) {
        const component::ListWindow w = window();
        const std::size_t earlier = component::earlier_marker(w);
        const std::size_t more = component::more_marker(w);
        ws::MenuShown said;
        said.menu = state_.menu;
        said.picture = pictures_.shown(w.first, w.count, earlier > 0, state_.menu);
        if (earlier > 0) {
            said.lines.push_back(surface::SurfaceTextRow{
                fitted("  ^ " + std::to_string(earlier) + " above"), surface::role::kMuted});
        }
        for (std::size_t i = w.first; i < w.end(); ++i) {
            const bool here = static_cast<std::int64_t>(i) == state_.cursor;
            bool standard = false;
            const ws::PaneMenuRow& row = row_of(static_cast<std::int64_t>(i), standard);
            said.lines.push_back(
                surface::SurfaceTextRow{line_for(i, row.label, here, state_.room_columns),
                                        here ? surface::role::kAccent : surface::role::kFill});
        }
        if (more > 0) {
            said.lines.push_back(surface::SurfaceTextRow{
                fitted("  v " + std::to_string(more) + " below"), surface::role::kMuted});
        }
        (void)mail.as_role(ws::kPresenterRole).send_to_role(kWorkshopRole, said);
    }

    // NOT STATE: this image's layout, and a press it armed -- a successor lays the menu out afresh
    // and arms nothing a hand began under another image.
    ws::MenuPictureTracker pictures_;
    std::int64_t pressed_row_ = -1;
    std::int64_t pressed_input_ = 0;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(NumberedPresenter)
