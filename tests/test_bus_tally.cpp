// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// Suite `bus_tally`: the tally a host keeps of where its bus spends its time
// (`workshop/bus_tally.hpp`), over a real bus and real handlers: each weave's handler time from
// Loom's own stamps, and an input reaching its paint through the weave it reached.

#include "doctest.h"

#include "graphics/vocabulary.hpp"
#include "workshop/bus_tally.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace g = zengine::graphics;
using zengine::workshop::BusTally;
using zengine::workshop::HandlerRow;
using zengine::workshop::PaintRow;

struct TallyPing {
    std::int64_t n = 0;
    ZEN_SHAPE(TallyPing, 1, ZEN_FIELD(n));
};

struct TallyBeat {
    std::int64_t n = 0;
    ZEN_SHAPE(TallyBeat, 1, ZEN_FIELD(n));
};

struct TallySuiteState {
    std::int64_t n = 0;
    ZEN_SHAPE(TallySuiteState, 1, ZEN_FIELD(n));
};

/// A handler that takes `ms` of wall time on every ping.
class Slow
    : public loom::WeaveBase<Slow, TallySuiteState, loom::Accept<TallyPing>, loom::Emit<>> {
public:
    explicit Slow(int ms) : ms_(ms) {}
    void on(const TallyPing&, loom::Mail&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms_));
    }

private:
    int ms_;
};

/// A pane that hears a key and draws on its next beat: its paint comes turns after the key.
class LaterPane
    : public loom::WeaveBase<LaterPane, TallySuiteState,
                             loom::Accept<g::AreaKeyPressed, TallyBeat>, loom::Emit<g::Drawn>> {
public:
    void on(const g::AreaKeyPressed& k, loom::Mail&) { due_ = k.area; }
    void on(const TallyBeat&, loom::Mail& mail) {
        if (due_.empty()) {
            return;
        }
        g::Drawn d;
        d.area = due_;
        due_.clear();
        (void)mail.send_to_role("suite.window", d);
    }

private:
    std::string due_;
};

class Window
    : public loom::WeaveBase<Window, TallySuiteState, loom::Accept<g::Drawn>, loom::Emit<>> {
public:
    void on(const g::Drawn&, loom::Mail&) { ++painted; }
    int painted = 0;
};

template <class W, class... Args>
loom::WeaveId mount_role(loom::Switchboard& bus, std::string role, Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    loom::Grant grant = loom::emit_default_grant(*raw);
    const loom::WeaveId id =
        bus.register_weave(std::move(weave), std::move(grant), std::move(role));
    raw->zen_set_self(id);
    return id;
}

const HandlerRow* row_of(const std::vector<HandlerRow>& rows, const std::string& who) {
    const auto it = std::find_if(rows.begin(), rows.end(),
                                 [&](const HandlerRow& r) { return r.who == who; });
    return it == rows.end() ? nullptr : &*it;
}

bool says(const std::vector<std::string>& lines, const std::string& part) {
    return std::any_of(lines.begin(), lines.end(),
                       [&](const std::string& l) { return l.find(part) != std::string::npos; });
}

} // namespace

TEST_CASE("a weave whose handler is slow shows in the tally of handler times, named with its "
          "time") {
    loom::Switchboard bus;
    BusTally tally(bus);
    const loom::WeaveId slow = mount_role<Slow>(bus, "suite.slow", 30);
    const loom::WeaveId quick = mount_role<Slow>(bus, "suite.quick", 0);
    for (int i = 0; i < 2; ++i) {
        (void)bus.send(slow, loom::Message(loom::to_value(TallyPing{i})));
        (void)bus.send(quick, loom::Message(loom::to_value(TallyPing{i})));
    }
    bus.drain_until_idle();

    const std::vector<HandlerRow> rows = tally.handlers();
    const HandlerRow* s = row_of(rows, "suite.slow");
    const HandlerRow* q = row_of(rows, "suite.quick");
    REQUIRE(s != nullptr);
    REQUIRE(q != nullptr);
    CHECK(rows.front().who == "suite.slow"); // the most time in all, first
    CHECK(s->deliveries == 2);
    CHECK(s->max_ns >= 30ull * 1000 * 1000);
    CHECK(s->total_ns >= 60ull * 1000 * 1000);
    CHECK(s->slowest == "TallyPing");
    CHECK(q->max_ns < s->max_ns);
    // ...and in words, its name beside its time.
    const std::vector<std::string> said = tally.lines();
    CHECK(says(said, "handlers of suite.slow: 2 deliveries, "));
    CHECK(says(said, " ms (TallyPing)"));
}

TEST_CASE("a key reaches its paint when the weave it reached draws, turns after the key") {
    loom::Switchboard bus;
    BusTally tally(bus);
    const loom::WeaveId pane = mount_role<LaterPane>(bus, "suite.pane");
    (void)mount_role<Window>(bus, "suite.window");
    g::AreaKeyPressed key;
    key.area = "suite.area";
    (void)bus.send(pane, loom::Message(loom::to_value(key)));
    bus.drain_until_idle();
    CHECK(tally.paints().empty()); // reached, and nothing painted yet
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    (void)bus.send(pane, loom::Message(loom::to_value(TallyBeat{1})));
    bus.drain_until_idle();

    const std::vector<PaintRow> paints = tally.paints();
    REQUIRE(paints.size() == 1);
    CHECK(paints.front().kind == "key");
    CHECK(paints.front().who == "suite.pane");
    CHECK(paints.front().count == 1);
    CHECK(paints.front().max_ns >= 20ull * 1000 * 1000); // measured from the key, not the beat
    CHECK(tally.unpainted() == 0);
    CHECK(says(tally.lines(), "a key to its paint by suite.pane: 1, median "));

    // A beat that paints with no key waiting measures nothing.
    (void)bus.send(pane, loom::Message(loom::to_value(TallyBeat{2})));
    bus.drain_until_idle();
    CHECK(tally.paints().front().count == 1);
}
