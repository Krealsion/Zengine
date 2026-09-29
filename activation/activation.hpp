// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_ACTIVATION_ACTIVATION_HPP
#define ZENGINE_ACTIVATION_ACTIVATION_HPP

// The activation cursor: whether a weave acts on an arriving `zen.Activated`. What an activation
// means, and who may attest one, are the Loom's (its lifecycle laws); the cursor reads the Loom's
// attestation and keeps one lineage. Not a lifecycle and not a scheduler: what a weave does with
// an accepted activation is its own. Reference: docs/reference/activation.md.

#include <zen/switchboard/message.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <string>

namespace zengine {

/// The activation a weave is living under. A new cursor is unactivated, so nothing sent to an
/// earlier incarnation can make this one act, whichever arrives first.
class ActivationCursor {
public:
    /// Offers an arriving activation. True iff it becomes the current one, which is when the weave
    /// does its once-per-activation work. False, changing nothing, for:
    /// - an activation the Loom did not attest (`Mail::lifecycle_attested`), whoever sent it;
    /// - one whose attested sequence (`Mail::attested_sequence`) is not the payload's;
    /// - an invalid sender, or a sequence below 1;
    /// - from the current sender, a sequence no newer than the current one: a duplicate or a
    ///   replay.
    /// An attested activation from a different sender begins a new lineage at any positive
    /// sequence. It takes the whole `Mail` because both deciding facts are delivery facts, which
    /// no payload field can carry.
    bool accept(const loom::Mail& mail, const loom::Activated& activated) {
        if (!mail.lifecycle_attested()) {
            return false;
        }
        if (mail.attested_sequence() != activated.sequence) {
            return false;
        }
        const loom::WeaveId sender = mail.sender();
        const std::int64_t sequence = activated.sequence;
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

    /// The current activation. Before the first accepted one: false, an invalid id and 0.
    bool activated() const { return activated_; }
    loom::WeaveId sender() const { return sender_; }
    std::int64_t sequence() const { return sequence_; }

    /// The current sender as it travels on a wire: canonical decimal Text, because a `WeaveId` is
    /// unsigned 64-bit and a wire `Int` is signed. "0" before the first accepted activation.
    std::string sender_text() const { return std::to_string(sender_.value); }

    /// Whether a carried key, a `sender_text()` and a sequence, names the current activation.
    /// Both halves must match; false before the first accepted activation.
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
