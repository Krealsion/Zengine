// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Menu Presenter -- the participant that presents a pane's offered menu: the default holder
// of `zengine.presenter`, an ordinary loadable weave a load plan names and a maker may replace
// (`examples/numbered-presenter` is a second one, and either may be reloaded in place of the
// other). Who owns what across the seam is written once, in `workshop/presenter_vocabulary.hpp`;
// what is written here is what THIS presenter decides:
//
//   the offer     refused in words when it has no rows, more than `kMaxPaneMenuRows`, or an id or
//                 label out of bounds -- the presenter owns what can be presented
//   the lines     "> label" for the row under the cursor, in the accent role, "  label" for the
//                 others, and a window of the rows the room holds, with "... n earlier" and
//                 "... n more" rows where it cuts, moved by the least it can
//   the keys      up and down move the cursor and stop at the ends; choose chooses it; back
//                 dismisses; a key the maker's contextual rows do not name means nothing here
//   the mouse     a press on a row's line chooses that row; a press outside dismisses; a release
//                 means nothing -- this presenter chooses on the press
//   the lifetime  one menu at a time, answered exactly once: chosen, dismissed, withdrawn by the
//                 host (in the host's words) or refused. The open menu is this weave's reload-kept
//                 state (`HeldMenu`), so a reloaded presenter -- this image or another keeping the
//                 same state -- says it carries the menu and shows it again: the maker's
//                 interaction continues across the reload. A menu it does not hold, it gives back.
//
// It performs nothing a row means, reads no subject, and holds no pane's authority: a choice is
// the requester's to judge and act on (`pane_menu::Asked`).
//
// It is one source file using only headers the installed Zengine and Loom packages publish, so a
// single-source recipe builds it with these links:
//     zengine::pane, zengine::activation, zengine::component, loom::switchboard

#include "workshop/pane_vocabulary.hpp"
#include "workshop/presenter_vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/list_window.hpp"
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

/// WHO GRANTS A MENU AND DRAWS IT, spelled as a stranger spells it.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// WHAT THE OFFER MAY BE, judged by the presenter that has to show it: nullptr when it can be.
// WL-CTX-10 -- agents/workshop/contextual.md
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
    if (g.room_rows <= 0 || g.room_columns <= 2) {
        return "no room to present a menu here";
    }
    return nullptr;
}

/// ONE LINE OF TEXT A CANVAS CAN DRAW, at most `columns` wide: an undrawable byte becomes a space
/// (a label is the requester's words, and one bad byte must not cost the maker the menu).
std::string drawable(const std::string& text, std::int64_t columns) {
    std::string out;
    for (const char c : text) {
        if (static_cast<std::int64_t>(out.size()) >= columns) {
            break;
        }
        const unsigned char byte = static_cast<unsigned char>(c);
        out += byte < 0x20u || byte >= 0x7Fu ? ' ' : c;
    }
    return out;
}

// WL-CTX-10 -- agents/workshop/contextual.md
class MenuPresenter
    : public loom::WeaveBase<MenuPresenter, ws::HeldMenu,
                             loom::Accept<loom::Activated, ws::MenuGranted, ws::MenuInput,
                                          ws::MenuWithdrawn>,
                             loom::Emit<ws::MenuShown, ws::MenuClosed, ws::PresenterReady,
                                        ws::PaneMenuAnswered>> {
public:
    /// EVERY ACTIVATION SAYS WHAT THIS IMAGE CARRIES -- nothing on a first load; after a reload,
    /// the menu a predecessor was presenting, shown again in this image's way.
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
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
        // ONE MENU AT A TIME. The host withdraws an older menu before it grants a newer one, so
        // this is the unreachable case -- answered all the same, because an answer owed is owed.
        if (state_.menu != 0 && state_.menu != g.menu) {
            answer(mail, false, std::string(), "replaced by a newer menu");
            state_ = ws::HeldMenu{};
        }
        if (const char* refused = refusal_of(g)) {
            // NOTHING TO PRESENT: answered here, and the menu given straight back to the host.
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
        show(mail);
    }

    void on(const ws::MenuInput& in, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || in.menu <= 0) {
            return;
        }
        if (in.menu != state_.menu) {
            // A MENU THIS IMAGE DOES NOT HOLD: given back, rather than left in front of a maker
            // with nobody to answer it.
            (void)mail.as_role(ws::kPresenterRole)
                .send_to_role(kWorkshopRole, ws::MenuClosed{in.menu, false, 0});
            return;
        }
        switch (in.kind) {
        case ws::menu_input::kKey: key(in, mail); break;
        case ws::menu_input::kPress: press(in, mail); break;
        case ws::menu_input::kOutside: close(mail, false, std::string(), "dismissed", in.input); break;
        default: break; // a release: this presenter chose on the press
        }
    }

    /// THE HOST ENDED IT: answered unchosen in the host's words, and nothing more.
    void on(const ws::MenuWithdrawn& w, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || w.menu <= 0 || w.menu != state_.menu) {
            return;
        }
        answer(mail, false, std::string(), w.why);
        state_ = ws::HeldMenu{};
    }

private:
    void key(const ws::MenuInput& in, loom::Mail& mail) {
        const std::int64_t last = static_cast<std::int64_t>(state_.rows.size()) - 1;
        switch (in.verb) {
        case ws::menu_verb::kUp:
            if (state_.cursor > 0) {
                --state_.cursor;
                show(mail);
            }
            break;
        case ws::menu_verb::kDown:
            if (state_.cursor < last) {
                ++state_.cursor;
                show(mail);
            }
            break;
        case ws::menu_verb::kChoose: choose(mail, state_.cursor, in.input); break;
        case ws::menu_verb::kBack: close(mail, false, std::string(), "dismissed", in.input); break;
        default: break;
        }
    }

    void press(const ws::MenuInput& in, loom::Mail& mail) {
        if (in.button != 1 || in.picture != picture_) {
            return; // not the primary button, or aimed at lines this image has since replaced
        }
        const std::int64_t row = row_at_line(in.line);
        if (row >= 0) {
            choose(mail, row, in.input);
        }
    }

    void choose(loom::Mail& mail, std::int64_t row, std::int64_t input) {
        if (row < 0 || row >= static_cast<std::int64_t>(state_.rows.size())) {
            return;
        }
        close(mail, true, state_.rows[static_cast<std::size_t>(row)].id, std::string(), input);
    }

    /// END THE MENU: the host first (it closes the popup and, for a choice, records the act that
    /// made it), then the requester's one answer.
    void close(loom::Mail& mail, bool chosen, const std::string& id, const std::string& why,
               std::int64_t input) {
        (void)mail.as_role(ws::kPresenterRole)
            .send_to_role(kWorkshopRole, ws::MenuClosed{state_.menu, chosen, input});
        answer(mail, chosen, id, why);
        state_ = ws::HeldMenu{};
    }

    void answer(loom::Mail& mail, bool chosen, const std::string& id, const std::string& why) {
        (void)mail.as_role(ws::kPresenterRole)
            .send_to_role(state_.office,
                          ws::PaneMenuAnswered{state_.pane, state_.subject, chosen,
                                               chosen ? id : std::string(),
                                               chosen ? std::string() : why},
                          static_cast<std::uint64_t>(state_.correlation));
    }

    /// THE WINDOW THE ROOM HOLDS, moved by the least it can from the last one shown.
    component::ListWindow window() const {
        return component::cursor_window(state_.rows.size(),
                                        static_cast<std::size_t>(state_.cursor), first_,
                                        static_cast<std::size_t>(state_.room_rows));
    }

    /// THE ROW A LINE CHOOSES, or -1 for a marker line or none.
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
        // THE PICTURE MOVES EXACTLY WHEN A LINE WOULD NOW CHOOSE SOMETHING ELSE: a scrolled window
        // or another menu. A cursor that moved inside the window is a highlight, not a new picture.
        if (!shown_ || w.first != first_ || w.count != shown_count_ || earlier != shown_earlier_ ||
            state_.menu != shown_menu_) {
            ++picture_;
        }
        shown_ = true;
        first_ = w.first;
        shown_count_ = w.count;
        shown_earlier_ = earlier;
        shown_menu_ = state_.menu;
        const std::int64_t columns = state_.room_columns;
        ws::MenuShown said;
        said.menu = state_.menu;
        said.picture = picture_;
        if (earlier) {
            said.lines.push_back(surface::SurfaceTextRow{
                drawable("  ... " + std::to_string(w.before) + " earlier", columns),
                surface::role::kMuted});
        }
        for (std::size_t i = w.first; i < w.end(); ++i) {
            const bool here = static_cast<std::int64_t>(i) == state_.cursor;
            said.lines.push_back(surface::SurfaceTextRow{
                drawable(std::string(here ? "> " : "  ") + state_.rows[i].label, columns),
                here ? surface::role::kAccent : surface::role::kFill});
        }
        if (more) {
            said.lines.push_back(surface::SurfaceTextRow{
                drawable("  ... " + std::to_string(w.after) + " more", columns),
                surface::role::kMuted});
        }
        (void)mail.as_role(ws::kPresenterRole).send_to_role(kWorkshopRole, said);
    }

    // NOT STATE, deliberately: how this image laid the menu out is its own, and a successor that
    // carries the menu lays it out afresh (and numbers its pictures afresh) in its own way.
    std::size_t first_ = 0;
    std::int64_t picture_ = 0;
    bool shown_ = false;
    std::size_t shown_count_ = 0;
    bool shown_earlier_ = false;
    std::int64_t shown_menu_ = 0;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(MenuPresenter)
