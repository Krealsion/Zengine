// The snake World weave — owns the simulation, emits the locked shapes, holds
// SnakeWorldState. One source, two libraries (the Loom weavelib pattern):
//
//   (default)        snake-world-v1 — state SnakeWorldState v1, the incumbent
//   SNAKE_WORLD_V2   snake-world-v2 — state v2 (larger board, +growths), the heir
//
// Both worlds CONVERSE: they accept zen.PrepareShutdown and answer it with a
// letter carrying their whole state as one bequest item, said in their own
// vocabulary (the state shape) — that declaration is what the steward reads to
// decide a graceful swap can be graceful. The v2 world is additionally an heir:
// on its first wake it asks the steward, by role, whether anyone left it
// anything, and folds what it inherits — a v2 item is adopted whole (same-shape
// succession), a v1 item goes through migrate() (the version change is detected
// by the gate itself: claim_item<T> re-admits the bytes against T's schema, so
// "which version is this?" is answered by the one validator, never by trusting
// a label).
//
// The world never knows its consumers: SnakeVisual / FoodEaten / SnakeDied are
// published, and whoever accepts them receives them. That is the entire
// mechanism behind two of the three moments (drawer replacement, late score).

#include "logic.hpp"
#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

namespace {

using namespace zengine::snake;

#if defined(SNAKE_WORLD_V2)
using State = v2::SnakeWorldState;
#else
using State = v1::SnakeWorldState;
#endif

class SnakeWorld
    : public loom::WeaveBase<
          SnakeWorld, State,
#if defined(SNAKE_WORLD_V2)
          loom::Accept<SnakeTick, SnakeTurn, loom::PrepareShutdown, loom::Bequest, loom::Refused>,
          loom::Emit<SnakeVisual, FoodEaten, SnakeDied, loom::Bequest, loom::ClaimBequest>
#else
          loom::Accept<SnakeTick, SnakeTurn, loom::PrepareShutdown>,
          loom::Emit<SnakeVisual, FoodEaten, SnakeDied, loom::Bequest>
#endif
          > {
public:
    void on(const SnakeTick&, loom::Mail& mail) {
#if defined(SNAKE_WORLD_V2)
        claim_once(mail);
#endif
        if (state_.snake.empty()) {
            // Empty snake = new game pending (fresh load, or a poke-reset).
            seed(state_);
            mail.publish(visual_of(state_));
            return;
        }
        const StepEvents ev = step(state_);
        if (ev.ate) {
            mail.publish(FoodEaten{});
        }
        if (ev.died) {
            mail.publish(SnakeDied{});
        }
        if (ev.moved || ev.died) {
            mail.publish(visual_of(state_));
        }
        // A dead world ticks silently until someone resets it (poke) or
        // replaces it (swap). Death is a state, not an exit.
    }

    void on(const SnakeTurn& t, loom::Mail&) { turn(state_, t.direction); }

    /// "You are being replaced. Say what you want your heir to know." This
    /// world says the one thing it is: its state, as a single item in its own
    /// vocabulary. The answer goes to the STAMPED SENDER (the steward that
    /// asked), echoing the correlation — PrepareShutdown arrives via send, not
    /// forward, so reply_to is deliberately unset (lifecycle.hpp's law: the
    /// letter is part of the steward's conversation, not the asker's).
    void on(const loom::PrepareShutdown&, loom::Mail& mail) {
        loom::Bequest letter;
        letter.role = kWorldRole;
        letter.items.push_back(loom::bequeath_item(state_));
        mail.send(mail.sender(), letter, mail.correlation());
    }

#if defined(SNAKE_WORLD_V2)
    /// The inheritance arrived. Fold it: a v2 item is adopted whole (same-shape
    /// succession — a v2 world replaced by a v2 world), a v1 item is migrated.
    /// Each item is re-admitted through the real gate by claim_item before a
    /// field is touched — the version detection IS the gate's verdict, tried
    /// newest-first. Unrecognized items are ignored (the letter is advice from
    /// the dead, not law).
    void on(const loom::Bequest& letter, loom::Mail& mail) {
        if (!awaiting_claim_ || mail.correlation() != kClaimCorrelation) {
            return; // unsolicited or stale: the consumer obligation, one-shot
        }
        awaiting_claim_ = false;
        for (const loom::Bytes& item : letter.items) {
            if (auto same = loom::claim_item<v2::SnakeWorldState>(item)) {
                state_ = *same;
                mail.publish(visual_of(state_));
                return;
            }
            if (auto old = loom::claim_item<v1::SnakeWorldState>(item)) {
                state_ = migrate(*old);
                mail.publish(visual_of(state_));
                return;
            }
        }
    }

    /// "No bequest is held for you." A real answer; fresh life is already
    /// underway.
    void on(const loom::Refused&, loom::Mail& mail) {
        if (mail.correlation() == kClaimCorrelation) {
            awaiting_claim_ = false;
        }
    }
#endif

private:
#if defined(SNAKE_WORLD_V2)
    // The claim correlation is a fixed constant: the heir makes exactly one
    // claim in its life, so one number distinguishes that conversation from
    // everything else it will ever receive. One-shot + correlation is the
    // consumer obligation's shape here; the stamped-sender half is honestly
    // WAIVED — the heir reaches the steward by role precisely because it
    // cannot know the steward's id, so it cannot pre-bind the answer's sender.
    // An in-process peer could forge a Bequest into the waiting window; that
    // is B1-tier ground (loaded code is trusted-in-process by declaration),
    // accepted and named, not hidden.
    static constexpr std::uint64_t kClaimCorrelation = 0xC1A1;
    bool asked_ = false;
    bool awaiting_claim_ = false;

    /// The heir asks when it WAKES — not when it was born, with no idea how
    /// long any letter has waited, addressed by the one name that outlives
    /// every swap: the steward's role. Life does not wait on the answer (a
    /// world with no steward still runs); if an inheritance arrives, it folds.
    void claim_once(loom::Mail& mail) {
        if (asked_) {
            return;
        }
        asked_ = true;
        awaiting_claim_ = true;
        mail.send_to_role(loom::kManagerRole, loom::ClaimBequest{kWorldRole}, kClaimCorrelation);
    }
#endif
};

} // namespace

ZEN_EXPORT_WEAVE(SnakeWorld)
