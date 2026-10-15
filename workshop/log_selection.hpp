// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LOG_SELECTION_HPP
#define ZENGINE_WORKSHOP_LOG_SELECTION_HPP

// WHAT WORKSHOP'S DURABLE LOG KEEPS, the one value the host's journal is built from.
// Agent law: agents/realization.md

#include <string>

#include <zen/history/logger.hpp>

#include "builder/vocabulary.hpp"

namespace zengine::workshop {

/// The shapes the journal keeps beyond Loom's default, each with its whole payload: a build that
/// finished or never started, and what the project made of an offered artifact -- the one place
/// a long refusal is kept whole. Not `BuildStatus`, republished on every chunk of a build's
/// output, nor `BuildOutput`, the output itself.
inline constexpr const char* kDurableBuildShapes[] = {
    builder::BuildFinished::zen_name, builder::BuildNotStarted::zen_name,
    builder::ArtifactRealized::zen_name};

/// Loom's default selection, refusals kept only when the weaver asks (`--log-refusals`), and
/// `kDurableBuildShapes` uncapped.
inline loom::LoggerSelection durable_log_selection(bool log_refusals) {
    loom::LoggerSelection selection = loom::default_selection();
    selection.log_refusals = log_refusals;
    for (const char* shape : kDurableBuildShapes) {
        selection.shapes.push_back(loom::LogRule{std::string(shape), /*cap=*/0});
    }
    return selection;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_LOG_SELECTION_HPP
