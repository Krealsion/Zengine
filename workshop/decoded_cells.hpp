// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_DECODED_CELLS_HPP
#define ZENGINE_WORKSHOP_DECODED_CELLS_HPP

// WHAT ONE VALUE COSTS ITS READER TO DECODE, counted by Loom's own rule for the decode budget
// (its bounds reference, "The decode-materialization bound"): one cell per declared field
// of every message the decoder enters, present or not, and one per element of every list. A
// reading Workshop answers whole is filled to that budget, so a reader on another host decodes it.

#include <zen/value.hpp>

#include <cstdint>

namespace zengine::workshop {

/// THE BUDGET ONE DECODED VALUE SPENDS AT MOST: Loom's `kMaxDecodedCells`, which Loom keeps
/// private, restated here and tied to it by a case that decodes a value of exactly this many cells.
inline constexpr std::int64_t kDecodedCellBudget = 65'536;

inline std::int64_t decoded_cells(const loom::Value& value) {
    auto cells = static_cast<std::int64_t>(value.field_count());
    for (std::size_t i = 0; i < value.field_count(); ++i) {
        const loom::Cell* cell = value.at(i);
        if (cell == nullptr) continue;
        if (cell->is(loom::Kind::Message)) {
            cells += decoded_cells(*cell->as_message());
        } else if (cell->is(loom::Kind::List)) {
            const loom::Cell::Array& list = cell->as_list();
            cells += static_cast<std::int64_t>(list.size());
            for (const loom::Cell& element : list) {
                if (element.is(loom::Kind::Message)) cells += decoded_cells(*element.as_message());
            }
        }
    }
    return cells;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_DECODED_CELLS_HPP
