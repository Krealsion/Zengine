// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_ACCORDION_HPP
#define ZENGINE_COMPONENT_ACCORDION_HPP

// An accordion: one section of what a pane shows, a header that always shows and a body that
// shows while it is open -- shut, open, or peeking while the pointer rests on its header, its body
// then drawn over what follows and taking none of the room. A press on the header opens a shut
// section, a peeking one too, and shuts an open one; a peek ends when the pointer leaves the
// header and the body it shows, and a press ends it until then. And a group over accordions: the
// independent one, each section opened and shut alone by its identity and every one shut by one
// act. Each owns a value and its arithmetic -- what a section spends of the room it is given, in
// the owner's own unit -- and knows no medium: where a fold is kept, what a header says and where
// the pointer is are the owner's.
// Reference: component/docs/component.md.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace zengine::component {

/// WHAT A SECTION SHOWS: its header alone, its header and its body, or its header and its body
/// drawn over what follows.
enum class Fold : std::uint8_t { kShut, kOpen, kPeek };

/// THE MARK A HEADER CARRIES where a character draws it: `-` on an open section, which a press
/// shuts, and `+` on one a press opens, a peeking one too.
inline const char* fold_mark(Fold fold) noexcept { return fold == Fold::kOpen ? "-" : "+"; }

/// ONE SECTION'S FOLD, and the pointer resting over it.
struct Accordion {
    bool open = false;    ///< the body shows, taking its room
    bool resting = false; ///< the pointer rests on the header, or on the body a peek shows
    bool spent = false;   ///< pressed while the pointer rested: no peek until the pointer leaves

    Fold fold() const noexcept {
        return open ? Fold::kOpen : (resting && !spent ? Fold::kPeek : Fold::kShut);
    }

    /// THE POINTER RESTS OVER THE SECTION -- its header, or the body its peek shows -- or has left
    /// both. True when what the section shows changed.
    bool rest(bool over) noexcept {
        const Fold was = fold();
        if (over != resting) {
            resting = over;
            spent = false;
        }
        return fold() != was;
    }

    /// A PRESS ON THE HEADER, or the key that is one: shut opens, a peek too, and open shuts. A
    /// section shut under the resting pointer does not peek until the pointer leaves it.
    void press() noexcept {
        open = !open;
        spent = resting;
    }

    /// THE PEEK SPENT AND THE FOLD LEFT AS IT IS, for an owner that keeps the fold elsewhere and
    /// asks for it there: what shows is what is kept, set back as `open` once it comes.
    void spend() noexcept { spent = resting; }
};

/// WHAT ONE SECTION SPENDS OF A ROOM, in the owner's unit (rows, or pixels): its header first,
/// then as much of its body as the room holds below the header -- in the room while it is open,
/// over what follows while it peeks -- and how much of the body did not fit, counted.
///
/// Invariants, over every input:
///   header + body <= room
///   body + cut == the body's extent while the section shows it, and both 0 while it is shut
///   over       == (the fold is a peek)
struct SectionSpan {
    std::size_t header = 0; ///< the header's extent, as much of it as the room holds
    std::size_t body = 0;   ///< the body's extent that shows
    std::size_t cut = 0;    ///< the body's extent that did not fit
    bool over = false;      ///< the body is drawn over what follows: a peek

    /// What the section takes from the room: a peek's body takes none of it.
    std::size_t taken() const noexcept { return header + (over ? 0 : body); }
};

/// THE SPAN OF A SECTION SHOWN AS `fold` IN `room`, its header `header` long and its body `body`.
/// `room` is what the section may spend from its header's start: the room it may take while open,
/// the rest of the owner's room below it while it peeks. TOTAL: a room shorter than the header
/// seats as much of the header as fits and none of the body.
inline SectionSpan section_span(Fold fold, std::size_t header, std::size_t body,
                                std::size_t room) noexcept {
    SectionSpan out;
    out.header = (std::min)(header, room);
    if (fold == Fold::kShut) {
        return out;
    }
    out.body = (std::min)(body, room - out.header);
    out.cut = body - out.body;
    out.over = fold == Fold::kPeek;
    return out;
}

/// THE INDEPENDENT GROUP: sections over one population, each opened and shut alone by its
/// identity, and every one shut by one act. The pointer rests over one section at most, so one at
/// most peeks. The open sections are kept by key, as `HeldChoice` keeps a choice, so a population
/// that moves keeps them open; `keep` forgets the keys a population no longer holds.
template <class Key>
class IndependentAccordions {
public:
    /// SECTION `key` AS AN ACCORDION: whether it is open, and the pointer resting over it.
    Accordion section(const Key& key) const {
        Accordion a;
        a.open = is_open(key);
        a.resting = resting_ && rest_ == key;
        a.spent = a.resting && spent_;
        return a;
    }
    Fold fold(const Key& key) const { return section(key).fold(); }
    bool is_open(const Key& key) const {
        return std::binary_search(open_.begin(), open_.end(), key);
    }

    /// A PRESS ON SECTION `key`'s HEADER (`Accordion::press`): that section alone opens or shuts.
    void press(const Key& key) {
        Accordion a = section(key);
        a.press();
        set(key, a.open);
        if (a.resting) {
            spent_ = a.spent;
        }
    }
    /// ...OR ITS FOLD NAMED: true when it moved.
    bool open(const Key& key) { return set(key, true); }
    bool shut(const Key& key) { return set(key, false); }

    /// EVERY SECTION SHUT, IN ONE ACT; a peek under the pointer waits until it leaves. True when
    /// any section was open.
    bool shut_all() {
        const bool any = !open_.empty();
        open_.clear();
        spent_ = resting_;
        return any;
    }

    /// THE POINTER RESTS OVER SECTION `key` -- its header, or the body its peek shows -- and over
    /// no other. True when what any section shows changed.
    bool rest(const Key& key) {
        const Fold was = resting_ ? fold(rest_) : Fold::kShut;
        if (resting_ && rest_ == key) {
            return false;
        }
        const bool moved = resting_ && was == Fold::kPeek;
        rest_ = key;
        resting_ = true;
        spent_ = false;
        return moved || fold(key) == Fold::kPeek;
    }
    /// ...OR OVER NONE, having left them all. True when a peek ended.
    bool rest_none() {
        const bool peeked = resting_ && fold(rest_) == Fold::kPeek;
        resting_ = false;
        spent_ = false;
        rest_ = Key{};
        return peeked;
    }
    /// The section the pointer rests over, if any: `nullptr` when it rests over none.
    const Key* resting() const noexcept { return resting_ ? &rest_ : nullptr; }

    /// THE SECTIONS OPEN, in key order, once each: what an owner keeps of the group.
    const std::vector<Key>& opened() const noexcept { return open_; }
    /// ...AND THE GROUP SET AGAIN FROM WHAT AN OWNER KEPT, every other section shut.
    void restore(std::vector<Key> keys) {
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
        open_ = std::move(keys);
    }

    /// FORGET EVERY OPEN SECTION `population` NO LONGER HOLDS, by `key_of` each member's key, and
    /// a rest over one. True when one was forgotten.
    template <class Population, class KeyOf>
    bool keep(const Population& population, KeyOf key_of) {
        std::vector<Key> held;
        held.reserve(population.size());
        for (const auto& member : population) {
            held.push_back(key_of(member));
        }
        std::sort(held.begin(), held.end());
        const std::size_t was = open_.size();
        open_.erase(std::remove_if(open_.begin(), open_.end(),
                                   [&held](const Key& k) {
                                       return !std::binary_search(held.begin(), held.end(), k);
                                   }),
                    open_.end());
        if (resting_ && !std::binary_search(held.begin(), held.end(), rest_)) {
            (void)rest_none();
        }
        return open_.size() != was;
    }

private:
    bool set(const Key& key, bool open) {
        const auto at = std::lower_bound(open_.begin(), open_.end(), key);
        const bool there = at != open_.end() && *at == key;
        if (open == there) {
            return false;
        }
        if (open) {
            open_.insert(at, key);
        } else {
            open_.erase(at);
        }
        return true;
    }

    std::vector<Key> open_; ///< sorted, unique
    Key rest_{};
    bool resting_ = false;
    bool spent_ = false;
};

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_ACCORDION_HPP
