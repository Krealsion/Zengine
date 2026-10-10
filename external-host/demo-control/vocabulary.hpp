// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_DEMO_CONTROL_VOCABULARY_HPP
#define ZENGINE_DEMO_CONTROL_VOCABULARY_HPP
#include <zen/weave/shape.hpp>
#include <cstdint>
#include <string>
namespace zengine::demo {
inline constexpr const char* kRole = "zengine.demo";
struct DemoServiceOpened {
    std::string name;
    ZEN_SHAPE(DemoServiceOpened, 1, ZEN_FIELD(name));
};
struct DemoServiceClosed { ZEN_SHAPE(DemoServiceClosed, 1); };
struct DemoWorkRequested { ZEN_SHAPE(DemoWorkRequested, 1); };
struct DemoWork {
    std::int64_t generation = 0;
    ZEN_SHAPE(DemoWork, 1, ZEN_FIELD(generation));
};
struct DemoWorkFinished {
    std::int64_t generation = 0;
    bool passed = false;
    std::string note;
    ZEN_SHAPE(DemoWorkFinished, 1, ZEN_FIELD(generation), ZEN_FIELD(passed), ZEN_FIELD(note));
};
struct DemoResetRequested { ZEN_SHAPE(DemoResetRequested, 1); };
struct DemoStatusRequested { ZEN_SHAPE(DemoStatusRequested, 1); };
struct DemoReadyRequested {
    std::int64_t generation = 0;
    ZEN_SHAPE(DemoReadyRequested, 1, ZEN_FIELD(generation));
};
struct DemoStatus {
    std::string name, state, note;
    std::int64_t generation = 0;
    ZEN_SHAPE(DemoStatus, 1, ZEN_FIELD(name), ZEN_FIELD(state), ZEN_FIELD(note), ZEN_FIELD(generation));
};
} // namespace zengine::demo
#endif
