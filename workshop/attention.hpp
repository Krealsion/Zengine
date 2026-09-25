// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_ATTENTION_HPP
#define ZENGINE_WORKSHOP_ATTENTION_HPP

// WHAT IS TRUE RIGHT NOW, AND WORTH A MAKER'S ATTENTION
// Workshop law: agents/workshop/attention.md

#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// ONE CONDITION THAT IS TRUE RIGHT NOW.
// WL-ATTN-01, WL-ATTN-10 -- agents/workshop/attention.md
struct Condition {
    std::string key;
    std::string compact;
    std::string detail;
    std::int64_t role = surface::role::kAccent;
    std::string action;

    /// WHAT A DISMISSAL IS MEASURED AGAINST -- the condition's content, as one opaque token.
    // WL-ATTN-08 -- agents/workshop/attention.md
    std::string stamp() const {
        return compact + '\n' + detail + '\n' + std::to_string(role) + '\n' + action;
    }
};

/// A tool this run stepped over, as the condition it is for the whole run: keyed and named by its
/// artifact on the host's own compact row, which needs no tool to be read (even when the missing
/// tool is the desktop). The refusing layer's sentence is the detail.
// WL-ATTN-01 -- agents/workshop/attention.md
inline Condition unavailable_tool(const std::string& stem, const std::string& said) {
    return Condition{"load.unavailable/" + stem, stem + " is not in this Workshop", said,
                     surface::role::kAlert, "build its artifact, then launch again"};
}

/// HOW LOUD, AS AN ORDER.
// WL-ATTN-07 -- agents/workshop/attention.md
inline int attention_rank(std::int64_t role) noexcept {
    switch (role) {
    case surface::role::kAlert: return 0;
    case surface::role::kAccent: return 1;
    case surface::role::kFill: return 2;
    default: return 3;
    }
}

/// THE ORDER CONDITIONS ARE PRESENTED IN, and it is derived from CURRENT TRUTH ALONE.
// WL-ATTN-07 -- agents/workshop/attention.md
inline bool ranks_before(const Condition& a, const Condition& b) {
    const int ra = attention_rank(a.role);
    const int rb = attention_rank(b.role);
    if (ra != rb) {
        return ra < rb;
    }
    return a.key < b.key;
}

/// THE CONDITIONS WORKSHOP ITSELF HOLDS -- the ones no live state already answers.
// WL-ATTN-01, WL-ATTN-04, WL-ATTN-11 -- agents/workshop/attention.md
struct HeldConditions {
    std::vector<Condition> rows;

    /// Write current truth under `c.key`, overwriting an existing key whole, so a condition is
    /// never half old and half new.
    void establish(Condition c) {
        for (Condition& row : rows) {
            if (row.key == c.key) {
                row = std::move(c);
                return;
            }
        }
        rows.push_back(std::move(c));
    }

    /// It is no longer true. Erasing an absent key is silence, not an error: an owner that
    /// retracts what it never established is saying the same thing either way.
    void retract(std::string_view key) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].key == key) {
                rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

    const Condition* find(std::string_view key) const {
        for (const Condition& row : rows) {
            if (row.key == key) {
                return &row;
            }
        }
        return nullptr;
    }

    bool holds(std::string_view key) const { return find(key) != nullptr; }
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_ATTENTION_HPP
