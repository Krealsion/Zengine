// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_FINGERPRINT_HPP
#define ZENGINE_WORKSHOP_FINGERPRINT_HPP

// A fingerprint Workshop takes of what it holds: 64-bit FNV-1a over the parts mixed in, each part
// ended by a separator byte, so two parts never read as one. The same parts give the same number
// in every build; it names what was sent, never when.

#include <cstdint>
#include <string_view>

namespace zengine::workshop {

struct Fingerprint {
    std::uint64_t h = 1469598103934665603ull;

    void mix(unsigned char byte) {
        h ^= byte;
        h *= 1099511628211ull;
    }
    /// One part: its bytes, then the separator.
    void bytes(std::string_view part) {
        for (const char c : part) mix(static_cast<unsigned char>(c));
        mix(0x1f);
    }
    /// One number as a part: mixed in whole, its high bits folded down, then the separator.
    void number(std::int64_t n) {
        h ^= static_cast<std::uint64_t>(n);
        h *= 1099511628211ull;
        h ^= h >> 29;
        mix(0x1f);
    }
    std::int64_t value() const { return static_cast<std::int64_t>(h & 0x7fffffffffffffffull); }
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_FINGERPRINT_HPP
