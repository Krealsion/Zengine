#ifndef ZENGINE_ACTIVATION_ACTIVATION_HPP
#define ZENGINE_ACTIVATION_ACTIVATION_HPP

// The activation cursor — Zengine's shared reading of `zen.Activated`.
//
// The Loom's control door tells a freshly committed dynamic incarnation, once,
// that it is live (weave/lifecycle.hpp). That fact is narrow on purpose: it
// says a new incarnation committed at this address and NOTHING else — not
// healthy, not ready, not "start a loop". What a weave DOES with it is the
// weave's own business, and in Zengine four weaves want the same thing from it:
// "this is my first breath; arrange the time I need."
//
// This is the small amount of bookkeeping that answer requires, written once.
// It is not a framework and not a Loom abstraction — it is a two-field cursor
// plus the comparison rule, kept here so Timer, Input, Skin and SnakeClock read
// an activation the same way instead of four subtly different ways.
//
// WHAT IT IS NOT: authentication. The pair (bus-stamped sender, sequence) gives
// LINEAGE and DEDUPLICATION — it tells a weave whether this activation is one it
// has already acted on, and whether it comes from the same operator lineage as
// the last one. It does not prove the sender is the one true lifecycle operator.
// At Zengine's current altitude every weave in the process is trusted code, so a
// different sender is treated as a new lineage rather than an intruder. **There
// is no activation trust anchor yet**, and nothing here should be read as one;
// when one is wanted it is a Loom-tier question (an operator role, a signed
// identity), not something a consumer can invent for itself.
//
// The sender half is load-bearing and stays load-bearing: a bare sequence is a
// small integer, and treating one as an identity would make a replayed number
// indistinguishable from a real succession.

#include <zen/switchboard/message.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <string>

namespace zengine {

/// Tracks which activation a weave is currently living under.
///
/// A freshly constructed cursor is UNACTIVATED, and that is the point: a new
/// incarnation begins owing nothing to anything its predecessor queued. Whatever
/// was in flight for the previous incarnation cannot make this one act, even if
/// it arrives first.
class ActivationCursor {
public:
    /// Offer an arriving activation; true iff it becomes the current one — i.e.
    /// iff the weave should do its once-per-activation work now.
    ///
    /// Accepted when the stamped sender is real, the sequence is positive, and
    /// it is either from a DIFFERENT sender (a new lineage replaces the current
    /// one) or NEWER than the last sequence seen from the current sender. A
    /// same-sender, non-newer sequence is a duplicate or a replay: ignored
    /// entirely, so re-delivery cannot make anything happen twice.
    bool accept(loom::WeaveId sender, std::int64_t sequence) {
        if (!sender.valid() || sequence <= 0) {
            return false;
        }
        if (activated_ && sender == sender_ && sequence <= sequence_) {
            return false;
        }
        sender_ = sender;
        sequence_ = sequence;
        activated_ = true;
        return true;
    }

    bool activated() const { return activated_; }
    loom::WeaveId sender() const { return sender_; }
    std::int64_t sequence() const { return sequence_; }

    /// The sender half, as it travels on a wire.
    ///
    /// A `WeaveId` is an unsigned 64-bit value and Zen's wire `Int` is signed,
    /// so putting one in an Int field would narrow the top half of the range
    /// silently. Canonical decimal Text is lossless, and it is already the
    /// house spelling for a WeaveId on the wire: the kernel's control door
    /// answers a load with `zen.Result{std::to_string(id.value)}` and the Weave
    /// Manager parses it back. Same representation, same reasons.
    std::string sender_text() const { return std::to_string(sender_.value); }

    /// Does a carried activation key name the activation this cursor is living
    /// under? Both halves must match — a matching sequence under a different
    /// sender is a different lineage's beat, not ours.
    bool matches(const std::string& sender_text_, std::int64_t sequence) const {
        return activated_ && sequence == sequence_ && sender_text_ == sender_text();
    }

private:
    loom::WeaveId sender_{};
    std::int64_t sequence_ = 0;
    bool activated_ = false;
};

} // namespace zengine

#endif // ZENGINE_ACTIVATION_ACTIVATION_HPP
