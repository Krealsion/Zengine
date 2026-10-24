// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `zengine::graphics` alone, as a stranger links it: an owner's picture painted for light by the
// installed painter, read back and pressed, and the terminal's ink, size and copy sequence from the
// installed claim -- with no other Zengine target on the link line, so a graphics header that needs
// another package stops this compiling.

#include "graphics/canvas.hpp"
#include "graphics/light_paint.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"
#include "graphics/window/terminal_io.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void check(bool ok, const char* what) {
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        std::exit(1);
    }
}

} // namespace

int main() {
    namespace gfx = zengine::graphics;
    std::printf("zengine::graphics alone, as an installed package sees it\n");

    gfx::Sketch picture;
    picture.fill_rect("plate", 0, 0, 48, 24, gfx::by_role(gfx::role::kAccent));
    picture.text("label", 4, 6, "soup", gfx::exact(gfx::role::kFill, 0x112233));
    gfx::PaintTask task;
    std::int64_t rank = 1;
    for (const gfx::Node& n : picture.nodes()) {
        task.nodes.push_back(gfx::PaintNode{n.path, {rank++}, n.op, n.args});
    }
    const gfx::PaintResult painted = gfx::light::paint_task(task, gfx::light::day_look());
    check(painted.nodes.size() == 2, "the painter for light paints each node");
    const auto plate = gfx::light::unpack_node(painted.nodes[0].cells);
    const auto label = gfx::light::unpack_node(painted.nodes[1].cells);
    check(plate && plate->quads.size() == 1 && plate->quads[0].w == 48,
          "a rectangle is one quad, in the area's pixels");
    check(label && label->texts.size() == 1 && label->texts[0].text == "soup" &&
              label->texts[0].rgba == gfx::light::rgba_of(0x112233),
          "a text is one run, in its exact colour");
    const gfx::TextMeasure face{8, 18};
    check(gfx::light::hits_node(*plate, 10, 10, face) &&
              !gfx::light::hits_node(*plate, 60, 10, face),
          "a point lands on what was painted, and nowhere else");
    check(gfx::light::hits_node(*label, label->texts[0].x + 4 * face.advance - 1,
                                label->texts[0].y, face) &&
              !gfx::light::hits_node(*label, label->texts[0].x + 4 * face.advance,
                                     label->texts[0].y, face),
          "a run of text is pressed over its characters, a character an advance of the face");
    check(!gfx::light::unpack_node(loom::Bytes{1, 2, 3}).has_value(),
          "bytes a character painter wrote are not read as light");

    check(std::string(gfx::window::sgr_for_role(static_cast<int>(gfx::role::kAccent))) ==
              "\x1b[36m",
          "the terminal's ink for a role comes with the graphics package");
    const gfx::window::TerminalSize size = gfx::window::native_terminal_size();
    check(size.cols >= 0 && size.rows >= 0, "a terminal's size is asked, and never negative");
    check(gfx::window::terminal_clipboard_sequence("soup") == "\x1b]52;c;c291cA==\x07",
          "a copy is the terminal's OSC 52 sequence, its text in base64");
    return 0;
}
