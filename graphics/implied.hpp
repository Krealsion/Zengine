// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_IMPLIED_HPP
#define ZENGINE_GRAPHICS_IMPLIED_HPP

// A value whose shape its receiver already knows: Loom's canonical bytes without the envelope that
// names the shape. A node's operator implies its operands' shape and an element's type implies its
// changes', so the sender leaves the envelope off and the receiver puts back its own before the one
// gate reads the value: a body of another shape, version or content is refused there, as a whole
// value would be.

#include <zen/serialize.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace zengine::graphics {

/// The envelope Loom writes before a value of `schema`: its format, the shape's name, version and
/// content id.
inline loom::Bytes envelope_of(const loom::Schema& schema) {
    loom::Bytes out{0x5A, 0x4E, 0x01};
    const std::string& name = schema.name();
    out.push_back(static_cast<std::uint8_t>(name.size() & 0xff));
    out.push_back(static_cast<std::uint8_t>((name.size() >> 8) & 0xff));
    out.insert(out.end(), name.begin(), name.end());
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((schema.version() >> (8 * i)) & 0xff));
    }
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((schema.content_id() >> (8 * i)) & 0xff));
    }
    return out;
}

/// A value's body: its canonical bytes after the envelope. Throws if Loom wrote another envelope,
/// so a change of Loom's format is a loud failure rather than a quiet misread.
inline loom::Bytes body_of_value(const loom::Value& v) {
    const std::string full = loom::serialize(v);
    const loom::Bytes head = envelope_of(v.schema());
    if (full.size() < head.size() || full.compare(0, head.size(),
                                                  std::string(head.begin(), head.end())) != 0) {
        throw std::logic_error("graphics: Loom wrote an envelope this body cannot leave off");
    }
    return loom::Bytes(full.begin() + static_cast<std::ptrdiff_t>(head.size()), full.end());
}

template <class T>
loom::Bytes body_of(const T& v) {
    return body_of_value(loom::to_value(v));
}

/// A body read as T, its envelope T's own, through the gate; nothing when it does not read.
template <class T>
std::optional<T> from_body(const loom::Bytes& body) {
    loom::Bytes whole = envelope_of(*loom::schema_of<T>());
    whole.insert(whole.end(), body.begin(), body.end());
    return loom::claim_item<T>(whole);
}

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_IMPLIED_HPP
