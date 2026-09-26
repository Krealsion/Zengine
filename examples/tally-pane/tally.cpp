// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Tally: a small Workshop pane you can open, change, and reload while it runs, keeping its count
// in its weave state across a reload in place. Walkthrough: docs/workshop/edit-a-running-pane.md
// One source file using only headers the installed Zengine and Loom packages publish, so a
// single-source recipe builds it with these links:
//     zengine::pane, zengine::activation, zengine::input, loom::switchboard
// and a load-plan row loads it under the role "example.tally" (kOffice, below).

#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
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

// ---- CHANGE THIS FIRST ------------------------------------------------------------------------
//
// The rows this pane says for a count. A new word here is a change a reload in place carries,
// because it changes the code and not the state's shape.
std::vector<surface::SurfaceTextRow> rows_for(std::int64_t count) {
    return {
        surface::SurfaceTextRow{"Tally: " + std::to_string(count), surface::role::kAccent},
        surface::SurfaceTextRow{"press into me, then Space adds one", surface::role::kMuted},
    };
}

// The office this weave speaks as. It must be the role its load-plan row gives it: Workshop
// knows a pane by the office that offered it, and a weave can only speak as an office it holds.
// If the pane never appears in the Pane Manager, compare this string with the role in the plan.
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

// Its whole conversation with Workshop, in the order it happens: it OFFERS one pane when it first
// breathes (or when Workshop asks who has panes) and DECLARES one action beside it; Workshop GRANTS
// the pane a room of rows and columns, and the pane answers with rows; once a maker has pressed
// into the pane, Space reaches it as that action, by name.
class Tally : public loom::WeaveBase<Tally, TallyState,
                                     loom::Accept<loom::Activated, ws::PaneCatalogRequested,
                                                  ws::PaneRoom, ws::PaneActionRequested>,
                                     loom::Emit<ws::PaneOffered, ws::PaneActions,
                                                ws::PaneContent>> {
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

    void on(const ws::PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || room.pane != kPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        show(mail);
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

    // The rows for the count, fitted to the room Workshop granted: Workshop refuses a row wider
    // than the room rather than cutting it, and a room holds only so many rows.
    void show(loom::Mail& mail) {
        ws::PaneContent said;
        said.pane = kPane;
        for (surface::SurfaceTextRow row : rows_for(state_.count)) {
            if (static_cast<std::int64_t>(said.rows.size()) >= rows_ || columns_ <= 0) {
                break;
            }
            if (static_cast<std::int64_t>(row.text.size()) > columns_) {
                row.text.resize(static_cast<std::size_t>(columns_));
            }
            said.rows.push_back(row);
        }
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, said);
    }

    std::int64_t rows_ = 0;    // the room Workshop last granted; not state, so a reload re-asks
    std::int64_t columns_ = 0;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Tally)
