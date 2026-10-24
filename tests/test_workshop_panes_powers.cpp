// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- the Terminal finding powers. A source (VM-POP-12) of the panes
// program, run in `workshop_panes_terminal`: a live Workshop on a real bus with the basic provider
// mounted, the discovery door mounted as the host mounts it, and the Terminal participant widened
// by the host's own call, a line typed through Workshop's own door, from `PaneRig`
// (`workshop_support.hpp`); and Flow's pane, loaded beside them, driven for its action census and
// asked for its manual.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"
#include "census.hpp"

#include "office_manual.hpp"

#include "flow/flow-pane/vocabulary.hpp"
#include "manuals/zengine.powers.hpp"
#include "workshop/terminal_speech.hpp"

#include <set>

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

/// THE TERMINAL AS THE HOST MOUNTS IT, by the host's own call, so what a typed line proves is what
/// the host grants.
struct FindingRig {
    PaneRig r;
    loom::TerminalSession* terminal = nullptr;
    LineTyper* typer = nullptr;
    loom::WeaveId typer_id{};

    /// The door is mounted before realization, as the host mounts it: a participant the plan
    /// loads may ask it at its first breath.
    explicit FindingRig(load::LoadPlan plan = pane_plan()) {
        r.mount_workshop();
        (void)r.mount_powers();
        const load::Executed done = r.run_plan(std::move(plan));
        REQUIRE_MESSAGE(done.ok, done.refusal);
        loom::TerminalVocabulary vocab;
        loom::Grant grant;
        let_terminal_speak(vocab, grant);
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
    CHECK(no.reason == "a kind is source, operator, conversion or form; 'sources' is none of them");
}

TEST_CASE("the Terminal may ask the discovery door its two questions, and nothing more") {
    // THE WIDENING IS EXACTLY THREE RULES, all to the door's office -- the two versions of the
    // finding question and the describing one: asking describes, and grants nothing to send,
    // mount or open anywhere.
    loom::TerminalVocabulary vocab;
    loom::Grant grant;
    let_terminal_find_powers(vocab, grant);
    CHECK(grant.rules().size() == 3);
    CHECK(grant.permits_role(kFindPowersName, 1, kPowersRole));
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

TEST_CASE("the host's Terminal finds powers as its ordinary verb, speaks only to the offices that "
          "widen it, and draws in no window") {
    // WHAT THE HOST GRANTS: every rule to one of the three offices that widen it -- none to a
    // medium, a window or everyone.
    loom::TerminalVocabulary vocab;
    loom::Grant grant;
    let_terminal_speak(vocab, grant);
    const std::set<std::string> widening{kPowersRole, kEditorSwitchRole, kWorkshopProvider};
    REQUIRE_FALSE(grant.rules().empty());
    for (const loom::SendRule& rule : grant.rules()) {
        CAPTURE(rule.shape_name);
        CHECK_FALSE(rule.any_target);
        CHECK(widening.count(rule.target_role) == 1);
        CHECK(rule.shape_name != zengine::graphics::Drawn::zen_name);
    }
    // ITS ORDINARY VERB is the discovery door's find, at the version this build asks with...
    CHECK(grant.permits_role(kFindPowersName, kFindPowersVersion, kPowersRole));
    // ...AND DRAWING IS A WORD IT KNOWS AND MAY NOT SAY: the completer describes it, and no rule
    // above sends it.
    CHECK(vocab.find(zengine::graphics::Drawn::zen_name, zengine::graphics::Drawn::zen_version) !=
          nullptr);

    // THE LIVE PARTICIPANT: the ordinary line, sent, reaches the door and is answered in the
    // Terminal...
    FindingRig s;
    std::vector<std::string> delivered;
    const loom::ObserverId tap = s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered) {
            delivered.push_back(e.schema_name);
        }
    });
    const std::size_t heard_before =
        of_kind(*s.terminal, loom::TranscriptKind::Received).size() +
        of_kind(*s.terminal, loom::TranscriptKind::AnswerReceived).size();
    (void)s.typed("send @zengine.powers FindPowers 2 text=larger");
    CHECK(std::count(delivered.begin(), delivered.end(), std::string(kFindPowersName)) == 1);
    std::vector<loom::TranscriptEntry> heard = of_kind(*s.terminal, loom::TranscriptKind::Received);
    for (const loom::TranscriptEntry& e :
         of_kind(*s.terminal, loom::TranscriptKind::AnswerReceived)) {
        heard.push_back(e);
    }
    REQUIRE(heard.size() == heard_before + 1);
    CHECK(std::any_of(heard.begin(), heard.end(), [](const loom::TranscriptEntry& e) {
        return e.shape == PowersFound::zen_name;
    }));

    // ...AND A DRAWING AIMED AT EVERYONE REACHES NO ONE: its lists cannot be typed, so the line
    // is never sent.
    delivered.clear();
    (void)s.typed("send * Drawn 1 area=desk");
    s.r.bus.remove_observer(tap);
    CHECK(std::count(delivered.begin(), delivered.end(),
                     std::string(zengine::graphics::Drawn::zen_name)) == 0);
    CHECK(of_kind(*s.terminal, loom::TranscriptKind::Submitted).back().shape ==
          std::string(kFindPowersName));
}

TEST_CASE("the discovery door answers its page for every shape it accepts, in every version") {
    FindingRig s;
    // BOTH VERSIONS OF THE FINDING QUESTION AND THE DESCRIBING ONE, each from its section.
    CHECK(check_office_manual(s.r.bus, kPowersRole) >= 3);
    check_page_names_accepted(zengine::manual::pages::zengine_powers::kManual,
                              described_shapes_of(s.r.bus, s.r.bus.role_holder(kPowersRole)));
}

namespace {

/// The panes tier's arrangement with Flow's pane loaded in its office beside the tool.
load::LoadPlan plan_with_flow() {
    load::LoadPlan plan = pane_plan();
    load::ArtifactIntent flow;
    flow.stem = "zengine-flow-pane";
    flow.weave = load::WeaveIntent{zengine::flow_pane::kRole};
    plan.artifacts.push_back(flow);
    return plan;
}

/// What one participant last asked the door, or was last answered, as bytes off the bus.
struct Said {
    loom::WeaveId who;
    std::string bytes;
};
std::string last_of(const std::vector<Said>& said, loom::WeaveId who) {
    for (std::size_t i = said.size(); i > 0; --i) {
        if (said[i - 1].who == who) {
            return said[i - 1].bytes;
        }
    }
    return std::string();
}

/// The bytes of the row an answer carries for one identity, or empty.
std::string row_in(const std::string& answer, const std::string& identity) {
    const auto admitted =
        loom::admit(loom::parse(answer), loom::schema_of<PowersFound>());
    REQUIRE(admitted);
    for (const PowerRow& row : loom::from_value<PowersFound>(admitted.value()).rows) {
        if (row.identity == identity) {
            return loom::serialize(loom::to_value(row));
        }
    }
    return std::string();
}

} // namespace

TEST_CASE("one question gives the same rows to Flow, Powers and the Terminal") {
    // THREE ASKERS ON ONE BUS, each the shipped participant -- Flow's pane, the Powers pane and the
    // Terminal -- and the one discovery door they ask. What each asked and heard is read off the
    // bus itself, not off any of them.
    FindingRig s(plan_with_flow());
    const loom::WeaveId flow = s.r.bus.role_holder(zengine::flow_pane::kRole);
    const loom::WeaveId tool = s.r.bus.role_holder(kIntroOffice);
    REQUIRE(flow.valid());
    REQUIRE(tool.valid());
    std::vector<Said> asked;
    std::vector<Said> heard;
    const loom::ObserverId tap = s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind != loom::EventKind::Delivered || e.payload == nullptr) {
            return;
        }
        if (e.schema_name == kFindPowersName) {
            asked.push_back({e.sender, loom::serialize(*e.payload)});
        } else if (e.schema_name == PowersFound::zen_name) {
            heard.push_back({e.target, loom::serialize(*e.payload)});
        }
    });
    const std::uint64_t spent = op::invocations();

    // FLOW: a maker types into its search line, the text Workshop hands the pane that holds the keys.
    REQUIRE(s.r.bus
                .office_send_to_role_as(
                    s.r.workshop_id, kWorkshopProvider, zengine::flow_pane::kRole,
                    loom::Message(loom::to_value(PaneTextInput{zengine::flow_pane::kPane, "larger"}),
                                  s.r.workshop_id, loom::WeaveId{}, 0))
                .valid());
    s.r.bus.drain_until_idle();

    // POWERS: a weaver opens it, moves to the Operators and types the same word.
    s.r.extent(160, 48);
    REQUIRE(intro_row(s.r, intro::kPowersPane) != nullptr);
    s.r.pick(PaneRef{kIntroOffice, intro::kPowersPane});
    const std::int64_t kind = intro_row(s.r, intro::kPowersPane)->kind;
    focus_pane(s.r, kind);
    s.r.key(input::scan::kTab);
    s.r.text("larger");

    const std::string flow_asked = last_of(asked, flow), flow_heard = last_of(heard, flow);
    const std::string tool_asked = last_of(asked, tool), tool_heard = last_of(heard, tool);
    REQUIRE_FALSE(flow_heard.empty());
    REQUIRE_FALSE(tool_heard.empty());

    // THE TERMINAL ASKS EACH OF THEIR QUESTIONS, typed as a weaver types them...
    const std::string page = "limit=" + std::to_string(kMaxPowerRows);
    const std::optional<loom::Value> as_flow =
        s.typed("ask @zengine.powers FindPowers 2 text=larger offered=true " + page);
    const std::string terminal_asked_as_flow = last_of(asked, s.terminal->id());
    const std::optional<loom::Value> as_tool =
        s.typed("ask @zengine.powers FindPowers 2 kind=operator text=larger " + page);
    const std::string terminal_asked_as_tool = last_of(asked, s.terminal->id());
    REQUIRE(as_flow.has_value());
    REQUIRE(as_tool.has_value());
    s.r.bus.remove_observer(tap);

    // ...which are the same questions, byte for byte -- so the answers are the same, byte for byte.
    CHECK(terminal_asked_as_flow == flow_asked);
    CHECK(terminal_asked_as_tool == tool_asked);
    CHECK(loom::serialize(*as_flow) == flow_heard);
    CHECK(loom::serialize(*as_tool) == tool_heard);

    // AND THE ROW EVERY ASKER WAS SHOWN FOR THE POWER IT WAS LOOKING FOR IS ONE ROW.
    const std::string row = row_in(flow_heard, "math.max");
    REQUIRE_FALSE(row.empty());
    CHECK(row_in(tool_heard, "math.max") == row);
    CHECK(row_in(loom::serialize(*as_tool), "math.max") == row);

    // ASKING, ANSWERING AND SHOWING RAN NOTHING.
    CHECK(op::invocations() == spent);
}

namespace {

/// FLOW'S ROWS AS WORKSHOP HOLDS THEM, for the one pane its office offers.
const RuntimePane* flow_rows(PaneRig& r) {
    return r.session().panes.runtime.find(zengine::flow_pane::kRole, zengine::flow_pane::kPane);
}

/// WHETHER THE ROWS HELD DECLARE ONE ID.
bool declares(const RuntimePane& held, const std::string& id) {
    return std::any_of(held.actions.begin(), held.actions.end(),
                       [&](const v4::PaneActionRow& a) { return a.id == id; });
}

/// EACH STATE FLOW DECLARES ITS ROWS IN, each pressed into as Workshop presses an action: none
/// open, then a dialog open, whose confirm and cancel join the others until it closes. Flow names
/// no mode, so both are the census's `-`.
void each_flow_state(FindingRig& s, const std::function<void(const RuntimePane&)>& in) {
    const RuntimePane* held = flow_rows(s.r);
    REQUIRE(held != nullptr);
    REQUIRE(declares(*held, "run"));
    REQUIRE_FALSE(declares(*held, "dialog-confirm"));
    in(*held);
    REQUIRE(s.r.workshop_action(zengine::flow_pane::kRole, zengine::flow_pane::kPane, "ask-new")
                .delivered);
    held = flow_rows(s.r);
    REQUIRE(held != nullptr);
    REQUIRE(declares(*held, "dialog-confirm"));
    in(*held);
    REQUIRE(s.r.workshop_action(zengine::flow_pane::kRole, zengine::flow_pane::kPane,
                                "dialog-cancel")
                .delivered);
    held = flow_rows(s.r);
    REQUIRE(held != nullptr);
    CHECK_FALSE(declares(*held, "dialog-confirm"));
}

/// FLOW'S CENSUS: its rows with no dialog open, then the rows a dialog adds, each id once.
const CensusDriverOf kFlowCensus{zengine::flow_pane::kRole, [] {
    OfficeCensus census;
    FindingRig s(plan_with_flow());
    std::set<std::string> taken;
    each_flow_state(s, [&](const RuntimePane& held) {
        CHECK(held.mode.empty()); // Flow declares its rows in no mode
        if (taken.empty()) {
            census_take(census, zengine::flow_pane::kRole, held);
            for (const v4::PaneActionRow& a : held.actions) taken.insert(a.id);
            return;
        }
        for (const v4::PaneActionRow& a : held.actions) {
            if (taken.insert(a.id).second) {
                census.lines.push_back(census_line(zengine::flow_pane::kRole, held.mode, a));
            }
        }
    });
    return census;
}};

/// FLOW'S PRESSES: each row with no dialog open and with one open, the walk replayed for each.
const PressDriverOf kFlowPresses{zengine::flow_pane::kRole, [](PressWitness& w) {
    press_each_state<FindingRig>(
        w, [] { return std::make_unique<FindingRig>(plan_with_flow()); },
        [](FindingRig& s, const std::function<void(const RuntimePane&)>& at) {
            each_flow_state(s, at);
        });
}};

} // namespace

TEST_CASE("the manual Flow answers names exactly the commands it declares, with and without a "
          "dialog open") {
    FindingRig s(plan_with_flow());
    ManualReader reader(s.r, zengine::flow_pane::kRole);
    std::set<std::string> declared;
    each_flow_state(s, [&](const RuntimePane& held) {
        for (const v4::PaneActionRow& a : held.actions) declared.insert(a.id);
    });
    CHECK(declared == census_ids(census_pinned_of(zengine::flow_pane::kRole)));
    check_answered_manual(reader, zengine::flow_pane::kPane, declared);

    // ...AND A PANE FLOW DOES NOT OFFER IS REFUSED, naming it, with no words.
    const PaneDocumentShown other = reader.ask("elsewhere", "");
    CHECK(other.refusal == "zengine.flow offers no pane `elsewhere`");
    CHECK(other.sections.empty());
}
