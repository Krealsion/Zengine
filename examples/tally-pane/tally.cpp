// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Tally: a small Workshop pane you can open, change, and reload while it runs, its count kept in
// its weave state across a reload in place; it draws its rows in its office's area of the graphics
// window that shows it. Walkthrough: builder/docs/edit-a-running-pane.md. One source file of the
// installed packages' headers: a single-source recipe links zengine::pane, zengine::graphics,
// zengine::activation, zengine::input and loom::switchboard, and a load-plan row loads it under
// the role "example.tally" (kOffice, below).

#include "workshop/pane_drawing.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
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

// ---- CHANGE THIS FIRST ------------------------------------------------------------------------
//
// The rows this pane says for a count. A new word here is a change a reload in place carries,
// because it changes the code and not the state's shape.
std::vector<ws::TextRow> rows_for(std::int64_t count) {
    return {
        ws::TextRow{"Tally: " + std::to_string(count), gfx::role::kAccent},
        ws::TextRow{"press into me, then Space adds one", gfx::role::kMuted},
    };
}

// The office this weave speaks as. It must be the role its load-plan row gives it: Workshop
// knows a pane by the office that offered it, and a weave can only speak as an office it holds.
// If the pane never appears in the Pane Manager, compare this string with the role in the plan.
// The office's one pane draws in the area of the same name.
constexpr const char* kOffice = "example.tally";

// The office Workshop holds. This pane answers Workshop and nobody else.
constexpr const char* kWorkshop = "zengine.workshop";

constexpr const char* kPane = "tally";
constexpr const char* kAddOne = "tally.add";

// What a reload in place carries across. Add, remove or retype a field and the next reload is
// refused before anything is replaced: the shape changed but kept its name and version. The
// running pane keeps working; put the shape back, rebuild, and reload again.
struct TallyState {
    std::int64_t count = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(TallyState, 1, ZEN_FIELD(count));
};

// Its whole conversation, in the order it happens: it OFFERS one pane to Workshop when it first
// breathes (or when Workshop asks who has panes) and DECLARES one action beside it; the graphics
// window that shows its area tells it the ROOM there, and the pane draws its rows in it; once a
// weaver has pressed into the pane, Space reaches it from Workshop as that action, by name.
class Tally : public loom::WeaveBase<
                  Tally, TallyState,
                  loom::Accept<loom::Activated, ws::PaneCatalogRequested, gfx::AreaRoom,
                               gfx::DrawnRefused, ws::PaneActionRequested>,
                  loom::Emit<ws::PaneOffered, ws::PaneActions, gfx::Drawn, gfx::RoomAsked>> {
public:
    // First breath, and again after every reload. The cursor refuses an activation that did
    // not come from the Loom, so another weave cannot make this pane announce itself.
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (activation_.accept(mail, activated)) {
            offer(mail);
        }
    }

    void on(const ws::PaneCatalogRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(kWorkshop)) {
            offer(mail);
        }
    }

    // The room the window gives the area: the rows are drawn there whole.
    void on(const gfx::AreaRoom& room, loom::Mail& mail) {
        if (window_.room(room, mail)) {
            show(mail);
        }
    }

    // A drawing the window refused leaves what it shows as it was.
    void on(const gfx::DrawnRefused& refused, loom::Mail& mail) {
        (void)window_.refused(refused, mail);
    }

    void on(const ws::PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || asked.pane != kPane) {
            return;
        }
        if (asked.id == kAddOne) {
            ++state_.count;
            show(mail);
        }
    }

    // The room asked once in each life while no window has said it: a reloaded Tally draws in
    // its window again.
    void after_delivery(loom::Mail& mail) {
        if (!asked_ && !window_.shown()) {
            asked_ = true;
            (void)mail.as_role(kOffice).send_to_role(gfx::kGraphicsRole,
                                                     gfx::RoomAsked{window_.area()});
        }
    }

private:
    void offer(loom::Mail& mail) {
        (void)mail.as_role(kOffice).send_to_role(
            kWorkshop, ws::PaneOffered{kPane, "Tally", "a count that survives a reload"});
        ws::PaneActions actions;
        actions.pane = kPane;
        actions.rows.push_back(ws::PaneActionRow{kAddOne, "add one", zengine::input::scan::kSpace,
                                                 zengine::input::mod::kNone});
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, actions);
    }

    // The rows for the count, drawn in the area: as many as its room holds, each cut to its
    // width.
    void show(loom::Mail& mail) {
        const ws::RowsRoom room = window_.rows_room();
        if (room.rows() <= 0 || room.columns() <= 0) {
            return;
        }
        std::vector<ws::TextRow> rows = rows_for(state_.count);
        for (ws::TextRow& row : rows) {
            if (static_cast<std::int64_t>(row.text.size()) > room.columns()) {
                row.text.resize(static_cast<std::size_t>(room.columns()));
            }
        }
        const ws::RowsDrawing drawn = ws::rows_drawing(rows, std::vector<ws::RowPart>{}, room,
                                                       room.columns(), {}, room.rows());
        window_.draw(drawn.sketch, mail);
    }

    ws::WindowDrawing window_{kOffice, kOffice}; // not state: a reload is told its room afresh
    bool asked_ = false;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Tally)
