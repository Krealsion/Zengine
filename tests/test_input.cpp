// The Input suite — the Input package V1, proven headless.
//
// Four tiers, deliberately ordered:
//   1. CONTRACT pins — the ZEN_SHAPE spellings in input/vocabulary.hpp derive
//      schemas content-id-identical to the locked contract's SchemaBuilder
//      spellings, and the scan:: constants ARE the SDL scancode values,
//      pinned as literals. A drift is a red test, not an opinion.
//   2. TRANSLATION pins — native events to locked shapes as pure math, both
//      backends on every lane (the Win32 paths take plain integers, so the
//      WSL run pins the Windows translation and the Windows run pins the
//      terminal's).
//   3. THE WEAVE through a real bus — an injected reader feeds scripted
//      batches; every one of the five shapes is published, delivered, and
//      heard in order by an ordinary accepter.
//   4. THE REAL LIBRARIES through the real Kernel — the zengine-input .so
//      loads into its role and answers its pump (headless: no console, so its
//      honest counters say "pumped, nothing to say"), and the snake-controls
//      .so turns a published KeyPressed into a SnakeTurn that steers the real
//      world .so — the chain that replaced the host's key-reading.
//
// What headless CANNOT prove is the platform edge itself (a real terminal's
// bytes arriving at the reader): tier 3 pins the weave from the reader in,
// tier 2 pins the translation the readers call, and the live pty run drives
// the whole chain through the real host — stated here so the suite's green
// means exactly what it says.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "input/input_weave.hpp"
#include "input/translate.hpp"
#include "input/vocabulary.hpp"

#include "lifecycle_door.hpp"

#include "timer/vocabulary.hpp" // the weave's own beat is part of its contract now
#include "vocabulary.hpp"       // snake's — the chain lane speaks both packages

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using namespace zengine::input;
using loom::schema_of;

namespace {

// ---- shared helpers ----------------------------------------------------------

std::vector<InputEvent> term(std::string_view bytes) {
    return terminal_bytes_to_events(reinterpret_cast<const unsigned char*>(bytes.data()),
                                    bytes.size());
}

/// The event at [i], REQUIRE'd to hold shape E (a wrong shape is a clean red,
/// never a null deref).
template <class E>
const E& as(const std::vector<InputEvent>& events, std::size_t i) {
    REQUIRE(i < events.size());
    const E* e = std::get_if<E>(&events[i]);
    REQUIRE(e != nullptr);
    return *e;
}

/// Assert events[i] and events[i+1] are the press/release pair a terminal
/// stroke synthesizes, and step past them.
void expect_stroke(const std::vector<InputEvent>& events, std::size_t& i, std::int64_t sc,
                   const std::string& name) {
    const KeyPressed& down = as<KeyPressed>(events, i);
    CHECK(down.scancode == sc);
    CHECK(down.name == name);
    const KeyReleased& up = as<KeyReleased>(events, i + 1);
    CHECK(up.scancode == sc);
    CHECK(up.name == name);
    i += 2;
}

// ---- tier 3/4 rigs -----------------------------------------------------------

/// A scripted reader: each poll() hands over the next batch. What the real
/// readers fetch from the platform, this one takes from the test.
struct FakeReader {
    std::vector<std::vector<InputEvent>>* feed = nullptr;
    std::vector<InputEvent> poll() {
        if (feed == nullptr || feed->empty()) {
            return {};
        }
        std::vector<InputEvent> batch = std::move(feed->front());
        feed->erase(feed->begin());
        return batch;
    }
};

struct EarsState {
    std::int64_t heard = 0;
    ZEN_SHAPE(EarsState, 1, ZEN_FIELD(heard));
};

/// An ordinary accepter of all five shapes — what any consumer of the Input
/// package looks like to the bus. Records exactly what it is delivered.
class Ears : public loom::WeaveBase<Ears, EarsState,
                                    loom::Accept<KeyPressed, KeyReleased, MouseButton,
                                                 MouseMoved, MouseWheel>,
                                    loom::Emit<>> {
public:
    explicit Ears(std::vector<InputEvent>& heard) : heard_(&heard) {}
    void on(const KeyPressed& e, loom::Mail&) { note(e); }
    void on(const KeyReleased& e, loom::Mail&) { note(e); }
    void on(const MouseButton& e, loom::Mail&) { note(e); }
    void on(const MouseMoved& e, loom::Mail&) { note(e); }
    void on(const MouseWheel& e, loom::Mail&) { note(e); }

private:
    template <class E>
    void note(const E& e) {
        ++state_.heard;
        heard_->push_back(e);
    }
    std::vector<InputEvent>* heard_;
};

/// What the kernel-lane witness has seen (the snake suite's Recorded, sized
/// to this suite's needs).
struct Seen {
    struct Answer {
        std::uint64_t corr = 0;
        int kind = 0; // 0 Result, 1 Ack, 2 Refused
        std::string text;
    };
    std::vector<Answer> answers;
    std::vector<zengine::snake::SnakeVisual> visuals;

    const Answer* find(std::uint64_t corr) const {
        for (const Answer& a : answers) {
            if (a.corr == corr) {
                return &a;
            }
        }
        return nullptr;
    }
};

struct WitnessState {
    std::int64_t noted = 0;
    ZEN_SHAPE(WitnessState, 1, ZEN_FIELD(noted));
};

/// What the weave asked the Timer package for, field by field.
struct BeatAsk {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role;
    std::string preferred;
    std::string fallback;
};

struct CatcherState {
    std::int64_t asks = 0;
    ZEN_SHAPE(CatcherState, 1, ZEN_FIELD(asks));
};

/// A stand-in TimerService: holds the timer role and records the exact asks.
/// It never beats — the case that uses it delivers the firings by hand, so
/// the weave's side of the contract is pinned without a clock in sight.
class BeatCatcher
    : public loom::WeaveBase<BeatCatcher, CatcherState,
                             loom::Accept<zengine::timer::EnsureRoleTimer>, loom::Emit<>> {
public:
    explicit BeatCatcher(std::vector<BeatAsk>& asks) : asks_(&asks) {}
    void on(const zengine::timer::EnsureRoleTimer& s, loom::Mail&) {
        ++state_.asks;
        asks_->push_back(BeatAsk{s.id, s.delay_ms, s.repeat, s.role, s.preferred, s.fallback});
    }

private:
    std::vector<BeatAsk>* asks_;
};

/// mount(), plus a role binding (the weave sugar has no role parameter; the
/// catcher must HOLD zengine.timer so the weave's role-addressed ask lands).
template <class W, class... Args>
loom::WeaveId mount_into_role(loom::Switchboard& bus, std::string role, Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    loom::Grant grant = loom::emit_default_grant(*raw);
    loom::allow_poke_answers(grant);
    const loom::WeaveId id =
        bus.register_weave(std::move(weave), std::move(grant), std::move(role));
    raw->zen_set_self(id);
    return id;
}

/// The kernel lane's hand and ears: holds the manager reach, hears the
/// standard answers, and accepts SnakeVisual so the world's motion is
/// observable. Deliberately does NOT accept the input shapes — the lane's
/// recipient-count assertions mean "the controls adapter, and nobody else".
class Witness
    : public loom::WeaveBase<Witness, WitnessState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused,
                                          zengine::snake::SnakeVisual>,
                             loom::Emit<>> {
public:
    explicit Witness(Seen& seen) : seen_(&seen) {}
    void on(const loom::Result& r, loom::Mail& mail) { note(mail, 0, r.value); }
    void on(const loom::Ack&, loom::Mail& mail) { note(mail, 1, ""); }
    void on(const loom::Refused& r, loom::Mail& mail) { note(mail, 2, r.reason); }
    void on(const zengine::snake::SnakeVisual& v, loom::Mail&) { seen_->visuals.push_back(v); }

private:
    void note(loom::Mail& mail, int kind, std::string text) {
        ++state_.noted;
        seen_->answers.push_back(Seen::Answer{mail.correlation(), kind, std::move(text)});
    }
    Seen* seen_;
};

struct Rig {
    loom::Switchboard bus;
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    Seen seen;
    loom::WeaveId witness = mount_witness();
    std::uint64_t next_corr = 1;

    loom::WeaveId mount_witness() {
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        return loom::mount_granted<Witness>(bus, std::move(reach), seen);
    }

    loom::WeaveId load(const char* name, const char* path, const char* role) {
        const std::uint64_t corr = next_corr++;
        bus.send_as(witness, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{name, path, role}), witness,
                                  witness, corr));
        bus.pump();
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        REQUIRE_MESSAGE(a->kind == 0, "load refused: ", a->text);
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(a->text))};
    }

    /// Root-send a poke with the witness as the answer address; return the
    /// answer by value (the recorder's vector grows — no references into it).
    template <class Poke>
    Seen::Answer poke(loom::WeaveId target, const Poke& p) {
        const std::uint64_t corr = next_corr++;
        bus.send(target, loom::Message(loom::to_value(p), loom::WeaveId{}, witness, corr));
        bus.pump();
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        return *a;
    }

    std::size_t publish_root(const loom::Value& v) {
        const std::size_t n = bus.publish(loom::Message(v, loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.pump();
        return n;
    }

    void pump_input_by_role() {
        bus.send_to_role(kInputRole, loom::Message(loom::to_value(PumpInput{})));
        bus.pump();
    }

    void tick() {
        bus.send_to_role(zengine::snake::kWorldRole,
                         loom::Message(loom::to_value(zengine::snake::SnakeTick{})));
        bus.pump();
    }
};

} // namespace

// ============================================================================
// Tier 1 — the locked contract, pinned by content-id
// ============================================================================

TEST_CASE("contract: ZEN_SHAPE spellings derive the locked schemas exactly") {
    using loom::Kind;
    using loom::SchemaBuilder;

    CHECK(schema_of<KeyPressed>()->content_id() == SchemaBuilder("KeyPressed", 1)
                                                       .field("scancode", Kind::Int)
                                                       .field("name", Kind::Text)
                                                       .build()
                                                       ->content_id());
    CHECK(schema_of<KeyReleased>()->content_id() == SchemaBuilder("KeyReleased", 1)
                                                        .field("scancode", Kind::Int)
                                                        .field("name", Kind::Text)
                                                        .build()
                                                        ->content_id());
    CHECK(schema_of<MouseButton>()->content_id() == SchemaBuilder("MouseButton", 1)
                                                        .field("button", Kind::Int)
                                                        .field("pressed", Kind::Bool)
                                                        .build()
                                                        ->content_id());
    CHECK(schema_of<MouseMoved>()->content_id() == SchemaBuilder("MouseMoved", 1)
                                                       .field("x", Kind::Float)
                                                       .field("y", Kind::Float)
                                                       .field("dx", Kind::Float)
                                                       .field("dy", Kind::Float)
                                                       .build()
                                                       ->content_id());
    CHECK(schema_of<MouseWheel>()->content_id() == SchemaBuilder("MouseWheel", 1)
                                                       .field("dx", Kind::Float)
                                                       .field("dy", Kind::Float)
                                                       .build()
                                                       ->content_id());
    // The named addition is frozen the same way the locked five are.
    CHECK(schema_of<PumpInput>()->content_id() ==
          SchemaBuilder("PumpInput", 1).build()->content_id());
}

TEST_CASE("contract: scan:: constants ARE the SDL scancode values") {
    // Pinned as the literals from SDL_scancode.h (== USB HID usage ids) — the
    // identity rule's whole meaning. A representative spread: the letter run's
    // ends and the snake keys, the digit run's ends, every special the
    // backends translate, and the arrow block.
    CHECK(scan::kA == 4);
    CHECK(scan::kC == 6);
    CHECK(scan::kD == 7);
    CHECK(scan::kQ == 20);
    CHECK(scan::kR == 21);
    CHECK(scan::kS == 22);
    CHECK(scan::kW == 26);
    CHECK(scan::kZ == 29);
    CHECK(scan::k1 == 30);
    CHECK(scan::k9 == 38);
    CHECK(scan::k0 == 39);
    CHECK(scan::kReturn == 40);
    CHECK(scan::kEscape == 41);
    CHECK(scan::kBackspace == 42);
    CHECK(scan::kTab == 43);
    CHECK(scan::kSpace == 44);
    CHECK(scan::kMinus == 45);
    CHECK(scan::kSlash == 56);
    CHECK(scan::kRight == 79);
    CHECK(scan::kLeft == 80);
    CHECK(scan::kDown == 81);
    CHECK(scan::kUp == 82);
}

// ============================================================================
// Tier 2 — translation as pure math (both backends, every lane)
// ============================================================================

TEST_CASE("terminal: letters, digits, and specials become stroke pairs") {
    std::size_t i = 0;
    auto ev = term("w");
    expect_stroke(ev, i, scan::kW, "W");
    CHECK(i == ev.size());

    i = 0;
    ev = term("W"); // shift survives only in the byte; same key identity
    expect_stroke(ev, i, scan::kW, "W");

    i = 0;
    ev = term("wasd"); // the snake controls, whole
    expect_stroke(ev, i, scan::kW, "W");
    expect_stroke(ev, i, scan::kA, "A");
    expect_stroke(ev, i, scan::kS, "S");
    expect_stroke(ev, i, scan::kD, "D");
    CHECK(i == ev.size());

    i = 0;
    ev = term("30");
    expect_stroke(ev, i, scan::k3, "3");
    expect_stroke(ev, i, scan::k0, "0");

    i = 0;
    ev = term(" \r\n\t");
    expect_stroke(ev, i, scan::kSpace, "Space");
    expect_stroke(ev, i, scan::kReturn, "Return");
    expect_stroke(ev, i, scan::kReturn, "Return");
    expect_stroke(ev, i, scan::kTab, "Tab");

    i = 0;
    ev = term("\x7f\x08");
    expect_stroke(ev, i, scan::kBackspace, "Backspace");
    expect_stroke(ev, i, scan::kBackspace, "Backspace");

    i = 0;
    ev = term("/;");
    expect_stroke(ev, i, scan::kSlash, "/");
    expect_stroke(ev, i, scan::kSemicolon, ";");
}

TEST_CASE("terminal: the arrow sequences and the bare escape") {
    std::size_t i = 0;
    auto ev = term("\x1b[A\x1b[B\x1b[C\x1b[D");
    expect_stroke(ev, i, scan::kUp, "Up");
    expect_stroke(ev, i, scan::kDown, "Down");
    expect_stroke(ev, i, scan::kRight, "Right");
    expect_stroke(ev, i, scan::kLeft, "Left");
    CHECK(i == ev.size());

    i = 0;
    ev = term("\x1b");
    expect_stroke(ev, i, scan::kEscape, "Escape");
    CHECK(i == ev.size());

    // Batch-local parse (the stance the host always took): an ESC that does
    // not open a whole arrow sequence within the batch is the Escape key, and
    // the leftover byte is itself.
    i = 0;
    ev = term("\x1b[");
    expect_stroke(ev, i, scan::kEscape, "Escape");
    expect_stroke(ev, i, scan::kLeftBracket, "[");
    CHECK(i == ev.size());

    // An unknown CSI final: ESC stands alone, '[' and the final translate as
    // themselves ('Z' is a real key).
    i = 0;
    ev = term("\x1b[Z");
    expect_stroke(ev, i, scan::kEscape, "Escape");
    expect_stroke(ev, i, scan::kLeftBracket, "[");
    expect_stroke(ev, i, scan::kZ, "Z");
}

TEST_CASE("terminal: Ctrl+C keeps its key identity; the modifier rides the name") {
    std::size_t i = 0;
    auto ev = term("\x03");
    expect_stroke(ev, i, scan::kC, "Ctrl+C");
    CHECK(i == ev.size());
}

TEST_CASE("contract, EXPLICITLY TEMPORARY: \"Ctrl+C\" is the one name the host may branch on") {
    // A debt, pinned as a debt. This package's own rule is that `name` is
    // convenience and never authority — and the snake host's quit path
    // nonetheless reads `k.name == "Ctrl+C"`, because V1 has no modifier
    // vocabulary and a scancode alone cannot say "with Ctrl held". Both sides
    // confess it in their comments; neither can fix it alone.
    //
    // So this pin is not an endorsement, it is a contract with an expiry: as
    // long as the debt exists, the two backends must agree on the spelling
    // BYTE FOR BYTE, and it must be the spelling the host matches. When the
    // modifier-vocabulary phase gives KeyPressed real modifiers, the host
    // branches on those instead and THIS CASE IS DELETED — not loosened.
    const std::string terminal_name = as<KeyPressed>(term("\x03"), 0).name;
    const std::string win32_name =
        as<KeyPressed>(win32_key_to_events('C', true, /*ctrl=*/true), 0).name;

    CHECK(terminal_name == win32_name); // the two backends agree...
    CHECK(terminal_name == "Ctrl+C");   // ...on exactly the string play.cpp tests
    // And the same key without the modifier must NOT wear the name — the host
    // quits on it, so a plain 'c' dressed as Ctrl+C would end the game.
    CHECK(as<KeyPressed>(win32_key_to_events('C', true, /*ctrl=*/false), 0).name != "Ctrl+C");
    // The scancode stays the authority either way: same key, both paths.
    CHECK(as<KeyPressed>(term("\x03"), 0).scancode == scan::kC);
    CHECK(as<KeyPressed>(win32_key_to_events('C', true, true), 0).scancode == scan::kC);
}

TEST_CASE("terminal: untranslatable bytes are dropped, order is preserved") {
    CHECK(term("\x01").empty()); // Ctrl+A: no modifier vocabulary in V1
    std::size_t i = 0;
    auto ev = term("wa\x1b[Cq");
    expect_stroke(ev, i, scan::kW, "W");
    expect_stroke(ev, i, scan::kA, "A");
    expect_stroke(ev, i, scan::kRight, "Right");
    expect_stroke(ev, i, scan::kQ, "Q");
    CHECK(i == ev.size());
}

TEST_CASE("win32 keys: real transitions, VK identities, dropped modifiers") {
    auto ev = win32_key_to_events(0x57 /*'W'*/, true, false);
    REQUIRE(ev.size() == 1);
    CHECK(as<KeyPressed>(ev, 0).scancode == scan::kW);
    CHECK(as<KeyPressed>(ev, 0).name == "W");

    ev = win32_key_to_events(0x57, false, false); // a REAL release — nothing synthesized
    REQUIRE(ev.size() == 1);
    CHECK(as<KeyReleased>(ev, 0).scancode == scan::kW);

    ev = win32_key_to_events(win32::kVkUp, true, false);
    REQUIRE(ev.size() == 1);
    CHECK(as<KeyPressed>(ev, 0).scancode == scan::kUp);

    ev = win32_key_to_events(win32::kVkEscape, true, false);
    REQUIRE(ev.size() == 1);
    CHECK(as<KeyPressed>(ev, 0).scancode == scan::kEscape);
    CHECK(as<KeyPressed>(ev, 0).name == "Escape");

    ev = win32_key_to_events('3', true, false);
    REQUIRE(ev.size() == 1);
    CHECK(as<KeyPressed>(ev, 0).scancode == scan::k3);

    ev = win32_key_to_events('C', true, /*ctrl=*/true);
    REQUIRE(ev.size() == 1);
    CHECK(as<KeyPressed>(ev, 0).scancode == scan::kC);
    CHECK(as<KeyPressed>(ev, 0).name == "Ctrl+C");

    ev = win32_key_to_events('C', true, /*ctrl=*/false);
    REQUIRE(ev.size() == 1);
    CHECK(as<KeyPressed>(ev, 0).name == "C");

    CHECK(win32_key_to_events(0x10 /*VK_SHIFT*/, true, false).empty());
}

TEST_CASE("win32 mouse: moves carry deltas, buttons are transitions, wheels are notches") {
    MouseTrack track;

    // First move: a position, no invented delta.
    auto ev = win32_mouse_to_events(track, 10, 5, 0, win32::kMouseMoved);
    REQUIRE(ev.size() == 1);
    CHECK(as<MouseMoved>(ev, 0).x == 10.0);
    CHECK(as<MouseMoved>(ev, 0).y == 5.0);
    CHECK(as<MouseMoved>(ev, 0).dx == 0.0);
    CHECK(as<MouseMoved>(ev, 0).dy == 0.0);

    ev = win32_mouse_to_events(track, 13, 4, 0, win32::kMouseMoved);
    REQUIRE(ev.size() == 1);
    CHECK(as<MouseMoved>(ev, 0).dx == 3.0);
    CHECK(as<MouseMoved>(ev, 0).dy == -1.0);

    // Left press: a button event has no move flag.
    ev = win32_mouse_to_events(track, 13, 4, win32::kButtonLeft, 0);
    REQUIRE(ev.size() == 1);
    CHECK(as<MouseButton>(ev, 0).button == 1);
    CHECK(as<MouseButton>(ev, 0).pressed);

    // A drag: the held button is unchanged, so only the move speaks.
    ev = win32_mouse_to_events(track, 14, 4, win32::kButtonLeft, win32::kMouseMoved);
    REQUIRE(ev.size() == 1);
    CHECK(as<MouseMoved>(ev, 0).dx == 1.0);

    // Left up + right down in one record: two transitions, locked numbering
    // (1 left, 2 middle, 3 right).
    ev = win32_mouse_to_events(track, 14, 4, win32::kButtonRight, 0);
    REQUIRE(ev.size() == 2);
    CHECK(as<MouseButton>(ev, 0).button == 1);
    CHECK_FALSE(as<MouseButton>(ev, 0).pressed);
    CHECK(as<MouseButton>(ev, 1).button == 3);
    CHECK(as<MouseButton>(ev, 1).pressed);

    ev = win32_mouse_to_events(track, 14, 4, win32::kButtonRight | win32::kButtonMiddle, 0);
    REQUIRE(ev.size() == 1);
    CHECK(as<MouseButton>(ev, 0).button == 2);
    CHECK(as<MouseButton>(ev, 0).pressed);

    // Wheel: +120 per notch in the high word; the wire speaks +1.0 per notch.
    ev = win32_mouse_to_events(track, 0, 0, 120u << 16, win32::kMouseWheeled);
    REQUIRE(ev.size() == 1);
    CHECK(as<MouseWheel>(ev, 0).dx == 0.0);
    CHECK(as<MouseWheel>(ev, 0).dy == 1.0);

    ev = win32_mouse_to_events(track, 0, 0,
                               static_cast<std::uint32_t>(static_cast<std::uint16_t>(-120))
                                   << 16,
                               win32::kMouseWheeled);
    REQUIRE(ev.size() == 1);
    CHECK(as<MouseWheel>(ev, 0).dy == -1.0);

    ev = win32_mouse_to_events(track, 0, 0, 240u << 16, win32::kMouseHWheeled);
    REQUIRE(ev.size() == 1);
    CHECK(as<MouseWheel>(ev, 0).dx == 2.0);
    CHECK(as<MouseWheel>(ev, 0).dy == 0.0);
}

// ============================================================================
// Tier 3 — the weave through a real bus (injected reader)
// ============================================================================

TEST_CASE("the weave publishes what its reader hears, in order, all five shapes") {
    loom::Switchboard bus;
    std::vector<std::vector<InputEvent>> feed;
    feed.push_back({KeyPressed{scan::kW, "W"}, KeyReleased{scan::kW, "W"}, MouseButton{1, true},
                    MouseMoved{3.0, 4.0, 1.0, -1.0}, MouseWheel{0.0, 1.0}});

    const loom::WeaveId weave = loom::mount<InputWeaveT<FakeReader>>(bus, FakeReader{&feed});
    std::vector<InputEvent> heard;
    (void)loom::mount<Ears>(bus, heard);

    const auto pump_input = [&] {
        bus.send(weave, loom::Message(loom::to_value(PumpInput{})));
        bus.pump();
    };

    pump_input();
    REQUIRE(heard.size() == 5);
    CHECK(as<KeyPressed>(heard, 0).scancode == scan::kW);
    CHECK(as<KeyPressed>(heard, 0).name == "W");
    CHECK(as<KeyReleased>(heard, 1).scancode == scan::kW);
    CHECK(as<MouseButton>(heard, 2).button == 1);
    CHECK(as<MouseButton>(heard, 2).pressed);
    CHECK(as<MouseMoved>(heard, 3).x == 3.0);
    CHECK(as<MouseMoved>(heard, 3).y == 4.0);
    CHECK(as<MouseMoved>(heard, 3).dx == 1.0);
    CHECK(as<MouseMoved>(heard, 3).dy == -1.0);
    CHECK(as<MouseWheel>(heard, 4).dy == 1.0);

    // A quiet platform: the pump costs nothing and says nothing.
    pump_input();
    CHECK(heard.size() == 5);
}

TEST_CASE("the weave arranges its own beat: activation asks, TimerReady asks again, "
          "the beat polls, alien ids do not") {
    loom::Switchboard bus;
    std::vector<std::vector<InputEvent>> feed;
    feed.push_back({KeyPressed{scan::kD, "D"}});

    const loom::WeaveId weave = loom::mount<InputWeaveT<FakeReader>>(bus, FakeReader{&feed});
    std::vector<InputEvent> heard;
    (void)loom::mount<Ears>(bus, heard);
    std::vector<BeatAsk> asks;
    (void)mount_into_role<BeatCatcher>(bus, zengine::timer::kTimerRole, asks);
    // R2B-1: a first breath is Loom's to grant. The suite activates through a
    // real lifecycle operator holding a real authority, because that is the only
    // way a weave can be activated at all now — hand-posting the public shape
    // is exactly the forgery the attestation refuses.
    const loom::WeaveId door = zengine::testing::mount_door(bus);

    // ITS OWN ACTIVATION is its first breath (R2A-2): the weave asks for ITS
    // beat, wire content pinned field by field. This is the trigger that makes
    // load order stop mattering — loaded long after the Timer, it still asks.
    zengine::testing::order_activation(bus, door, weave, 1);
    bus.pump();
    REQUIRE(asks.size() == 1);
    CHECK(asks[0].id == kPumpTimerId);
    CHECK(asks[0].delay_ms == kPumpBeatMs);
    CHECK(asks[0].repeat);
    CHECK(asks[0].role == kInputRole);
    // The ORDER travels with the ask (R2B-0): this weave prefers to keep the
    // remaining time across a Timer succession, and accepts a restart when there
    // is nothing to keep.
    CHECK(asks[0].preferred == zengine::timer::kPreserveRemaining);
    CHECK(asks[0].fallback == zengine::timer::kRestartDelay);

    // A duplicate activation asks for nothing — the cursor's whole job, and
    // what keeps a re-delivered stimulus from doing non-idempotent work.
    zengine::testing::order_activation(bus, door, weave, 1);
    bus.pump();
    CHECK(asks.size() == 1);

    // The TimerService's availability notice arrives (root-published, as the
    // fanout would deliver it — and the recipient count pins that in this rig
    // the input weave is the ONLY listener): the weave asks AGAIN. That is the
    // opposite load order's rescue, and it is an upsert, never a doubling.
    CHECK(bus.publish(loom::Message(loom::to_value(zengine::timer::TimerReady{}))) == 1);
    bus.pump();
    REQUIRE(asks.size() == 2);
    CHECK(asks[1].id == kPumpTimerId);
    CHECK(asks[1].role == kInputRole);

    // The beat opens the same hands the pump does...
    bus.send(weave, loom::Message(loom::to_value(zengine::timer::TimerFired{kPumpTimerId})));
    bus.pump();
    REQUIRE(heard.size() == 1);
    CHECK(as<KeyPressed>(heard, 0).scancode == scan::kD);

    // ...and someone else's timer aimed at this role opens nothing.
    feed.push_back({KeyPressed{scan::kW, "W"}});
    bus.send(weave, loom::Message(loom::to_value(zengine::timer::TimerFired{"someone.else"})));
    bus.pump();
    CHECK(heard.size() == 1); // the alien id did not poll the reader
}

// ============================================================================
// Tier 4 — the real libraries through the real Kernel
// ============================================================================

TEST_CASE("the input .so loads into its role and answers its pump, honestly idle") {
    // Headless is the honest case here: stdin is not a console, the reader
    // stays disabled, and the counters must say "I ran; I had nothing".
    // (A dev running this exe from a live terminal briefly lends the weave
    // raw mode — restored at unload — and it still emits only what is typed,
    // which during ctest is nothing.)
    Rig r;
    const loom::WeaveId input_id = r.load("zengine-input", INPUT_SO, kInputRole);

    r.pump_input_by_role();
    r.pump_input_by_role();
    r.pump_input_by_role();

    Seen::Answer a = r.poke(input_id, loom::PokeRead{"pumped"});
    CHECK(a.kind == 0);
    CHECK(a.text == "3");
    a = r.poke(input_id, loom::PokeRead{"emitted"});
    CHECK(a.kind == 0);
    CHECK(a.text == "0");
}

TEST_CASE("keys become turns: KeyPressed steers the real world through the real adapter") {
    using namespace zengine::snake;
    Rig r;
    (void)r.load("snake-world-v1", WORLD_V1_SO, kWorldRole);
    const loom::WeaveId controls = r.load("snake-controls", CONTROLS_SO, "");

    // First tick seeds: head centered at (12,8) on the 24x16 board, heading
    // right (the snake suite's own seed pin).
    r.tick();
    REQUIRE(!r.seen.visuals.empty());
    REQUIRE(r.seen.visuals.back().snake.size() == 3);
    CHECK(r.seen.visuals.back().snake.front().x == 12);
    CHECK(r.seen.visuals.back().snake.front().y == 8);

    // W arrives as a published message — the producer's stand-in is the root,
    // speaking the exact shape the input weave publishes (tier 3 pinned that
    // emission; the pty run drives the whole chain live). Exactly ONE accepter
    // exists: the controls adapter. The world turns up on the next tick.
    CHECK(r.publish_root(loom::to_value(KeyPressed{scan::kW, "W"})) == 1);
    r.tick();
    CHECK(r.seen.visuals.back().snake.front().x == 12);
    CHECK(r.seen.visuals.back().snake.front().y == 7);

    // A non-steering key is delivered (the adapter accepts all KeyPressed)
    // and correctly does nothing: still climbing.
    CHECK(r.publish_root(loom::to_value(KeyPressed{scan::kX, "X"})) == 1);
    r.tick();
    CHECK(r.seen.visuals.back().snake.front().y == 6);

    // The doors snake deliberately does not open: releases and the mouse
    // have ZERO accepters in this game — published, delivered to nobody,
    // and that is legal (the recipient count is the pin).
    CHECK(r.publish_root(loom::to_value(KeyReleased{scan::kW, "W"})) == 0);
    CHECK(r.publish_root(loom::to_value(MouseMoved{1.0, 2.0, 1.0, 2.0})) == 0);
    CHECK(r.publish_root(loom::to_value(MouseButton{1, true})) == 0);
    CHECK(r.publish_root(loom::to_value(MouseWheel{0.0, 1.0})) == 0);

    // The adapter's own honest counter: one steering key became one turn.
    const Seen::Answer a = r.poke(controls, loom::PokeRead{"turns"});
    CHECK(a.kind == 0);
    CHECK(a.text == "1");
}
