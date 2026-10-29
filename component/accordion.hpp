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
// act, over the open sections its owner keeps. And a tree, an accordion at every level: the rows
// it shows, each with its depth and whether it holds rows of its own. Each owns a value and its
// arithmetic -- what a section spends of the room it is given, in the owner's own unit -- and
// knows no medium: where a fold is kept, what a header says and where the pointer is are the
// owner's.
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
/// identity, and every one shut by one act. The open sections are the owner's, kept in its own
/// state and lent to the group, which reads them there and writes them there and keeps no copy: an
/// edit to the state is what the next fold reads. The list holds keys in any order, as an edit may
/// leave it, and a section is open while its key stands there. The group keeps only the pointer:
/// it rests over one section at most, so one at most peeks.
template <class Key>
class IndependentAccordions {
public:
    /// A GROUP OVER THE OPEN SECTIONS `open` HOLDS, which outlives it.
    explicit IndependentAccordions(std::vector<Key>& open) noexcept : open_(&open) {}

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
        return std::find(open_->begin(), open_->end(), key) != open_->end();
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
        const bool any = !open_->empty();
        open_->clear();
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
        const std::size_t was = open_->size();
        open_->erase(std::remove_if(open_->begin(), open_->end(),
                                    [&held](const Key& k) {
                                        return !std::binary_search(held.begin(), held.end(), k);
                                    }),
                     open_->end());
        if (resting_ && !std::binary_search(held.begin(), held.end(), rest_)) {
            (void)rest_none();
        }
        return open_->size() != was;
    }

private:
    /// Open: the key added once, in key order where the list keeps one. Shut: every copy of it
    /// gone.
    bool set(const Key& key, bool open) {
        if (open == is_open(key)) {
            return false;
        }
        if (open) {
            open_->insert(std::upper_bound(open_->begin(), open_->end(), key), key);
        } else {
            open_->erase(std::remove(open_->begin(), open_->end(), key), open_->end());
        }
        return true;
    }

    std::vector<Key>* open_; ///< the owner's, never null
    Key rest_{};
    bool resting_ = false;
    bool spent_ = false;
};

/// ONE ROW OF A TREE AS IT SHOWS: which of the owner's rows it is, how deep it stands, and whether
/// it holds rows of its own -- a branch, which folds.
struct TreeRow {
    std::size_t row = 0;
    std::size_t depth = 0;
    bool branch = false;
    bool operator==(const TreeRow&) const = default;
};

/// THE ROWS A TREE SHOWS, from the depth of each of the owner's rows in depth-first order: a row
/// is a branch when the row after it stands deeper, and a row shows while every branch above it
/// is open (`is_open(row)`). The tree is an accordion at each level: one level deep, its branches
/// are the sections. TOTAL: a depth that jumps by more than one still nests under the row before.
template <class IsOpen>
std::vector<TreeRow> shown_tree(const std::vector<std::size_t>& depths, IsOpen is_open) {
    std::vector<TreeRow> out;
    bool hiding = false;
    std::size_t hidden_below = 0; ///< while hiding, every row deeper than this is under a shut one
    for (std::size_t i = 0; i < depths.size(); ++i) {
        const std::size_t depth = depths[i];
        if (hiding && depth > hidden_below) {
            continue;
        }
        hiding = false;
        const bool branch = i + 1 < depths.size() && depths[i + 1] > depth;
        out.push_back(TreeRow{i, depth, branch});
        if (branch && !is_open(i)) {
            hiding = true;
            hidden_below = depth;
        }
    }
    return out;
}

/// ROW `row`'s PARENT: the nearest row before it that stands shallower, or `row` itself at the
/// top.
inline std::size_t tree_parent(const std::vector<std::size_t>& depths, std::size_t row) noexcept {
    if (row >= depths.size()) {
        return row;
    }
    for (std::size_t i = row; i-- > 0;) {
        if (depths[i] < depths[row]) {
            return i;
        }
    }
    return row;
}

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_ACCORDION_HPP
