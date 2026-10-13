// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_PAINTER_EXPORT_HPP
#define ZENGINE_GRAPHICS_PAINTER_EXPORT_HPP

// Exporting a skin for a medium of characters behind the painter seam: the shared painter
// (graphics/term_paint.hpp) and a Look. Ours and a stranger's are exported by the same macro:
//
//   struct Neon { static constexpr const char* kName = "neon"; static term::Look look(); };
//   ZENGINE_EXPORT_PAINTER(Neon)

#include "graphics/painter_abi.h"
#include "graphics/term_paint.hpp"

#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

namespace zengine::graphics::painter_export {

inline int refuse(ZenByteSink reason, const std::string& why, int status) {
    reason.write(reason.ctx, reinterpret_cast<const std::uint8_t*>(why.data()), why.size());
    return status;
}

/// No exception crosses the seam: a refusal is its words and a status.
template <class L>
int paint(const std::uint8_t* task, std::size_t task_len, ZenByteSink result,
          ZenByteSink reason) {
    try {
        const auto t = loom::claim_item<PaintTask>(loom::Bytes(task, task + task_len));
        if (!t) {
            return refuse(reason, "the task does not read as a PaintTask", 1);
        }
        const loom::Bytes b = loom::bequeath_item(term::paint_task(*t, L::look()));
        result.write(result.ctx, b.data(), b.size());
        return 0;
    } catch (const std::exception& ex) {
        return refuse(reason, ex.what(), 2);
    }
}

} // namespace zengine::graphics::painter_export

#define ZENGINE_EXPORT_PAINTER(LOOK)                                                               \
    extern "C" {                                                                                   \
    ZEN_KERNEL_EXPORT const ZenginePainterV1* zengine_painter(void) {                              \
        static const ZenginePainterV1 table{ZENGINE_PAINTER_ABI_VERSION, LOOK::kName,              \
                                            &::zengine::graphics::painter_export::paint<LOOK>};    \
        return &table;                                                                             \
    }                                                                                              \
    }

#endif // ZENGINE_GRAPHICS_PAINTER_EXPORT_HPP
