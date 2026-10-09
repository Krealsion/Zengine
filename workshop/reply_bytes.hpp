// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_REPLY_BYTES_HPP
#define ZENGINE_WORKSHOP_REPLY_BYTES_HPP

// WHAT ONE VALUE COSTS TO CARRY: its bytes in each serialization an answer to another host crosses
// in -- Loom's native binary, between Workshop's guest door and a session host, and Loom's JSON
// (`compat`), between a session host and its runs -- and the bound a reply Workshop fills stays
// inside, so the frame carrying it fits Loom's `kMaxFrameLen` with its envelope beside it.

#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/wire.hpp>

#include <algorithm>
#include <cstdint>

namespace zengine::workshop {

/// THE BYTES ONE REPLY CARRIES AT MOST, in the larger of its serializations: a quarter of Loom's
/// frame, leaving the delivery's envelope and the frames queued before it room in the channel.
inline constexpr std::int64_t kReplyByteBudget = 16'777'216;
static_assert(kReplyByteBudget <= static_cast<std::int64_t>(loom::kMaxFrameLen) / 4,
              "a reply leaves three quarters of Loom's frame to its envelope and its neighbours");

/// The bytes `value` takes in the larger of the serializations an answer crosses in. A value
/// carried inside a list or a message takes no more than this in either, beside one byte for the
/// comma JSON writes between list elements: its own serialization is its body under a header.
inline std::int64_t reply_bytes(const loom::Value& value) {
    return static_cast<std::int64_t>(
        std::max(loom::serialize(value).size(), loom::compat::serialize(value).size()));
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_REPLY_BYTES_HPP
