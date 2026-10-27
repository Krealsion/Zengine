// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_APP_HOST_HPP
#define ZENGINE_APP_HOST_HPP

// The app host (app/docs/app.md): what `zengine-app` composes over one bus, and nothing a weave
// could hold: one operator catalog, the Kernel asking Loom's authority store in the app's folder,
// Loom's warden adopting each load, the operator seat, the plan's booter and executor (its barrier
// asking the store before each row), the Flow host and the view host, the place door, the close,
// and the console's words a line at a time, so a program and a suite drive it alike.

#include "app/session_file.hpp"
#include "app/vocabulary.hpp"

#include "builder/vocabulary.hpp"
#include "flow/flow-host/runtime.hpp"
#include "graphics/vocabulary.hpp"
#include "operator/catalog.hpp"
#include "operator/host_surface.hpp"
#include "view/host.hpp"
#include "workshop/load_execute.hpp"
#include "workshop/load_persist.hpp"

#include <zen/content_id.hpp>
#include <zen/host/authority.hpp>
#include <zen/host/authority_command.hpp>
#include <zen/host/grant_wiring.hpp>
#include <zen/host/store_lock.hpp>
#include <zen/host/warden.hpp>
#include <zen/kernel/admission.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace zengine::app {

namespace load = zengine::workshop::load;
namespace load_persist = zengine::workshop::load_persist;

/// The most turns of the bus the seat gives the warden to answer one question.
inline constexpr int kSeatTurns = 64;

// ---- the operator seat ---------------------------------------------------------------------

/// The seat's own nudge: a turn in which it sends the question it holds.
struct AppSeatTurn {
    ZEN_SHAPE(AppSeatTurn, 1);
};

struct AppSeatState {
    std::int64_t answers = 0;
    ZEN_SHAPE(AppSeatState, 1, ZEN_FIELD(answers));
};

/// THE OPERATOR SEAT, the one participant the warden obeys. A question it sends from inside its
/// own delivery is a participant's send, gated and stamped with its id, and an answer counts only
/// when Loom attributes it to that question (`loom::AskBook`).
class Seat final
    : public loom::WeaveBase<Seat, AppSeatState,
                             loom::Accept<AppSeatTurn, loom::Result, loom::Refused,
                                          loom::AuthorityDescription>> {
public:
    void set_self(loom::WeaveId id) { me_ = id; }

    /// Ask the warden one question and turn the bus until it answers, at most `kSeatTurns` turns.
    template <class Question>
    loom::host::WardenAnswer ask(loom::Switchboard& bus, loom::WeaveId warden,
                                 const Question& question) {
        reply_.reset();
        errand_ = [this, warden, question](loom::Mail& mail) {
            const loom::AskOpened opened =
                book_.open(warden, Question::zen_name, Question::zen_version);
            if (opened.ok) {
                (void)mail.send(warden, question, opened.correlation);
            }
        };
        bus.send(me_, loom::Message(loom::to_value(AppSeatTurn{})));
        for (int turn = 0; turn < kSeatTurns && !reply_; ++turn) {
            if (bus.pump_pending() == 0 && bus.pending() == 0) {
                break;
            }
        }
        errand_ = nullptr;
        loom::host::WardenAnswer a;
        a.reply = reply_;
        if (!a.reply) {
            a.trouble = "the warden did not answer within " + std::to_string(kSeatTurns) +
                        " turns of the bus";
        }
        return a;
    }

    void on(const AppSeatTurn&, loom::Mail& mail) {
        if (errand_ && !mail.sender().valid()) {
            errand_(mail);
        }
    }
    void on(const loom::Result& r, loom::Mail& mail) { keep(r, mail); }
    void on(const loom::Refused& r, loom::Mail& mail) { keep(r, mail); }
    void on(const loom::AuthorityDescription& d, loom::Mail& mail) { keep(d, mail); }

private:
    template <class Answer>
    void keep(const Answer& answer, loom::Mail& mail) {
        if (book_.settle(mail.correlation(), mail.sender())) {
            ++state_.answers;
            reply_ = loom::to_value(answer);
        }
    }

    loom::AskBook book_{4};
    std::function<void(loom::Mail&)> errand_;
    std::optional<loom::Value> reply_;
    loom::WeaveId me_{};
};

/// The command's half the host carries: each question from the seat to the warden.
class SeatDesk final : public loom::host::AuthorityDesk {
public:
    SeatDesk(loom::Switchboard& bus, Seat& seat, loom::WeaveId warden)
        : bus_(&bus), seat_(&seat), warden_(warden) {}
    loom::host::WardenAnswer sync(const std::string& artifact) override {
        return seat_->ask(*bus_, warden_, loom::host::HostAuthoritySync{artifact});
    }
    loom::host::WardenAnswer describe(const std::string& artifact) override {
        return seat_->ask(*bus_, warden_, loom::host::HostDescribeAuthority{artifact});
    }

private:
    loom::Switchboard* bus_;
    Seat* seat_;
    loom::WeaveId warden_;
};

// ---- the place door, and the close -----------------------------------------------------------

struct AppPlaceDoorState {
    std::int64_t told = 0;
    ZEN_SHAPE(AppPlaceDoorState, 1, ZEN_FIELD(told));
};

/// `zengine.app.host`: where the app's folder is, and the directory the host opens artifacts from,
/// answered to whoever asks. Facts, not powers.
class PlaceDoor final
    : public loom::WeaveBase<PlaceDoor, AppPlaceDoorState, loom::Accept<AppPlaceAsked>,
                             loom::Emit<AppPlaceSaid>> {
public:
    explicit PlaceDoor(AppPlaceSaid said) : said_(std::move(said)) {}
    void on(const AppPlaceAsked&, loom::Mail& mail) {
        ++state_.told;
        (void)mail.answer(said_);
    }

private:
    AppPlaceSaid said_;
};

struct AppCloseState {
    std::int64_t heard = 0;
    ZEN_SHAPE(AppCloseState, 1, ZEN_FIELD(heard));
};

/// THE CLOSE: the app's window is the first window that offers itself, as its office, and the
/// app's weave arranges in that one. Its `SurfaceCloseRequested`, said as that office, ends the
/// run; a close any other office says, or one said with no office, ends nothing.
class CloseDoor final
    : public loom::WeaveBase<CloseDoor, AppCloseState,
                             loom::Accept<zengine::graphics::WindowOffered,
                                          zengine::graphics::SurfaceCloseRequested>> {
public:
    explicit CloseDoor(std::function<void()> close) : close_(std::move(close)) {}
    void on(const zengine::graphics::WindowOffered& w, loom::Mail& mail) {
        const std::string office = std::string(zengine::graphics::kWindowOfficePrefix) + w.window;
        if (window_.empty() && mail.authored_from_role(office)) {
            window_ = office;
        }
    }
    void on(const zengine::graphics::SurfaceCloseRequested&, loom::Mail& mail) {
        ++state_.heard;
        if (!window_.empty() && mail.authored_from_role(window_)) {
            close_();
        }
    }

private:
    std::string window_; ///< the office of the app's window, once one offered itself
    std::function<void()> close_;
};

// ---- the host ----------------------------------------------------------------------------------

/// WHERE AN APP RUNS: its folder, the directory artifacts are opened from, and how a stem is
/// spelled as a file there.
struct AppPlace {
    std::string folder;
    std::string artifacts;
    std::string suffix;
};

class AppHost {
public:
    /// Over `bus`, which outlives this host, saying what it does to `out`.
    AppHost(loom::Switchboard& bus, AppPlace place, std::ostream& out)
        : bus_(bus), place_(std::move(place)), out_(out) {}

    AppHost(const AppHost&) = delete;
    AppHost& operator=(const AppHost&) = delete;

    /// Read the plan and open the decisions, the store's lock claimed for this run, then compose
    /// the host. False, with nothing composed and `why` saying it, for a plan that is refused, a
    /// store that cannot be read, or one another host holds.
    bool open(std::string* why) {
        const std::string plan_path = place_.folder + "/" + kPlanName;
        const load_persist::LoadedPlan loaded = load_persist::load_file(plan_path);
        if (!loaded.outcome.accepted) {
            *why = "the plan " + plan_path + " is refused: " + loaded.outcome.refusal;
            return false;
        }
        if (!loaded.plan.choices.empty()) {
            *why = "the plan " + plan_path + " authors choices for an office; an app's plan " +
                   "names each artifact it runs";
            return false;
        }
        plan_ = loaded.plan;
        store_path_ = place_.folder + "/" + kAuthorityName;
        bool held_by_another = false;
        std::string refusal;
        if (!lock_.claim(store_path_, &refusal, &held_by_another)) {
            *why = held_by_another ? "this app is already running: " + refusal : refusal;
            return false;
        }
        if (!store_.open(store_path_, &refusal)) {
            *why = "the decisions " + store_path_ + " cannot be read: " + refusal;
            return false;
        }
        // WHILE IT RUNS, its process id stands in the folder (`app-session.json`), for whoever
        // asks whether the app runs; the one host holding the decisions writes it.
        session_ = SessionFile(place_.folder);
        compose();
        return true;
    }

    /// Say where the app stands, then begin its plan: what can run now runs, and the first row the
    /// console has not decided waits there.
    void begin() {
        out_ << "zengine-app - app: " << place_.folder << "\n"
             << "zengine-app - plan: " << kPlanName << " (" << plan_.artifacts.size()
             << " artifacts)\n"
             << "zengine-app - decisions: " << kAuthorityName << " (" << store_.rules().size()
             << " standing)\n"
             << "zengine-app - artifacts: " << place_.artifacts << "\n"
             << "zengine-app - session: "
             << (session_.held() ? std::string(kSessionName) + " (process " +
                                       std::to_string(this_process_id()) + ")"
                                 : session_.trouble())
             << "\n"
             << "zengine-app - containment: " << loom::Kernel::containment_note() << "\n"
             << "zengine-app - what may run is this console's to decide; a row of the plan, once "
                "it runs, may say anything to anyone (the plan's posture), and anything else "
                "loaded only what the console allows\n";
        out_.flush();
        executor_->begin(plan_);
        say_notes();
    }

    /// ONE LINE AT THE CONSOLE: Loom's `authority ...`, `trust plan`, `status`, `help` or `quit`.
    /// After it, the row the plan waits on is asked again, and realized once the console has said
    /// yes.
    void command(const std::string& line) {
        std::vector<std::string> words;
        std::istringstream split(line);
        for (std::string w; split >> w;) {
            words.push_back(w);
        }
        if (words.empty()) {
            return;
        }
        if (words[0] == "authority") {
            loom::host::run_authority_command(store_, *warden_, *desk_, words, out_);
        } else if (words[0] == "trust" && words.size() == 2 && words[1] == "plan") {
            trust_plan();
        } else if (words[0] == "status" && words.size() == 1) {
            status();
        } else if (words[0] == "help" && words.size() == 1) {
            out_ << loom::host::authority_help()
                 << "  trust plan              let each artifact this plan names run, as built here\n"
                 << "  status                  what the plan runs, and what it waits on\n"
                 << "  quit                    end this app\n";
        } else if (words[0] == "quit" && words.size() == 1) {
            end("quit at the console");
        } else {
            out_ << "zengine-app - '" << words[0] << "' is not a command here (help lists them)\n";
        }
        out_.flush();
        resume();
    }

    /// After a turn of the bus: what became of a row the console said yes to, said once. A row the
    /// Kernel refused after that yes waits again, and the console is told why.
    void turned() { report_realization(/*then_waiting=*/true); }

    /// Has the run ended -- `quit` at the console, the app's window closed, or its plan refused?
    bool ended() const noexcept { return ended_; }
    /// The process's exit: 0 for a run its console or its window ended, 4 for a refused plan.
    int exit_code() const noexcept { return exit_code_; }

    /// What realization has made of the plan now.
    const load::PlanExecutor& executor() const { return *executor_; }
    const op::Catalog& operators() const noexcept { return operators_; }
    const loom::host::AuthorityStore& store() const noexcept { return store_; }
    /// The view host this app runs its views in.
    zengine::view::Host& views() { return *views_; }

private:
    void compose() {
        kernel_ = std::make_unique<loom::Kernel>(bus_, admission_policy());

        // The seat and the warden first: the warden adopts each load inside the control door,
        // before the weave is told it is live, so every load is governed through one door.
        auto seat = std::make_unique<Seat>();
        Seat* seat_raw = seat.get();
        loom::Grant seat_grant;
        seat_grant.allow_to_any(loom::host::HostAuthoritySync::zen_name,
                                loom::host::HostAuthoritySync::zen_version);
        seat_grant.allow_to_any(loom::host::HostDescribeAuthority::zen_name,
                                loom::host::HostDescribeAuthority::zen_version);
        const loom::WeaveId seat_id = bus_.register_weave(std::move(seat), seat_grant);
        seat_raw->set_self(seat_id);
        auto warden = std::make_unique<loom::host::HostWarden>(store_, seat_id);
        warden_ = warden.get();
        const loom::WeaveId warden_id =
            bus_.register_weave(std::move(warden), loom::host::warden_capability(seat_id));
        warden_->zen_set_self(warden_id);
        desk_ = std::make_unique<SeatDesk>(bus_, *seat_raw, warden_id);

        loom::LifecycleAdoption adoption;
        adoption.admitted = [this](loom::Mail& mail, const std::string& name, loom::WeaveId id) {
            // The ceiling is full: what bounds the install is the file the console wrote. `id` is
            // the Kernel's fact about what it registered, never a payload.
            const loom::host::Installed done = warden_->adopt(
                mail, name,
                loom::host_grant_authority(
                    bus_, id, loom::LiveAuthority{}.allow_any().allow_observe_any()));
            if (!done.ok) {
                out_ << "zengine-app - could not administer '" << name << "': " << done.report
                     << "\n";
            }
        };
        adoption.retired = [this](loom::Mail&, const std::string& name) {
            warden_->release(name);
        };
        const loom::WeaveId control = loom::mount_control(*kernel_, bus_, std::move(adoption));
        manager_ = loom::mount_manager(control, bus_);

        // The plan booter's reach, the Manager target-scoped, is the dangerous grant here, and the
        // host writes it: a load and nothing else.
        loom::Grant operate;
        operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager_);
        auto booter = std::make_unique<load::PlanBooter>(answers_);
        booter_ = booter.get();
        const loom::WeaveId booter_id = bus_.register_weave(
            std::move(booter), std::move(operate), zengine::builder::kRealizationRole);
        booter_->zen_set_self(booter_id);

        executor_ = std::make_unique<load::PlanExecutor>(
            bus_, operators_, operator_host_, *booter_, manager_, answers_,
            [this](const std::string& stem) { return path_of(stem); },
            [this](const load::Executed& done) { settled(done); },
            [this](const std::string& stem) { return !decided(stem); });

        // The place door, and the close, which hears only the app's own window.
        loom::Grant say_place;
        say_place.allow_to_any(AppPlaceSaid::zen_name, AppPlaceSaid::zen_version);
        auto door = std::make_unique<PlaceDoor>(AppPlaceSaid{place_.folder, place_.artifacts});
        PlaceDoor* door_raw = door.get();
        door_raw->zen_set_self(
            bus_.register_weave(std::move(door), std::move(say_place), std::string(kAppHostRole)));
        auto close = std::make_unique<CloseDoor>([this] { end("its window was closed"); });
        CloseDoor* close_raw = close.get();
        close_raw->zen_set_self(bus_.register_weave(std::move(close), loom::Grant{}));

        // The two hosts, composed as Workshop composes them, before the plan runs.
        flow_ = std::make_unique<zengine::flow_host::RuntimeHost>(bus_, operators_);
        flow_->mount();
        views_ = std::make_unique<zengine::view::Host>(bus_);
        views_->mount();
    }

    /// THE KERNEL'S POLICY: the store's, which refuses at `Open` what the console has not decided
    /// and pins the build it decided; at `Speak` a plan row, from the plan's own file, speaks with
    /// the plan's posture, and anything else with the store's baseline.
    loom::AdmissionPolicy admission_policy() {
        decide_ = store_.policy();
        return [this](const loom::AdmissionRequest& request) {
            if (request.stage == loom::AdmissionStage::Speak && plan_row(request)) {
                return loom::AdmissionVerdict::admit(
                    loom::Grant{}.allow_any(),
                    "a row of this app's plan speaks with the plan's posture");
            }
            return decide_(request);
        };
    }

    bool plan_row(const loom::AdmissionRequest& request) const {
        for (const load::ArtifactIntent& row : plan_.artifacts) {
            if (row.stem == request.name && request.path == path_of(row.stem)) {
                return true;
            }
        }
        return false;
    }

    std::string path_of(const std::string& stem) const {
        return place_.artifacts + "/" + stem + place_.suffix;
    }

    /// HAS THE CONSOLE SAID YES TO THIS ROW'S BUILD? The store's own policy, asked at `Open` for
    /// the file the row names, before anything of it is mounted or loaded: an undecided or
    /// rebuilt artifact, a provider among them, is refused there and recorded as pending.
    bool decided(const std::string& stem) {
        loom::AdmissionRequest request;
        request.stage = loom::AdmissionStage::Open;
        request.kind = loom::AdmissionKind::Load;
        request.name = stem;
        request.path = path_of(stem);
        for (const load::ArtifactIntent& row : plan_.artifacts) {
            if (row.stem == stem && row.weave.has_value()) {
                request.role = row.weave->role;
            }
        }
        request.build = loom::BuildIdentity(request.path);
        const loom::AdmissionVerdict verdict = decide_(request);
        refusal_ = verdict.admitted ? std::string() : verdict.reason;
        return verdict.admitted;
    }

    /// After a line at the console: the row the plan waits on, asked again, realized on a yes. A
    /// row still refused is said again when the refusal changed: a decision the console made that
    /// the file cannot meet is not a silent wait.
    void resume() {
        if (ended_ || executor_->state() != load::Realization::Waiting) {
            return;
        }
        const std::string stem = executor_->waiting_on();
        if (!decided(stem)) {
            if (stem + "\n" + refusal_ != said_waiting_) {
                waiting(stem);
            }
            return;
        }
        const load::PlanExecutor::Asked asked = executor_->realize(stem);
        if (!asked.started) {
            out_ << "zengine-app - '" << stem << "' did not start: " << asked.refusal << "\n";
            out_.flush();
        }
        say_notes();
    }

    /// Realization came to rest: waiting on the console, every row running, or refused.
    void settled(const load::Executed& done) {
        // A row the console said yes to is said before where the walk went on to.
        report_realization(/*then_waiting=*/false);
        say_notes();
        if (!done.refusal.empty()) {
            out_ << "zengine-app - " << done.refusal << "\n"
                 << "zengine-app - the plan was not completed; " << done.resolved.size()
                 << " artifact(s) ran before it stopped.\n";
            out_.flush();
            exit_code_ = 4;
            end("its plan was refused");
            return;
        }
        if (!done.waiting_on.empty()) {
            offer();
            waiting(done.waiting_on);
            return;
        }
        if (done.ok) {
            out_ << "zengine-app - running: " << done.resolved.size() << " of "
                 << plan_.artifacts.size()
                 << " artifacts; close its window, or say quit, to end it\n";
            out_.flush();
        }
    }

    /// WHERE EACH ROW OF THE PLAN STANDS WITH THE CONSOLE, without asking the store's policy: its
    /// file's build now, and whether a standing decision lets that build run, denies it, or there
    /// is none (or one about other bytes), so the row would wait.
    struct Standing {
        std::string stem;
        std::string build; ///< `loom::file_content_id` of its file now; empty when unreadable
        bool denied = false;
        bool waits = false;
    };
    std::vector<Standing> standings() const {
        std::vector<Standing> out;
        for (const load::ArtifactIntent& row : plan_.artifacts) {
            Standing s{row.stem, loom::file_content_id_or_empty(path_of(row.stem))};
            const loom::host::AuthorityRule* rule = store_.find(row.stem);
            s.denied = rule != nullptr && !rule->may_run;
            s.waits = !s.denied && (rule == nullptr || (!rule->trust_rebuilds &&
                                                        !rule->content_id.empty() &&
                                                        rule->content_id != s.build));
            out.push_back(std::move(s));
        }
        return out;
    }

    /// THE PLAN'S ONE LINE, OFFERED ONCE A RUN, before the first row the walk waits on: each
    /// artifact of the plan that would wait, by name and the build it would let run, and the line
    /// that lets them all. Nothing is decided until the weaver types it.
    void offer() {
        if (offered_) {
            return;
        }
        offered_ = true;
        std::vector<Standing> waiting_rows;
        for (Standing& s : standings()) {
            if (s.waits && !s.build.empty()) {
                shown_[s.stem] = s.build;
                waiting_rows.push_back(std::move(s));
            }
        }
        if (waiting_rows.empty()) {
            return;
        }
        std::size_t width = 0;
        for (const Standing& s : waiting_rows) {
            width = std::max(width, s.stem.size());
        }
        out_ << "zengine-app - to let this plan's waiting artifacts run, each as built here, type:  "
                "trust plan\n";
        for (const Standing& s : waiting_rows) {
            out_ << "  " << s.stem << std::string(width - s.stem.size(), ' ') << "  build "
                 << s.build.substr(0, 8) << "\n";
        }
        out_ << "  or decide one at a time:  authority trust <name>; a rebuild of any asks again\n";
        out_.flush();
    }

    /// `trust plan`: each artifact the plan names that would wait, pinned to the build of its file
    /// now, as `authority trust` decides one; an artifact the console denied stays denied, one
    /// whose file cannot be read is not decided, and one whose file changed since the console showed
    /// its build is not decided until the line is typed again. Nothing the plan does not name is
    /// touched.
    void trust_plan() {
        bool any = false;
        for (const Standing& s : standings()) {
            if (s.denied) {
                out_ << "  '" << s.stem << "' stays denied, as decided here; authority trust "
                     << s.stem << " lets it run\n";
                continue;
            }
            if (!s.waits) {
                continue;
            }
            any = true;
            if (s.build.empty()) {
                out_ << "  '" << s.stem << "' was not decided: its file " << path_of(s.stem)
                     << " cannot be read\n";
                continue;
            }
            // THE BUILD THE WEAVER WAS SHOWN, or none: bytes that changed since are shown now and
            // wait for the line again.
            if (const auto shown = shown_.find(s.stem); shown == shown_.end()) {
                out_ << "  '" << s.stem << "' (build " << s.build.substr(0, 8)
                     << ") was not shown before and was not decided; trust plan again lets it "
                        "run as built now\n";
                shown_[s.stem] = s.build;
                continue;
            } else if (shown->second != s.build) {
                out_ << "  '" << s.stem << "' changed since it was offered (build "
                     << shown->second.substr(0, 8) << ", now " << s.build.substr(0, 8)
                     << ") and was not decided; trust plan again lets it run as built now\n";
                shown->second = s.build;
                continue;
            }
            const loom::host::AuthorityRule* existing = store_.find(s.stem);
            loom::host::AuthorityRule rule =
                existing != nullptr ? *existing : loom::host::AuthorityRule{};
            rule.artifact = s.stem;
            rule.may_run = true;
            rule.content_id = s.build;
            std::string error;
            if (!store_.put(std::move(rule), &error)) {
                out_ << "  " << error << "\n  '" << s.stem << "' is unchanged.\n";
                continue;
            }
            out_ << "  '" << s.stem << "' may run, as built (build " << s.build.substr(0, 8)
                 << "); a rebuild of it will ask again\n";
        }
        if (!any) {
            out_ << "  nothing of this plan waits on the console\n";
        }
    }

    /// The row the plan waits on, and why, in the store's own words: the build it was shown, and
    /// the line that answers it where a line can.
    void waiting(const std::string& stem) {
        said_waiting_ = stem + "\n" + refusal_;
        out_ << "zengine-app - waiting on this console: " << stem << "\n  " << refusal_ << "\n";
        out_.flush();
    }

    void status() {
        for (const load::ArtifactIntent& row : plan_.artifacts) {
            std::string now;
            switch (executor_->state_of(row.stem)) {
            case load::RowState::Resolved: now = "running"; break;
            case load::RowState::Pending: now = "waiting on this console"; break;
            case load::RowState::Loading: now = "loading"; break;
            case load::RowState::Refused: now = "refused: " + executor_->reason_of(row.stem); break;
            case load::RowState::Unavailable:
                now = "unavailable: " + executor_->reason_of(row.stem);
                break;
            default: now = "not reached"; break;
            }
            out_ << "  " << row.stem << "  " << now << "\n";
        }
    }

    /// What became of the row the console last said yes to, said once: running, or why not, and
    /// then, if asked, the row the plan waits on again.
    void report_realization(bool then_waiting) {
        const load::PlanExecutor::Realized r = executor_->take_realization();
        if (!r.settled) {
            return;
        }
        if (r.realized) {
            out_ << "zengine-app - running: " << r.stem << " (" << r.detail << ")\n";
        } else {
            out_ << "zengine-app - " << r.detail << "\n";
            if (then_waiting && executor_->state() == load::Realization::Waiting) {
                waiting(executor_->waiting_on());
            }
        }
        out_.flush();
        say_notes();
    }

    void say_notes() {
        for (std::size_t i = notes_said_; i < store_.notes().size(); ++i) {
            out_ << "zengine-app - " << store_.notes()[i] << "\n";
        }
        notes_said_ = store_.notes().size();
        out_.flush();
    }

    void end(const std::string& why) {
        if (!ended_) {
            ended_ = true;
            out_ << "zengine-app - ending: " << why << "\n";
            out_.flush();
        }
        bus_.stop();
    }

    // Declared in the order they must be destroyed last-first: the decisions outlive the Kernel
    // that asks them, the catalog outlives the Kernel and the executor, and the hosts go first.
    loom::Switchboard& bus_;
    AppPlace place_;
    std::ostream& out_;
    load::LoadPlan plan_;
    std::string store_path_;
    loom::host::StoreLock lock_;
    SessionFile session_; ///< removed before the lock is let go
    loom::host::AuthorityStore store_;
    loom::AdmissionPolicy decide_;
    op::Catalog operators_;
    op::OperatorHostSurface operator_host_{operators_};
    std::unique_ptr<loom::Kernel> kernel_;
    loom::host::HostWarden* warden_ = nullptr;
    std::unique_ptr<SeatDesk> desk_;
    loom::WeaveId manager_{};
    load::BootAnswers answers_;
    load::PlanBooter* booter_ = nullptr;
    std::unique_ptr<load::PlanExecutor> executor_;
    std::unique_ptr<zengine::flow_host::RuntimeHost> flow_;
    std::unique_ptr<zengine::view::Host> views_;
    std::size_t notes_said_ = 0;
    std::string refusal_;      ///< the store's words for the row last asked and refused, or empty
    std::string said_waiting_; ///< the row and the words the console was last told it waits on
    bool offered_ = false;     ///< the plan's one line was offered this run
    std::map<std::string, std::string> shown_; ///< each build the console showed, by artifact
    bool ended_ = false;
    int exit_code_ = 0;
};

} // namespace zengine::app

#endif // ZENGINE_APP_HOST_HPP
