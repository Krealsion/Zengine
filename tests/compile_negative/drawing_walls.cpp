// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The compile-negative fixture for the drawing weave and colours by name: one source, four targets
// (ZENGINE_CN_CASE), so each refusal and its control are the same drawer but for the thing under
// test (VM-WALL-04). 2 defines its own end-of-delivery hook; 4 names a pool colour nobody declared;
// 5 names a colour by a role; 3, the control, does none of these.

#include "graphics/drawing.hpp"

#include <zen/weave.hpp>

#include <cstdint>

namespace {

struct CnDrawerState {
    std::int64_t beats = 0;
    ZEN_SHAPE(CnDrawerState, 1, ZEN_FIELD(beats));
};

struct CnBeat {
    ZEN_SHAPE(CnBeat, 1);
};

struct CnDrawer;
using CnBase = zengine::graphics::DrawingWeave<CnDrawer, CnDrawerState, loom::Accept<CnBeat>>;

#if ZENGINE_CN_CASE == 2 // ---- its own end-of-delivery hook: MUST FAIL ---------

struct CnDrawer : CnBase {
    using DrawingWeave::on;
    CnDrawer() : DrawingWeave("cn.drawer") {}
    void on(const CnBeat&, loom::Mail&) { ++state_.beats; }
    /// THE REPLACING HOOK: Loom would call this one, never the drawing weave's.
    void after_delivery(loom::Mail&) { ++state_.beats; }
};

#elif ZENGINE_CN_CASE == 3 // ---- the positive control: MUST COMPILE --------------

struct CnDrawer : CnBase {
    using DrawingWeave::on;
    CnDrawer() : DrawingWeave("cn.drawer") {}
    void on(const CnBeat&, loom::Mail&) { ++state_.beats; }
    void picture(zengine::graphics::Sketch& f) const {
        f.text("beats", 0, 0, "beats", zengine::graphics::colors::accent);
    }
};

#elif ZENGINE_CN_CASE == 4 // ---- a pool colour nobody declared: MUST FAIL --------

struct CnDrawer : CnBase {
    using DrawingWeave::on;
    CnDrawer() : DrawingWeave("cn.drawer") {}
    void on(const CnBeat&, loom::Mail&) { ++state_.beats; }
    void picture(zengine::graphics::Sketch& f) const {
        f.text("beats", 0, 0, "beats", zengine::graphics::colors::acent);
    }
};

#elif ZENGINE_CN_CASE == 5 // ---- a colour by a role: MUST FAIL -----------------------

struct CnDrawer : CnBase {
    using DrawingWeave::on;
    CnDrawer() : DrawingWeave("cn.drawer") {}
    void on(const CnBeat&, loom::Mail&) { ++state_.beats; }
    void picture(zengine::graphics::Sketch& f) const {
        f.text("beats", 0, 0, "beats",
               zengine::graphics::by_role(zengine::graphics::role::kAccent));
    }
};

#else
#error "ZENGINE_CN_CASE must be 2 (after_delivery), 3 (drawer_ok), 4 or 5"
#endif

} // namespace

// Instantiating it is what runs the checks: the hook's wall is anchored in the drawing weave's
// constructor.
int main() {
    CnDrawer d;
    (void)d;
    return 0;
}
