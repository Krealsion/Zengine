// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- the Terminal finding powers. A source (VM-POP-12) of the
// `workshop_panes` entry: a live Workshop on a real bus with the basic provider mounted, the
// discovery door mounted as the host mounts it, and the Terminal participant widened by the host's
// own call, a line typed through Workshop's own door, from `PaneRig` (`workshop_support.hpp`).

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

namespace {

inline constexpr const char* kLineOffice = "zengine.test.line-typer";

/// A PARTY THAT TYPES LINES, the way the Terminal pane hands Workshop a whole line to run.
class LineTyper : public loom::WeaveBase<LineTyper, DoorAskerState,
                                         loom::Accept<TerminalActed, SeatDo>,
                                         loom::Emit<TerminalActRequested>> {
public:
    std::string line;
    std::vector<TerminalActed> acted;
    void on(const SeatDo&, loom::Mail& mail) {
        (void)mail.as_role(kLineOffice)
            .send_to_role(kWorkshopProvider, TerminalActRequested{kTerminalSubmitAct, line});
    }
    void on(const TerminalActed& said, loom::Mail&) { acted.push_back(said); }
};

/// THE TERMINAL AS THE HOST MOUNTS IT: its one baseline rule, then the discovery door's two asks
/// through the host's own call, so what a typed line proves is what the host grants.
struct FindingRig {
    PaneRig r;
    loom::TerminalSession* terminal = nullptr;
    LineTyper* typer = nullptr;
    loom::WeaveId typer_id{};

    FindingRig() {
        r.mount_workshop();
        const load::Executed done = r.run_plan(pane_plan());
        REQUIRE_MESSAGE(done.ok, done.refusal);
        (void)r.mount_powers();
        loom::TerminalVocabulary vocab;
        vocab.knows(loom::schema_of<surface::SurfaceText>())
            .accepts(loom::schema_of<loom::Ack>())
            .accepts(loom::schema_of<loom::Result>())
            .accepts(loom::schema_of<loom::Refused>());
        loom::Grant grant;
        grant.allow_to_role(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version,
                            surface::kSkinRole);
        let_terminal_find_powers(vocab, grant);
        const loom::MountedTerminal mounted = loom::host_mount_terminal(
            r.bus, std::make_unique<loom::TerminalSession>("workshop", std::move(vocab)),
            std::move(grant));
        r.host.terminal = mounted.session;
        terminal = mounted.session;
        auto seat = std::make_unique<LineTyper>();
        typer = seat.get();
        loom::Grant may;
        may.allow_to_role(TerminalActRequested::zen_name, TerminalActRequested::zen_version,
                          kWorkshopProvider);
        typer_id = r.bus.register_weave(std::move(seat), std::move(may), std::string(kLineOffice));
        typer->zen_set_self(typer_id);
        r.ready();
    }

    /// TYPE ONE LINE AND SUBMIT IT through Workshop's own door, and answer what came back to the
    /// participant as an authenticated answer, if anything did.
    std::optional<loom::Value> typed(const std::string& line) {
        const std::size_t answers_before =
            of_kind(*terminal, loom::TranscriptKind::AnswerReceived).size();
        typer->line = line;
        (void)r.bus.send(typer_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                 loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
        REQUIRE_FALSE(typer->acted.empty());
        REQUIRE_MESSAGE(typer->acted.back().accepted, typer->acted.back().refusal);
        const std::vector<loom::TranscriptEntry> now =
            of_kind(*terminal, loom::TranscriptKind::AnswerReceived);
        if (now.size() == answers_before) {
            return std::nullopt;
        }
        CHECK(now.back().authored_role.empty()); // the door answers; it speaks as no office
        const std::optional<loom::ReceivedMessage> held = terminal->received(now.back().message);
        REQUIRE(held.has_value());
        return held->value;
    }
};

} // namespace

TEST_CASE("the documented discovery lines, typed through Workshop's own door, answer what the "
          "door derives") {
    FindingRig s;

    // THE FIND LINE, exactly as documented: one field typed, every other one absent.
    const std::optional<loom::Value> found =
        s.typed("ask @zengine.powers FindPowers 1 text=larger");
    REQUIRE(found.has_value());
    REQUIRE(loom::same_identity(found->schema(), *loom::schema_of<PowersFound>()));
    const PowersFound said = loom::from_value<PowersFound>(*found);
    FindPowers asked;
    asked.text = "larger";
    // THE SAME ROWS THE DOOR DERIVES FOR THAT QUESTION, byte for byte: one source for every asker.
    CHECK(loom::serialize(*found) == loom::serialize(loom::to_value(find_powers(s.r.catalog, asked))));
    REQUIRE(said.rows.size() == 1);
    CHECK(said.rows[0].identity == "math.max");
    CHECK(said.rows[0].about == "the larger of two integers");
    CHECK(said.rows[0].provider == "zengine.operators.basic");

    // THE DESCRIBE LINE: one identity's contribution in force and its stack.
    const std::optional<loom::Value> described =
        s.typed("ask @zengine.powers DescribePower 1 identity=math.max");
    REQUIRE(described.has_value());
    const PowerDescribed one = loom::from_value<PowerDescribed>(*described);
    CHECK(one.ok);
    CHECK(one.row.identity == "math.max");
    REQUIRE(one.stack.size() == 1);
    CHECK(one.stack[0].provider == "zengine.operators.basic");
    CHECK_FALSE(one.contribution.empty());

    // ...AND A QUESTION THE DOOR REFUSES IS ANSWERED IN WORDS, not silence.
    const std::optional<loom::Value> refused =
        s.typed("ask @zengine.powers FindPowers 1 kind=sources");
    REQUIRE(refused.has_value());
    const PowersFound no = loom::from_value<PowersFound>(*refused);
    CHECK_FALSE(no.ok);
    CHECK(no.reason == "a kind is source, operator or conversion; 'sources' is none of them");
}

TEST_CASE("the Terminal may ask the discovery door its two questions, and nothing more") {
    // THE WIDENING IS EXACTLY TWO RULES, both to the door's office: asking describes, and grants
    // nothing to send, mount or open anywhere.
    loom::TerminalVocabulary vocab;
    loom::Grant grant;
    let_terminal_find_powers(vocab, grant);
    CHECK(grant.rules().size() == 2);
    CHECK(grant.permits_role(kFindPowersName, kFindPowersVersion, kPowersRole));
    CHECK(grant.permits_role(DescribePower::zen_name, DescribePower::zen_version, kPowersRole));
    for (const char* office : {kArrangementRole, kSampleRole, kWorkshopProvider}) {
        CHECK_FALSE(grant.permits_role(kFindPowersName, kFindPowersVersion, office));
        CHECK_FALSE(grant.permits_role(DescribePower::zen_name, DescribePower::zen_version, office));
    }
    CHECK_FALSE(grant.permits_role(SampleRequested::zen_name, SampleRequested::zen_version,
                                   kSampleRole));
    CHECK_FALSE(grant.permits_role(ArrangementRequested::zen_name,
                                   ArrangementRequested::zen_version, kArrangementRole));

    // ...AND THE LIVE PARTICIPANT AGREES: the same question aimed at another office is refused at
    // delivery, and nothing answers it.
    FindingRig s;
    (void)s.r.mount_sampler();
    std::vector<loom::Refusal> denied;
    const loom::ObserverId tap = s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Refused && e.schema_name == kFindPowersName) {
            denied.push_back(e.refusal);
        }
    });
    CHECK_FALSE(s.typed("ask @zengine.sources FindPowers 1 text=larger").has_value());
    s.r.bus.remove_observer(tap);
    REQUIRE(denied.size() == 1);
    CHECK(denied[0].reason == loom::RefusalReason::CapabilityDenied);
}
