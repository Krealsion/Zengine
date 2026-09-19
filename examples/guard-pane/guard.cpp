// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Guard: a small Workshop pane that BLOCKS while the right mouse button is held on it -- the
// game-like consumer of the second button. It shows how a pane takes a right press as ordinary
// input and ends the interaction there: no menu opens, nothing is handed back, and Workshop's own
// pane menu is reached through the Pane Manager's row or the pane's chrome instead.
// Walkthrough: docs/workshop/panes.md ("The second button").
//
// It is one source file using only headers the installed Zengine and Loom packages publish, so a
// single-source recipe builds it with these links:
//     zengine::pane, zengine::activation, zengine::input, loom::switchboard
// and a load-plan row loads it under the role "example.guard" (kOffice, below).

#include "workshop/pane_menu.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
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

// ---- CHANGE THIS FIRST ------------------------------------------------------------------------
//
// What the pane says in each of its three states: guard down, guard up, and a guard the host had
// to drop for it (the pane closed, or the button was pressed again with no release between).
std::vector<surface::SurfaceTextRow> rows_for(const ws::pane_menu::HeldButton& guard,
                                              std::int64_t blocks) {
    const std::string first = guard.held ? "GUARD UP -- holding (right button down)"
                              : guard.lost ? "guard dropped -- the host could not keep the hold"
                                           : "guard down";
    return {
        surface::SurfaceTextRow{first, guard.held ? surface::role::kAccent
                                                  : surface::role::kMuted},
        surface::SurfaceTextRow{"blocks so far: " + std::to_string(blocks),
                                surface::role::kFill},
        surface::SurfaceTextRow{"hold the right button in me; no menu opens here",
                                surface::role::kMuted},
    };
}

// The office this weave speaks as. It must be the role its load-plan row gives it: Workshop
// knows a pane by the office that offered it, and a weave can only speak as an office it holds.
constexpr const char* kOffice = "example.guard";
constexpr const char* kPane = "guard";

// What a reload in place carries across: how many times the guard went up. THE HOLD ITSELF IS
// NOT HERE, deliberately -- a hold is a fact about a hand and a button this image heard about,
// and a successor that inherited one would claim a hand it never saw. A reloaded image starts
// with the guard down; the release that follows, if any, reaches whoever holds the office then
// and is a release of a button this image does not hold, so `HeldButton::take` ignores it.
struct GuardState {
    std::int64_t blocks = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(GuardState, 1, ZEN_FIELD(blocks));
};

// Its whole conversation with Workshop: it OFFERS one pane when it first breathes (or when
// Workshop asks who has panes); Workshop GRANTS it a room and the pane answers with rows; and
// because its accept set has the `PaneButton` door, a right press in its body is DELIVERED to it
// rather than opening Workshop's menu -- the press, then the release wherever the hand let go,
// or a `lost` release when no hand could.
class Guard : public loom::WeaveBase<Guard, GuardState,
                                     loom::Accept<loom::Activated, ws::PaneCatalogRequested,
                                                  ws::PaneRoom, ws::PaneButton>,
                                     loom::Emit<ws::PaneOffered, ws::PaneContent>> {
public:
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (activation_.accept(mail, activated)) {
            offer(mail);
        }
    }

    void on(const ws::PaneCatalogRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(ws::pane_menu::kWorkshopRole)) {
            offer(mail);
        }
    }

    void on(const ws::PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::pane_menu::kWorkshopRole) || room.pane != kPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        show(mail);
    }

    // THE SECOND BUTTON, CONSUMED: a right press raises the guard, its release lowers it, and a
    // `lost` release lowers it and says why. The middle button means nothing here. Nothing is
    // handed back and no menu is asked for -- delivery was the whole disposition.
    void on(const ws::PaneButton& b, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::pane_menu::kWorkshopRole) || b.pane != kPane ||
            b.button != 3) {
            return;
        }
        if (!guard_.take(b)) {
            return; // a release of a button this image does not hold: nothing to do
        }
        if (guard_.held) {
            ++state_.blocks;
        }
        show(mail);
    }

private:
    void offer(loom::Mail& mail) {
        (void)mail.as_role(kOffice).send_to_role(
            ws::pane_menu::kWorkshopRole,
            ws::PaneOffered{kPane, "Guard", "blocks while the right button is held"});
    }

    void show(loom::Mail& mail) {
        ws::PaneContent said;
        said.pane = kPane;
        for (surface::SurfaceTextRow row : rows_for(guard_, state_.blocks)) {
            if (static_cast<std::int64_t>(said.rows.size()) >= rows_ || columns_ <= 0) {
                break;
            }
            if (static_cast<std::int64_t>(row.text.size()) > columns_) {
                row.text.resize(static_cast<std::size_t>(columns_));
            }
            said.rows.push_back(row);
        }
        (void)mail.as_role(kOffice).send_to_role(ws::pane_menu::kWorkshopRole, said);
    }

    std::int64_t rows_ = 0; // the room Workshop last granted; not state, so a reload re-asks
    std::int64_t columns_ = 0;
    ws::pane_menu::HeldButton guard_; // not state either: see GuardState
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Guard)
