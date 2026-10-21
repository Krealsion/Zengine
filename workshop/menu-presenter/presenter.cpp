// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Menu Presenter: the participant that presents a pane's offered menu, the default holder
// of `zengine.presenter`, an ordinary loadable weave a weaver may replace or reload in place
// (`examples/numbered-presenter` is another). Who owns what across the seam is in
// `workshop/presenter_vocabulary.hpp`; here is what this presenter decides. It performs nothing
// a row means, reads no subject, and holds no pane's authority (`pane_menu::Asked`).
// Pane law: agents/panes.md

// An offer with no rows, too many, or an id or label out of bounds is refused in words, and so are
// such standard rows. Lines: the requester's rows, a rule naming the pane, the standard rows;
// "> label" at the cursor, "  label" otherwise, windowed with "... n earlier" / "... n more", a
// row's line named by its id. Up and down pass over the rule and stop at the ends, choose chooses,
// back dismisses; a press on a row chooses it, one outside dismisses, a release means nothing. A
// standard row chosen is named to the host, which spends it, and the requester is answered unchosen.

// One menu at a time, answered exactly once: chosen, dismissed, withdrawn by the host or refused.
// The open menu is reload-kept state (`HeldMenu`), so a reloaded presenter shows it again; a menu
// it does not hold, it gives back. One source file on installed headers only, so a single-source
// recipe builds it with zengine::pane, zengine::activation, zengine::component, loom::switchboard.

#include "workshop/pane_parts.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/presenter_vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/list_window.hpp"
#include "graphics/role.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>

namespace {

namespace ws = zengine::workshop;
namespace surface = zengine::surface;
namespace gfx = zengine::graphics;
namespace component = zengine::component;

/// WHO GRANTS A MENU AND DRAWS IT.
constexpr const char* kWorkshopRole = ws::pane_menu::kWorkshopRole;

/// WHAT THE OFFER MAY BE, judged by the presenter that has to show it: empty when it can be. A
/// line spends two columns, "> ", before a row's label.
// WL-CTX-10 -- agents/workshop/pane-menu.md
std::string refusal_of(const ws::MenuGranted& g) { return ws::menu_refusal_of(g, 2); }

/// THE RULE ABOVE THE STANDARD ROWS, naming the pane they act on -- "  -- Name --" -- and drawn
/// out with dashes as far as the widest row's line, so it widens the popup only for a long name.
std::string rule_line(const std::string& pane_name, std::size_t widest) {
    std::string out = "  --";
    if (!pane_name.empty()) {
        out += " " + pane_name + " --";
    }
    while (out.size() < widest + 2) {
        out += '-';
    }
    return out;
}

/// ONE LINE OF TEXT A CANVAS CAN DRAW, at most `columns` wide: an undrawable byte becomes a space
/// (a label is the requester's words, and one bad byte must not cost the weaver the menu).
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

// WL-CTX-10 -- agents/workshop/pane-menu.md
class MenuPresenter
    : public loom::WeaveBase<MenuPresenter, ws::HeldMenu,
                             loom::Accept<loom::Activated, ws::MenuGranted, ws::MenuInput,
                                          ws::MenuWithdrawn>,
                             loom::Emit<ws::v2::MenuShown, ws::MenuClosed, ws::MenuReturned,
                                        ws::PresenterReady, ws::PaneMenuAnswered>> {
public:
    /// EVERY ACTIVATION SAYS WHAT THIS IMAGE CARRIES -- nothing on a first load; after a reload,
    /// the menu a predecessor was presenting, shown again in this image's way.
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        ws::announce_presenter_ready(mail, state_);
        if (state_.menu != 0) {
            show(mail);
        }
    }

    /// One menu at a time; one this image cannot show is answered and given straight back.
    void on(const ws::MenuGranted& g, loom::Mail& mail) {
        if (ws::accept_menu_grant(g, mail, state_, refusal_of(g))) {
            pictures_.first = 0;
            show(mail);
        }
    }

    void on(const ws::MenuInput& in, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || in.menu <= 0) {
            return;
        }
        if (in.menu != state_.menu) {
            // A MENU THIS IMAGE DOES NOT HOLD: the interaction is given back, rather than left
            // in front of a weaver with nobody to answer it. It says this image cannot carry it
            // NOW -- not that nobody ever answered that requester, which an image holding no
            // such menu cannot know.
            give_back(in.menu, mail);
            return;
        }
        switch (in.kind) {
        case ws::menu_input::kKey: key(in, mail); break;
        case ws::menu_input::kPress: press(in, mail); break;
        case ws::menu_input::kOutside: close(mail, false, std::string(), "dismissed", in.input); break;
        default: break; // a release: this presenter chose on the press
        }
    }

    /// THE HOST ENDED IT: answered unchosen in the host's words, and nothing more; one for a menu
    /// this image does not hold is given back, from inside this handler.
    void on(const ws::MenuWithdrawn& w, loom::Mail& mail) {
        ws::settle_menu_withdrawn(w, mail, state_);
    }

private:
    /// AN INTERACTION THIS IMAGE CANNOT CARRY, HANDED BACK (`workshop::give_back_menu`).
    void give_back(std::int64_t menu, loom::Mail& mail) { ws::give_back_menu(mail, menu); }

    /// THE MENU AS SHOWN, ENTRY BY ENTRY: the requester's rows, then -- when the host granted
    /// standard rows -- the rule, then those rows. The cursor and a line name an entry.
    std::int64_t entries() const {
        const std::size_t standard = state_.standard.size();
        return static_cast<std::int64_t>(state_.rows.size() + (standard > 0 ? standard + 1 : 0));
    }
    bool is_rule(std::int64_t entry) const {
        return !state_.standard.empty() && entry == static_cast<std::int64_t>(state_.rows.size());
    }
    /// The row an entry is, and whether it is the host's (a standard row) rather than the
    /// requester's; nullptr for the rule.
    const ws::PaneMenuRow* row_of(std::int64_t entry, bool& standard) const {
        const std::int64_t mine = static_cast<std::int64_t>(state_.rows.size());
        standard = entry > mine;
        if (entry < 0 || entry >= entries() || is_rule(entry)) {
            return nullptr;
        }
        return standard ? &state_.standard[static_cast<std::size_t>(entry - mine - 1)]
                        : &state_.rows[static_cast<std::size_t>(entry)];
    }

    void key(const ws::MenuInput& in, loom::Mail& mail) {
        const std::int64_t last = entries() - 1;
        switch (in.verb) {
        case ws::menu_verb::kUp:
            if (state_.cursor > 0) {
                --state_.cursor;
                if (is_rule(state_.cursor)) {
                    --state_.cursor; // the rule is no row: the requester's always precede it
                }
                show(mail);
            }
            break;
        case ws::menu_verb::kDown:
            if (state_.cursor < last) {
                ++state_.cursor;
                if (is_rule(state_.cursor)) {
                    ++state_.cursor; // ...and standard rows always follow it
                }
                show(mail);
            }
            break;
        case ws::menu_verb::kChoose: choose(mail, state_.cursor, in.input); break;
        case ws::menu_verb::kBack: close(mail, false, std::string(), "dismissed", in.input); break;
        default: break;
        }
    }

    void press(const ws::MenuInput& in, loom::Mail& mail) {
        if (in.button != 1 || !pictures_.matches(in)) {
            return; // not the primary button, or aimed at lines this image has since replaced
        }
        const std::int64_t row = row_at_line(in.line);
        if (row >= 0) {
            choose(mail, row, in.input);
        }
    }

    void choose(loom::Mail& mail, std::int64_t entry, std::int64_t input) {
        bool standard = false;
        const ws::PaneMenuRow* row = row_of(entry, standard);
        if (row == nullptr) {
            return; // the rule, or no entry at all
        }
        if (standard) {
            close(mail, false, std::string(), "a standard row was chosen", input, row->id);
            return;
        }
        close(mail, true, row->id, std::string(), input);
    }

    /// END THE MENU: the host first, then the requester's one answer (`workshop::close_held_menu`).
    void close(loom::Mail& mail, bool chosen, const std::string& id, const std::string& why,
               std::int64_t input, const std::string& standard = std::string()) {
        ws::close_held_menu(mail, state_, chosen, id, why, input, standard);
    }

    /// THE WINDOW THE ROOM HOLDS, moved by the least it can from the last one shown.
    component::ListWindow window() const {
        return component::cursor_window(static_cast<std::size_t>(entries()),
                                        static_cast<std::size_t>(state_.cursor), pictures_.first,
                                        static_cast<std::size_t>(state_.room_rows));
    }

    /// THE ENTRY A LINE CHOOSES, or -1 for a marker line or none.
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
        const std::int64_t columns = state_.room_columns;
        ws::v2::MenuShown said;
        std::set<std::string> named;
        said.menu = state_.menu;
        said.picture = pictures_.shown(w.first, w.count, earlier > 0, state_.menu);
        if (earlier > 0) {
            said.lines.push_back(surface::SurfaceTextRow{
                drawable("  ... " + std::to_string(earlier) + " earlier", columns),
                gfx::role::kMuted});
        }
        // THE RULE NAMES THE PANE THE STANDARD ROWS ACT ON, as wide as the widest row's line.
        std::size_t widest = 0;
        for (const std::vector<ws::PaneMenuRow>* run : {&state_.rows, &state_.standard}) {
            for (const ws::PaneMenuRow& r : *run) {
                widest = r.label.size() > widest ? r.label.size() : widest;
            }
        }
        for (std::size_t i = w.first; i < w.end(); ++i) {
            const std::int64_t entry = static_cast<std::int64_t>(i);
            bool standard = false;
            const ws::PaneMenuRow* row = row_of(entry, standard);
            if (row == nullptr) {
                said.lines.push_back(surface::SurfaceTextRow{
                    drawable(rule_line(state_.pane_name, widest), columns), gfx::role::kMuted});
                continue;
            }
            const bool here = entry == state_.cursor;
            said.lines.push_back(surface::SurfaceTextRow{
                drawable(std::string(here ? "> " : "  ") + row->label, columns),
                here ? gfx::role::kAccent : gfx::role::kFill});
            // THE LINE IS NAMED BY ITS ROW'S ID, as the id the choice comes back as: a requester's
            // row and a standard row alike, the first line to carry an id keeping it.
            const auto width = static_cast<std::int64_t>(said.lines.back().text.size());
            if (width > 0 && ws::pane_part_name_problem(row->id) == nullptr &&
                named.insert(row->id).second) {
                said.parts.push_back(ws::PaneRowPart{
                    row->id, static_cast<std::int64_t>(said.lines.size()) - 1, 0, width});
            }
        }
        if (more > 0) {
            said.lines.push_back(surface::SurfaceTextRow{
                drawable("  ... " + std::to_string(more) + " more", columns),
                gfx::role::kMuted});
        }
        (void)mail.as_role(ws::kPresenterRole).send_to_role(kWorkshopRole, said);
    }

    // NOT STATE, deliberately: how this image laid the menu out is its own, and a successor that
    // carries the menu lays it out afresh (and numbers its pictures afresh) in its own way.
    ws::MenuPictureTracker pictures_;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(MenuPresenter)
