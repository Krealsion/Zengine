// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

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

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "input/input_weave.hpp"
#include "input/translate.hpp"
#include "input/translate_sdl.hpp"
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
/// stroke synthesizes AND THAT NOTHING WAS TYPED, then step past them. This is
/// the shape of a key that is a control rather than a character: Escape, an
/// arrow, Backspace, Ctrl+something.
void expect_stroke(const std::vector<InputEvent>& events, std::size_t& i, std::int64_t sc,
                   const std::string& name, std::int64_t mods = mod::kNone) {
    const KeyPressed& down = as<KeyPressed>(events, i);
    CHECK(down.scancode == sc);
    CHECK(down.name == name);
    CHECK(down.modifiers == mods);
    const KeyReleased& up = as<KeyReleased>(events, i + 1);
    CHECK(up.scancode == sc);
    CHECK(up.name == name);
    CHECK(up.modifiers == mods); // the moment is one moment: both halves agree
    i += 2;
}

/// Assert events[i..i+2] are one whole keystroke that PRODUCED TEXT: the
/// transition, what it typed, and the transition back — in that order, because
/// that is the order a consumer reads a moment in.
void expect_typed(const std::vector<InputEvent>& events, std::size_t& i, std::int64_t sc,
                  const std::string& name, const std::string& text,
                  std::int64_t mods = mod::kNone) {
    const KeyPressed& down = as<KeyPressed>(events, i);
    CHECK(down.scancode == sc);
    CHECK(down.name == name);
    CHECK(down.modifiers == mods);
    CHECK(as<TextEntered>(events, i + 1).text == text);
    const KeyReleased& up = as<KeyReleased>(events, i + 2);
    CHECK(up.scancode == sc);
    CHECK(up.modifiers == mods);
    i += 3;
}

/// The text of every TextEntered in order, joined — "what a maker would have
/// seen appear", with the key traffic filtered out.
std::string typed(const std::vector<InputEvent>& events) {
    std::string s;
    for (const InputEvent& e : events) {
        if (const TextEntered* t = std::get_if<TextEntered>(&e)) {
            s += t->text;
        }
    }
    return s;
}

/// Every scancode a batch pressed, in order. Used to assert what a sequence
/// did NOT leak as much as what it delivered.
std::vector<std::int64_t> pressed_codes(const std::vector<InputEvent>& events) {
    std::vector<std::int64_t> out;
    for (const InputEvent& e : events) {
        if (const KeyPressed* k = std::get_if<KeyPressed>(&e)) {
            out.push_back(k->scancode);
        }
    }
    return out;
}

/// Feed a parser one read's worth of bytes (the read boundary is the point).
std::vector<InputEvent> feed(TerminalParser& p, std::string_view bytes) {
    return p.feed(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
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

/// An ordinary accepter of every shape — what any consumer of the Input
/// package looks like to the bus. Records exactly what it is delivered.
class Ears : public loom::WeaveBase<Ears, EarsState,
                                    loom::Accept<KeyPressed, KeyReleased, TextEntered,
                                                 PointerMoved, PointerButton, PointerWheel>,
                                    loom::Emit<>> {
public:
    explicit Ears(std::vector<InputEvent>& heard) : heard_(&heard) {}
    void on(const KeyPressed& e, loom::Mail&) { note(e); }
    void on(const KeyReleased& e, loom::Mail&) { note(e); }
    void on(const TextEntered& e, loom::Mail&) { note(e); }
    void on(const PointerMoved& e, loom::Mail&) { note(e); }
    void on(const PointerButton& e, loom::Mail&) { note(e); }
    void on(const PointerWheel& e, loom::Mail&) { note(e); }

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

TEST_CASE("contract: ZEN_SHAPE spellings derive the intended schemas exactly") {
    using loom::Kind;
    using loom::SchemaBuilder;

    // The contract changed deliberately, and says so here rather than in a report
    // only: the keys are at v2 (they gained `modifiers` — same concept, one more
    // fact), and the three pointer shapes are NEW NAMES at v1 because
    // they are not the old shapes with fields bolted on. `MouseButton` named a
    // field; `PointerButton` names a moment. Renaming makes the break a compile
    // error at every accept-list instead of a message that silently stops being
    // delivered.
    CHECK(schema_of<KeyPressed>()->content_id() == SchemaBuilder("KeyPressed", 2)
                                                       .field("scancode", Kind::Int)
                                                       .field("name", Kind::Text)
                                                       .field("modifiers", Kind::Int)
                                                       .build()
                                                       ->content_id());
    CHECK(schema_of<KeyReleased>()->content_id() == SchemaBuilder("KeyReleased", 2)
                                                        .field("scancode", Kind::Int)
                                                        .field("name", Kind::Text)
                                                        .field("modifiers", Kind::Int)
                                                        .build()
                                                        ->content_id());
    CHECK(schema_of<TextEntered>()->content_id() ==
          SchemaBuilder("TextEntered", 1).field("text", Kind::Text).build()->content_id());
    CHECK(schema_of<PointerButton>()->content_id() == SchemaBuilder("PointerButton", 1)
                                                          .field("button", Kind::Int)
                                                          .field("pressed", Kind::Bool)
                                                          .field("x", Kind::Int)
                                                          .field("y", Kind::Int)
                                                          .field("space", Kind::Int)
                                                          .field("modifiers", Kind::Int)
                                                          .build()
                                                          ->content_id());
    CHECK(schema_of<PointerMoved>()->content_id() == SchemaBuilder("PointerMoved", 1)
                                                         .field("x", Kind::Int)
                                                         .field("y", Kind::Int)
                                                         .field("dx", Kind::Int)
                                                         .field("dy", Kind::Int)
                                                         .field("space", Kind::Int)
                                                         .field("modifiers", Kind::Int)
                                                         .build()
                                                         ->content_id());
    CHECK(schema_of<PointerWheel>()->content_id() == SchemaBuilder("PointerWheel", 1)
                                                        .field("dx", Kind::Float)
                                                        .field("dy", Kind::Float)
                                                        .field("x", Kind::Int)
                                                        .field("y", Kind::Int)
                                                        .field("space", Kind::Int)
                                                        .field("modifiers", Kind::Int)
                                                        .build()
                                                        ->content_id());
    // The named addition is frozen the same way the rest are.
    CHECK(schema_of<PumpInput>()->content_id() ==
          SchemaBuilder("PumpInput", 1).build()->content_id());

    // A POSITION IS AN INTEGER, and that is a contract claim, not a detail. The
    // old shapes carried doubles, which bought no precision either backend has
    // and cost every consumer a bounded narrowing cast on the press path.
    CHECK(schema_of<PointerButton>()->fields().size() == 6);
    CHECK(schema_of<PointerMoved>()->fields().size() == 6);
}

TEST_CASE("contract: the modifier and coordinate-space vocabularies are disjoint flags") {
    // A bitmask is only a bitmask if the bits do not collide.
    CHECK(mod::kNone == 0);
    CHECK((mod::kShift & mod::kCtrl) == 0);
    CHECK((mod::kShift & mod::kAlt) == 0);
    CHECK((mod::kShift & mod::kSuper) == 0);
    CHECK((mod::kCtrl & mod::kAlt) == 0);
    CHECK((mod::kCtrl & mod::kSuper) == 0);
    CHECK((mod::kAlt & mod::kSuper) == 0);
    CHECK((mod::kShift | mod::kCtrl | mod::kAlt | mod::kSuper) == 15);

    // The space vocabulary is an ENUMERATION, not a mask: a coordinate is in
    // one space or it is in none, and "none" is a distinct answer from "cells".
    CHECK(space::kUnknown == 0);
    CHECK(space::kCells != space::kPixels);
    CHECK(space::kCells != space::kUnknown);
    // The default a consumer sees if nobody stamps it is kUnknown, so an
    // unstamped coordinate can never be mistaken for a stamped one.
    CHECK(PointerButton{}.space == space::kUnknown);
    CHECK(PointerMoved{}.space == space::kUnknown);
    CHECK(PointerWheel{}.space == space::kUnknown);
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

TEST_CASE("terminal: a printable key is a transition AND the text it produced") {
    std::size_t i = 0;
    auto ev = term("w");
    expect_typed(ev, i, scan::kW, "W", "w");
    CHECK(i == ev.size());

    i = 0;
    ev = term("wasd"); // the snake controls, whole
    expect_typed(ev, i, scan::kW, "W", "w");
    expect_typed(ev, i, scan::kA, "A", "a");
    expect_typed(ev, i, scan::kS, "S", "s");
    expect_typed(ev, i, scan::kD, "D", "d");
    CHECK(i == ev.size());

    i = 0;
    ev = term("30");
    expect_typed(ev, i, scan::k3, "3", "3");
    expect_typed(ev, i, scan::k0, "0", "0");
    CHECK(i == ev.size());

    i = 0;
    ev = term("/;");
    expect_typed(ev, i, scan::kSlash, "/", "/");
    expect_typed(ev, i, scan::kSemicolon, ";", ";");
    CHECK(i == ev.size());
}

TEST_CASE("terminal: EDITING CONTROLS are keys and are never text") {
    // The whole of the §10 distinction, in one case. A terminal delivers these
    // as control bytes; they change a draft, they are not part of one, and
    // Workshop owns what each of them MEANS.
    std::size_t i = 0;
    auto ev = term("\r\n\t");
    expect_stroke(ev, i, scan::kReturn, "Return");
    expect_stroke(ev, i, scan::kReturn, "Return");
    expect_stroke(ev, i, scan::kTab, "Tab");
    CHECK(i == ev.size());

    i = 0;
    ev = term("\x7f\x08");
    expect_stroke(ev, i, scan::kBackspace, "Backspace");
    expect_stroke(ev, i, scan::kBackspace, "Backspace");
    CHECK(i == ev.size());

    i = 0;
    ev = term("\x1b");
    expect_stroke(ev, i, scan::kEscape, "Escape");
    CHECK(i == ev.size());

    // Not one byte of text came out of any of them.
    CHECK(typed(term("\r\n\t\x7f\x08\x1b")).empty());

    // Space, by contrast, IS text — a space is a character a maker means to
    // type — and it is a key as well.
    i = 0;
    ev = term(" ");
    expect_typed(ev, i, scan::kSpace, "Space", " ");
    CHECK(i == ev.size());
}

TEST_CASE("terminal: `%` arrives as TEXT and nobody computes Shift+5") {
    // The headline, and the reason a scancode vocabulary makes `70p` a workaround
    // worth committing. The terminal has already applied the layout: the byte IS the
    // answer, and this package refuses to invent a key identity it cannot know.
    auto ev = term("%");
    CHECK(typed(ev) == "%");
    // No key event at all: `%` is not a key this backend can name, and claiming
    // scancode 5 would be a claim about a US keyboard rather than about a
    // keyboard.
    CHECK(pressed_codes(ev).empty());

    CHECK(typed(term("70%")) == "70%");
    // The whole Workshop value, and the digits kept their key identities while
    // the `%` did not — two different truths, told separately.
    const auto codes = pressed_codes(term("70%"));
    REQUIRE(codes.size() == 2);
    CHECK(codes[0] == scan::k7);
    CHECK(codes[1] == scan::k0);

    // The other characters a scancode alone cannot reach.
    CHECK(typed(term("@")) == "@");
    CHECK(typed(term("Panel")) == "Panel");
    CHECK(typed(term("A")) == "A");
}

TEST_CASE("terminal: an uppercase letter is the SAME key, with Shift, and its own text") {
    // The physical key and the entered text disagree here, and both are right.
    // `H` is scancode H (there is one H key) with Shift held (the terminal
    // delivered the shifted form), and it typed "H".
    std::size_t i = 0;
    auto ev = term("H");
    expect_typed(ev, i, scan::kH, "H", "H", mod::kShift);
    CHECK(i == ev.size());

    i = 0;
    ev = term("h");
    expect_typed(ev, i, scan::kH, "H", "h", mod::kNone);
    CHECK(i == ev.size());

    // Workshop's whole new binding, on the canonical lane: hjkl and HJKL are
    // one gesture family, told apart by a modifier rather than by four more
    // unrelated keys.
    i = 0;
    ev = term("HJKL");
    expect_typed(ev, i, scan::kH, "H", "H", mod::kShift);
    expect_typed(ev, i, scan::kJ, "J", "J", mod::kShift);
    expect_typed(ev, i, scan::kK, "K", "K", mod::kShift);
    expect_typed(ev, i, scan::kL, "L", "L", mod::kShift);
    CHECK(i == ev.size());

    // THE LIMIT, asserted so it cannot be forgotten: this backend infers Shift
    // from the byte's CASE, and CapsLock produces the same byte. A digit or a
    // symbol carries no inferable Shift at all — `%` is a shifted `5` on a US
    // layout and is not one anywhere else, so nothing is claimed.
    CHECK(as<KeyPressed>(term("7"), 0).modifiers == mod::kNone);
    for (const InputEvent& e : term("%")) {
        CHECK_FALSE(std::holds_alternative<KeyPressed>(e));
    }
}

TEST_CASE("terminal: Ctrl is a MEASURED modifier, and the dressed name is gone") {
    // A terminal genuinely sends control byte N for Ctrl + the Nth letter, so
    // this is not an inference like Shift — it is what arrived.
    std::size_t i = 0;
    auto ev = term("\x03");
    expect_stroke(ev, i, scan::kC, "C", mod::kCtrl);
    CHECK(i == ev.size());
    CHECK(typed(ev).empty()); // Ctrl+C types nothing

    i = 0;
    ev = term("\x01"); // Ctrl+A — V1 dropped this entirely for want of a modifier
    expect_stroke(ev, i, scan::kA, "A", mod::kCtrl);
    CHECK(i == ev.size());

    // THE DEBT IS PAID. V1 dressed the convenience NAME as "Ctrl+C" because
    // there was nowhere else to put the modifier, and both snake and Workshop
    // branched on that string. The name is a name again.
    CHECK(as<KeyPressed>(term("\x03"), 0).name == "C");
    CHECK(as<KeyPressed>(term("c"), 0).name == "C");
    // ...and the two are still told apart, by the fact that actually differs.
    CHECK(as<KeyPressed>(term("c"), 0).modifiers == mod::kNone);
    CHECK(as<KeyPressed>(term("\x03"), 0).modifiers == mod::kCtrl);
}

TEST_CASE("terminal: the arrow sequences, and an unknown sequence leaks nothing") {
    std::size_t i = 0;
    auto ev = term("\x1b[A\x1b[B\x1b[C\x1b[D");
    expect_stroke(ev, i, scan::kUp, "Up");
    expect_stroke(ev, i, scan::kDown, "Down");
    expect_stroke(ev, i, scan::kRight, "Right");
    expect_stroke(ev, i, scan::kLeft, "Left");
    CHECK(i == ev.size());
    CHECK(typed(ev).empty()); // an arrow is not text

    // An unknown CSI is CONSUMED WHOLE and dropped. V1 emitted Escape, then
    // `[`, then the final byte as three keystrokes -- and `[` is a key Workshop
    // binds. This is the same defect the mouse report has, in miniature.
    TerminalParser p;
    CHECK(feed(p, "\x1b[Z").empty());
    CHECK(p.malformed() == 1);
    CHECK_FALSE(p.mid_sequence());

    // And an ESC that is genuinely followed by an ordinary key is still both.
    i = 0;
    ev = term("\x1b" "q");
    expect_stroke(ev, i, scan::kEscape, "Escape");
    expect_typed(ev, i, scan::kQ, "Q", "q");
    CHECK(i == ev.size());
}

TEST_CASE("terminal: order is preserved across keys, text, and sequences") {
    std::size_t i = 0;
    auto ev = term("wa\x1b[Cq");
    expect_typed(ev, i, scan::kW, "W", "w");
    expect_typed(ev, i, scan::kA, "A", "a");
    expect_stroke(ev, i, scan::kRight, "Right");
    expect_typed(ev, i, scan::kQ, "Q", "q");
    CHECK(i == ev.size());
}

TEST_CASE("terminal: multi-byte UTF-8 is one character, however the reads fall") {
    // The backend can honestly produce this: a terminal in a UTF-8 locale sends
    // the bytes and this parser assembles them. What is NOT claimed anywhere is
    // grapheme clustering, normalisation, or display width.
    CHECK(typed(term("\xc3\xa9")) == "\xc3\xa9");             // e-acute, 2 bytes
    CHECK(typed(term("\xe2\x86\x92")) == "\xe2\x86\x92");     // an arrow, 3 bytes
    CHECK(typed(term("\xf0\x9f\x8e\xb2")) == "\xf0\x9f\x8e\xb2"); // a die, 4 bytes
    // One TextEntered per character, not one per byte.
    CHECK(term("\xc3\xa9").size() == 1);
    CHECK(term("\xf0\x9f\x8e\xb2").size() == 1);
    // A multi-byte character produces no key event: there is no key to name.
    CHECK(pressed_codes(term("\xc3\xa9")).empty());

    // Split across reads, at every interior boundary.
    for (std::size_t cut = 1; cut < 4; ++cut) {
        TerminalParser p;
        const std::string all = "\xf0\x9f\x8e\xb2";
        auto first = feed(p, std::string_view(all).substr(0, cut));
        CHECK(first.empty()); // half a character is not a character
        CHECK(p.mid_sequence());
        auto rest = feed(p, std::string_view(all).substr(cut));
        CHECK(typed(rest) == all);
    }

    // A truncated character does not eat the key that follows it.
    TerminalParser p;
    auto ev = feed(p, "\xc3q"); // a lead byte, then something that cannot continue it
    CHECK(p.malformed() == 1);
    CHECK(typed(ev) == "q");
    std::size_t i = 0;
    expect_typed(ev, i, scan::kQ, "Q", "q");
    CHECK(i == ev.size());
}

TEST_CASE("win32 keys: real transitions, VK identities, modifiers, and the layout's text") {
    KeyTrack keys;
    const auto key = [&keys](std::uint16_t vk, std::uint32_t ch, bool down, std::uint32_t cks) {
        return win32_key_to_events(keys, vk, ch, down, cks);
    };

    auto ev = key(0x57 /*'W'*/, L'w', true, 0);
    REQUIRE(ev.size() == 2);
    CHECK(as<KeyPressed>(ev, 0).scancode == scan::kW);
    CHECK(as<KeyPressed>(ev, 0).name == "W");
    CHECK(as<KeyPressed>(ev, 0).modifiers == mod::kNone);
    CHECK(as<TextEntered>(ev, 1).text == "w");

    ev = key(0x57, L'w', false, 0); // a REAL release — nothing synthesized, nothing typed
    REQUIRE(ev.size() == 1);
    CHECK(as<KeyReleased>(ev, 0).scancode == scan::kW);

    ev = key(win32::kVkUp, 0, true, 0);
    REQUIRE(ev.size() == 1); // an arrow types nothing
    CHECK(as<KeyPressed>(ev, 0).scancode == scan::kUp);

    ev = key(win32::kVkEscape, 0x1b, true, 0);
    REQUIRE(ev.size() == 1); // a control character is never text
    CHECK(as<KeyPressed>(ev, 0).scancode == scan::kEscape);
    CHECK(as<KeyPressed>(ev, 0).name == "Escape");

    // Ctrl+C: the key, with the modifier, and no text — uChar is 0x03.
    ev = key('C', 0x03, true, win32::kLeftCtrl);
    REQUIRE(ev.size() == 1);
    CHECK(as<KeyPressed>(ev, 0).scancode == scan::kC);
    CHECK(as<KeyPressed>(ev, 0).name == "C"); // the dressed name is retired
    CHECK(as<KeyPressed>(ev, 0).modifiers == mod::kCtrl);

    // The RIGHT Ctrl is the same semantic modifier as the left one.
    CHECK(as<KeyPressed>(key('C', 0x03, true, win32::kRightCtrl), 0).modifiers == mod::kCtrl);
    // Alt, either side; Shift, its one bit; and combinations.
    CHECK(as<KeyPressed>(key('A', 0, true, win32::kLeftAlt), 0).modifiers == mod::kAlt);
    CHECK(as<KeyPressed>(key('A', 0, true, win32::kRightAlt), 0).modifiers == mod::kAlt);
    CHECK(as<KeyPressed>(key('H', L'H', true, win32::kShift), 0).modifiers == mod::kShift);
    CHECK(as<KeyPressed>(key('A', 0, true, win32::kShift | win32::kLeftCtrl), 0).modifiers ==
          (mod::kShift | mod::kCtrl));
    // Super is never claimed on this backend: dwControlKeyState has no such bit,
    // and the lock/enhanced bits must not be mistaken for one.
    CHECK((as<KeyPressed>(key('A', L'a', true, 0x0080u /*CAPSLOCK_ON*/), 0).modifiers &
           mod::kSuper) == 0);
    CHECK(as<KeyPressed>(key('A', L'a', true, 0x0020u | 0x0040u | 0x0080u | 0x0100u), 0)
              .modifiers == mod::kNone);

    // A modifier key ITSELF is still not a key event: what a consumer wants is
    // "Shift was held when H was pressed", and that is where it is reported.
    CHECK(key(0x10 /*VK_SHIFT*/, 0, true, win32::kShift).empty());

    // The OEM punctuation keys, which V1 dropped entirely -- `[` and `]` are the
    // two keys Workshop binds to its workspace width, so they were simply not
    // deliverable on this backend.
    CHECK(as<KeyPressed>(key(win32::kVkOem4, L'[', true, 0), 0).scancode == scan::kLeftBracket);
    CHECK(as<KeyPressed>(key(win32::kVkOem6, L']', true, 0), 0).scancode == scan::kRightBracket);
    CHECK(as<KeyPressed>(key(win32::kVkOemComma, L',', true, 0), 0).scancode == scan::kComma);
    CHECK(as<KeyPressed>(key(win32::kVkOemMinus, L'-', true, 0), 0).scancode == scan::kMinus);
    CHECK(as<KeyPressed>(key(win32::kVkOemPlus, L'=', true, 0), 0).scancode == scan::kEquals);
}

TEST_CASE("win32 keys: `%` is the console's own answer, not a computation") {
    KeyTrack keys;
    // Shift+5 on a US layout. The console already resolved it; the VK is still
    // '5', and the two facts are reported as the two facts they are.
    auto ev = win32_key_to_events(keys, '5', L'%', true, win32::kShift);
    REQUIRE(ev.size() == 2);
    CHECK(as<KeyPressed>(ev, 0).scancode == scan::k5);
    CHECK(as<KeyPressed>(ev, 0).modifiers == mod::kShift);
    CHECK(as<TextEntered>(ev, 1).text == "%");

    // A character outside ASCII, and one outside the BMP -- the console delivers
    // the astral one as two surrogate records, and they are joined rather than
    // emitted as two broken halves.
    CHECK(typed(win32_key_to_events(keys, 0, 0x00E9 /*e-acute*/, true, 0)) == "\xc3\xa9");

    auto high = win32_key_to_events(keys, 0, 0xD83C, true, 0);
    CHECK(typed(high).empty()); // half a character is not a character
    auto low = win32_key_to_events(keys, 0, 0xDFB2, true, 0);
    CHECK(typed(low) == "\xf0\x9f\x8e\xb2");
    // An orphan low surrogate produces nothing rather than garbage.
    CHECK(typed(win32_key_to_events(keys, 0, 0xDFB2, true, 0)).empty());
}

TEST_CASE("win32 pointer: every record's own position and modifiers, deltas that follow") {
    PointerTrack track;
    const auto mouse = [&track](std::int64_t x, std::int64_t y, std::uint32_t buttons,
                                std::uint32_t flags, std::uint32_t cks = 0) {
        return win32_mouse_to_events(track, x, y, buttons, flags, cks);
    };

    // First move: a position, no invented delta.
    auto ev = mouse(10, 5, 0, win32::kMouseMoved);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerMoved>(ev, 0).x == 10);
    CHECK(as<PointerMoved>(ev, 0).y == 5);
    CHECK(as<PointerMoved>(ev, 0).dx == 0);
    CHECK(as<PointerMoved>(ev, 0).dy == 0);
    CHECK(as<PointerMoved>(ev, 0).space == space::kCells);

    ev = mouse(13, 4, 0, win32::kMouseMoved);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerMoved>(ev, 0).dx == 3);
    CHECK(as<PointerMoved>(ev, 0).dy == -1);

    // Left press: a button event has no move flag, AND CARRIES ITS POSITION.
    ev = mouse(13, 4, win32::kButtonLeft, 0);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerButton>(ev, 0).button == 1);
    CHECK(as<PointerButton>(ev, 0).pressed);
    CHECK(as<PointerButton>(ev, 0).x == 13);
    CHECK(as<PointerButton>(ev, 0).y == 4);
    CHECK(as<PointerButton>(ev, 0).space == space::kCells);

    // A drag: the held button is unchanged, so only the move speaks.
    ev = mouse(14, 4, win32::kButtonLeft, win32::kMouseMoved);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerMoved>(ev, 0).dx == 1);

    // Left up + right down in one record: two transitions, locked numbering
    // (1 left, 2 middle, 3 right), both carrying the same moment's position.
    ev = mouse(14, 4, win32::kButtonRight, 0);
    REQUIRE(ev.size() == 2);
    CHECK(as<PointerButton>(ev, 0).button == 1);
    CHECK_FALSE(as<PointerButton>(ev, 0).pressed);
    CHECK(as<PointerButton>(ev, 0).x == 14);
    CHECK(as<PointerButton>(ev, 1).button == 3);
    CHECK(as<PointerButton>(ev, 1).pressed);
    CHECK(as<PointerButton>(ev, 1).y == 4);

    ev = mouse(14, 4, win32::kButtonRight | win32::kButtonMiddle, 0);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerButton>(ev, 0).button == 2);
    CHECK(as<PointerButton>(ev, 0).pressed);

    // Modifiers at pointer time: Shift+click and Ctrl+drag are one message, not
    // a pointer event joined to keyboard state read later. (Both held buttons
    // come up first -- a transition is a transition.)
    ev = mouse(20, 9, 0, 0, win32::kShift);
    REQUIRE(ev.size() == 2);
    CHECK_FALSE(as<PointerButton>(ev, 0).pressed);
    CHECK_FALSE(as<PointerButton>(ev, 1).pressed);
    CHECK(as<PointerButton>(ev, 0).modifiers == mod::kShift);
    ev = mouse(20, 9, 0, 0, win32::kShift);
    CHECK(ev.empty()); // nothing changed: nothing happened
    ev = mouse(20, 9, win32::kButtonLeft, 0, win32::kShift);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerButton>(ev, 0).modifiers == mod::kShift);
    ev = mouse(21, 9, win32::kButtonLeft, win32::kMouseMoved, win32::kLeftCtrl);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerMoved>(ev, 0).modifiers == mod::kCtrl);
    ev = mouse(21, 9, 0, 0, win32::kShift | win32::kLeftAlt);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerButton>(ev, 0).modifiers == (mod::kShift | mod::kAlt));

    // Wheel: +120 per notch in the high word; the wire speaks +1.0 per notch,
    // and the wheel is a pointer moment too — it has a position.
    ev = mouse(4, 7, 120u << 16, win32::kMouseWheeled);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerWheel>(ev, 0).dx == 0.0);
    CHECK(as<PointerWheel>(ev, 0).dy == 1.0);
    CHECK(as<PointerWheel>(ev, 0).x == 4);
    CHECK(as<PointerWheel>(ev, 0).y == 7);
    CHECK(as<PointerWheel>(ev, 0).space == space::kCells);

    ev = mouse(4, 7, static_cast<std::uint32_t>(static_cast<std::uint16_t>(-120)) << 16,
               win32::kMouseWheeled);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerWheel>(ev, 0).dy == -1.0);

    ev = mouse(4, 7, 240u << 16, win32::kMouseHWheeled);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerWheel>(ev, 0).dx == 2.0);
    CHECK(as<PointerWheel>(ev, 0).dy == 0.0);
}

// ---- What a drag asks of this vocabulary, and GETS (two pins, deliberately flipped) ----
//
// These two cases once pinned CURRENT behaviour rather than desired behaviour,
// so that "the Input phase that fixes either one has to come and delete a test
// on purpose, rather than discovering afterwards that something depended on the
// loss." They are those two cases,
// rewritten to assert the repair in the same words that described the defect.

TEST_CASE("win32 pointer: a button record knows where it happened, AND SO DOES THE WIRE") {
    // Was: "a button record KNOWS where it happened; the wire does not."
    //
    // The Win32 console hands the reader dwMousePosition on EVERY mouse record,
    // including a pure button transition (dwEventFlags == 0). V1 dropped it,
    // because MouseButton had nowhere to put it. PointerButton has somewhere.
    PointerTrack track;

    auto ev = win32_mouse_to_events(track, 10, 5, 0, win32::kMouseMoved, 0);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerMoved>(ev, 0).x == 10);

    // A press 32 cells away, with no intervening move record. A consumer
    // reconstructing from the last motion answers 10,5 for this click. The
    // message answers 42,7 -- because that is what the platform said.
    ev = win32_mouse_to_events(track, 42, 7, win32::kButtonLeft, 0, 0);
    REQUIRE(ev.size() == 1);
    const PointerButton& press = as<PointerButton>(ev, 0);
    CHECK(press.button == 1);
    CHECK(press.pressed);
    CHECK(press.x == 42);
    CHECK(press.y == 7);

    // And the STALENESS is gone at its root, not merely routed around: a
    // button-only record advances the tracker too, so the next move's delta is
    // measured from where the pointer actually was. A tracker that skipped the
    // press measures 33 here (43-10, spanning right across it); the honest
    // answer is 1.
    ev = win32_mouse_to_events(track, 43, 7, win32::kButtonLeft, win32::kMouseMoved, 0);
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerMoved>(ev, 0).dx == 1); // 43 - 42, not 43 - 10
    CHECK(as<PointerMoved>(ev, 0).dy == 0);
}

TEST_CASE("pointer moments: an event-time position is not the LAST position") {
    // The failure a reconstruction has to live with, recreated explicitly and
    // shown to be unreachable through these messages: a press at A, motion afterwards
    // to B, and the press event still says A. A consumer that reads the press
    // cannot get B by accident, because the press is not asking anybody.
    PointerTrack track;
    auto press = win32_mouse_to_events(track, 12, 3, win32::kButtonLeft, 0, 0);
    REQUIRE(press.size() == 1);
    const PointerButton captured = as<PointerButton>(press, 0);

    // Now the pointer travels a long way, several times.
    for (std::int64_t x = 13; x < 40; ++x) {
        (void)win32_mouse_to_events(track, x, 11, win32::kButtonLeft, win32::kMouseMoved, 0);
    }
    // The message is a value. It was true when it was made and it stays true.
    CHECK(captured.x == 12);
    CHECK(captured.y == 3);

    // The same, on the terminal: a press, then motion, then a release
    // somewhere else. Three moments, three positions, none of them borrowed.
    TerminalParser p;
    auto down = feed(p, "\x1b[<0;5;5M");
    REQUIRE(down.size() == 1);
    CHECK(as<PointerButton>(down, 0).x == 4);
    CHECK(as<PointerButton>(down, 0).y == 4);
    auto move = feed(p, "\x1b[<32;9;5M");
    REQUIRE(move.size() == 1);
    CHECK(as<PointerMoved>(move, 0).x == 8);
    auto up = feed(p, "\x1b[<0;9;5m");
    REQUIRE(up.size() == 1);
    CHECK_FALSE(as<PointerButton>(up, 0).pressed);
    CHECK(as<PointerButton>(up, 0).x == 8);
    CHECK(as<PointerButton>(up, 0).y == 4);
    // ...and the press we captured first is still where it was.
    CHECK(as<PointerButton>(down, 0).x == 4);
}

TEST_CASE("terminal: THE CANONICAL LANE HAS A POINTER, and a report is not keystrokes") {
    // Was: "the canonical lane has no pointer, and a mouse report is keystrokes."
    //
    // A parser with no SGR support fed exactly these bytes counts NINE keystrokes
    // coming out, one of them `[` -- the key Workshop binds to "narrow the workspace".
    // So a single click on a terminal that had been asked to report one would
    // silently have resized a maker's workspace.
    const auto ev = term("\x1b[<0;10;5M"); // "left button pressed at column 10, row 5"

    // One message, and it is a pointer press.
    REQUIRE(ev.size() == 1);
    const PointerButton& b = as<PointerButton>(ev, 0);
    CHECK(b.button == 1);
    CHECK(b.pressed);
    CHECK(b.x == 9); // the terminal counts from 1; the contract counts from 0
    CHECK(b.y == 4);
    CHECK(b.space == space::kCells);

    // NOTHING LEAKED. Not the `[`, not the digits, not the `M`, not the Escape.
    CHECK(pressed_codes(ev).empty());
    CHECK(typed(ev).empty());
}

TEST_CASE("terminal SGR: presses, releases, motion, wheels, buttons, modifiers") {
    TerminalParser p;

    // Buttons: low two bits, and the final byte says which transition.
    auto ev = feed(p, "\x1b[<0;3;4M");
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerButton>(ev, 0).button == 1);
    CHECK(as<PointerButton>(ev, 0).pressed);
    ev = feed(p, "\x1b[<0;3;4m");
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerButton>(ev, 0).button == 1);
    CHECK_FALSE(as<PointerButton>(ev, 0).pressed);
    ev = feed(p, "\x1b[<1;3;4M");
    CHECK(as<PointerButton>(ev, 0).button == 2); // middle
    ev = feed(p, "\x1b[<2;3;4M");
    CHECK(as<PointerButton>(ev, 0).button == 3); // right

    // Motion while a button is held (mode 1002's whole purpose): bit 32 set,
    // and the delta is derived here rather than by every consumer.
    ev = feed(p, "\x1b[<32;10;10M");
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerMoved>(ev, 0).x == 9);
    CHECK(as<PointerMoved>(ev, 0).y == 9);
    CHECK(as<PointerMoved>(ev, 0).dx == 7); // 9 - 2, from the last report's position
    CHECK(as<PointerMoved>(ev, 0).dy == 6);
    ev = feed(p, "\x1b[<32;12;10M");
    CHECK(as<PointerMoved>(ev, 0).dx == 2);
    CHECK(as<PointerMoved>(ev, 0).dy == 0);

    // Wheels: bit 64, and the low bits pick the direction.
    ev = feed(p, "\x1b[<64;5;5M");
    REQUIRE(ev.size() == 1);
    CHECK(as<PointerWheel>(ev, 0).dy == 1.0);
    CHECK(as<PointerWheel>(ev, 0).x == 4);
    ev = feed(p, "\x1b[<65;5;5M");
    CHECK(as<PointerWheel>(ev, 0).dy == -1.0);

    // Modifiers at pointer time, which the terminal genuinely reports: 4 Shift,
    // 8 Alt, 16 Ctrl, added to the button number.
    ev = feed(p, "\x1b[<4;3;4M");
    CHECK(as<PointerButton>(ev, 0).modifiers == mod::kShift);
    ev = feed(p, "\x1b[<16;3;4M");
    CHECK(as<PointerButton>(ev, 0).modifiers == mod::kCtrl);
    ev = feed(p, "\x1b[<8;3;4M");
    CHECK(as<PointerButton>(ev, 0).modifiers == mod::kAlt);
    ev = feed(p, "\x1b[<20;3;4M");
    CHECK(as<PointerButton>(ev, 0).modifiers == (mod::kShift | mod::kCtrl));
    ev = feed(p, "\x1b[<36;7;4M"); // 32 motion + 4 shift
    CHECK(as<PointerMoved>(ev, 0).modifiers == mod::kShift);

    // The 1-based to 0-based translation, at the origin where an off-by-one
    // shows: the terminal's top-left cell is 1;1.
    ev = feed(p, "\x1b[<0;1;1M");
    CHECK(as<PointerButton>(ev, 0).x == 0);
    CHECK(as<PointerButton>(ev, 0).y == 0);

    CHECK(p.malformed() == 0); // every one of those was well-formed
}

TEST_CASE("terminal SGR: the parser survives read boundaries wherever they fall") {
    // The §18 list, entry by entry. An OS read boundary is not an event
    // boundary, and V1's batch-local parse assumed it was.
    const std::string report = "\x1b[<0;10;5M";

    // 1. complete report in one read.
    {
        TerminalParser p;
        auto ev = feed(p, report);
        REQUIRE(ev.size() == 1);
        CHECK(as<PointerButton>(ev, 0).x == 9);
    }

    // 2/3. split ANYWHERE -- after ESC, inside the numeric body, before the
    // final byte. Every interior cut, not a chosen one.
    for (std::size_t cut = 1; cut < report.size(); ++cut) {
        TerminalParser p;
        auto first = feed(p, std::string_view(report).substr(0, cut));
        INFO("cut at ", cut);
        CHECK(first.empty()); // emits nothing yet
        CHECK(p.mid_sequence());
        CHECK(p.malformed() == 0);
        auto second = feed(p, std::string_view(report).substr(cut));
        REQUIRE(second.size() == 1);
        CHECK(as<PointerButton>(second, 0).x == 9);
        CHECK(as<PointerButton>(second, 0).y == 4);
        CHECK_FALSE(p.mid_sequence());
    }

    // 4. multiple reports in one read.
    {
        TerminalParser p;
        auto ev = feed(p, "\x1b[<0;1;1M\x1b[<32;2;1M\x1b[<0;2;1m");
        REQUIRE(ev.size() == 3);
        CHECK(as<PointerButton>(ev, 0).pressed);
        CHECK(as<PointerMoved>(ev, 1).x == 1);
        CHECK_FALSE(as<PointerButton>(ev, 2).pressed);
    }

    // 5. a report adjacent to ordinary input, both sides.
    {
        TerminalParser p;
        auto ev = feed(p, "n\x1b[<0;4;4Mq");
        CHECK(typed(ev) == "nq");
        const auto codes = pressed_codes(ev);
        REQUIRE(codes.size() == 2);
        CHECK(codes[0] == scan::kN);
        CHECK(codes[1] == scan::kQ);
        bool saw_press = false;
        for (const InputEvent& e : ev) {
            saw_press = saw_press || std::holds_alternative<PointerButton>(e);
        }
        CHECK(saw_press);
    }

    // 6. an incomplete prefix emits nothing, and keeps waiting.
    {
        TerminalParser p;
        CHECK(feed(p, "\x1b[<0;10").empty());
        CHECK(p.mid_sequence());
        CHECK(p.idle().empty()); // an unfinished sequence is NOT an Escape key
        CHECK(p.malformed() == 0);
    }

    // 7. a lone ESC is the Escape key, and the pump's empty poll is what says
    // so -- the parser's only clock.
    {
        TerminalParser p;
        CHECK(feed(p, "\x1b").empty()); // cannot know yet
        CHECK(p.mid_sequence());
        auto ev = p.idle();
        std::size_t i = 0;
        expect_stroke(ev, i, scan::kEscape, "Escape");
        CHECK(i == ev.size());
        CHECK_FALSE(p.mid_sequence());
        CHECK(p.idle().empty()); // and only once
    }

    // ...but a lone ESC whose tail arrives in the NEXT read is a sequence, not
    // an Escape. This is the case that made the parser stateful.
    {
        TerminalParser p;
        CHECK(feed(p, "\x1b").empty());
        auto ev = feed(p, "[<0;10;5M");
        REQUIRE(ev.size() == 1);
        CHECK(as<PointerButton>(ev, 0).x == 9);
        CHECK(pressed_codes(ev).empty()); // no Escape, no `[`
    }
}

TEST_CASE("terminal SGR: malformed reports fail explicitly and leak no keys") {
    const char* bad[] = {
        "\x1b[<0;10M",        // two fields, not three
        "\x1b[<0;10;5;7M",    // four fields
        "\x1b[<;10;5M",       // an empty field
        "\x1b[<0;10;M",       // a trailing empty field
        "\x1b[<3;10;5M",      // "no button" is not a transition
        "\x1b[<0;99999999;5M" // a coordinate that is not a coordinate
    };
    for (const char* s : bad) {
        TerminalParser p;
        INFO("report: ", s);
        const auto ev = feed(p, s);
        CHECK(ev.empty());              // nothing invented
        CHECK(p.malformed() == 1);      // and the refusal is COUNTED, not silent
        CHECK(pressed_codes(ev).empty());
        CHECK(typed(ev).empty());
        CHECK_FALSE(p.mid_sequence());  // the parser resynced
    }

    // Garbage that never terminates is bounded rather than accumulated for
    // ever -- AND the tail of it does not become keystrokes. Measured before
    // the resync existed: the parser gave up at 32 bytes, returned to ground,
    // and typed the remaining thirty-odd `1`s into whatever had focus. Giving
    // up in the middle of the garbage is the very defect this parser removes.
    TerminalParser p;
    std::string flood = "\x1b[<";
    flood.append(64, '1');
    const auto ev = feed(p, flood);
    CHECK(ev.empty());
    CHECK(pressed_codes(ev).empty());
    CHECK(typed(ev).empty()); // not one `1`
    CHECK(p.malformed() >= 1);
    CHECK(p.mid_sequence()); // still swallowing: the sequence never ended
    // A CSI ends at its final byte, and that is exactly where the stream is
    // rejoined -- one byte later, ordinary input works again.
    auto tail = feed(p, "M");
    CHECK(tail.empty());
    CHECK_FALSE(p.mid_sequence());
    std::size_t j = 0;
    auto resumed = feed(p, "n");
    expect_typed(resumed, j, scan::kN, "N", "n");
    CHECK(j == resumed.size());

    // And after any of it, ordinary input still works -- a resync that ate the
    // next keystroke would be its own defect.
    TerminalParser q;
    (void)feed(q, "\x1b[<0;10M");
    auto after = feed(q, "n");
    std::size_t i = 0;
    expect_typed(after, i, scan::kN, "N", "n");
    CHECK(i == after.size());
}

// ============================================================================
// Tier 2e — SDL, as pure translation
// ============================================================================
//
// The same discipline the Win32 paths get: no SDL headers here, so the WSL lane
// and the Windows-stranger lane (which builds no SDL at all) both pin the whole
// SDL translation. The local constants ARE SDL's, and input_sdl.cpp -- the one
// translation unit where both spellings exist -- static_asserts them against the
// real headers, so a drift is a build failure rather than an opinion.

namespace {

/// The SDL event at [i], REQUIRE'd to hold shape E.
template <class E>
const E& sdl_at(const std::vector<SdlEvent>& events, std::size_t i) {
    REQUIRE(i < events.size());
    const E* e = std::get_if<E>(&events[i]);
    REQUIRE(e != nullptr);
    return *e;
}

} // namespace

TEST_CASE("sdl: a key transition is SDL'S OWN scancode, and the modifiers of that moment") {
    // The identity claim. Every other backend TRANSLATES a native key id into
    // the wire's space; SDL is the backend that space was defined in, so there
    // is no table here at all -- and a table would be a copy of the identity
    // function, which is a thing that can drift.
    std::vector<SdlEvent> e = sdl_key_to_events(scan::kQ, 0, /*down=*/true, /*repeat=*/false);
    REQUIRE(e.size() == 1);
    CHECK(sdl_at<KeyPressed>(e, 0).scancode == scan::kQ);
    CHECK(sdl_at<KeyPressed>(e, 0).name == "Q");
    CHECK(sdl_at<KeyPressed>(e, 0).modifiers == mod::kNone);

    e = sdl_key_to_events(scan::kQ, 0, /*down=*/false, false);
    REQUIRE(e.size() == 1);
    CHECK(sdl_at<KeyReleased>(e, 0).scancode == scan::kQ);

    // Left and right fold to one semantic bit, the same fold the Win32 path
    // performs, because "was Ctrl held" is the question consumers ask.
    CHECK(sdl_modifiers_of(sdl::kModLShift) == mod::kShift);
    CHECK(sdl_modifiers_of(sdl::kModRShift) == mod::kShift);
    CHECK(sdl_modifiers_of(sdl::kModLCtrl) == mod::kCtrl);
    CHECK(sdl_modifiers_of(sdl::kModRCtrl) == mod::kCtrl);
    CHECK(sdl_modifiers_of(sdl::kModLAlt) == mod::kAlt);
    CHECK(sdl_modifiers_of(sdl::kModRAlt) == mod::kAlt);
    // AND kSuper HAS ITS FIRST PRODUCER. The terminal cannot see the key at all
    // and the Win32 console has no bit for it; the vocabulary declared kSuper
    // anyway and until now nothing could ever set it.
    CHECK(sdl_modifiers_of(sdl::kModLGui) == mod::kSuper);
    CHECK(sdl_modifiers_of(sdl::kModRGui) == mod::kSuper);
    CHECK(sdl_modifiers_of(static_cast<std::uint16_t>(sdl::kModRCtrl | sdl::kModLShift)) ==
          (mod::kCtrl | mod::kShift));
    // Lock states are not semantic modifiers and are not smuggled in as one.
    CHECK(sdl_modifiers_of(0x2000 /* SDL_KMOD_CAPS */) == mod::kNone);
    CHECK(sdl_modifiers_of(0x1000 /* SDL_KMOD_NUM */) == mod::kNone);

    // A key with no courtesy name is still a real key: the scancode is the
    // identity and it is passed through rather than dropped.
    e = sdl_key_to_events(225 /* SDL_SCANCODE_LSHIFT */, sdl::kModLShift, true, false);
    REQUIRE(e.size() == 1);
    CHECK(sdl_at<KeyPressed>(e, 0).scancode == 225);
    CHECK(sdl_at<KeyPressed>(e, 0).name.empty());
    CHECK(sdl_at<KeyPressed>(e, 0).modifiers == mod::kShift);
}

TEST_CASE("sdl: an auto-repeat is a PRESS, because the wire has no other word for it") {
    // input/vocabulary.hpp: "Auto-repeat arrives as repeated presses (SDL's own
    // stance); there is deliberately no repeat flag." SDL states the flag; this
    // backend deliberately does not invent a semantic category to carry it, and
    // a held key is a stream of ordinary presses.
    const std::vector<SdlEvent> first = sdl_key_to_events(scan::kJ, 0, true, /*repeat=*/false);
    const std::vector<SdlEvent> again = sdl_key_to_events(scan::kJ, 0, true, /*repeat=*/true);
    REQUIRE(first.size() == 1);
    REQUIRE(again.size() == 1);
    CHECK(sdl_at<KeyPressed>(first, 0).scancode == sdl_at<KeyPressed>(again, 0).scancode);
    CHECK(sdl_at<KeyPressed>(first, 0).modifiers == sdl_at<KeyPressed>(again, 0).modifiers);
}

TEST_CASE("sdl: `%` is the platform's own text, and no key was consulted to get it") {
    // The third backend to answer the character question, and the easiest:
    // SDL_EVENT_TEXT_INPUT is already UTF-8 the platform committed, through the
    // active layout, dead keys and IME. There is nothing to decode.
    std::vector<SdlEvent> e = sdl_text_to_events("%");
    REQUIRE(e.size() == 1);
    CHECK(sdl_at<TextEntered>(e, 0).text == "%");

    // Multi-byte arrives whole -- it is one platform event, not one per byte.
    e = sdl_text_to_events("\xc3\xa9");
    REQUIRE(e.size() == 1);
    CHECK(sdl_at<TextEntered>(e, 0).text == "\xc3\xa9");

    // Nothing typed is not an event. An empty TextEntered would be a consumer's
    // problem forever after.
    CHECK(sdl_text_to_events("").empty());
    CHECK(sdl_text_to_events(nullptr).empty());
}

TEST_CASE("sdl: pointer motion carries SDL's OWN delta, in pixels") {
    // The other two backends DERIVE dx/dy by remembering the last position,
    // because their platforms report only a position. SDL states xrel/yrel on
    // the event, so this reader keeps no tracker at all -- there is nothing here
    // that could go stale, and nothing that behaves differently after a period
    // with no motion.
    const std::vector<SdlEvent> e = sdl_mouse_motion_to_events(120.0f, 36.0f, 5.0f, -2.0f);
    REQUIRE(e.size() == 1);
    const PointerMoved& m = sdl_at<PointerMoved>(e, 0);
    CHECK(m.x == 120);
    CHECK(m.y == 36);
    CHECK(m.dx == 5);
    CHECK(m.dy == -2);
    // PIXELS, and labelled as pixels. Calling them cells to make a cell-native
    // application work is the one thing this field exists to prevent.
    CHECK(m.space == space::kPixels);
    CHECK(m.space != space::kCells);
}

TEST_CASE("sdl: a button event states where it happened, and an unknown button is silence") {
    const std::vector<SdlEvent> e =
        sdl_mouse_button_to_events(sdl::kButtonLeft, /*down=*/true, 77.0f, 5.0f);
    REQUIRE(e.size() == 1);
    const PointerButton& b = sdl_at<PointerButton>(e, 0);
    CHECK(b.button == 1);
    CHECK(b.pressed);
    CHECK(b.x == 77);
    CHECK(b.y == 5);
    CHECK(b.space == space::kPixels);

    // SDL numbers left/middle/right 1/2/3 and so does PointerButton, so the
    // mapping is the identity and no table was written for it.
    CHECK(sdl_at<PointerButton>(sdl_mouse_button_to_events(sdl::kButtonMiddle, true, 0, 0), 0)
              .button == 2);
    CHECK(sdl_at<PointerButton>(sdl_mouse_button_to_events(sdl::kButtonRight, false, 0, 0), 0)
              .button == 3);

    // X1/X2 are outside the vocabulary's stated set. Silence, NOT a Left click:
    // reporting a press that did not happen is worse than reporting nothing, and
    // widening the wire for a thumb button no consumer asks about would be
    // building for a mouse nobody here owns.
    CHECK(sdl_mouse_button_to_events(4, true, 10, 10).empty());
    CHECK(sdl_mouse_button_to_events(5, true, 10, 10).empty());
    CHECK(sdl_mouse_button_to_events(0, true, 10, 10).empty());
}

TEST_CASE("sdl: a pointer moment claims NO modifiers, because SDL states none") {
    // Measured, not assumed: SDL_MouseMotionEvent, SDL_MouseButtonEvent and
    // SDL_MouseWheelEvent carry no modifier field at all (SDL 3.4.12). The only
    // way to produce one is SDL_GetModState(), which reads CURRENT keyboard
    // state at some later instant -- exactly the reconstruction this vocabulary
    // exists to remove from the pointer path.
    //
    // So a clear bit here means what the vocabulary says it means: "nothing this
    // backend can see was held". A shift-click is not expressible on this
    // backend, and that is the honest answer rather than a plausible one.
    CHECK(sdl_at<PointerMoved>(sdl_mouse_motion_to_events(1, 2, 0, 0), 0).modifiers ==
          mod::kNone);
    CHECK(sdl_at<PointerButton>(sdl_mouse_button_to_events(sdl::kButtonLeft, true, 1, 2), 0)
              .modifiers == mod::kNone);
    CHECK(sdl_at<PointerWheel>(sdl_mouse_wheel_to_events(0, 1, sdl::kWheelNormal, 1, 2), 0)
              .modifiers == mod::kNone);
}

TEST_CASE("sdl: a float coordinate FLOORS, and a non-coordinate saturates") {
    // SDL's pointer coordinates are floats; the wire's are int64. For the window
    // this package creates -- no high-pixel-density flag, no logical
    // presentation, not resizable -- every value SDL delivers is integral and
    // this conversion is exact. Outside that, a fraction is genuinely lost, and
    // the rule is FLOOR because that is the one that agrees with what a maker
    // sees: 3.7 is inside pixel 3, which is inside cell 0.
    CHECK(sdl_pixel(0.0f) == 0);
    CHECK(sdl_pixel(11.0f) == 11);
    CHECK(sdl_pixel(11.9f) == 11);
    CHECK(sdl_pixel(12.0f) == 12);
    // Toward zero would send -0.5 to 0 -- a pointer just outside the window
    // reading as inside it.
    CHECK(sdl_pixel(-0.5f) == -1);
    CHECK(sdl_pixel(-1.0f) == -1);
    CHECK(sdl_pixel(-1.5f) == -2);

    // Total over every float, including the ones SDL will never send: a bare
    // cast of these is undefined behaviour, and the values come off a platform
    // edge.
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    CHECK(sdl_pixel(std::numeric_limits<float>::infinity()) == kMax);
    CHECK(sdl_pixel(-std::numeric_limits<float>::infinity()) == kMin);
    CHECK(sdl_pixel(std::numeric_limits<float>::quiet_NaN()) == kMin);
    CHECK(sdl_pixel(1e30f) == kMax);
    CHECK(sdl_pixel(-1e30f) == kMin);
}

TEST_CASE("sdl: the wheel keeps its fraction, and FLIPPED is put back") {
    // The one pointer fact where the existing vocabulary is WIDER than the
    // backend: PointerWheel::dx/dy are doubles precisely because a
    // high-resolution wheel reports a fraction of a detent, so SDL's floats
    // cross without losing anything.
    std::vector<SdlEvent> e = sdl_mouse_wheel_to_events(0.0f, 1.0f, sdl::kWheelNormal, 40, 8);
    REQUIRE(e.size() == 1);
    CHECK(sdl_at<PointerWheel>(e, 0).dy == doctest::Approx(1.0));
    CHECK(sdl_at<PointerWheel>(e, 0).x == 40);
    CHECK(sdl_at<PointerWheel>(e, 0).y == 8);
    CHECK(sdl_at<PointerWheel>(e, 0).space == space::kPixels);

    e = sdl_mouse_wheel_to_events(0.0f, 0.25f, sdl::kWheelNormal, 0, 0);
    CHECK(sdl_at<PointerWheel>(e, 0).dy == doctest::Approx(0.25));

    // FLIPPED means SDL already inverted the values for natural scrolling. The
    // wire's convention is SDL's normal one (+1 per notch away from the user),
    // so it is put back -- a consumer must not have to know how a maker's
    // trackpad is configured.
    e = sdl_mouse_wheel_to_events(0.0f, 1.0f, sdl::kWheelFlipped, 0, 0);
    CHECK(sdl_at<PointerWheel>(e, 0).dy == doctest::Approx(-1.0));
    e = sdl_mouse_wheel_to_events(-2.0f, 0.0f, sdl::kWheelFlipped, 0, 0);
    CHECK(sdl_at<PointerWheel>(e, 0).dx == doctest::Approx(2.0));
}

TEST_CASE("sdl: a close request is LIFECYCLE, and it is not any kind of key") {
    // The mandatory separation. SDL happens to report window lifecycle and input
    // on one queue; that does not make them one semantic population, and the
    // cheapest wrong answer here -- synthesizing KeyPressed{Q} because Workshop
    // already knows how to quit from a key -- is exactly what this asserts is
    // not happening.
    const std::vector<SdlEvent> e = sdl_close_to_events();
    REQUIRE(e.size() == 1);
    CHECK(std::holds_alternative<zengine::surface::SurfaceCloseRequested>(e[0]));
    CHECK_FALSE(std::holds_alternative<KeyPressed>(e[0]));
    CHECK_FALSE(std::holds_alternative<PointerButton>(e[0]));
}

TEST_CASE("sdl: the translated populations are exactly the ones with an obligation") {
    // The Reader sees events it does not translate, and ignoring them is fine --
    // Zen has no current semantic obligation to gamepads, sensors, pens, drops
    // or clipboards. What must never happen is one of the four this application
    // lives on quietly joining that set.
    CHECK(sdl_event_is_translated(sdl::kEventKeyDown));
    CHECK(sdl_event_is_translated(sdl::kEventKeyUp));
    CHECK(sdl_event_is_translated(sdl::kEventTextInput));
    CHECK(sdl_event_is_translated(sdl::kEventMouseMotion));
    CHECK(sdl_event_is_translated(sdl::kEventMouseButtonDown));
    CHECK(sdl_event_is_translated(sdl::kEventMouseButtonUp));
    CHECK(sdl_event_is_translated(sdl::kEventMouseWheel));
    CHECK(sdl_event_is_translated(sdl::kEventQuit));
    CHECK(sdl_event_is_translated(sdl::kEventWindowCloseRequested));

    // And the named exclusions, so "ignored" is a decision with a list rather
    // than a default. TEXT_EDITING is the interesting one: IME composition is
    // deliberately NOT supported (input/vocabulary.hpp says so at the door), and
    // what a platform commits still arrives as ordinary TEXT_INPUT.
    CHECK_FALSE(sdl_event_is_translated(sdl::kEventTextEditing));
    CHECK_FALSE(sdl_event_is_translated(0x202 /* SDL_EVENT_WINDOW_SHOWN */));
    CHECK_FALSE(sdl_event_is_translated(0x20e /* SDL_EVENT_WINDOW_FOCUS_LOST */));
    CHECK_FALSE(sdl_event_is_translated(0x600 /* SDL_EVENT_JOYSTICK_AXIS_MOTION */));
    CHECK_FALSE(sdl_event_is_translated(0));
}

// ============================================================================
// Tier 3 — the weave through a real bus (injected reader)
// ============================================================================

TEST_CASE("the weave publishes what its reader hears, in order, every shape") {
    loom::Switchboard bus;
    std::vector<std::vector<InputEvent>> batches;
    batches.push_back({KeyPressed{scan::kW, "W", mod::kShift},
                       TextEntered{"W"},
                       KeyReleased{scan::kW, "W", mod::kShift},
                       PointerButton{1, true, 42, 7, space::kCells, mod::kCtrl},
                       PointerMoved{3, 4, 1, -1, space::kCells, mod::kNone},
                       PointerWheel{0.0, 1.0, 3, 4, space::kCells, mod::kNone}});

    const loom::WeaveId weave = loom::mount<InputWeaveT<FakeReader>>(bus, FakeReader{&batches});
    std::vector<InputEvent> heard;
    (void)loom::mount<Ears>(bus, heard);

    const auto pump_input = [&] {
        bus.send(weave, loom::Message(loom::to_value(PumpInput{})));
        bus.pump();
    };

    pump_input();
    REQUIRE(heard.size() == 6);
    CHECK(as<KeyPressed>(heard, 0).scancode == scan::kW);
    CHECK(as<KeyPressed>(heard, 0).name == "W");
    CHECK(as<KeyPressed>(heard, 0).modifiers == mod::kShift);
    CHECK(as<TextEntered>(heard, 1).text == "W");
    CHECK(as<KeyReleased>(heard, 2).scancode == scan::kW);
    // Every fact of the press moment survived the wire, which is the point of
    // the whole phase: a consumer reads WHERE and WITH WHAT off the message it
    // was handed.
    CHECK(as<PointerButton>(heard, 3).button == 1);
    CHECK(as<PointerButton>(heard, 3).pressed);
    CHECK(as<PointerButton>(heard, 3).x == 42);
    CHECK(as<PointerButton>(heard, 3).y == 7);
    CHECK(as<PointerButton>(heard, 3).space == space::kCells);
    CHECK(as<PointerButton>(heard, 3).modifiers == mod::kCtrl);
    CHECK(as<PointerMoved>(heard, 4).x == 3);
    CHECK(as<PointerMoved>(heard, 4).y == 4);
    CHECK(as<PointerMoved>(heard, 4).dx == 1);
    CHECK(as<PointerMoved>(heard, 4).dy == -1);
    CHECK(as<PointerWheel>(heard, 5).dy == 1.0);
    CHECK(as<PointerWheel>(heard, 5).x == 3);

    // A quiet platform: the pump costs nothing and says nothing.
    pump_input();
    CHECK(heard.size() == 6);
}

namespace {

/// A reader shaped like the SDL one: its poll() hands back the SDL variant, so
/// the weave built over it derives the SEVEN-shape Emit set.
struct FakeSdlReader {
    std::vector<std::vector<SdlEvent>>* feed = nullptr;
    std::vector<SdlEvent> poll() {
        if (feed == nullptr || feed->empty()) {
            return {};
        }
        std::vector<SdlEvent> batch = std::move(feed->front());
        feed->erase(feed->begin());
        return batch;
    }
};

/// Ears for everything an SDL reader can produce, INCLUDING the one shape that
/// is not an input moment.
class SdlEars
    : public loom::WeaveBase<SdlEars, EarsState,
                             loom::Accept<KeyPressed, KeyReleased, TextEntered, PointerMoved,
                                          PointerButton, PointerWheel,
                                          zengine::surface::SurfaceCloseRequested>,
                             loom::Emit<>> {
public:
    explicit SdlEars(std::vector<SdlEvent>& heard) : heard_(&heard) {}
    void on(const KeyPressed& e, loom::Mail&) { note(e); }
    void on(const KeyReleased& e, loom::Mail&) { note(e); }
    void on(const TextEntered& e, loom::Mail&) { note(e); }
    void on(const PointerMoved& e, loom::Mail&) { note(e); }
    void on(const PointerButton& e, loom::Mail&) { note(e); }
    void on(const PointerWheel& e, loom::Mail&) { note(e); }
    void on(const zengine::surface::SurfaceCloseRequested& e, loom::Mail&) { note(e); }

private:
    template <class E>
    void note(const E& e) {
        ++state_.heard;
        heard_->push_back(e);
    }
    std::vector<SdlEvent>* heard_;
};

} // namespace

TEST_CASE("the weave's EMIT SET is whatever its reader can hand it, and order is the reader's") {
    // Two claims in one case, because they are one mechanism.
    //
    // ORDER: `KeyPressed, TextEntered, KeyReleased` is the sequence SDL queues
    // for a typed character, and it reaches the bus in that order. Nothing in
    // the weave sorts, batches by kind, or defers one population to serve
    // another -- it walks the reader's vector.
    //
    // EMIT: this weave is over a reader whose variant carries SEVEN
    // alternatives, six input shapes and one lifecycle fact, and the seventh is
    // published like any other. The terminal weave beside it derives six and
    // could not publish this one at all -- which is the point of deriving the
    // set rather than spelling it: what a weave says it can say is exactly what
    // its reader can hand it.
    loom::Switchboard bus;
    std::vector<std::vector<SdlEvent>> batches;
    batches.push_back({KeyPressed{scan::k5, "5", mod::kShift},
                       TextEntered{"%"},
                       KeyReleased{scan::k5, "5", mod::kShift},
                       PointerMoved{120, 36, 5, -2, space::kPixels, mod::kNone},
                       PointerButton{1, true, 77, 5, space::kPixels, mod::kNone},
                       PointerWheel{0.0, 0.25, 40, 8, space::kPixels, mod::kNone},
                       zengine::surface::SurfaceCloseRequested{}});

    const loom::WeaveId weave =
        loom::mount<InputWeaveT<FakeSdlReader>>(bus, FakeSdlReader{&batches});
    std::vector<SdlEvent> heard;
    (void)loom::mount<SdlEars>(bus, heard);

    bus.send(weave, loom::Message(loom::to_value(PumpInput{})));
    bus.pump();

    REQUIRE(heard.size() == 7);
    CHECK(sdl_at<KeyPressed>(heard, 0).scancode == scan::k5);
    CHECK(sdl_at<TextEntered>(heard, 1).text == "%");
    CHECK(sdl_at<KeyReleased>(heard, 2).scancode == scan::k5);
    CHECK(sdl_at<PointerMoved>(heard, 3).space == space::kPixels);
    CHECK(sdl_at<PointerButton>(heard, 4).x == 77);
    CHECK(sdl_at<PointerWheel>(heard, 5).dy == doctest::Approx(0.25));
    CHECK(std::holds_alternative<zengine::surface::SurfaceCloseRequested>(heard[6]));
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
    // A first breath is Loom's to grant. The suite activates through a real
    // lifecycle operator holding a real authority, because that is the only way a
    // weave can be activated at all — hand-posting the public shape is exactly
    // the forgery the attestation refuses.
    const loom::WeaveId door = zengine::testing::mount_door(bus);

    // ITS OWN ACTIVATION is its first breath (TIMER-02): the weave asks for ITS
    // beat, wire content pinned field by field. This is the trigger that makes
    // load order stop mattering — loaded long after the Timer, it still asks.
    zengine::testing::order_activation(bus, door, weave, 1);
    bus.pump();
    REQUIRE(asks.size() == 1);
    CHECK(asks[0].id == kPumpTimerId);
    CHECK(asks[0].delay_ms == kPumpBeatMs);
    CHECK(asks[0].repeat);
    CHECK(asks[0].role == kInputRole);
    // The ORDER travels with the ask (TIMER-03): this weave prefers to keep the
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

    // The doors snake deliberately does not open: releases, text and the
    // pointer have ZERO accepters in this game — published, delivered to
    // nobody, and that is legal (the recipient count is the pin).
    CHECK(r.publish_root(loom::to_value(KeyReleased{scan::kW, "W"})) == 0);
    CHECK(r.publish_root(loom::to_value(TextEntered{"w"})) == 0);
    CHECK(r.publish_root(loom::to_value(PointerMoved{1, 2, 1, 2, space::kCells})) == 0);
    CHECK(r.publish_root(loom::to_value(PointerButton{1, true, 3, 4, space::kCells})) == 0);
    CHECK(r.publish_root(loom::to_value(PointerWheel{0.0, 1.0, 3, 4, space::kCells})) == 0);

    // The adapter's own honest counter: one steering key became one turn.
    const Seen::Answer a = r.poke(controls, loom::PokeRead{"turns"});
    CHECK(a.kind == 0);
    CHECK(a.text == "1");
}

// ============================================================================
// Tier 5 — the REAL SDL event queue, through the real weave library
// ============================================================================
//
// Everything above is pure translation or a scripted reader. This is the claim
// neither of those can make, and it is the SDL reader's central one:
//
//     THIS TEST PROCESS PUSHES EVENTS INTO SDL, AND A SEPARATELY dlopen'ED
//     WEAVE LIBRARY POLLS THEM OUT.
//
// That only works if there is ONE SDL in the process. The Loom loads weaves
// with dlopen(RTLD_LOCAL) / LoadLibraryA, so a statically archived SDL would
// give zengine-input-sdl.so its own event queue, and every push below would go
// into a queue nothing reads -- the reader would see nothing, forever, with no
// error anywhere. cmake/ZengineSdl.cmake requires a shared SDL3 for exactly
// this reason, and this case is the positive evidence that the requirement is
// satisfied rather than merely written down.
//
// It also proves the things a unit test of a translator cannot: that the queue
// has an owner at all, that the owner is reached by the ordinary pump, that
// dequeue ORDER survives to the bus, and that a native close request travels
// end to end from the platform's queue to an ordinary listener.

#if defined(INPUT_HAS_SDL)

#include <SDL3/SDL.h>

TEST_CASE("the SDL reader owns the real queue: pushed events come out as Zen messages") {
    // The dummy video driver: a real SDL, a real queue, no display. Set in the
    // environment by tests/CMakeLists.txt and here as well, belt to braces.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));

    // A real window, so this is not merely "a queue exists": the reader is the
    // ear for a window somebody else owns, which is the whole arrangement.
    SDL_Window* window = SDL_CreateWindow("zengine-input-sdl test", 96, 48, 0);
    REQUIRE(window != nullptr);
    const SDL_WindowID window_id = SDL_GetWindowID(window);

    Rig r;
    const loom::WeaveId reader = r.load("zengine-input-sdl", INPUT_SDL_SO, kInputRole);

    // Drain anything the window's own creation queued (shown, exposed, focus
    // gained...), so the assertions below are about what THIS case pushed. Those
    // are precisely the events the reader ignores, and this proves it ignores
    // them rather than turning them into messages.
    r.pump_input_by_role();
    std::vector<SdlEvent> heard;
    const loom::WeaveId ears = loom::mount<SdlEars>(r.bus, heard);
    (void)ears;

    const auto push = [](SDL_Event e) { REQUIRE(SDL_PushEvent(&e)); };

    // The typed-character sequence, in the order SDL queues it.
    SDL_Event down{};
    down.type = SDL_EVENT_KEY_DOWN;
    down.key.windowID = window_id;
    down.key.scancode = SDL_SCANCODE_5;
    down.key.mod = SDL_KMOD_LSHIFT;
    down.key.down = true;
    push(down);

    SDL_Event text{};
    text.type = SDL_EVENT_TEXT_INPUT;
    text.text.windowID = window_id;
    text.text.text = "%";
    push(text);

    SDL_Event up = down;
    up.type = SDL_EVENT_KEY_UP;
    up.key.down = false;
    push(up);

    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = window_id;
    motion.motion.x = 36.0f;
    motion.motion.y = 24.0f;
    motion.motion.xrel = 4.0f;
    motion.motion.yrel = -1.0f;
    push(motion);

    SDL_Event click{};
    click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    click.button.windowID = window_id;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.down = true;
    click.button.clicks = 1;
    click.button.x = 37.0f;
    click.button.y = 25.0f;
    push(click);

    // ONE pump drains everything pending, in order.
    r.pump_input_by_role();

    REQUIRE(heard.size() == 5);
    CHECK(sdl_at<KeyPressed>(heard, 0).scancode == scan::k5);
    CHECK(sdl_at<KeyPressed>(heard, 0).modifiers == mod::kShift);
    // TEXT IS TEXT: the `%` came out of SDL's own text population, and nothing
    // anywhere computed it from `Shift+5`.
    CHECK(sdl_at<TextEntered>(heard, 1).text == "%");
    CHECK(sdl_at<KeyReleased>(heard, 2).scancode == scan::k5);
    CHECK(sdl_at<PointerMoved>(heard, 3).x == 36);
    CHECK(sdl_at<PointerMoved>(heard, 3).dx == 4);
    CHECK(sdl_at<PointerMoved>(heard, 3).space == space::kPixels);
    // THE EVENT-TIME POSITION. The button is at (37,25) and the last motion was
    // at (36,24); the published press says 37,25.
    CHECK(sdl_at<PointerButton>(heard, 4).x == 37);
    CHECK(sdl_at<PointerButton>(heard, 4).y == 25);
    CHECK(sdl_at<PointerButton>(heard, 4).space == space::kPixels);

    // The reader's own honest counters, through the ordinary poke door.
    CHECK(r.poke(reader, loom::PokeRead{"emitted"}).text == "5");

    // ---- the native close, end to end -------------------------------------
    heard.clear();
    SDL_Event close{};
    close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    close.window.windowID = window_id;
    push(close);
    r.pump_input_by_role();

    REQUIRE(heard.size() == 1);
    CHECK(std::holds_alternative<zengine::surface::SurfaceCloseRequested>(heard[0]));

    // SDL_EVENT_QUIT is the same lifecycle fact by another path -- some
    // platforms deliver an application quit that way and never as a window
    // event -- and it must not be the one that goes missing.
    heard.clear();
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    push(quit);
    r.pump_input_by_role();
    REQUIRE(heard.size() == 1);
    CHECK(std::holds_alternative<zengine::surface::SurfaceCloseRequested>(heard[0]));

    // ---- the ignored classes really are ignored ----------------------------
    heard.clear();
    SDL_Event shown{};
    shown.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    shown.window.windowID = window_id;
    push(shown);
    SDL_Event editing{};
    editing.type = SDL_EVENT_TEXT_EDITING;
    editing.edit.windowID = window_id;
    editing.edit.text = "";
    editing.edit.start = 0;
    editing.edit.length = 0;
    push(editing);
    SDL_Event thumb{};
    thumb.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    thumb.button.windowID = window_id;
    thumb.button.button = SDL_BUTTON_X1;
    thumb.button.down = true;
    thumb.button.x = 1.0f;
    thumb.button.y = 1.0f;
    push(thumb);
    r.pump_input_by_role();
    CHECK(heard.empty());

    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


TEST_CASE("both SDL weaves live: the Skin services its window and takes NOTHING off the queue") {
    // THE ONE-OWNER PROPERTY, asserted directly and with the exact interleaving
    // that used to break it.
    //
    // A Skin draining the whole queue every 10ms and dropping it is correct for
    // an output-only medium and is the single most
    // destructive thing it could do now, because SDL_PollEvent REMOVES what it
    // returns: a Skin that keeps calling it is not a second poller, it is a
    // thief. The Skin's `pump()` is empty now, and this case runs a real Skin
    // and a real reader side by side, pumps the SKIN between the push and the
    // read, and requires the events to survive.
    //
    // Note what is NOT asserted: that the Skin is idle. It still owns the window
    // and is still given execution time on its own beat. What it may not do is
    // consume.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");

    Rig r;
    const loom::WeaveId skin = r.load("zengine-skin-sdl", SKIN_SO_SDL, "zengine.skin");
    (void)r.load("zengine-input-sdl", INPUT_SDL_SO, kInputRole);

    // A canvas, so the Skin actually creates its window: an idle Skin with no
    // window would prove nothing about a Skin with one.
    zengine::surface::SurfaceCanvas canvas;
    canvas.width = 8;
    canvas.height = 4;
    canvas.layers.emplace_back();
    canvas.layers.back().rects.push_back(zengine::surface::SurfaceRect{0, 0, 2, 2, 0});
    r.publish_root(loom::to_value(canvas));
    CHECK(r.poke(skin, loom::PokeRead{"frames"}).text == "1");

    std::vector<SdlEvent> heard;
    (void)loom::mount<SdlEars>(r.bus, heard);

    SDL_Event down{};
    down.type = SDL_EVENT_KEY_DOWN;
    down.key.scancode = SDL_SCANCODE_Q;
    down.key.down = true;
    REQUIRE(SDL_PushEvent(&down));
    SDL_Event close{};
    close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    REQUIRE(SDL_PushEvent(&close));

    // The Skin is given its hands FIRST -- twice -- which is precisely when the
    // old code would have swallowed both events.
    r.bus.send_to_role("zengine.skin",
                       loom::Message(loom::to_value(zengine::surface::PumpSurface{})));
    r.bus.pump();
    r.bus.send_to_role("zengine.skin",
                       loom::Message(loom::to_value(zengine::surface::PumpSurface{})));
    r.bus.pump();
    CHECK(r.poke(skin, loom::PokeRead{"pumps"}).text == "2"); // it really did run

    // ...and the reader still finds everything, in order.
    r.pump_input_by_role();
    REQUIRE(heard.size() == 2);
    CHECK(sdl_at<KeyPressed>(heard, 0).scancode == scan::kQ);
    CHECK(std::holds_alternative<zengine::surface::SurfaceCloseRequested>(heard[1]));
}

#endif // INPUT_HAS_SDL
