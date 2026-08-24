// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
//
// The stranger's own vocabulary. Two shapes, both belonging to the consuming project rather
// than to Zengine -- which is the point: a guest brings its own nouns and borrows a service.

#ifndef KITCHEN_HPP
#define KITCHEN_HPP

#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace kitchen {

/// "Put this in the oven for `minutes` minutes."
struct BakeOrder {
    std::string dish;
    std::int64_t minutes = 0;
    ZEN_SHAPE(BakeOrder, 1, ZEN_FIELD(dish), ZEN_FIELD(minutes));
};

/// "It is done."
struct BakeDone {
    std::string dish;
    ZEN_SHAPE(BakeDone, 1, ZEN_FIELD(dish));
};

} // namespace kitchen

#endif
