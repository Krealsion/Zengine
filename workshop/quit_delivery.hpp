// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_QUIT_DELIVERY_HPP
#define ZENGINE_WORKSHOP_QUIT_DELIVERY_HPP

// The host's witness of a quit question Loom could not deliver. The question is a publication,
// which `zen.DispatchRefused` does not cover, so the host observes the bus tap's `Refused` event
// (sender, target, correlation), writes it in a book in `HostContext`, and wakes Workshop with an
// empty delivery. The wake-up is not authority: Workshop acts only on an entry for its own quit.

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
