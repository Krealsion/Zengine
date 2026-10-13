// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The compile-negative fixture for the drawing weave and the canvas: one source, three targets
// (ZENGINE_CN_CASE), so each refusal and its control are the same drawer but for the thing under
// test (VM-WALL-04). 1 changes an element by a type outside its code's list of changes; 2 defines
// its own end-of-delivery hook, which would leave its drawing unshown; 3, the control, does
// neither.

#include "graphics/drawing.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <tuple>

namespace {

struct CnDrawerState {
    std::int64_t beats = 0;
    ZEN_SHAPE(CnDrawerState, 1, ZEN_FIELD(beats));
};

struct CnBeat {
    ZEN_SHAPE(CnBeat, 1);
};

struct CnStep {
    std::int64_t to = 0;
    ZEN_SHAPE(CnStep, 1, ZEN_FIELD(to));
};

struct CnReset {
    ZEN_SHAPE(CnReset, 1);
};

/// The changes an element's code takes: a change travels as its index here.
using CnChanges = std::tuple<CnStep>;

struct CnDrawer;
using CnBase = zengine::graphics::DrawingWeave<
    CnDrawer, CnDrawerState, loom::Accept<CnBeat>,
    loom::Emit<zengine::graphics::ElementPut, zengine::graphics::ElementChange>>;

#if ZENGINE_CN_CASE == 1 // ---- a change outside the list: MUST FAIL ---------------

struct CnDrawer : CnBase {
    using DrawingWeave::on;
    CnDrawer() : DrawingWeave("cn.drawer") {}
    void on(const CnBeat&, loom::Mail&) { canvas().change<CnChanges>(1, CnReset{}); }
};

#elif ZENGINE_CN_CASE == 2 // ---- its own end-of-delivery hook: MUST FAIL ---------

struct CnDrawer : CnBase {
    using DrawingWeave::on;
    CnDrawer() : DrawingWeave("cn.drawer") {}
    void on(const CnBeat&, loom::Mail&) { canvas().change<CnChanges>(1, CnStep{}); }
    /// THE REPLACING HOOK: Loom would call this one, never the drawing weave's.
    void after_delivery(loom::Mail&) { ++state_.beats; }
};

#elif ZENGINE_CN_CASE == 3 // ---- the positive control: MUST COMPILE --------------

struct CnDrawer : CnBase {
    using DrawingWeave::on;
    CnDrawer() : DrawingWeave("cn.drawer") {}
    void on(const CnBeat&, loom::Mail&) {
        ++state_.beats;
        canvas().change<CnChanges>(1, CnStep{});
    }
    void picture(zengine::graphics::Sketch& f) const {
        f.text("beats", 0, 0, "beats", zengine::graphics::by_role(zengine::graphics::role::kFill));
    }
};

#else
#error "ZENGINE_CN_CASE must be 1 (change_outside), 2 (after_delivery) or 3 (drawer_ok)"
#endif

} // namespace

// Instantiating it is what runs the checks: the hook's wall is anchored in the drawing weave's
// constructor, and the change's where the handler that sends it is compiled.
int main() {
    CnDrawer d;
    (void)d;
    return 0;
}
