// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_ATTENTION_HPP
#define ZENGINE_WORKSHOP_ATTENTION_HPP

// WHAT IS TRUE RIGHT NOW, AND WORTH A MAKER'S ATTENTION (WUX-4)
//
// Workshop has always had two structurally different ways to say something outwardly, and
// only one of them can be wrong:
//
//     A DERIVED CONDITION          a pure function of live state, recomputed at every paint.
//                                  Nobody emits it, nobody holds it, it cannot go stale, and
//                                  it retracts by ceasing to be returned.
//     A LATCHED UTTERANCE          a string written at an instant (`Session::notice`, said by
//                                  `say`). It is right about a moment that has passed and it
//                                  has no lifetime of its own at all.
//
// The utterance is the correct shape for `committed Width = 40%`, `removed Info`,
// `released #12` -- things that HAPPENED. It is the wrong shape for a refused keymap file,
// which is still refused an hour later, and the measured cost of using it anyway was that
// RESOLVING A CONDITION COULD NOT UN-SAY IT: a refused external pane cleared its refusal on
// the next valid content and the notice row kept the refusal sentence for the rest of the
// process.
//
// This header is the third shape -- a HELD condition -- and the presentation state that
// belongs beside it. It is deliberately small:
//
//     establish/update   the owner writes current truth under its stable key
//     retract            the owner erases that key
//     dismiss            the PRESENTATION hides that key, this session only
//     change             materially changed content re-arms a prior dismissal
//
// Truth flows one way, owner to projection to presentation, and never back. Nothing here is
// a manager, a registry, a scheduler, a callback store or an event system: there is no
// subscription, no ordering by arrival, no history, no expiry and no timer. A condition
// disappears because it resolved or because a maker hid its presentation, and for no other
// reason.
//
// A DERIVED CONDITION NEEDS NONE OF THIS AND MUST NOT BE GIVEN IT. `pane_state_of`,
// `ProjectFrontier` and `ExternalPane::refusal` are already correct by construction;
// copying one into held state to make it presentable would buy a staleness it currently
// cannot have. The projection reads both families and owns neither (`attention_conditions`,
// screen.hpp).

#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// ONE CONDITION THAT IS TRUE RIGHT NOW.
///
/// FIVE FIELDS, AND FOUR OF THEM ARE THE OWNER'S OWN WORDS. `key` is the identity every
/// operation is scoped to -- a durable dotted string, `ActionRow::id`'s own kind of name,
/// stable across the whole life of the condition and shared by nothing else. `compact` is
/// the owner's semantic word plus its subject, composed where both are known; `detail` is
/// the human explanation the owner already possesses (a refusal's sentence, a load's
/// outcome, the transition note) and is never a second prose copy written for this screen.
///
/// `role` IS THE ONE SEVERITY VOCABULARY. `surface::role` is what the media already resolve
/// to ink, so a condition says how loudly it should currently speak in the words the canvas
/// already understands. There is no WUX-4 severity enum: LOUDNESS AND ACTIONABILITY ARE
/// INDEPENDENT AXES, and folding them would make one of the two unsayable. A serious
/// condition may have nothing a maker can do about it; an ordinary one may have a useful
/// action.
///
/// `action` IS A NAME AND NEVER A POWER. It holds an `ActionRow::id` or nothing at all --
/// no callback, no function pointer, no availability flag and no private execution path.
/// What a presentation may do with it is look up its CURRENT gesture through the effective
/// keymap and paint that; what a press then does is the existing dispatch's business, under
/// the existing authorization, with this struct never consulted (HD-8's law: a control never
/// invents a reason, and holds no callback).
struct Condition {
    std::string key;
    std::string compact;
    std::string detail;
    std::int64_t role = surface::role::kAccent;
    std::string action;

    /// WHAT A DISMISSAL IS MEASURED AGAINST -- the condition's content, as one opaque token.
    ///
    /// It is a comparison token and NOT a copy of anything: nothing reads it back apart, no
    /// presentation renders it, and its only question is "is this still the same statement
    /// the maker hid?". The key alone would be too coarse -- a wall that changed its reason
    /// would stay hidden -- and keeping the whole condition would be a second store of
    /// truth. This is the Terminal completion's `dismissed_at` rule one layer out: a
    /// dismissal scoped to a stable identity, re-armed the moment that identity's content
    /// moves.
    std::string stamp() const {
        return compact + '\n' + detail + '\n' + std::to_string(role) + '\n' + action;
    }
};

/// HOW LOUD, AS AN ORDER.
///
/// `surface::role`'s integers are a VOCABULARY and not a ladder -- `kAccent` is 1 and
/// `kMuted` is 2 because that is the order somebody wrote them in, and sorting on the raw
/// value would rank a quiet condition above an ordinary one for no reason a maker could
/// read. So the ranking is written down here, once, and it is the only place in this
/// application that claims one role is more urgent than another.
///
/// TOTAL, including over roles this vocabulary does not have yet: an unknown role ranks last
/// rather than first, which is the same widening-direction safety `role::kNone`'s negative
/// sentinel buys.
inline int attention_rank(std::int64_t role) noexcept {
    switch (role) {
    case surface::role::kAlert: return 0;
    case surface::role::kAccent: return 1;
    case surface::role::kFill: return 2;
    default: return 3;
    }
}

/// THE ORDER CONDITIONS ARE PRESENTED IN, and it is derived from CURRENT TRUTH ALONE.
///
/// Loudness first, then the key. Deliberately NOT arrival order: nothing here records when a
/// condition became true, a derived condition has no such moment at all, and a compact
/// presentation that showed whichever condition happened to be established last would
/// re-rank itself every time an unrelated one changed. Two conditions can never tie, because
/// a key belongs to exactly one condition.
inline bool ranks_before(const Condition& a, const Condition& b) {
    const int ra = attention_rank(a.role);
    const int rb = attention_rank(b.role);
    if (ra != rb) {
        return ra < rb;
    }
    return a.key < b.key;
}

/// THE CONDITIONS WORKSHOP ITSELF HOLDS -- the ones no live state already answers.
///
/// THREE DOORS AND A VECTOR. `establish` writes under a key and an existing key is an
/// UPDATE, not a second row; `retract` erases one; `find` reads one. That is the whole
/// lifecycle, and it is the owner's to spend: the weave establishes its own walls, the host
/// hands over the ones it computed before the weave existed, and nothing on the presentation
/// side is allowed to call either writer.
///
/// IT IS A VALUE ON THE SESSION AND NOT A SERVICE. There is no registration, no
/// subscription, no ordering by arrival and no identity beyond the key -- which is why a
/// condition costs an owner one call and a reader one loop, and why nothing had to be
/// invented for a condition to stop being true.
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
///
/// SESSION, PRESENTATION-OWNED, AND NEVER PERSISTED. A dismissal changes no underlying
/// truth: the condition is still true, still returned by the projection, still readable by
/// its owner. What it changes is whether this presentation shows it. It dies with the
/// process for `Session::clipboard`'s reason -- what a maker chose to stop looking at this
/// afternoon is not a fact about their desk (WUX-0 keeps the desk, never the work in
/// progress).
struct Dismissal {
    std::string key;
    std::string stamp;
};

/// THE CURRENT-CONDITION VIEW, AND WHAT IT HIDES -- presentation state, all of it.
///
/// `open` and `cursor` are the same kind of fact `PanelPicker` holds: a mode a maker
/// entered and the row they are on. `dismissed` is the one piece of attention state that is
/// not derived, and it is exactly the Terminal completion's `dismissed`/`dismissed_at` shape
/// one layer out.
///
/// NOTHING HERE IS A COPY OF ANY DOMAIN TRUTH. There is no cached condition list, no
/// BuildStatus, no pane state, no keymap validity and no legacy-file fact: the projection is
/// recomputed at every paint from its owners, and what survives between paints is a cursor
/// and a set of hidden statements.
struct AttentionView {
    bool open = false;
    std::size_t cursor = 0;
    std::vector<Dismissal> dismissed;

    /// IS THIS EXACT STATEMENT HIDDEN? The key AND the stamp, both, and that conjunction is
    /// the whole re-arm rule: a condition whose content moved is a different statement, so
    /// the dismissal of the old one does not reach it and it is visible again with nobody
    /// having to clear anything.
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
