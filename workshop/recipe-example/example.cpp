// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The weave Workshop's shipped recipe catalog builds (workshop/default-build-recipes.json.in): an
// artifact this tree always builds and Workshop never loads or stages, so a build from the shipped
// catalog changes no image a running Workshop has mapped. It counts the builds it is told of and
// draws nothing. Reference: builder/docs/recipes.md.

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>

namespace {

struct RecipeExampleState {
    std::int64_t told = 0; ///< how many builds it was told of
    ZEN_EXPOSE();
    ZEN_SHAPE(RecipeExampleState, 1, ZEN_FIELD(told));
};

class RecipeExample : public loom::WeaveBase<RecipeExample, RecipeExampleState, loom::Accept<>> {};

} // namespace

ZEN_EXPORT_WEAVE(RecipeExample)
