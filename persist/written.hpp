// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_PERSIST_WRITTEN_HPP
#define ZENGINE_PERSIST_WRITTEN_HPP

// What a write came to: accepted, or refused with its reason.

#include <string>
#include <utility>

namespace zengine::persist {

/// The outcome of an attempted write. A refusal carries its reason, in words a
/// weaver can act on -- the reason IS the feature, so there is no bare `false`.
struct Written {
    bool accepted = false;
    std::string refusal;

    static Written ok() { return Written{true, {}}; }
    static Written no(std::string why) { return Written{false, std::move(why)}; }
};

} // namespace zengine::persist

#endif // ZENGINE_PERSIST_WRITTEN_HPP
