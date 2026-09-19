// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_HELD_CHOICE_HPP
#define ZENGINE_COMPONENT_HELD_CHOICE_HPP

// A CHOICE HELD BY IDENTITY ACROSS A LIST THAT MOVES -- the row a maker chose, found again in
// every fresh reading of the population by its durable key; lost when the key is not there,
// and still a choice while it is lost.
//
// WHY IT EXISTS, as a measurement. The desktop's Pane Manager (`find_cursor`, `hold`, `step`,
// `lost_`, `held_name_`; WL-DESK-10) and Info's pane list (`find_list_cursor`, `hold_list`,
// `step_list`, `lost_`, `held_name_`; WL-INFO-09) were the same forty lines twice, written one
// phase apart, and the cleanup phase repaired the same defect in both (a lost choice cleared
// its keys and a reload took a neighbour). The Pane Manager and the Hotkeys pane's row cursor
// are its two consumers now; Info keeps its copy until it chooses to move. Powers (`revalidate`) keeps a third selection by
// identity with a DIFFERENT policy -- a fresh reading whose population lacks the identity CLEARS
// it -- and is deliberately not this component's consumer: what the two consumers here share is
// the keep-lost rule, and a policy flag for a third would be a component learning a policy.
//
// WHAT IT OWNS: the key, whether one was ever chosen, where the marker stands, and whether the
// key is missing from the last population it was found against. The key type and how a member
// yields one are the consumer's; the reload shape (`DesktopState`, `InfoPaneState`) stays the
// consumer's too -- it copies `key` and `at` out to the shape when they move and back in at a
// reload, so no state shape changes for this.

#include <cstddef>
#include <cstdint>

namespace zengine::component {

template <class Key>
struct HeldChoice {
    Key key{};
    bool chosen = false; ///< a row was held at least once; both empty keys read as never chosen
    std::size_t at = 0;  ///< where the marker stands in the last population found against
    bool lost = false;   ///< the key was not in that population; `at` is where it was

    /// FIND THE HELD KEY IN A FRESH POPULATION. By identity, so a member inserted above it
    /// moves the marker with it, and one that returns is found again. A key that is not there
    /// leaves the marker where it was, holding nothing, and Return-like acts refuse until a row
    /// is deliberately chosen (`actionable`). Only a choice never made takes the row the marker
    /// stands on: that is the first activation's default.
    template <class Population, class KeyOf>
    void find(const Population& population, KeyOf key_of) {
        const std::size_t n = population.size();
        if (chosen) {
            for (std::size_t i = 0; i < n; ++i) {
                if (key_of(population[i]) == key) {
                    at = i;
                    lost = false;
                    return;
                }
            }
        }
        lost = chosen;
        if (at >= n) {
            at = n > 0 ? n - 1 : 0;
        }
        if (!chosen && n > 0) {
            hold(population, at, key_of);
        }
    }

    /// HOLD THE MEMBER AT `i`: the marker moves there and the key is that member's.
    template <class Population, class KeyOf>
    void hold(const Population& population, std::size_t i, KeyOf key_of) {
        if (i >= population.size()) {
            return;
        }
        at = i;
        key = key_of(population[i]);
        chosen = true;
        lost = false;
    }

    /// STEP THE MARKER BY `by` ROWS, clamped, holding what it lands on. Returns whether it moved.
    template <class Population, class KeyOf>
    bool step(const Population& population, std::int64_t by, KeyOf key_of) {
        const std::size_t n = population.size();
        if (n == 0) {
            return false;
        }
        std::int64_t next = static_cast<std::int64_t>(at) + by;
        if (next < 0) {
            next = 0;
        }
        if (next >= static_cast<std::int64_t>(n)) {
            next = static_cast<std::int64_t>(n) - 1;
        }
        const bool moved = static_cast<std::size_t>(next) != at || lost || !chosen;
        hold(population, static_cast<std::size_t>(next), key_of);
        return moved;
    }

    /// MAY AN ACT SPEND THIS CHOICE? Not while nothing was ever chosen, and not while the chosen
    /// row has left: acting on whichever member slid into the marker's place is the defect the
    /// keep-lost rule exists to refuse.
    bool actionable() const noexcept { return chosen && !lost; }
};

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_HELD_CHOICE_HPP
