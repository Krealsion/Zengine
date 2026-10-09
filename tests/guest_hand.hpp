// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_GUEST_HAND_HPP
#define ZENGINE_TESTS_GUEST_HAND_HPP
// A GUEST'S HAND AND THE WEAVER'S, IN ANY LIVE WORKSHOP RIG: the real Input weave in its office, a
// participant that opens an input session and injects through it, the host seam answering that
// participant as a row a guest door admitted -- its name, powers and version -- on the host a case
// chooses, and the weaver's own attributed moments beside it. A case on what a guest's hand may do
// on a weaver's or a development host starts here; docs/contributing/testing-workshop-panes.md says
// which traps are the rig's.
#include "workshop_support.hpp"
#include "input/input_weave.hpp"
#include "workshop/actor_scope.hpp"
#include <zen/host/grant_wiring.hpp>

#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace guest_hand {

struct GuestHandState {
    ZEN_SHAPE(GuestHandState, 1);
};
struct GuestHandDo {
    ZEN_SHAPE(GuestHandDo, 1);
};

/// The guest: it asks Input for a session and injects through it, and keeps what it is told.
class GuestHandWeave
    : public loom::WeaveBase<GuestHandWeave, GuestHandState,
                             loom::Accept<GuestHandDo, input::InputSessionOpened, input::InputInjected,
                                          loom::Refused>,
                             loom::Emit<input::InputSessionRequested, input::InjectInput>> {
public:
    std::function<void(loom::Mail&)> next;
    std::int64_t session = 0;
    std::vector<std::string> refusals;
    void on(const GuestHandDo&, loom::Mail& m) {
        if (next) next(m);
    }
    void on(const input::InputSessionOpened& s, loom::Mail&) { session = s.session; }
    void on(const input::InputInjected&, loom::Mail&) {}
    void on(const loom::Refused& r, loom::Mail&) { refusals.push_back(r.reason); }
};

/// The platform's moments, as a case queues them: the weaver's own hand, attributed `local`.
struct WeaverReader {
    using Event = std::variant<input::KeyPressed, input::KeyReleased, input::TextEntered,
                               input::PointerMoved, input::PointerButton, input::PointerWheel>;
    std::shared_ptr<std::vector<Event>> pending;
    std::vector<Event> poll() {
        std::vector<Event> out;
        if (pending) out.swap(*pending);
        return out;
    }
};

struct GuestHand {
    PaneRig& r;
    GuestHandWeave* hand = nullptr;
    loom::WeaveId id{};
    loom::WeaveId input_id{};
    std::shared_ptr<std::vector<WeaverReader::Event>> weaver =
        std::make_shared<std::vector<WeaverReader::Event>>();
    /// What the host seam answers for this participant: a row the guest door admitted.
    zengine::workshop::scope::GuestRowFacts row;

    /// `development` names the host fact: a weaver's host by default. The rig must outlive this
    /// hand, and this hand must not move: the seam answers through it.
    GuestHand(PaneRig& rig, std::vector<std::string> powers, bool development = false,
              std::string name = "agent", std::int64_t version = 2)
        : r(rig) {
        using Input = input::InputWeaveT<WeaverReader>;
        auto reader = std::make_unique<Input>(WeaverReader{weaver});
        Input* raw = reader.get();
        const loom::Grant input_grant = loom::emit_default_grant(*reader);
        input_id = r.bus.register_weave(std::move(reader), input_grant, input::kInputRole);
        raw->zen_set_self(input_id);
        auto actor = std::make_unique<GuestHandWeave>();
        hand = actor.get();
        loom::Grant g;
        g.allow_to_role(input::InputSessionRequested::zen_name, 1, input::kInputRole);
        g.allow_to_role(input::InjectInput::zen_name, 1, input::kInputRole);
        id = r.bus.register_weave(std::move(actor), g);
        hand->zen_set_self(id);
        row.admitted = true;
        row.name = std::move(name);
        row.powers = std::move(powers);
        row.version = version;
        r.host.host_fact.development = development;
        r.host.guest_row = [this](loom::WeaveId who) {
            return who == id ? row : zengine::workshop::scope::GuestRowFacts{};
        };
        if (!r.host.input_authority) {
            r.host.input_authority = [&bus = r.bus](loom::WeaveId who) {
                return bus.alive(who) ? loom::host_grant_authority(bus, who, loom::LiveAuthority::nothing())
                                      : loom::GrantAuthority{};
            };
        }
        act([](loom::Mail& m) {
            m.send_to_role(input::kInputRole, input::InputSessionRequested{"guest hand"});
        });
        REQUIRE(hand->session > 0);
    }
    GuestHand(const GuestHand&) = delete;
    GuestHand& operator=(const GuestHand&) = delete;

    void act(std::function<void(loom::Mail&)> action) {
        hand->next = std::move(action);
        r.bus.send(id, loom::Message(loom::to_value(GuestHandDo{})));
        r.bus.drain_until_idle();
        hand->next = {};
    }

    // ---- the guest's moments, injected -------------------------------------------------
    static input::InjectedEvent key_event(std::int64_t scan, std::int64_t mods, bool down) {
        input::InjectedEvent e;
        e.kind = down ? "KeyPressed" : "KeyReleased";
        e.scancode = scan;
        e.modifiers = mods;
        return e;
    }
    static input::InjectedEvent text_event(const std::string& text) {
        input::InjectedEvent e;
        e.kind = "TextEntered";
        e.text = text;
        return e;
    }
    /// A button at a canvas cell, as the terminal medium reports it (`PaneRig::press_cell`).
    static input::InjectedEvent button_event(std::int64_t cx, std::int64_t cy, bool down, std::int64_t button = 1) {
        input::InjectedEvent e;
        e.kind = "PointerButton";
        e.button = button;
        e.pressed = down;
        e.space = input::space::kCells;
        e.x = cx;
        e.y = cy + surface::kTuiCanvasTopRow;
        return e;
    }
    void inject(std::vector<input::InjectedEvent> events) {
        act([&](loom::Mail& m) {
            m.send_to_role(input::kInputRole, input::InjectInput{hand->session, std::move(events)});
        });
    }
    void key(std::int64_t scan, std::int64_t mods = input::mod::kNone) {
        inject({key_event(scan, mods, true), key_event(scan, mods, false)});
    }
    void text(const std::string& text) { inject({text_event(text)}); }
    void press_cell(std::int64_t cx, std::int64_t cy, std::int64_t button = 1) {
        inject({button_event(cx, cy, true, button), button_event(cx, cy, false, button)});
    }

    // ---- the weaver's moments, through the platform's reader --------------------------------
    void pump() {
        r.bus.send_to_role(input::kInputRole, loom::Message(loom::to_value(input::PumpInput{})));
        r.bus.drain_until_idle();
    }
    void weaver_key(std::int64_t scan, std::int64_t mods = input::mod::kNone) {
        weaver->push_back(input::KeyPressed{scan, "", mods});
        weaver->push_back(input::KeyReleased{scan, "", mods});
        pump();
    }
    void weaver_text(const std::string& text) {
        weaver->push_back(input::TextEntered{text});
        pump();
    }
    void weaver_press_cell(std::int64_t cx, std::int64_t cy, std::int64_t button = 1) {
        weaver->push_back(input::PointerButton{button, true, cx, cy + surface::kTuiCanvasTopRow,
                                               input::space::kCells, input::mod::kNone});
        weaver->push_back(input::PointerButton{button, false, cx, cy + surface::kTuiCanvasTopRow,
                                               input::space::kCells, input::mod::kNone});
        pump();
    }

    /// THE WEAVER'S MOMENTS AND THEN THE GUEST'S, queued before either is delivered: Workshop
    /// routes the weaver's first, and the guest's lands before anything the weaver's act made a
    /// pane ask in return -- the interleaving an approval must survive.
    void weaver_then_guest(std::vector<WeaverReader::Event> mine, std::vector<input::InjectedEvent> theirs) {
        for (auto& e : mine) weaver->push_back(std::move(e));
        r.bus.send_to_role(input::kInputRole, loom::Message(loom::to_value(input::PumpInput{})));
        hand->next = [this, theirs](loom::Mail& m) {
            m.send_to_role(input::kInputRole, input::InjectInput{hand->session, theirs});
        };
        r.bus.send(id, loom::Message(loom::to_value(GuestHandDo{})));
        r.bus.drain_until_idle();
        hand->next = {};
    }
};

} // namespace guest_hand

#endif // ZENGINE_TESTS_GUEST_HAND_HPP
