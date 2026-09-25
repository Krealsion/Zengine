// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The quit delivery watch (`quit_delivery.hpp`): one seam for the host and the suites.

#include "quit_delivery.hpp"

#include <new>
#include <stdexcept>

namespace zengine::workshop {

namespace {

/// WHAT A REFUSAL MEANS FOR THE PARTICIPANT IT NAMES -- the maker's half; Loom's reason follows it.
const char* refusal_meaning(loom::RefusalReason reason) {
    switch (reason) {
    case loom::RefusalReason::ApplicationFailed:
        return "it is held until it is reloaded or removed";
    case loom::RefusalReason::TargetUnavailable:
        return "it is not running";
    case loom::RefusalReason::NoSuchTarget:
        return "it is no longer mounted";
    case loom::RefusalReason::NotAccepted:
        return "it no longer takes the question";
    case loom::RefusalReason::CapabilityDenied:
        return "this Workshop is not granted the question";
    case loom::RefusalReason::GateRefused:
        return "the question did not admit at its door";
    case loom::RefusalReason::SenderLifeEnded:
        return "the Workshop that asked ended before the question arrived";
    default:
        return "Loom refused the delivery";
    }
}

} // namespace

// WL-SESSION-19 -- agents/workshop/session.md
QuitDeliveryWatch::QuitDeliveryWatch(loom::Switchboard& bus, loom::WeaveId workshop,
                                     UndeliveredQuits& book)
    : bus_(bus) {
    tap_ = bus.add_observer([&bus, workshop, &book](const loom::BusEvent& ev) {
        // THREE FACTS, ALL LOOM'S: a delivery was refused, the bus stamped THIS Workshop as its
        // sender, and it carried the quit question. The correlation then names which of that
        // Workshop's own asks it was -- a number only that sender chose -- and Workshop matches
        // it to the quit in flight. A refusal of any other shape, or of anybody else's quit
        // question, is not this ask's fact.
        if (ev.kind != loom::EventKind::Refused || ev.sender != workshop ||
            ev.schema_name != PaneQuitRequested::zen_name ||
            ev.schema_version != PaneQuitRequested::zen_version) {
            return;
        }
        // Copied now, while the event and the participant's record are what they say. Best-effort,
        // as Loom treats its own refusal notice: an allocation failure here must not replace the
        // evidence the delivery produced, and a lost entry leaves the quit waiting.
        try {
            book.note(UndeliveredQuit{ev.correlation, ev.target, bus.role_of(ev.target),
                                      ev.refusal.reason});
            (void)bus.send(workshop, loom::Message(loom::to_value(QuitDeliveryRefusalNoted{}),
                                                   loom::WeaveId{}, loom::WeaveId{}, 0));
        } catch (const std::bad_alloc&) {
        } catch (const std::overflow_error&) {
        }
    });
}

QuitDeliveryWatch::~QuitDeliveryWatch() { bus_.remove_observer(tap_); }

// WL-SESSION-19 -- agents/workshop/session.md
std::string undelivered_quit_words(const UndeliveredQuit& one) {
    const std::string weave = "weave " + std::to_string(one.participant.value);
    return (one.office.empty() ? weave : one.office + " (" + weave + ")") + " -- " +
           refusal_meaning(one.reason) + " (" + loom::name_of(one.reason) + ")";
}

} // namespace zengine::workshop
