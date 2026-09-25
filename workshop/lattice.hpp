// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LATTICE_HPP
#define ZENGINE_WORKSHOP_LATTICE_HPP

// The one bound every authored cell count has: a pane's place and extent on the desk, and a
// maker-made pane's region, share it rather than inventing a second number.

#include <cstdint>

namespace zengine::workshop {

/// The most cells an authored extent or coordinate may name.
inline constexpr std::int64_t kMaxCells = 4096; ///< an authored size, not a workspace size

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_LATTICE_HPP
