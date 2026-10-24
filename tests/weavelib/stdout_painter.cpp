// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A weave that writes to its standard output when asked: the snake suite's negative control, the
// paint a stdout meter must catch from a loaded library before its zero means anything. A fixture
// loaded only by that suite, through the real ABI and Kernel; it writes nothing but stdout.

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <cstdio>
#include <string>

namespace {

/// What to write, said to the painter.
struct PaintStdout {
    std::string text;
    ZEN_SHAPE(PaintStdout, 1, ZEN_FIELD(text));
};

struct StdoutPainterState {
    std::int64_t painted = 0;
    ZEN_SHAPE(StdoutPainterState, 1, ZEN_FIELD(painted));
};

class StdoutPainter : public loom::WeaveBase<StdoutPainter, StdoutPainterState,
                                             loom::Accept<PaintStdout>, loom::Emit<>> {
public:
    void on(const PaintStdout& p, loom::Mail&) {
        ++state_.painted;
        std::fputs(p.text.c_str(), stdout);
        std::fflush(stdout);
    }
};

} // namespace

ZEN_EXPORT_WEAVE(StdoutPainter)
