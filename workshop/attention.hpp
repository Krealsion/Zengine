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

    /// Write current truth under `c.key`. An existing key is overwritten WHOLE -- the same
    /// republish-the-picture discipline `builder::BuildStatus` is under, so a condition can
    /// never be half of what it used to be and half of what it is.
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

/// ONE CONDITION THE MAKER HAS HIDDEN, and the statement they hid.
// WL-ATTN-08 -- agents/workshop/attention.md
struct Dismissal {
    std::string key;
    std::string stamp;
};

/// THE CURRENT-CONDITION VIEW, AND WHAT IT HIDES -- presentation state, all of it.
// WL-ATTN-03, WL-ATTN-08 -- agents/workshop/attention.md
struct AttentionView {
    bool open = false;
    std::size_t cursor = 0;
    std::vector<Dismissal> dismissed;

/// IS THIS EXACT STATEMENT HIDDEN? The key AND the stamp, both.
    // WL-ATTN-08 -- agents/workshop/attention.md
    bool hides(const Condition& c) const {
        for (const Dismissal& d : dismissed) {
            if (d.key == c.key) {
                return d.stamp == c.stamp();
            }
        }
        return false;
    }

    /// Hide this statement. Re-dismissing a condition that has since changed REPLACES the
    /// old stamp rather than adding a row, so the set stays one entry per key and a maker
    /// who hides the same condition twice has hidden it once.
    void dismiss(const Condition& c) {
        for (Dismissal& d : dismissed) {
            if (d.key == c.key) {
                d.stamp = c.stamp();
                return;
            }
        }
        dismissed.push_back(Dismissal{c.key, c.stamp()});
    }
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_ATTENTION_HPP
