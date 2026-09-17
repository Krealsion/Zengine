// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WORKSHOP_SWITCH_RIG_HPP
#define ZENGINE_TESTS_WORKSHOP_SWITCH_RIG_HPP

// THE RIG AN EDITOR SWITCH IS WITNESSED IN: a live Workshop whose load plan loads one editor into
// `zengine.editor` and authors its choices, the real switch coordinator wired as the host wires it,
// the project door, a party that asks for switches and opens, a probe that reads declared fields,
// and the Terminal participant widened by the host's own call. Suites `workshop_editor_switch`
// (standard Editors only) and `workshop_neovim` (the Neovim-backed choice) share it, so the two
// prove their claims through one wiring.

#include "workshop_support.hpp"

#include "editor-pane/vocabulary.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/editor_switch.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/pane_doors.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace pane = zengine::editor_pane;

inline PaneRef editor_ref() { return PaneRef{pane::kEditorPaneRole, pane::kEditorPane}; }

inline void put_bytes(const std::filesystem::path& at, const std::string& bytes) {
    std::ofstream out(at, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

inline std::string spelled(const std::filesystem::path& at) {
    return persist::resolved_against(std::string(), at.generic_string());
}

inline constexpr const char* kSwitchAskerOffice = "zengine.test.switch-asker";

/// A SEAT THAT READS DECLARED FIELDS, the way a maker's probe does (`zen.PokeRead`).
class PokeReader : public loom::WeaveBase<PokeReader, SeenState,
                                          loom::Accept<loom::Result, loom::Refused>, loom::Emit<>> {
public:
    std::vector<std::pair<std::uint64_t, std::string>> answers;
    void on(const loom::Result& r, loom::Mail& mail) { answers.emplace_back(mail.correlation(), r.value); }
    void on(const loom::Refused& r, loom::Mail& mail) {
        answers.emplace_back(mail.correlation(), "REFUSED: " + r.reason);
    }
};

/// A PARTY THAT ASKS FOR SWITCHES AND OPENS, and keeps every answer -- what the Terminal pane and
/// Files are to a maker, reduced to what the cases need.
class SwitchAsker
    : public loom::WeaveBase<SwitchAsker, DoorAskerState,
                             loom::Accept<EditorSwitchAnswered, EditorSwitchProgress, SourceOpened,
                                          ProjectRoot, TerminalActed, SeatDo>,
                             loom::Emit<EditorSwitchRequested, EditorSwitchConfirmed,
                                        EditorSwitchCancelled, EditorSwitchStatusRequested,
                                        OpenSourceRequested, TerminalActRequested>> {
public:
    std::function<void(SwitchAsker&, loom::Mail&)> next;
    std::vector<EditorSwitchAnswered> answers;
    std::vector<EditorSwitchProgress> progress;
    std::vector<SourceOpened> opens;
    loom::WeaveId id{};

    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            auto what = next;
            next = nullptr;
            what(*this, mail);
        }
    }
    void on(const EditorSwitchAnswered& said, loom::Mail& mail) {
        if (mail.answers_ask()) {
            answers.push_back(said);
        }
    }
    void on(const EditorSwitchProgress& said, loom::Mail&) { progress.push_back(said); }
    void on(const SourceOpened& said, loom::Mail&) { opens.push_back(said); }
    void on(const ProjectRoot&, loom::Mail&) {}
    std::vector<TerminalActed> acted;
    void on(const TerminalActed& said, loom::Mail&) { acted.push_back(said); }
};

struct SwitchRig {
    TempDir dir;
    std::filesystem::path root;
    PaneRig r;
    std::int64_t kind = 0;
    SwitchAsker* asker = nullptr;
    PokeReader* reader = nullptr;
    loom::WeaveId reader_id{};
    std::uint64_t reads = 0;
    EditorSwitchCoordinator* switcher = nullptr;
    loom::WeaveId switcher_id{};
    SkinSeat* skin = nullptr;

    /// HELD HERE, because the project door keeps the path it is handed by reference.
    std::string marks_path;

    explicit SwitchRig(const char* tag) : dir(tag) {
        root = dir.path();
        r.host.project_dir = root.generic_string();
        marks_path = (root / "marks.json").generic_string();
    }

    /// A LIVE WORKSHOP WHOSE PLAN LOADS `start` INTO `zengine.editor` AND AUTHORS `choices`.
    void open(const std::vector<load::ChoiceIntent>& choices,
              const char* start = pane::kEditorPaneStem) {
        r.host.managed_pane = editor_ref();
        r.mount_workshop();
        r.mount_opening();
        mount_project_door();
        skin = r.mount_skin_seat();
        load::LoadPlan plan;
        load::ArtifactIntent seat;
        seat.stem = start;
        seat.weave = load::WeaveIntent{pane::kEditorPaneRole};
        plan.artifacts.push_back(seat);
        plan.choices = choices;
        REQUIRE(load::check_plan(plan).accepted);
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        mount_switcher();
        mount_asker();
        auto probe = std::make_unique<PokeReader>();
        reader = probe.get();
        reader_id = r.bus.register_weave(std::move(probe), loom::Grant{}, std::string());
        reader->zen_set_self(reader_id);
        r.ready();
        r.extent(160, 48);
        const RuntimePane* row = r.session().panels.runtime.find(pane::kEditorPaneRole, pane::kEditorPane);
        REQUIRE_MESSAGE(row != nullptr, "the loaded image offered no `editor` pane");
        kind = row->kind;
        r.pick(editor_ref());
        REQUIRE(r.session().panels.has(kind));
    }

    void mount_project_door() {
        auto door = std::make_unique<ProjectDoor>(r.host.project_dir, marks_path,
                                                  ProjectDoor::Frontier{}, ProjectDoor::Names{},
                                                  r.host.recipe_source);
        ProjectDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
        const loom::WeaveId id = r.bus.register_weave(std::move(door), std::move(say), std::string(kProjectRole));
        raw->zen_set_self(id);
    }

    /// THE COORDINATOR, WIRED AS THE HOST WIRES IT: the bus and the Kernel, and the realization
    /// owner's four readings.
    void mount_switcher() {
        EditorSwitchHost host;
        host.bus = &r.bus;
        host.kernel = &r.kernel;
        host.office = r.host.managed_pane.provider;
        host.choices = [this] { return r.plan_->plan().choices; };
        host.holder = [this] { return r.plan_->choice_holder(pane::kEditorPaneRole); };
        host.image_of = [this](const std::string& stem) { return r.plan_->image_of(stem); };
        host.record = [this](const std::string& stem, loom::WeaveId weave, const std::string& image) {
            const load::PlanExecutor::Recorded done =
                r.plan_->record_choice_holder(pane::kEditorPaneRole, stem, weave, image);
            return done.accepted ? std::string() : done.refusal;
        };
        auto weave = std::make_unique<EditorSwitchCoordinator>(std::move(host));
        switcher = weave.get();
        switcher_id = r.bus.register_weave(std::move(weave), editor_switch_grant(pane::kEditorPaneRole),
                                           std::string(kEditorSwitchRole));
        switcher->zen_set_self(switcher_id);
    }

    void mount_asker() {
        auto held = std::make_unique<SwitchAsker>();
        asker = held.get();
        loom::Grant grant;
        grant.allow_to_role(EditorSwitchRequested::zen_name, EditorSwitchRequested::zen_version, kEditorSwitchRole);
        grant.allow_to_role(EditorSwitchConfirmed::zen_name, EditorSwitchConfirmed::zen_version, kEditorSwitchRole);
        grant.allow_to_role(EditorSwitchCancelled::zen_name, EditorSwitchCancelled::zen_version, kEditorSwitchRole);
        grant.allow_to_role(EditorSwitchStatusRequested::zen_name, EditorSwitchStatusRequested::zen_version,
                            kEditorSwitchRole);
        grant.allow_to_any(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version);
        grant.allow_to_role(TerminalActRequested::zen_name, TerminalActRequested::zen_version,
                            kWorkshopProvider);
        const loom::WeaveId id = r.bus.register_weave(std::move(held), std::move(grant), std::string(kSwitchAskerOffice));
        asker->zen_set_self(id);
        asker->id = id;
    }

    void enqueue(std::function<void(SwitchAsker&, loom::Mail&)> what) {
        asker->next = std::move(what);
        (void)r.bus.send(asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    }

    /// ASK, SETTLE, AND HAND BACK THE ONE ANSWER THE ASK EARNED.
    template <class Shape>
    EditorSwitchAnswered ask(const Shape& shape) {
        const std::size_t before = asker->answers.size();
        enqueue([shape](SwitchAsker&, loom::Mail& mail) {
            (void)mail.as_role(kSwitchAskerOffice).send_to_role(kEditorSwitchRole, shape);
        });
        r.bus.drain_until_idle();
        REQUIRE_MESSAGE(asker->answers.size() == before + 1, "the switch office did not answer (stage `",
                        switcher->stage() == EditorSwitchCoordinator::Stage::Idle ? "idle" : "busy", "`)");
        return asker->answers.back();
    }

    EditorSwitchAnswered switch_to(const std::string& destination) {
        return ask(EditorSwitchRequested{destination});
    }

    std::string open_file(const char* name, const std::string& bytes) {
        put_bytes(root / name, bytes);
        const std::string path = spelled(root / name);
        const std::size_t before = asker->opens.size();
        enqueue([path](SwitchAsker&, loom::Mail& mail) {
            (void)mail.as_role(kSwitchAskerOffice).send_to_role(kOpeningRole, OpenSourceRequested{path});
        });
        r.bus.drain_until_idle();
        REQUIRE(asker->opens.size() == before + 1);
        REQUIRE_MESSAGE(asker->opens.back().accepted, asker->opens.back().refusal);
        return path;
    }

    loom::WeaveId holder() const { return r.bus.role_holder(pane::kEditorPaneRole); }

    std::vector<std::string> shown() { return pane_rows(r, kind); }
    const ExternalPane* seat() { return r.session().panels.external_pane(kind); }

    std::string status() {
        const std::vector<std::string> rows = shown();
        REQUIRE_FALSE(rows.empty());
        return rows[0];
    }
    bool shows(const std::string& piece) {
        for (const std::string& row : shown()) {
            if (row.find(piece) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    /// PRESS A DOCUMENT ROW -- under the status row, and under the notice row when the pane has a
    /// standing notice (the Editor's own composition rule in a room this tall).
    void press_doc(std::int64_t row, std::int64_t col) {
        const std::int64_t above = read("notice").empty() ? 1 : 2;
        press_pane(r, kind, above + row, col);
    }
    void type(const std::string& s) {
        for (const char c : s) {
            r.text(std::string(1, c));
        }
    }

    /// ONE DECLARED FIELD OF THE OFFICE'S HOLDER, as `zen.PokeRead` answers it.
    std::string read(const char* field) {
        const std::uint64_t corr = ++reads;
        (void)r.bus.send(holder(), loom::Message(loom::to_value(loom::PokeRead{field}), loom::WeaveId{},
                                                 reader_id, corr));
        r.bus.drain_until_idle();
        for (const auto& one : reader->answers) {
            if (one.first == corr) {
                return one.second;
            }
        }
        FAIL_CHECK("reading `", field, "` was never answered");
        return std::string();
    }

    /// TURN BY TURN UNTIL THE COORDINATOR STANDS AT `stage`.
    void pump_until(EditorSwitchCoordinator::Stage stage, int turns = 64) {
        for (int i = 0; i < turns && switcher->stage() != stage; ++i) {
            (void)r.bus.pump_pending();
        }
        REQUIRE(switcher->stage() == stage);
    }

    void enqueue_switch(const std::string& destination) {
        enqueue([destination](SwitchAsker&, loom::Mail& mail) {
            (void)mail.as_role(kSwitchAskerOffice).send_to_role(kEditorSwitchRole, EditorSwitchRequested{destination});
        });
    }

    /// WORKSHOP'S TERMINAL PARTICIPANT, WIDENED FOR THE SWITCH BY THE HOST'S OWN CALL
    /// (`let_terminal_switch_editors`) over the baseline the host gives every terminal.
    loom::TerminalSession* terminal = nullptr;
    void mount_terminal() {
        loom::TerminalVocabulary vocab;
        vocab.knows(loom::schema_of<surface::SurfaceText>())
            .accepts(loom::schema_of<loom::Ack>())
            .accepts(loom::schema_of<loom::Result>())
            .accepts(loom::schema_of<loom::Refused>());
        loom::Grant grant;
        grant.allow_to_role(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version,
                            surface::kSkinRole);
        let_terminal_switch_editors(vocab, grant);
        const loom::MountedTerminal mounted = loom::host_mount_terminal(
            r.bus, std::make_unique<loom::TerminalSession>("workshop", std::move(vocab)),
            std::move(grant));
        r.host.terminal = mounted.session;
        terminal = mounted.session;
    }

    /// TYPE ONE LINE INTO THE TERMINAL AND SUBMIT IT, through Workshop's own door -- the path the
    /// Terminal pane's Return takes -- and hand back the answer the participant received for it.
    EditorSwitchAnswered typed(const std::string& line) {
        const std::size_t answers_before = of_kind(*terminal, loom::TranscriptKind::AnswerReceived).size();
        const std::size_t acted_before = asker->acted.size();
        enqueue([line](SwitchAsker&, loom::Mail& mail) {
            (void)mail.as_role(kSwitchAskerOffice)
                .send_to_role(kWorkshopProvider, TerminalActRequested{kTerminalSubmitAct, line});
        });
        r.bus.drain_until_idle();
        REQUIRE(asker->acted.size() == acted_before + 1);
        REQUIRE_MESSAGE(asker->acted.back().accepted, asker->acted.back().refusal);
        const std::vector<loom::TranscriptEntry> now = of_kind(*terminal, loom::TranscriptKind::AnswerReceived);
        REQUIRE_MESSAGE(now.size() == answers_before + 1, "the typed ask `", line, "` was not answered");
        CHECK(now.back().shape == std::string(EditorSwitchAnswered::zen_name));
        CHECK(now.back().authored_role.empty());
        const std::optional<loom::ReceivedMessage> held = terminal->received(now.back().message);
        REQUIRE(held.has_value());
        return loom::from_value<EditorSwitchAnswered>(held->value);
    }

    void enqueue_typed(const std::string& text) {
        (void)r.bus.publish(loom::Message(loom::to_value(input::TextEntered{text}), loom::WeaveId{},
                                          loom::WeaveId{}, 0));
    }
};

} // namespace

#endif // ZENGINE_TESTS_WORKSHOP_SWITCH_RIG_HPP
