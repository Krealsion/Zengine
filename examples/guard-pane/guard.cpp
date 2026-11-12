// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// Guard: a small Workshop pane that blocks while the right mouse button is held on it -- a right
// press that is ordinary input, opening no menu. It draws its rows in its pane's area of the
// graphics window that shows it and hears the pointer there. Walkthrough: workshop/docs/panes.md
// ("The second button"). One source file: a single-source recipe links zengine::pane,
// zengine::graphics, zengine::activation, zengine::input and loom::switchboard, and a load-plan
// row loads it under the role "example.guard" (kRole, below).

#include "pane/drawing.hpp"
#include "pane/menu.hpp" // Workshop's role
#include "pane/rows.hpp"
#include "pane/vocabulary.hpp"

#include "activation/activation.hpp"
#include "graphics/colors.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace gfx = zengine::graphics;

// The role this weave speaks as. It must be the role its load-plan row gives it: Workshop
// knows a pane by the role that offered it, and a weave can only speak as a role it holds.
// Its one pane, kPane, draws in the area `<role>.<pane>`: "example.guard.guard".
constexpr const char* kRole = "example.guard";
constexpr const char* kPane = "guard";

// THE HOLD: the right press this image heard go down, by the window's number for it, and
// whether the window ended a hold it could not keep.
struct Hold {
    std::uint64_t press = 0;
    bool lost = false;
    bool held() const { return press != 0; }
};

// ---- CHANGE THIS FIRST ------------------------------------------------------------------------
//
// What the pane says in each of its three states: guard down, guard up, and a guard the window
// had to drop for it (the pane closed, or the hand could not be kept).
std::vector<zengine::pane::TextRow> rows_for(const Hold& guard, std::int64_t blocks) {
    const std::string first = guard.held() ? "GUARD UP -- holding (right button down)"
                              : guard.lost ? "guard dropped -- the host could not keep the hold"
                                           : "guard down";
    return {
        zengine::pane::TextRow{first, guard.held() ? gfx::colors::accent : gfx::colors::quiet},
        zengine::pane::TextRow{"blocks so far: " + std::to_string(blocks), gfx::colors::text},
        zengine::pane::TextRow{"hold the right button in me; no menu opens here", gfx::colors::quiet},
    };
}

// What a reload in place carries across: how many times the guard went up. THE HOLD ITSELF IS
// NOT HERE, deliberately -- a hold is a fact about a hand and a button this image heard about,
// and a successor that inherited one would claim a hand it never saw. A reloaded image starts
// with the guard down; the release that follows, if any, reaches whoever holds the role then
// and is the release of a press this image never heard, so it is ignored.
struct GuardState {
    std::int64_t blocks = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(GuardState, 1, ZEN_FIELD(blocks));
};

// Its whole conversation: it OFFERS one pane to Workshop when it first breathes (or when Workshop
// asks who has panes); the graphics window that shows its area tells it the ROOM there, and the
// pane DRAWS its rows in it; a right press in the area is DELIVERED to it by that window -- the
// press, then the release wherever the hand let go, or the end of a hold the window could not
// keep -- and since it passes nothing back, no menu opens.
class Guard : public loom::WeaveBase<
                  Guard, GuardState,
                  loom::Accept<loom::Activated, zengine::pane::PaneCatalogRequested, gfx::AreaRoom,
                               gfx::DrawnRefused, gfx::AreaButtonChanged, gfx::AreaHoldEnded>,
                  loom::Emit<zengine::pane::PaneOffered, gfx::Drawn, gfx::RoomAsked>> {
public:
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (activation_.accept(mail, activated)) {
            offer(mail);
        }
    }

    void on(const zengine::pane::PaneCatalogRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(zengine::pane::pane_menu::kWorkshopRole)) {
            offer(mail);
        }
    }

    // THE ROOM THE WINDOW GIVES THE AREA: the rows are drawn there whole.
    void on(const gfx::AreaRoom& room, loom::Mail& mail) {
        if (window_.room(room, mail)) {
            show(mail);
        }
    }

    // A drawing the window refused leaves what it shows as it was.
    void on(const gfx::DrawnRefused& refused, loom::Mail& mail) {
        (void)window_.refused(refused, mail);
    }

    // THE SECOND BUTTON, CONSUMED: a right press raises the guard, its release lowers it, and a
    // hold the window ended lowers it and says why. The middle button means nothing here. Nothing
    // is handed back and no menu is asked for -- delivery was the whole disposition.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        const std::optional<zengine::pane::WindowPointer> p = window_.pointer(b, mail, 0, 0);
        if (!p || p->button != 3) {
            return;
        }
        if (p->phase == zengine::pane::canvas_pointer::kPress) {
            guard_ = Hold{p->press, false};
            ++state_.blocks;
        } else if (guard_.held() && p->press == guard_.press) {
            guard_ = Hold{};
        } else {
            return; // the release of a press this image never heard: nothing to do
        }
        show(mail);
    }

    void on(const gfx::AreaHoldEnded& ended, loom::Mail& mail) {
        const std::optional<zengine::pane::WindowPointer> p = window_.pointer(ended, mail);
        if (!p || p->button != 3 || !guard_.held()) {
            return;
        }
        guard_ = Hold{0, true};
        show(mail);
    }

    // The room asked once in each life while no window has said it: a guard reloaded into a
    // window draws there again.
    void after_delivery(loom::Mail& mail) {
        if (!asked_ && !window_.shown()) {
            asked_ = true;
            (void)mail.as_role(kRole).send_to_role(gfx::kGraphicsRole,
                                                     gfx::RoomAsked{window_.area()});
        }
    }

private:
    void offer(loom::Mail& mail) {
        (void)mail.as_role(kRole).send_to_role(
            zengine::pane::pane_menu::kWorkshopRole,
            zengine::pane::PaneOffered{kPane, "Guard", "blocks while the right button is held"});
    }

    // THE ROWS, DRAWN in the area: as many as its room holds, each cut to its width.
    void show(loom::Mail& mail) {
        const zengine::pane::RowsRoom room = window_.rows_room();
        if (room.rows() <= 0 || room.columns() <= 0) {
            return;
        }
        std::vector<zengine::pane::TextRow> rows = rows_for(guard_, state_.blocks);
        for (zengine::pane::TextRow& row : rows) {
            if (static_cast<std::int64_t>(row.text.size()) > room.columns()) {
                row.text.resize(static_cast<std::size_t>(room.columns()));
            }
        }
        const zengine::pane::RowsDrawing drawn = zengine::pane::rows_drawing(rows,
                                                                             std::vector<zengine::pane::RowPart>{}, room,
                                                       room.columns(), {}, room.rows());
        window_.draw(drawn.sketch, mail);
    }

    zengine::pane::WindowDrawing window_{kRole, zengine::pane::pane_area(kRole, kPane)}; // told its room afresh
    Hold guard_;                                 // not state either: see GuardState
    bool asked_ = false;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Guard)
