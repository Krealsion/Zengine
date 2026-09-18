// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LATTICE_HPP
#define ZENGINE_WORKSHOP_LATTICE_HPP

// THE ONE BOUND EVERY AUTHORED CELL COUNT HAS -- a pane's place and extent on the desk, a
// maker-made pane's region. It was the object document's (`doc::kMaxCells`) first, and the desk
// and the pane definition adopted it rather than inventing a second number; it stayed when the
// document retired.

#include <cstdint>

namespace zengine::workshop {

/// The most cells an authored extent or coordinate may name.
inline constexpr std::int64_t kMaxCells = 4096; ///< an authored size, not a workspace size

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_LATTICE_HPP
