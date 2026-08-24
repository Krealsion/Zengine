// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
//
// EVERY EXPORTED TARGET AND EVERY INSTALLED PUBLIC HEADER, USED FROM OUTSIDE.
//
// The oven proves one capability end to end. This proves the other seven exist as more than
// a line in an export set: it includes all twenty-four installed headers, links all eight
// exported targets, and does something real with each -- so a header that quietly stopped
// being self-contained, a target that lost a dependency it needed, or a package installed
// with a piece missing fails HERE rather than for the first stranger who reaches for it.
//
// Every include below is spelled exactly as this project's documentation spells it, which is
// the same spelling its own tree uses. That identity is the point of the install layout.

#include "activation/activation.hpp"

#include "component/text_box.hpp"

#include "input/input_weave.hpp"
#include "input/translate.hpp"
#include "input/vocabulary.hpp"

#include "operator/catalog.hpp"
#include "operator/host.hpp"
#include "operator/host_abi.h"
#include "operator/host_surface.hpp"
#include "operator/image.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"
#include "operator/provider.hpp"
#include "operator/provider_abi.h"
#include "operator/provider_host.hpp"

#include "surface/cells.hpp"
#include "surface/pointing.hpp"
#include "surface/region.hpp"
#include "surface/terminal_size.hpp"
#include "surface/vocabulary.hpp"

#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <cstdio>
#include <string>
#include <variant>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (ok) {
        return;
    }
    std::fprintf(stderr, "  FAILED: %s\n", what);
    ++failures;
}

// ---- zengine::activation ---------------------------------------------------------------
void activation_surface() {
    zengine::ActivationCursor cursor;
    // There is no bus here, so what this asks is that the cursor exists, compiles against the
    // installed header, and starts in the honest state: nothing has activated it.
    check(!cursor.activated(), "a fresh ActivationCursor has accepted nothing");
}

// ---- zengine::timer --------------------------------------------------------------------
void timer_surface() {
    namespace timer = zengine::timer;
    const timer::StartTimer order{"kitchen.oven.bake", 40, /*repeat=*/false};
    check(order.id == "kitchen.oven.bake", "a StartTimer carries the id it was given");
    check(order.delay_ms == 40, "and the delay it was given");
    check(std::string(timer::kTimerRole) == "zengine.timer", "the Timer's role has a name");

    const timer::TimerFired fired{"kitchen.oven.bake"};
    check(fired.id == order.id, "a firing is matched to its order by id");
}

// ---- zengine::surface ------------------------------------------------------------------
void surface_surface() {
    namespace sf = zengine::surface;

    sf::SurfaceTextRegion region;
    region.x = 2;
    region.y = 1;
    region.w = 10;
    region.h = 2;
    region.rows.push_back(sf::SurfaceTextRow{"hello"});
    region.rows.push_back(sf::SurfaceTextRow{"stranger"});

    sf::SurfaceLayer layer;
    layer.texts.push_back(region);
    const std::vector<sf::ProjectedRow> rows = sf::project_text_regions(layer);
    check(rows.size() == 2, "a two-row region projects two cell rows");

    const sf::CellSpan span = sf::clip_span(/*origin=*/8, /*length=*/5, /*extent=*/10);
    check(span.count() == 2, "a span running past the edge is clipped to the edge");

    const sf::CanvasPoint p = sf::canvas_of_terminal_cells(4, 5);
    check(p.y == 5 - sf::kTuiCanvasTopRow, "a terminal row is a canvas row, minus the slots");

    // Answers on every platform, including one with no terminal on the other end.
    const sf::TerminalSize size = sf::native_terminal_size();
    check(size.cols >= 0 && size.rows >= 0, "a terminal size is never negative");
}

// ---- zengine::input --------------------------------------------------------------------
void input_surface() {
    namespace in = zengine::input;
    check(in::scancode_name(in::scan::kQ) == "Q", "a scancode has a courtesy name");

    const unsigned char bytes[] = {'q'};
    const std::vector<in::InputEvent> events = in::terminal_bytes_to_events(bytes, 1);
    check(!events.empty(), "one printable byte produces input");
    bool pressed_q = false;
    for (const in::InputEvent& e : events) {
        if (const in::KeyPressed* k = std::get_if<in::KeyPressed>(&e)) {
            pressed_q = pressed_q || k->scancode == in::scan::kQ;
        }
    }
    check(pressed_q, "and it carries the scancode the byte names");
}

// ---- zengine::ui -----------------------------------------------------------------------
void ui_surface() {
    namespace ui = zengine::ui;
    ui::Element panel;
    panel.id = 1;
    panel.label = "panel";
    panel.context = ui::kRootContext;
    panel.x = 2;
    panel.y = 1;
    panel.width = ui::Extent{ui::kExtentCells, 8};
    panel.height = ui::Extent{ui::kExtentCells, 3};

    const std::vector<ui::Element> authored{panel};
    const ui::Scene scene = ui::resolve(authored, ui::Viewport{40, 12});
    check(scene.items.size() == 1, "one authored element resolves to one placed rectangle");

    const ui::Placed* under = ui::hit(scene, 3, 2);
    check(under != nullptr && under->id == 1, "a cell inside it hits the element that owns it");
    check(ui::hit(scene, 30, 10) == nullptr, "and a cell outside it hits nothing");
}

// ---- zengine::component ----------------------------------------------------------------
void component_surface() {
    zengine::component::TextBox box;
    box.type("sourdough");
    check(box.text() == "sourdough", "a TextBox holds what was typed into it");
    check(box.at_end(), "and the caret follows the typing");
    box.backspace();
    check(box.text() == "sourdoug", "backspace removes one character");
    box.home();
    check(box.caret() == 0, "and the caret can be sent back to the start");
}

// ---- zengine::operator / zengine::operator-consumer ------------------------------------
void operator_surface() {
    namespace op = zengine::op;
    op::Catalog catalog;
    op::publish_primitives(catalog);

    const op::OperatorDef* max = catalog.find(op::kMaxInt);
    check(max != nullptr, "the basic primitives publish math.max");
    if (max == nullptr) {
        return;
    }

    loom::Value args(max->inputs());
    args.set("lhs", loom::Cell::integer(3));
    args.set("rhs", loom::Cell::integer(9));
    const op::Evaluation answer = catalog.evaluate(op::kMaxInt, std::move(args));
    check(answer.ok(), "math.max evaluates against its own declared inputs");

    // The consumer half, whose whole claim is that it contains no catalog: an OperatorHost is
    // a handle onto somebody else's truth, and an unbound one has nothing to spend.
    op::OperatorHost host;
    check(!host.bound(), "an unbound OperatorHost has no host truth to spend");
}

} // namespace

int main() {
    std::printf("zengine public surface, as an installed package sees it\n");
    activation_surface();
    timer_surface();
    surface_surface();
    input_surface();
    ui_surface();
    component_surface();
    operator_surface();

    if (failures != 0) {
        std::fprintf(stderr, "public surface: %d check(s) failed\n", failures);
        return 1;
    }
    std::printf("public surface: eight exported targets used, twenty-four headers included\n");
    return 0;
}
