# The activation cursor — reference

**Reference.** How a Zengine weave decides whether an arriving `zen.Activated` is its own to act
on, and why the rule has the shape it has. If your weave uses the Timer binding, the binding
keeps a cursor for you ([timer binding](../../timer/docs/timer-binding.md)); read on if your weave accepts
`zen.Activated` itself.

Source: [`activation/activation.hpp`](../activation.hpp). Link
`zengine::activation`. Header-only over the Loom's lifecycle vocabulary and its `WeaveId`, so it
exists in every configuration, with or without a kernel.

What an activation means is the Loom's, not this package's: `zen.Activated{sequence}` says that
a new incarnation committed at this address, and nothing else — not healthy, not ready, not
"start a loop" ([lifecycle laws](https://github.com/Krealsion/Loom/blob/main/docs/laws/lifecycle-laws.md),
LIFE-01). What a weave does with an accepted activation is the weave's own business. The cursor
is only the bookkeeping needed to answer "is this one mine, and is it new?" the same way in every
weave: a weave keeps one as a member, offers it each `zen.Activated` it receives, and does its
once-per-activation work only when `accept` returns true.

## Two questions, in this order

`ActivationCursor::accept` answers two different questions, and keeping them apart is the whole
of the design.

1. **Provenance: did the Loom itself attest a lifecycle commit for this incarnation?** The Loom
   answers it, not the cursor. `Mail::lifecycle_attested()` is a delivery fact the bus sets and
   no payload can carry; only host infrastructure holding the Loom's lifecycle authority can
   attest (LIFE-04). The attested sequence (`Mail::attested_sequence()`) must also equal the
   payload's, so an attestation minted for one activation cannot authenticate another. An
   unattested `zen.Activated` — however well formed, however plausible its sequence, whoever
   sent it — is an ordinary message in the shape of a lifecycle fact, and the cursor ignores it
   entirely.
2. **Lineage: has this weave already acted on it?** The cursor answers it. Among attested
   activations, identity is the pair (attested sender, sequence) (LIFE-03). A sequence must be
   positive; from the current sender it must be newer than the last one accepted; a same-sender
   sequence that is not newer is a duplicate or a replay and changes nothing, so a re-delivery
   cannot make anything happen twice.

One call owns both halves so that no consumer has to rediscover the trust rule, and so there is
one place to get it right. It takes the whole `Mail`, not a sender and a number, because the
deciding facts are delivery facts: a signature of loose integers would invite a caller to pass
values read off a payload, and this one makes that mistake unrepresentable.

## Why a sender is never an identity on its own

An activation's identity is never inferred from a stamped sender alone. Read that way, any weave
granted the public shape could manufacture a first breath for someone else's incarnation, and the
consumer could not tell: a different sender would simply read as a new lineage. That is why
provenance comes first and an unattested activation is not a lineage at all.

The sender half still carries weight once provenance holds. A different attested sender is a
different authorized operator's lineage, and its sequences are compared only with its own: a new
operator's 1 is not a replay of the previous operator's 2. A bare sequence is a small integer;
treated as an identity by itself, a replayed number would be indistinguishable from a real
succession.

## A new incarnation owes nothing to the last

A freshly constructed cursor is unactivated, and that is deliberate. A cursor belongs to one
incarnation: whatever was sent for the previous incarnation cannot make this one act, even if it
arrives before this incarnation's own activation.

## Carrying the key on a wire

A weave that tags its own traffic with the activation it was sent under carries two halves:
`sender_text()` and `sequence()`. The sender travels as canonical decimal Text because a
`WeaveId` is an unsigned 64-bit value and the wire's `Int` is signed, so an `Int` field would
narrow the top half of the range silently. Decimal Text is lossless, and it is the spelling the
Loom already uses for a `WeaveId` on the wire: the kernel's control door answers a load with the
new weave's id as `zen.Result` text, and the Weave Manager parses it back. `matches(sender_text,
sequence)` is true only when both halves name the current activation: a matching sequence under a
different sender is another lineage's traffic, not this one's.

## What it does not claim

- **That the host's wiring is the right one.** An accepted activation proves the Loom authorized
  the commit. Which infrastructure holds the lifecycle authority is the host's decision, and a
  host that hands it to two operators has two lineages by its own choice.
- **Anything across a process boundary.** An out-of-process weave receives no attestation, so a
  cursor there accepts no activation.

## Tests

Zengine suite `timer` exercises the cursor through the Timer and its binding: an unattested
activation, a replay, a newer sequence and a second operator's lineage.
