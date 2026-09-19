// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The probe suite -- THE REUSABLE WORKSHOP JOURNEY, JUDGED BY WHAT ITS OWNERS SAID.
//
// THIS FILE OWNS the orchestration and the assertions of `examples/workshop-probe`: when the
// probe may say PASS, which answers may move it forward, what it closes before it stops, and
// which row of an inventory is "this session". It drives the REAL probe weave (probe.hpp, the
// code the host loads) on one bus, with the real Input weave behind a scripted link: the link
// keeps the link's contract -- every ask answered once, with Loom's answer authority, or told an
// outcome; its status answered with the far session it holds -- relays the Input office's asks
// to the real Input weave, and answers the guest door's and the Skin's from a script. The
// crossing itself is Loom's and is proven there (suite `bridge`); the socket, the door, the real
// Skin and settlement against real owners are `workshop_guests` and the two-process journey.

// main() and the framework live in doctest_main.cpp -- the shared one that refuses a run
// selecting zero cases (POP-01).
#include "doctest.h"

#include "../examples/workshop-probe/probe.hpp"

#include "input/input_weave.hpp"

#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>

#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

TEST_SUITE_BEGIN("workshop_probe");

namespace {

namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
using zengine::workshop_probe::ProbeWeave;
using zengine::workshop_probe::RunProbe;

// ---- the rig ------------------------------------------------------------------------------

using InputEvent = input::InputEvent;

struct FakeReader {
    std::vector<InputEvent> poll() { return {}; }
};

struct RigState {
    std::int64_t n = 0;
    ZEN_SHAPE(RigState, 1, ZEN_FIELD(n));
};

/// What the scripted link does with one ask.
struct Reply {
    enum class Kind { Relay, Answer, Outcome, Hold };
    Kind kind = Kind::Relay;
    std::optional<loom::Value> value; ///< Answer: what the far owner says
    std::string state;                ///< Outcome: the link's word
    std::string reason;
};

Reply relay() { return Reply{}; }
template <class T>
Reply answer(const T& msg) {
    Reply r;
    r.kind = Reply::Kind::Answer;
    r.value = loom::to_value(msg);
    return r;
}
Reply outcome(std::string state, std::string reason) {
    Reply r;
    r.kind = Reply::Kind::Outcome;
    r.state = std::move(state);
    r.reason = std::move(reason);
    return r;
}
Reply hold() {
    Reply r;
    r.kind = Reply::Kind::Hold;
    return r;
}

/// A trigger for the link's release of what it holds, sent from outside any delivery.
struct Release {
    ZEN_SHAPE(Release, 1);
};

/// One ask the link was handed, as the probe composed it.
struct Asked {
    std::string role;
    std::string shape;
    bool settle = false;
    std::uint64_t correlation = 0;
};

/// THE SCRIPTED LINK: office `loom.link.workshop`, one answer per ask with Loom's answer
/// authority, the Input office relayed to the real Input weave and everything else scripted.
class LoopLink
    : public loom::WeaveBase<
          LoopLink, RigState,
          loom::Accept<loom::link::Ask, loom::link::StatusRequested, input::InputSessionOpened,
                       input::InputInjected, loom::Ack, loom::Refused, Release>,
          // The far requests it carries are declared here so this bus resolves them, as the
          // far bus would: nothing on this bus accepts the door's or the Skin's shapes.
          loom::Emit<input::InputSessionRequested, input::InjectInput, input::InputSessionClosed,
                     ws::GuestConnectionsRequested, surface::SurfaceCaptureRequested,
                     surface::SurfaceCaptureChunkRequested, input::InputSessionOpened,
                     input::InputInjected, loom::Ack, loom::Refused, ws::GuestConnections,
                     surface::SurfaceCaptured, surface::SurfaceCaptureChunk, loom::link::Outcome,
                     loom::link::Status>> {
public:
    /// Decides what happens to one ask: (far role, the asked value).
    std::function<Reply(const std::string&, const loom::Value&)> script;
    loom::Switchboard* bus = nullptr;
    std::string link_state = "admitted";
    std::int64_t far_session = 20;
    std::string established = "agent";
    std::vector<Asked> asked; ///< every ask, in order

    void on(const loom::link::Ask& ask, loom::Mail& mail) {
        loom::DeferredAnswer due = mail.defer_answer();
        REQUIRE(due.valid());
        const std::string_view bytes(reinterpret_cast<const char*>(ask.payload.data()),
                                     ask.payload.size());
        loom::Unverified u = loom::parse(bytes);
        const std::string shape = u.claimed_name();
        const std::shared_ptr<const loom::Schema> schema =
            bus->resolve_schema(u.claimed_name(), u.claimed_version());
        REQUIRE(schema != nullptr);
        loom::Admission a = loom::admit(u, schema);
        REQUIRE(a.ok());
        loom::Value value = std::move(a).value();
        asked.push_back(Asked{ask.role, shape, ask.settle, mail.correlation()});
        const Reply r = script ? script(ask.role, value) : relay();
        switch (r.kind) {
        case Reply::Kind::Relay:
            forward(ask.role, std::move(value), std::move(due), mail);
            return;
        case Reply::Kind::Answer:
            (void)mail.bus().spend_deferred(due, loom::Message(*r.value));
            return;
        case Reply::Kind::Outcome: {
            loom::link::Outcome o;
            o.state = r.state;
            o.reason = r.reason;
            o.shape = shape;
            (void)loom::answer_deferred(due, mail, o);
            return;
        }
        case Reply::Kind::Hold:
            held_.push_back(Held{ask.role, std::move(value), std::move(due)});
            return;
        }
    }

    /// Hold the status answer until a `Release`, when set.
    bool hold_status = false;
    std::uint64_t status_correlation = 0; ///< the correlation the probe asked its status under

    void on(const loom::link::StatusRequested&, loom::Mail& mail) {
        status_correlation = mail.correlation();
        if (hold_status) {
            status_due_ = mail.defer_answer();
            return;
        }
        (void)mail.answer(status());
    }

    loom::link::Status status() const {
        loom::link::Status s;
        s.name = "workshop";
        s.state = link_state;
        s.session = far_session;
        s.established_name = established;
        s.detail = link_state == "admitted" ? std::string() : "the far host said no";
        return s;
    }

    // ---- the real Input weave's answers, handed back as the far owner's --------------------
    void on(const input::InputSessionOpened& a, loom::Mail& mail) { back(loom::to_value(a), mail); }
    void on(const input::InputInjected& a, loom::Mail& mail) { back(loom::to_value(a), mail); }
    void on(const loom::Ack& a, loom::Mail& mail) { back(loom::to_value(a), mail); }
    void on(const loom::Refused& a, loom::Mail& mail) { back(loom::to_value(a), mail); }

    /// Hand every held ask on now, through the script as it stands.
    void on(const Release&, loom::Mail& mail) {
        if (status_due_.valid()) {
            (void)loom::answer_deferred(status_due_, mail, status());
        }
        std::vector<Held> held;
        held.swap(held_);
        for (Held& h : held) {
            const Reply r = script ? script(h.role, h.value) : relay();
            if (r.kind == Reply::Kind::Answer) {
                (void)mail.bus().spend_deferred(h.due, loom::Message(*r.value));
            } else {
                forward(h.role, std::move(h.value), std::move(h.due), mail);
            }
        }
    }
    std::size_t holding() const { return held_.size(); }

private:
    struct Held {
        std::string role;
        loom::Value value;
        loom::DeferredAnswer due;
    };
    struct Relayed {
        std::uint64_t correlation = 0;
        loom::DeferredAnswer due;
    };
    void forward(const std::string& role, loom::Value value, loom::DeferredAnswer due,
                 loom::Mail& mail) {
        const std::uint64_t corr = ++next_;
        relayed_.push_back(Relayed{corr, std::move(due)});
        (void)mail.bus().send_to_role(role, loom::Message(std::move(value), this->self_,
                                                          loom::WeaveId{}, corr));
    }
    void back(loom::Value v, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        for (auto it = relayed_.begin(); it != relayed_.end(); ++it) {
            if (it->correlation == mail.correlation()) {
                loom::DeferredAnswer due = std::move(it->due);
                relayed_.erase(it);
                (void)mail.bus().spend_deferred(due, loom::Message(std::move(v)));
                return;
            }
        }
    }
    loom::DeferredAnswer status_due_;
    std::vector<Held> held_;
    std::vector<Relayed> relayed_;
    std::uint64_t next_ = 0;
};

/// The operator: sends `RunProbe` and keeps the probe's one answer per run.
class Operator : public loom::WeaveBase<Operator, RigState, loom::Accept<RunProbe, loom::Result, loom::Refused>,
                                        loom::Emit<RunProbe>> {
public:
    loom::WeaveId probe{};
    std::vector<std::string> answers;
    void on(const RunProbe& run, loom::Mail& mail) { (void)mail.send(probe, run); }
    void on(const loom::Result& r, loom::Mail& mail) {
        if (mail.answers_ask()) {
            answers.push_back(r.value);
        }
    }
    void on(const loom::Refused& r, loom::Mail& mail) {
        if (mail.answers_ask()) {
            answers.push_back("refused: " + r.reason);
        }
    }
};

/// A local participant that is not the link, saying answer-shaped things to the probe under the
/// correlation of the probe's open ask.
class Imposter : public loom::WeaveBase<Imposter, RigState, loom::Accept<input::PumpInput>,
                                        loom::Emit<input::InputSessionOpened, input::InputInjected, loom::link::Status>> {
public:
    loom::WeaveId probe{};
    std::uint64_t correlation = 0;
    std::optional<loom::link::Status> forged_status;
    void on(const input::PumpInput&, loom::Mail& mail) {
        if (forged_status) {
            (void)mail.send(probe, *forged_status, correlation);
            return;
        }
        (void)mail.send(probe, input::InputSessionOpened{999}, correlation);
        input::InputInjected fake;
        fake.session = 999;
        fake.admitted = 2;
        fake.first_seq = 1;
        fake.last_seq = 2;
        (void)mail.send(probe, fake, correlation);
    }
};

/// A four-byte picture, "abc\n", and the chunk door over it.
const std::string kPicture = "abc\n";

surface::SurfaceCaptured captured_ok() {
    surface::SurfaceCaptured c;
    c.ok = true;
    c.capture = 1;
    c.frame = 5;
    c.width = 3;
    c.height = 1;
    c.format = "text/cells";
    c.bytes = static_cast<std::int64_t>(kPicture.size());
    return c;
}

surface::SurfaceCaptureChunk chunk_of(const loom::Value& asked) {
    const surface::SurfaceCaptureChunkRequested want =
        loom::from_value<surface::SurfaceCaptureChunkRequested>(asked);
    surface::SurfaceCaptureChunk c;
    c.capture = want.capture;
    c.offset = want.offset;
    c.total = static_cast<std::int64_t>(kPicture.size());
    // Two bytes at a time, so a picture is more than one chunk.
    const std::string rest = kPicture.substr(static_cast<std::size_t>(want.offset), 2);
    c.data.assign(rest.begin(), rest.end());
    return c;
}

ws::GuestConnection row(std::int64_t session, const char* established, const char* claimed) {
    ws::GuestConnection c;
    c.connection = session;
    c.state = "admitted";
    c.established = established;
    c.claimed = claimed;
    c.session = session;
    return c;
}

struct Rig {
    loom::Switchboard bus;
    LoopLink* link = nullptr;
    loom::WeaveId link_id{};
    loom::WeaveId probe_id{};
    ProbeWeave* probe = nullptr;
    Operator* op = nullptr;
    loom::WeaveId op_id{};
    ws::GuestConnections inventory;

    Rig() {
        {
            auto w = std::make_unique<input::InputWeaveT<FakeReader>>();
            input::InputWeaveT<FakeReader>* raw = w.get();
            loom::Grant grant = loom::emit_default_grant(*raw);
            loom::allow_poke_answers(grant);
            const loom::WeaveId id = bus.register_weave(std::move(w), std::move(grant),
                                                        std::string(input::kInputRole));
            raw->zen_set_self(id);
        }
        {
            auto w = std::make_unique<LoopLink>();
            link = w.get();
            loom::Grant grant = loom::emit_default_grant(*link);
            loom::allow_poke_answers(grant);
            link_id = bus.register_weave(std::move(w), std::move(grant),
                                         loom::link::role_of("workshop"));
            link->zen_set_self(link_id);
            link->bus = &bus;
        }
        probe_id = loom::mount<ProbeWeave>(bus);
        probe = static_cast<ProbeWeave*>(bus.weave(probe_id));
        op_id = loom::mount<Operator>(bus);
        op = static_cast<Operator*>(bus.weave(op_id));
        op->probe = probe_id;
        inventory.listen = "127.0.0.1:7654";
        inventory.rows.push_back(row(20, "agent", "agent-from-elsewhere"));
        link->script = [this](const std::string& role, const loom::Value& asked) -> Reply {
            const std::string shape = asked.schema().name();
            if (role == input::kInputRole) {
                return relay();
            }
            if (shape == ws::GuestConnectionsRequested::zen_name) {
                return answer(inventory);
            }
            if (shape == surface::SurfaceCaptureRequested::zen_name) {
                return answer(captured_ok());
            }
            if (shape == surface::SurfaceCaptureChunkRequested::zen_name) {
                return answer(chunk_of(asked));
            }
            return outcome(loom::link::kOutcomeRefused, "the script has no answer for " + shape);
        };
    }

    /// Run the journey and drain; the probe's answer, or "(pending)".
    std::string run(RunProbe r = default_run()) {
        const std::size_t before = op->answers.size();
        (void)bus.send(op_id, loom::Message(loom::to_value(r)));
        bus.drain_until_idle();
        return op->answers.size() > before ? op->answers.back() : std::string("(pending)");
    }
    static RunProbe default_run() {
        RunProbe r;
        r.picture = "zen-probe-test-picture.bin";
        return r;
    }
    /// Script one far owner's answer to one request shape, over the rig's script as it stands.
    void answer_with(const char* request, std::function<Reply(const loom::Value&)> say) {
        auto base = link->script;
        link->script = [base, request, say](const std::string& role, const loom::Value& asked) {
            if (asked.schema().name() == request) {
                return say(asked);
            }
            return base(role, asked);
        };
    }
    std::size_t asks_of(const char* shape) const {
        std::size_t n = 0;
        for (const Asked& a : link->asked) {
            n += a.shape == shape ? std::size_t{1} : std::size_t{0};
        }
        return n;
    }
};

bool starts(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }
bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

struct RemoveOnExit {
    std::string path;
    ~RemoveOnExit() { std::remove(path.c_str()); }
};

} // namespace

// =============================================================================
// PASS means every promised result, and nothing less
// =============================================================================

TEST_CASE("probe: PASS carries every promised result, and the session is closed for the next run") {
    Rig r;
    RemoveOnExit tidy{Rig::default_run().picture};
    const std::string said = r.run();
    INFO("the run: " << said);
    REQUIRE(starts(said, "PASS"));
    CHECK(has(said, "link 'workshop' admitted as 'agent' (far session 20)"));
    CHECK(has(said, "session 1 opened by zengine.input"));
    CHECK(has(said, "injected 2 moment(s), session seq 1..2, settled on Workshop's bus"));
    CHECK(has(said, "this session is 'agent' (claimed 'agent-from-elsewhere', weave 20)"));
    CHECK(has(said, "capture 1 at frame 5: 3x1 text/cells, 4 bytes"));
    CHECK(has(said, "(4 of 4 bytes, text/cells, read back whole)"));
    CHECK(has(said, "session 1 closed"));
    std::error_code ec;
    CHECK(std::filesystem::file_size(Rig::default_run().picture, ec) == 4);
    CHECK(r.probe->step() == "idle");
    // The session was really closed: the next run opens the next one.
    const std::string again = r.run();
    CHECK(starts(again, "PASS"));
    CHECK(has(again, "session 2 opened"));
}

TEST_CASE("probe: the injection is settled before the picture is asked for") {
    Rig r;
    RemoveOnExit tidy{Rig::default_run().picture};
    REQUIRE(starts(r.run(), "PASS"));
    std::size_t inject = r.link->asked.size();
    std::size_t capture = r.link->asked.size();
    for (std::size_t i = 0; i < r.link->asked.size(); ++i) {
        const Asked& a = r.link->asked[i];
        if (a.shape == input::InjectInput::zen_name) {
            inject = i;
            CHECK(a.settle); // the answer waits for what the injection set in motion
        } else {
            CHECK_FALSE(a.settle);
        }
        if (a.shape == surface::SurfaceCaptureRequested::zen_name) {
            capture = i;
        }
    }
    REQUIRE(inject < r.link->asked.size());
    REQUIRE(capture < r.link->asked.size());
    CHECK(inject < capture);
    // The capture is asked for once, NOW -- after settlement -- not after a remembered frame.
    CHECK(r.asks_of(surface::SurfaceCaptureRequested::zen_name) == 1);
}

TEST_CASE("probe: a refusal after the session opened still closes it, and the next run opens one") {
    Rig r;
    RemoveOnExit tidy{Rig::default_run().picture};
    bool refuse_inspection = true;
    r.answer_with(ws::GuestConnectionsRequested::zen_name, [&](const loom::Value&) {
        return refuse_inspection ? answer(loom::Refused{"not today"}) : answer(r.inventory);
    });
    const std::string first = r.run();
    INFO("first run: " << first);
    CHECK(starts(first, "STOPPED"));
    CHECK(has(first, "refused at inspect: not today"));
    CHECK(has(first, "session 1 closed in cleanup"));
    refuse_inspection = false;
    const std::string second = r.run();
    INFO("second run: " << second);
    CHECK(starts(second, "PASS"));
    CHECK(has(second, "session 2 opened"));
}

TEST_CASE("probe: a picture that cannot be written stops the run; it is never a PASS") {
    Rig r;
    RunProbe run = Rig::default_run();
    run.picture = "zen-probe-no-such-directory/picture.bin";
    const std::string said = r.run(run);
    INFO("the run: " << said);
    CHECK(starts(said, "STOPPED"));
    CHECK(has(said, "the picture could not be opened for writing at zen-probe-no-such-directory"));
    CHECK(has(said, "session 1 closed in cleanup"));
    CHECK_FALSE(has(said, "PASS"));
}

TEST_CASE("probe: an inventory that does not list this link's session stops the run") {
    Rig r;
    RemoveOnExit tidy{Rig::default_run().picture};
    r.inventory.rows.clear();
    const std::string empty = r.run();
    INFO("an empty inventory: " << empty);
    CHECK(starts(empty, "STOPPED"));
    CHECK(has(empty, "none is this link's session (far session 20)"));
    // A row for this session under another name is not this session admitted as itself.
    r.inventory.rows.push_back(row(20, "impostor", "agent"));
    const std::string renamed = r.run();
    INFO("a renamed row: " << renamed);
    CHECK(starts(renamed, "STOPPED"));
    CHECK(has(renamed, "not admitted as 'agent'"));
    CHECK(r.asks_of(surface::SurfaceCaptureRequested::zen_name) == 0); // no picture of either
}

TEST_CASE("probe: the row it names is this link's own session, whoever else is admitted") {
    Rig r;
    RemoveOnExit tidy{Rig::default_run().picture};
    // Ours first, another host's after it -- and another before it, on a second run.
    r.inventory.rows.push_back(row(31, "other", "other-host"));
    const std::string said = r.run();
    INFO("the run: " << said);
    CHECK(starts(said, "PASS"));
    CHECK(has(said, "'agent'"));
    CHECK_FALSE(has(said, "'other'"));
    r.inventory.rows.insert(r.inventory.rows.begin(), row(7, "earlier", "earlier-host"));
    const std::string again = r.run();
    CHECK(starts(again, "PASS"));
    CHECK(has(again, "lists 3 connection(s); this session is 'agent'"));
}

TEST_CASE("probe: ordinary initial status cannot establish the link respondent") {
    Rig r;
    RemoveOnExit tidy{Rig::default_run().picture};
    r.link->hold_status = true;
    const loom::WeaveId imposter_id = loom::mount<Imposter>(r.bus);
    Imposter* imposter = static_cast<Imposter*>(r.bus.weave(imposter_id));
    imposter->probe = r.probe_id;
    (void)r.bus.send(r.op_id, loom::Message(loom::to_value(Rig::default_run())));
    r.bus.drain_until_idle();
    REQUIRE(r.probe->step() == "status");
    REQUIRE(r.link->asked.empty());
    // Before any authenticated reply pins the link, plausible words are still not an answer.
    imposter->correlation = r.link->status_correlation;
    imposter->forged_status = r.link->status();
    (void)r.bus.send(imposter_id, loom::Message(loom::to_value(input::PumpInput{})));
    r.bus.drain_until_idle();
    CHECK(r.probe->step() == "status");
    CHECK(r.link->asked.empty());
    CHECK(r.op->answers.empty());
    // The genuine answer must remain spendable and finish the original journey.
    (void)r.bus.send(r.link_id, loom::Message(loom::to_value(Release{})));
    r.bus.drain_until_idle();
    REQUIRE(r.op->answers.size() == 1);
    CHECK(starts(r.op->answers.back(), "PASS"));
}

TEST_CASE("probe: answer-shaped words from a participant that is not the link move nothing") {
    Rig r;
    RemoveOnExit tidy{Rig::default_run().picture};
    r.answer_with(input::InputSessionRequested::zen_name,
                  [](const loom::Value&) { return hold(); }); // the link is silent for now
    const loom::WeaveId imposter_id = loom::mount<Imposter>(r.bus);
    Imposter* imposter = static_cast<Imposter*>(r.bus.weave(imposter_id));
    imposter->probe = r.probe_id;
    (void)r.bus.send(r.op_id, loom::Message(loom::to_value(Rig::default_run())));
    r.bus.drain_until_idle();
    REQUIRE(r.link->holding() == 1);
    REQUIRE(r.link->asked.size() == 1);
    // The imposter says "session 999 opened" and "injected" under the probe's own correlation...
    imposter->correlation = r.link->asked.back().correlation;
    (void)r.bus.send(imposter_id, loom::Message(loom::to_value(input::PumpInput{})));
    r.bus.drain_until_idle();
    CHECK(r.link->asked.size() == 1); // ...and the probe asked nothing more because of it
    CHECK(r.probe->step() == "open");
    // ...then the link's genuine answer arrives, and the journey runs on it.
    r.answer_with(input::InputSessionRequested::zen_name, [](const loom::Value&) { return relay(); });
    (void)r.bus.send(r.link_id, loom::Message(loom::to_value(Release{})));
    r.bus.drain_until_idle();
    REQUIRE_FALSE(r.op->answers.empty());
    INFO("the run: " << r.op->answers.back());
    CHECK(starts(r.op->answers.back(), "PASS"));
    CHECK(has(r.op->answers.back(), "session 1 opened"));
}

// =============================================================================
// What a stopped run cleans up, and what it says it did
// =============================================================================

TEST_CASE("probe: a refused capture, and a chunk that does not continue the picture, stop and clean up") {
    Rig r;
    r.answer_with(surface::SurfaceCaptureRequested::zen_name, [](const loom::Value&) {
        surface::SurfaceCaptured c;
        c.ok = false;
        c.refusal = "this medium has no picture to read back right now";
        return answer(c);
    });
    const std::string refused = r.run();
    INFO("refused: " << refused);
    CHECK(starts(refused, "STOPPED"));
    CHECK(has(refused, "zengine.skin refused the capture: this medium has no picture"));
    CHECK(has(refused, "session 1 closed in cleanup"));
    // A chunk from another picture.
    Rig s;
    s.answer_with(surface::SurfaceCaptureChunkRequested::zen_name, [](const loom::Value& asked) {
        surface::SurfaceCaptureChunk c = chunk_of(asked);
        c.capture = 9;
        return answer(c);
    });
    const std::string foreign = s.run();
    INFO("foreign chunk: " << foreign);
    CHECK(starts(foreign, "STOPPED"));
    CHECK(has(foreign, "sent a chunk of capture 9 at 0"));
    CHECK(has(foreign, "session 1 closed in cleanup"));
    // A chunk at the wrong offset.
    Rig t;
    t.answer_with(surface::SurfaceCaptureChunkRequested::zen_name, [](const loom::Value& asked) {
        surface::SurfaceCaptureChunk c = chunk_of(asked);
        c.offset += 1;
        return answer(c);
    });
    const std::string skipped = t.run();
    CHECK(starts(skipped, "STOPPED"));
    CHECK(has(skipped, "session 1 closed in cleanup"));
}

TEST_CASE("probe: a lost link leaves the session to Workshop's door, and says so") {
    Rig r;
    r.answer_with(input::InjectInput::zen_name, [](const loom::Value&) {
        return outcome(loom::link::kOutcomeLost, "the far host closed the connection");
    });
    const std::string said = r.run();
    INFO("the run: " << said);
    CHECK(starts(said, "STOPPED"));
    CHECK(has(said, "link: lost at inject"));
    CHECK(has(said, "session 1 is Workshop's to close"));
    CHECK_FALSE(has(said, "closed in cleanup"));
    // Nothing was asked after the loss: no close into a dead link, no retry of the injection.
    CHECK(r.asks_of(input::InputSessionClosed::zen_name) == 0);
    CHECK(r.asks_of(input::InjectInput::zen_name) == 1);
}

TEST_CASE("probe: a cleanup the owner refuses is said beside the reason the run stopped") {
    Rig r;
    r.answer_with(ws::GuestConnectionsRequested::zen_name,
                  [](const loom::Value&) { return answer(loom::Refused{"not today"}); });
    r.answer_with(input::InputSessionClosed::zen_name,
                  [](const loom::Value&) { return answer(loom::Refused{"cannot close that"}); });
    const std::string said = r.run();
    INFO("the run: " << said);
    CHECK(starts(said, "STOPPED"));
    CHECK(has(said, "refused at inspect: not today"));
    CHECK(has(said, "cleanup refused: cannot close that"));
}

TEST_CASE("probe: a close nobody has answered leaves the run visibly pending, and it can finish later") {
    Rig r;
    RemoveOnExit tidy{Rig::default_run().picture};
    r.answer_with(ws::GuestConnectionsRequested::zen_name,
                  [](const loom::Value&) { return answer(loom::Refused{"not today"}); });
    r.answer_with(input::InputSessionClosed::zen_name, [](const loom::Value&) { return hold(); });
    CHECK(r.run() == "(pending)");
    CHECK(r.probe->step() == "cleanup");
    // A second run is refused while this one stands, and says where it stands.
    CHECK(r.run() == "refused: a run is already in flight (cleanup); wait for its answer");
    // The close is answered after all: the run ends, and the next one can open a session.
    r.answer_with(input::InputSessionClosed::zen_name, [](const loom::Value&) { return relay(); });
    r.answer_with(ws::GuestConnectionsRequested::zen_name,
                  [&r](const loom::Value&) { return answer(r.inventory); });
    (void)r.bus.send(r.link_id, loom::Message(loom::to_value(Release{})));
    r.bus.drain_until_idle();
    REQUIRE(r.op->answers.size() == 2);
    CHECK(starts(r.op->answers.back(), "STOPPED"));
    CHECK(has(r.op->answers.back(), "session 1 closed in cleanup"));
    CHECK(starts(r.run(), "PASS"));
}

TEST_CASE("probe: an answer the step did not ask for stops the run") {
    Rig r;
    r.answer_with(ws::GuestConnectionsRequested::zen_name,
                  [](const loom::Value&) { return answer(captured_ok()); });
    const std::string said = r.run();
    INFO("the run: " << said);
    CHECK(starts(said, "STOPPED"));
    CHECK(has(said, "an answer for GuestConnectionsRequested arrived at step inspect"));
    CHECK(has(said, "session 1 closed in cleanup"));
}

TEST_CASE("probe: a link that is not admitted stops the run before anything is asked of Workshop") {
    Rig r;
    r.link->link_state = "denied";
    const std::string said = r.run();
    INFO("the run: " << said);
    CHECK(said == "STOPPED: the link 'workshop' is denied: the far host said no");
    CHECK(r.link->asked.empty());
}

TEST_SUITE_END();
