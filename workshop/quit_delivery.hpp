// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_QUIT_DELIVERY_HPP
#define ZENGINE_WORKSHOP_QUIT_DELIVERY_HPP

// THE HOST'S WITNESS OF A QUIT QUESTION LOOM COULD NOT DELIVER.
//
// Workshop asks whether it may end with one office PUBLICATION (`PaneQuitRequested`), because it
// cannot know which weaves hold a maker's work; Loom fans it out to every live accepter and hands
// back the count, which is the number of answers Workshop then waits for. A delivery Loom REFUSES
// -- the participant is held behind a claim it could not apply, dead, gone, no longer accepts the
// question -- runs no handler, so no answer can ever come for it.
//
// ⚠ LOOM'S `zen.DispatchRefused` DOES NOT COVER A PUBLICATION. `Switchboard::fanout` builds each
// envelope without capturing a refusal recipient, so the notice exists for directed and
// role-addressed sends only; the messaging reference lists publication aggregation among the
// capabilities it does not provide. Accepting the notice would therefore hear nothing, and turning
// the ask into directed sends would need a second copy of fanout's rule for who accepts.
//
// THE REFUSAL IS STILL LOOM'S RECORDED FACT, ON THE TAP: a `BusEvent` of kind `Refused`, its sender
// the bus's own stamp, its target the recipient fanout resolved, its correlation the number the
// sender put on the ask. This host observes that tap -- observation authority stays the host's;
// Workshop holds none -- and writes what one event says into a book that lives in `HostContext`,
// then wakes Workshop with an ordinary delivery that carries nothing. THE WAKE-UP IS NOT
// AUTHORITY: Workshop reads the book, acts only on an entry for the quit it has in flight, and a
// forged wake-up meets a book that says nothing about it.
//
// WHAT THIS IS NOT: a delivered question nobody answers (that silence stays pending); a handler
// that failed after delivery (`HandlerFailed` is a delivery); an escaped exception, which leaves
// the turn as it came. Absence of an entry proves neither a delivery nor a permission.

#include "pane_vocabulary.hpp"

#include <zen/switchboard/switchboard.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// ONE DELIVERY OF A QUIT QUESTION THAT LOOM REFUSED, copied off the tap while the event was valid.
struct UndeliveredQuit {
    std::uint64_t ask = 0;         ///< the correlation the refused envelope carried: the asker's own
    loom::WeaveId participant{};   ///< the recipient fanout resolved, as the bus stamped it
    std::string office;            ///< the office that recipient held at the refusal; empty for none
    loom::RefusalReason reason = loom::RefusalReason::None;
};

/// THE HOST'S BOOK OF REFUSED QUIT DELIVERIES. Written by `QuitDeliveryWatch` and emptied by the
/// Workshop weave at each wake-up, nothing else. Unbounded by type and bounded by what feeds it:
/// one entry per quit envelope Loom refused, every one of which queued a wake-up that empties it.
class UndeliveredQuits {
public:
    void note(UndeliveredQuit one) { noted_.push_back(std::move(one)); }
    /// Everything written since the last take, in the order the refusals happened.
    std::vector<UndeliveredQuit> take() { return std::exchange(noted_, {}); }
    bool empty() const noexcept { return noted_.empty(); }

private:
    std::vector<UndeliveredQuit> noted_;
};

/// THE WAKE-UP: "a quit delivery was refused; read the book". It carries nothing, deliberately, so
/// nothing on the wire can be mistaken for the fact.
struct QuitDeliveryRefusalNoted {
    ZEN_SHAPE(QuitDeliveryRefusalNoted, 1);
};

/// THE OBSERVER, for as long as this object lives. Registered on construction and removed on
/// destruction, so a host declares it after the bus and after the `HostContext` whose book it
/// writes, and it is gone before either. It holds no participant and no weave, only ids.
class QuitDeliveryWatch {
public:
    QuitDeliveryWatch(loom::Switchboard& bus, loom::WeaveId workshop, UndeliveredQuits& book);
    ~QuitDeliveryWatch();
    QuitDeliveryWatch(const QuitDeliveryWatch&) = delete;
    QuitDeliveryWatch& operator=(const QuitDeliveryWatch&) = delete;

private:
    loom::Switchboard& bus_;
    loom::ObserverId tap_ = 0;
};

/// WHO COULD NOT BE ASKED, AND WHY, in a maker's words and Loom's: the office and weave, what the
/// refusal means for that participant, and the reason's own name.
std::string undelivered_quit_words(const UndeliveredQuit& one);

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_QUIT_DELIVERY_HPP
