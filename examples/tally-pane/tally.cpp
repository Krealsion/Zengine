// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Tally: a small Workshop pane you can open, change, and reload while it runs, its count kept in
// its weave state across a reload in place; it draws its own picture -- shapes, colour, a button
// where it puts it, its words as rows inside -- in its office's area of the window that shows it.
// Walkthrough: builder/docs/edit-a-running-pane.md. One source file of the installed packages'
// headers: a single-source recipe links zengine::pane, zengine::graphics, zengine::activation,
// zengine::input and loom::switchboard; a load-plan row loads it as "example.tally" (kOffice).

#include "workshop/pane_drawing.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_rows_block.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace ws = zengine::workshop;
namespace gfx = zengine::graphics;

// ---- CHANGE THIS FIRST ------------------------------------------------------------------------
//
// The words this pane says for a count. A new word here is a change a reload in place carries,
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

// The picture's own colours, each with the role a window that cannot show it paints instead.
const gfx::Paint kCard = gfx::exact(gfx::role::kMediumGround, 0x24324A);
const gfx::Paint kBadge = gfx::exact(gfx::role::kAccent, 0xF2A541);
const gfx::Paint kButton = gfx::exact(gfx::role::kAccent, 0x2E7D5B);
const gfx::Paint kButtonWords = gfx::exact(gfx::role::kFill, 0xFFFFFF);

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
// window that shows its area tells it the ROOM there, and the pane draws its picture in it; once a
// weaver has pressed into the pane, Space reaches it from Workshop as that action, by name, and a
// press on its button reaches it from the window that shows it.
class Tally : public loom::WeaveBase<
                  Tally, TallyState,
                  loom::Accept<loom::Activated, ws::PaneCatalogRequested, gfx::AreaRoom,
                               gfx::DrawnRefused, gfx::AreaButtonChanged, gfx::ActionRequested,
                               ws::PaneActionRequested>,
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

    // The room the window gives the area: the picture is drawn there whole.
    void on(const gfx::AreaRoom& room, loom::Mail& mail) {
        if (window_.room(room, mail)) {
            show(mail);
        }
    }

    // A drawing the window refused leaves what it shows as it was.
    void on(const gfx::DrawnRefused& refused, loom::Mail& mail) {
        (void)window_.refused(refused, mail);
    }

    // A PRESS ON THE BUTTON, given by the window that shows the area: the first button pressed
    // anywhere on its face adds one. A press the window landed on another element than the newest
    // picture shows there (`moved`) acts on nothing.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        const std::optional<ws::WindowPointer> p = window_.pointer(b, mail, 0, 0);
        if (!p || p->phase != ws::canvas_pointer::kPress || p->button != 1 || p->moved) {
            return;
        }
        if (window_.part_at(p->x, p->y) == kAddOne) {
            add(mail);
        }
    }

    // ...or the button asked by name, as a window asks a part's action.
    void on(const gfx::ActionRequested& asked, loom::Mail& mail) {
        if (window_.from_window(asked.area, mail) && asked.action == kAddOne) {
            add(mail);
        }
    }

    void on(const ws::PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || asked.pane != kPane) {
            return;
        }
        if (asked.id == kAddOne) {
            add(mail);
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

    void add(loom::Mail& mail) {
        ++state_.count;
        show(mail);
    }

    // THE PICTURE: a card, a badge beside the count, the words as rows inside the card, and the
    // button to the right of it -- each placed in the window's pixels, a line of its text tall.
    void show(loom::Mail& mail) {
        const gfx::TextMeasure& text = window_.text();
        const std::int64_t width = window_.width(), height = window_.height();
        if (width <= 0 || height <= 0 || text.advance <= 0 || text.line <= 0) {
            return;
        }
        const std::int64_t pad = text.line / 2;
        const std::int64_t button_w = 6 * text.advance, button_h = 2 * text.line;
        const std::int64_t card_w =
            (std::max)(std::int64_t{0}, (std::min)(width - 3 * pad - button_w, 40 * text.advance));
        const std::int64_t card_h = (std::min)(height - 2 * pad, 3 * text.line);
        gfx::Sketch sketch;
        sketch.fill_rect("room", 0, 0, width, height, gfx::by_role(gfx::role::kGround));
        sketch.fill_rect("card", pad, pad, card_w, card_h, kCard);
        sketch.ellipse("badge", pad + text.advance / 2, pad + text.line / 4, text.advance,
                       text.line / 2, kBadge);

        // THE WORDS, AS ROWS INSIDE THE PICTURE (workshop/pane_rows_block.hpp), standing on the
        // card: a row a line of the window's text, cut to the card's width.
        ws::RowsBlock<int> words;
        words.begin(card_h / text.line, (card_w - 2 * text.advance) / text.advance);
        for (ws::TextRow row : rows_for(state_.count)) {
            const std::int64_t fits = (std::max)(std::int64_t{0}, words.columns());
            if (static_cast<std::int64_t>(row.text.size()) > fits) {
                row.text.resize(static_cast<std::size_t>(fits));
            }
            (void)words.push(std::move(row));
        }
        const bool cells = window_.rows_room().cells;
        (void)words.draw(
            sketch, "words", pad + 2 * text.advance, pad, text, cells,
            [](int) { return std::string(); }, [](int) { return true; }, false, false);

        // THE BUTTON, where this picture puts it: its face and its words both ask `tally.add`.
        const std::int64_t bx = pad + card_w + pad, by = pad;
        sketch.fill_rect("add", bx, by, button_w, button_h, kButton);
        sketch.action("add", kAddOne);
        sketch.text("add-words", bx + 2 * text.advance, by + text.line / 2, "+1", kButtonWords);
        sketch.action("add-words", kAddOne);
        window_.draw(sketch, mail);
    }

    ws::WindowDrawing window_{kOffice, ws::pane_area(kOffice, kPane)}; // told its room afresh
    bool asked_ = false;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Tally)
