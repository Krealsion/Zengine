// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Suite `workshop_neovim` -- the Neovim-backed Editor (`neovim-editor/`) holding the Editor's office
// in a LIVE Workshop, through the same rig the switch suite uses (`workshop_switch_rig.hpp`).
//
// ALWAYS: the choice with no Neovim at all, and with a fake one (`neovim-fixture`) that fails at
// start in each way a real one can -- too old, stopped at a prompt, silent -- so what a maker is told
// when Neovim is not usable is pinned on every lane; and a reload refused while a Neovim runs.
//
// BEHIND THE `neovim` GATE: the real Neovim named at configure time. Documents cross to Neovim and
// back exactly; Neovim edits between; an open through the office shows the file in Neovim; the save
// chord writes; the orderly quit asks Neovim; losses are named for consent; `:qa` leaves the office
// held and honest; the clipboard crosses both ways; an unfinished count is reset and said while a
// prompt refuses; and a Loom with no Workshop starts Neovim for a second terminal.
//
// TIME IS THE TIMER'S, GIVEN BY HAND: this rig mounts no Timer, so a case hands the switch
// coordinator its beat and the office's Neovim holder its beat, drains, and looks -- within a bound
// of wall time, because a real process answers in its own time. Every Neovim here keeps its state
// under the case's own directory: the program, the profile and the XDG directories are set for the
// case and put back after it, so no case reads or writes the maker's own Neovim.

#include "doctest.h"

#include "workshop_switch_rig.hpp"

#include "neovim-editor/vocabulary.hpp"
#include "neovim/child.hpp"
#include "timer/vocabulary.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace nve = zengine::neovim_editor;

void set_env(const char* name, const std::string& value) {
#if defined(_WIN32)
    (void)_putenv_s(name, value.c_str());
#else
    (void)setenv(name, value.c_str(), 1);
#endif
}

void unset_env(const char* name) {
#if defined(_WIN32)
    (void)_putenv_s(name, "");
#else
    (void)unsetenv(name);
#endif
}

/// THE NEOVIM THIS CASE'S EDITOR WILL START, and where that Neovim keeps its state -- set for the
/// case, and every variable put back as it was when the case ends.
class NeovimEnvironment {
public:
    NeovimEnvironment(const std::filesystem::path& root, const std::string& program, const char* fixture_mode = nullptr) {
        keep("ZENGINE_NEOVIM", program);
        keep("ZENGINE_NEOVIM_PROFILE", "clean");
        keep("ZENGINE_NEOVIM_FIXTURE_MODE", fixture_mode != nullptr ? fixture_mode : "");
        for (const char* v : {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME",
                              "XDG_RUNTIME_DIR", "LOCALAPPDATA"}) {
            const std::filesystem::path d = root / "neovim-env" / v;
            std::filesystem::create_directories(d);
            keep(v, d.string());
        }
        keep("NVIM_LOG_FILE", (root / "neovim-env" / "nvim.log").string());
    }
    ~NeovimEnvironment() {
        for (auto it = saved_.rbegin(); it != saved_.rend(); ++it) {
            if (it->second.has_value()) {
                set_env(it->first.c_str(), *it->second);
            } else {
                unset_env(it->first.c_str());
            }
        }
    }
    NeovimEnvironment(const NeovimEnvironment&) = delete;
    NeovimEnvironment& operator=(const NeovimEnvironment&) = delete;

private:
    void keep(const char* name, const std::string& value) {
        const char* was = std::getenv(name);
        saved_.emplace_back(name, was != nullptr ? std::optional<std::string>(was) : std::nullopt);
        if (value.empty()) {
            unset_env(name);
        } else {
            set_env(name, value);
        }
    }
    std::vector<std::pair<std::string, std::optional<std::string>>> saved_;
};

inline std::vector<load::ChoiceIntent> standard_and_neovim() {
    return {load::ChoiceIntent{pane::kEditorPaneRole, "standard", pane::kEditorPaneStem},
            load::ChoiceIntent{pane::kEditorPaneRole, "neovim", nve::kNeovimEditorStem}};
}

/// ONE BEAT, AS THE TIMER WOULD GIVE IT: to the switch coordinator, and to the office's holder when
/// the Neovim-backed Editor holds it. Then drain.
void beat(SwitchRig& s) {
    (void)s.r.bus.send(s.switcher_id, loom::Message(loom::to_value(zengine::timer::TimerFired{
                                                        "zengine.editor-switch.beat"}),
                                                    loom::WeaveId{}, loom::WeaveId{}, 0));
    if (s.r.plan_->choice_holder(pane::kEditorPaneRole) == nve::kNeovimEditorStem) {
        (void)s.r.bus.send(s.holder(), loom::Message(loom::to_value(zengine::timer::TimerFired{
                                                         "zengine.neovim-editor.beat"}),
                                                     loom::WeaveId{}, loom::WeaveId{}, 0));
    }
    s.r.bus.drain_until_idle();
}

/// BEAT UNTIL `done`, within `ms` of wall time.
template <class Done>
bool beat_until(SwitchRig& s, Done done, int ms = 20000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    for (;;) {
        beat(s);
        if (done()) {
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

/// ASK FOR A SWITCH AND BEAT UNTIL IT IS ANSWERED.
EditorSwitchAnswered switch_live(SwitchRig& s, const std::string& destination, int ms = 30000) {
    const std::size_t before = s.asker->answers.size();
    s.enqueue_switch(destination);
    const bool answered = beat_until(s, [&] { return s.asker->answers.size() > before; }, ms);
    REQUIRE_MESSAGE(answered, "the switch to `", destination, "` was not answered (stage `",
                    s.switcher->stage() == EditorSwitchCoordinator::Stage::Idle ? "idle" : "busy", "`)");
    return s.asker->answers.back();
}

/// ASK THE OFFICE TO OPEN A FILE, and hand back what the opening came to.
SourceOpened open_through_office(SwitchRig& s, const char* name, const std::string& bytes) {
    put_bytes(s.root / name, bytes);
    const std::string path = spelled(s.root / name);
    const std::size_t before = s.asker->opens.size();
    s.enqueue([path](SwitchAsker&, loom::Mail& mail) {
        (void)mail.as_role(kSwitchAskerOffice).send_to_role(kOpeningRole, OpenSourceRequested{path});
    });
    s.r.bus.drain_until_idle();
    REQUIRE(s.asker->opens.size() == before + 1);
    return s.asker->opens.back();
}

std::string file_text(const std::filesystem::path& at) {
    std::ifstream in(at, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

/// THE KEYS TO THE PANE: a press on its status row (which moves nothing in it).
void focus(SwitchRig& s) { press_pane(s.r, s.kind, 0, 0); }

/// A CONSOLE, reduced to what the baseline case needs: it asks the office one sentence from inside
/// its own delivery -- an ask, with the answer right Loom gives an ask -- and keeps what came back.
class ConsoleSeat : public loom::WeaveBase<ConsoleSeat, SeenState,
                                           loom::Accept<SeatDo, loom::Result, loom::Refused>,
                                           loom::Emit<nve::NeovimStartRequested, nve::NeovimStopRequested,
                                                      nve::NeovimStatusRequested>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<std::string> answers;

    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            auto what = std::move(next);
            next = nullptr;
            what(mail);
        }
    }
    void on(const loom::Result& r, loom::Mail& mail) {
        if (mail.answers_ask()) {
            answers.push_back(r.value);
        }
    }
    void on(const loom::Refused& r, loom::Mail& mail) {
        if (mail.answers_ask()) {
            answers.push_back("REFUSED: " + r.reason);
        }
    }
};

/// THE TIMER, AS FAR AS AN ORDER GOES: it records what it was asked and answers it by the Timer's
/// own rule for reading an order's continuity (`continuity_from`) -- refused when that rule cannot
/// read it, or when the case says to refuse.
class TimerSeat : public loom::WeaveBase<TimerSeat, SeenState,
                                         loom::Accept<zengine::timer::EnsureTimer, zengine::timer::CancelTimer>,
                                         loom::Emit<zengine::timer::TimerResolution>> {
public:
    std::vector<zengine::timer::EnsureTimer> orders;
    bool refuse = false;

    void on(const zengine::timer::EnsureTimer& order, loom::Mail& mail) {
        orders.push_back(order);
        namespace timer = zengine::timer;
        const bool readable = timer::continuity_from(order.preferred).has_value();
        const bool refused = refuse || !readable;
        (void)mail.send(mail.sender(),
                        timer::TimerResolution{order.id,
                                               refused ? timer::kResolutionRefused : timer::kResolutionRestarted,
                                               refused ? (readable ? "refused by the case" : "unknown continuity preference")
                                                       : "restarted"});
    }
    void on(const zengine::timer::CancelTimer&, loom::Mail&) {}
};

TimerSeat* mount_timer(SwitchRig& s) {
    auto held = std::make_unique<TimerSeat>();
    TimerSeat* raw = held.get();
    loom::Grant grant;
    grant.allow_to_any(zengine::timer::TimerResolution::zen_name, zengine::timer::TimerResolution::zen_version);
    const loom::WeaveId id = s.r.bus.register_weave(std::move(held), std::move(grant),
                                                    std::string(zengine::timer::kTimerRole));
    raw->zen_set_self(id);
    return raw;
}

/// WORKSHOP'S ORDERLY QUIT, the maker's way: keys back to the workspace, then `q`.
bool quit_by_key(SwitchRig& s) {
    s.r.press_cell(0, screen_of(s.r.session()).h - 1);
    s.r.key(input::scan::kQ);
    return s.r.host.quit;
}

} // namespace

TEST_SUITE("workshop_neovim") {

// ============================================================================
// ALWAYS: A NEOVIM THAT IS NOT USABLE IS SAID SO
// ============================================================================

TEST_CASE("a Neovim choice whose program is not there refuses the switch in words, and the standard Editor keeps editing") {
    SwitchRig s("nvim-missing");
    NeovimEnvironment env(s.root, (s.root / "no-such-dir" / "nvim").string());
    s.open(standard_and_neovim());
    const loom::WeaveId first = s.holder();
    (void)s.open_file("keep.cpp", "keep\n");
    const EditorSwitchAnswered no = switch_live(s, "neovim");
    CHECK(no.outcome == switch_outcome::kRefused);
    CHECK_MESSAGE(no.detail.find("`neovim` could not start") != std::string::npos, no.detail);
    CHECK_MESSAGE(no.detail.find("Neovim is not available") != std::string::npos, no.detail);
    CHECK(s.holder() == first);
    CHECK_FALSE(s.r.kernel.is_loaded(nve::kNeovimEditorStem));
    s.press_doc(0, 0);
    s.type("x");
    CHECK(s.read("text") == "xkeep\n");
}

TEST_CASE("a Neovim older than 0.11 refuses the switch naming its version") {
    SwitchRig s("nvim-old");
    NeovimEnvironment env(s.root, NEOVIM_FIXTURE, "old");
    s.open(standard_and_neovim());
    (void)s.open_file("keep.cpp", "keep\n");
    const EditorSwitchAnswered no = switch_live(s, "neovim");
    CHECK(no.outcome == switch_outcome::kRefused);
    CHECK_MESSAGE(no.detail.find("older than this Workshop supports") != std::string::npos, no.detail);
    CHECK_FALSE(s.r.kernel.is_loaded(nve::kNeovimEditorStem));
}

TEST_CASE("a Neovim that stops at a prompt while starting refuses the switch in its words") {
    SwitchRig s("nvim-prompt");
    NeovimEnvironment env(s.root, NEOVIM_FIXTURE, "prompt");
    s.open(standard_and_neovim());
    (void)s.open_file("keep.cpp", "keep\n");
    const EditorSwitchAnswered no = switch_live(s, "neovim");
    CHECK(no.outcome == switch_outcome::kRefused);
    CHECK_MESSAGE(no.detail.find("stopped at a prompt") != std::string::npos, no.detail);
}

TEST_CASE("a Neovim that never answers keeps the switch pending and cancellable, and the candidate goes with the cancel") {
    SwitchRig s("nvim-silent");
    NeovimEnvironment env(s.root, NEOVIM_FIXTURE, "silent");
    s.open(standard_and_neovim());
    const loom::WeaveId first = s.holder();
    (void)s.open_file("keep.cpp", "keep\n");
    const std::size_t before = s.asker->answers.size();
    s.enqueue_switch("neovim");
    (void)beat_until(s, [] { return false; }, 300);
    CHECK(s.asker->answers.size() == before);
    CHECK(s.switcher->stage() == EditorSwitchCoordinator::Stage::Warming);
    CHECK(s.r.kernel.is_loaded(nve::kNeovimEditorStem));
    const std::int64_t op = s.switcher->op();
    s.enqueue([op](SwitchAsker&, loom::Mail& mail) {
        (void)mail.as_role(kSwitchAskerOffice).send_to_role(kEditorSwitchRole, EditorSwitchCancelled{op});
    });
    s.r.bus.drain_until_idle();
    REQUIRE(s.asker->answers.size() == before + 2);
    CHECK(s.asker->answers.back().outcome == switch_outcome::kCancelled);
    CHECK_FALSE(s.r.kernel.is_loaded(nve::kNeovimEditorStem));
    CHECK(s.holder() == first);
}

TEST_CASE("the Neovim editor holding the office with no Neovim says so on its pane, refuses an open in words, and permits the quit") {
    SwitchRig s("nvim-absent-holder");
    NeovimEnvironment env(s.root, (s.root / "no-such-dir" / "nvim").string());
    s.open(standard_and_neovim(), nve::kNeovimEditorStem);
    CHECK(s.read("running") == "false");
    CHECK_MESSAGE(s.shows("Neovim is not available"), s.status());
    const SourceOpened opened = open_through_office(s, "a.cpp", "a\n");
    CHECK_FALSE(opened.accepted);
    CHECK_MESSAGE(opened.refusal.find("Neovim is not available") != std::string::npos, opened.refusal);
    CHECK(quit_by_key(s));
}

TEST_CASE("the Neovim editor orders its beat in words the Timer reads, and a refused beat is said on its pane") {
    SwitchRig s("nvim-beat");
    NeovimEnvironment env(s.root, NEOVIM_FIXTURE, "ok");
    TimerSeat* timer = nullptr;
    s.before_plan = [&] { timer = mount_timer(s); };
    s.open(standard_and_neovim(), nve::kNeovimEditorStem);
    REQUIRE(timer != nullptr);
    REQUIRE(timer->orders.size() == 1);
    const zengine::timer::EnsureTimer& order = timer->orders.front();
    CHECK(order.id == "zengine.neovim-editor.beat");
    CHECK(order.delay_ms == 10);
    CHECK(order.repeat);
    CHECK(zengine::timer::continuity_from(order.preferred).has_value());
    CHECK(zengine::timer::continuity_from(order.fallback).has_value());
    CHECK_FALSE(s.shows("refused this editor's beat"));

    // A TIMER THAT SAYS NO is said where the maker is looking.
    timer->refuse = true;
    const std::size_t before = timer->orders.size();
    (void)s.r.bus.send(s.holder(), loom::Message(loom::to_value(zengine::timer::TimerReady{}), loom::WeaveId{},
                                                 loom::WeaveId{}, 0));
    s.r.bus.drain_until_idle();
    REQUIRE(timer->orders.size() == before + 1);
    CHECK_MESSAGE(s.shows("the Timer refused this editor's beat"), s.status());
}

TEST_CASE("a reload of the Neovim editor is refused while its Neovim runs, said on its pane, and Neovim keeps running") {
    SwitchRig s("nvim-reload");
    NeovimEnvironment env(s.root, NEOVIM_FIXTURE, "ok");
    s.open(standard_and_neovim(), nve::kNeovimEditorStem);
    REQUIRE(beat_until(s, [&] { return s.read("ready") == "true"; }));
    const std::filesystem::path image(WORKSHOP_SO_NEOVIM_EDITOR);
    const std::filesystem::path rebuilt = s.root / ("rebuilt" + image.extension().string());
    std::filesystem::copy_file(image, rebuilt);
    const loom::WeaveId before = s.holder();
    const loom::ReloadResult reloaded = s.r.kernel.reload_from(nve::kNeovimEditorStem, rebuilt.string());
    CHECK_FALSE(reloaded.reloaded);
    MESSAGE("the kernel said: " << reloaded.error);
    beat(s);
    CHECK(s.holder() == before);
    CHECK(s.read("running") == "true");
    CHECK_MESSAGE(s.shows("a reload of the Neovim editor was refused"), s.status());
}

#if defined(NEOVIM_PROGRAM)

// ============================================================================
// BEHIND THE `neovim` GATE: A REAL NEOVIM
// ============================================================================

TEST_CASE("standard to Neovim and back carries the unsaved document and its caret exactly, with Neovim editing between") {
    SwitchRig s("nvim-carry");
    NeovimEnvironment env(s.root, NEOVIM_PROGRAM);
    s.open(standard_and_neovim());
    const std::string path = s.open_file("carry.txt", "alpha beta\n\tgamma\n");
    s.press_doc(0, 6);
    s.type("X");
    REQUIRE(s.read("text") == "alpha Xbeta\n\tgamma\n");
    REQUIRE(s.seat() != nullptr);
    const std::int64_t incumbent_generation = s.seat()->content_generation;

    const EditorSwitchAnswered away = switch_live(s, "neovim");
    REQUIRE_MESSAGE(away.outcome == switch_outcome::kSwitched, away.detail);
    CHECK(away.active == "neovim");
    REQUIRE(s.seat() != nullptr);
    CHECK(s.seat()->content_generation > incumbent_generation); // Neovim's rows pass the standard Editor's
    CHECK(s.r.plan_->choice_holder(pane::kEditorPaneRole) == nve::kNeovimEditorStem);
    CHECK_FALSE(s.r.kernel.is_loaded(pane::kEditorPaneStem));
    CHECK(s.read("path") == path);
    CHECK(s.read("modified") == "true");
    CHECK(beat_until(s, [&] { return s.shows("alpha Xbeta"); }));
    CHECK(s.shows("UNSAVED"));

    // NEOVIM EDITS: the caret it was handed is after `X`, in Normal mode on `b`; append at the end.
    focus(s);
    s.type("A!");
    s.r.key(input::scan::kEscape);
    REQUIRE(beat_until(s, [&] { return s.shows("alpha Xbeta!"); }));
    const std::int64_t neovim_generation = s.seat()->content_generation;

    const EditorSwitchAnswered back = switch_live(s, "standard");
    REQUIRE_MESSAGE(back.outcome == switch_outcome::kSwitched, back.detail);
    CHECK(s.seat()->content_generation > neovim_generation); // and the standard Editor's pass Neovim's
    CHECK(s.read("path") == path);
    CHECK(s.read("text") == "alpha Xbeta!\n\tgamma\n");
    CHECK(s.read("saved_text") == "alpha beta\n\tgamma\n");
    CHECK(s.read("caret_row") == "0");
    CHECK(s.read("caret_byte") == "11");
    CHECK_FALSE(s.r.kernel.is_loaded(nve::kNeovimEditorStem));
    CHECK(file_text(s.root / "carry.txt") == "alpha beta\n\tgamma\n"); // nothing was saved on the way
}

TEST_CASE("a selection crosses to Neovim as Visual and comes back as the same range, in its direction") {
    SwitchRig s("nvim-selection");
    NeovimEnvironment env(s.root, NEOVIM_PROGRAM);
    s.open(standard_and_neovim());
    (void)s.open_file("select.txt", "one two three\nfour\n");
    s.press_doc(0, 8); // before `three`
    for (int i = 0; i < 5; ++i) {
        s.r.key(input::scan::kLeft, input::mod::kShift); // back over `two ` and the space before it
    }
    const std::string anchor_row = s.read("anchor_row");
    const std::string anchor_byte = s.read("anchor_byte");
    const std::string caret_byte = s.read("caret_byte");
    REQUIRE(anchor_byte == "8");
    REQUIRE(caret_byte == "3");

    REQUIRE(switch_live(s, "neovim").outcome == switch_outcome::kSwitched);
    CHECK(beat_until(s, [&] { return s.read("mode") == "v"; }));
    const EditorSwitchAnswered back = switch_live(s, "standard");
    REQUIRE_MESSAGE(back.outcome == switch_outcome::kSwitched, back.detail);
    CHECK(s.read("anchor_row") == anchor_row);
    CHECK(s.read("anchor_byte") == anchor_byte);
    CHECK(s.read("caret_row") == "0");
    CHECK(s.read("caret_byte") == caret_byte);
}

TEST_CASE("an open through the office shows the file in Neovim, and the save chord writes it") {
    SwitchRig s("nvim-open");
    NeovimEnvironment env(s.root, NEOVIM_PROGRAM);
    s.open(standard_and_neovim(), nve::kNeovimEditorStem);
    REQUIRE(beat_until(s, [&] { return s.read("ready") == "true"; }));
    const SourceOpened opened = open_through_office(s, "open.txt", "first line\n");
    REQUIRE_MESSAGE(opened.accepted, opened.refusal);
    CHECK(s.read("path") == spelled(s.root / "open.txt"));
    REQUIRE(beat_until(s, [&] { return s.shows("first line"); }));
    focus(s);
    s.type("Inew ");
    s.r.key(input::scan::kEscape);
    REQUIRE(beat_until(s, [&] { return s.read("modified") == "true"; }));
    s.r.key(input::scan::kS, input::mod::kCtrl);
    REQUIRE(beat_until(s, [&] { return s.read("modified") == "false"; }));
    CHECK(file_text(s.root / "open.txt") == "new first line\n");
}

TEST_CASE("after a switch to Neovim, an open through the office shows another file in Neovim, beside the unsaved one") {
    SwitchRig s("nvim-open-after-switch");
    NeovimEnvironment env(s.root, NEOVIM_PROGRAM);
    s.open(standard_and_neovim());
    (void)s.open_file("first.txt", "first\n");
    s.press_doc(0, 0);
    s.type("x");
    REQUIRE(switch_live(s, "neovim").outcome == switch_outcome::kSwitched);
    REQUIRE(beat_until(s, [&] { return s.shows("xfirst"); }));
    // THE TIMER DOES NOT WAIT FOR AN OPEN: a beat reaches the office between every turn of the
    // opening's conversation, as it does in a running Workshop. Loading the file hidden must not
    // move the document's claim while the opening binds it (measured: it did, and every such open
    // was refused as "the desk changed").
    put_bytes(s.root / "second.txt", "second\n");
    const std::string second = spelled(s.root / "second.txt");
    const std::size_t before = s.asker->opens.size();
    s.enqueue([second](SwitchAsker&, loom::Mail& mail) {
        (void)mail.as_role(kSwitchAskerOffice).send_to_role(kOpeningRole, OpenSourceRequested{second});
    });
    for (int turn = 0; turn < 400 && s.asker->opens.size() == before; ++turn) {
        (void)s.r.bus.pump_pending();
        (void)s.r.bus.send(s.holder(), loom::Message(loom::to_value(zengine::timer::TimerFired{
                                                         "zengine.neovim-editor.beat"}),
                                                     loom::WeaveId{}, loom::WeaveId{}, 0));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    s.r.bus.drain_until_idle();
    REQUIRE(s.asker->opens.size() == before + 1);
    const SourceOpened opened = s.asker->opens.back();
    REQUIRE_MESSAGE(opened.accepted, opened.refusal);
    CHECK(s.read("path") == second);
    CHECK(beat_until(s, [&] { return s.shows("second"); }));
    // ...AND THE CARRIED WORK STAYS IN NEOVIM, UNSAVED, BESIDE IT: the orderly quit names it.
    CHECK_FALSE(quit_by_key(s));
    CHECK(s.r.session().notice.find("first.txt") != std::string::npos);
    CHECK(file_text(s.root / "first.txt") == "first\n");
}

TEST_CASE("the orderly quit is refused while Neovim holds unsaved changes, naming the file, and permitted once written") {
    SwitchRig s("nvim-quit");
    NeovimEnvironment env(s.root, NEOVIM_PROGRAM);
    s.open(standard_and_neovim(), nve::kNeovimEditorStem);
    REQUIRE(beat_until(s, [&] { return s.read("ready") == "true"; }));
    REQUIRE(open_through_office(s, "quit.txt", "text\n").accepted);
    focus(s);
    s.type("x");
    REQUIRE(beat_until(s, [&] { return s.read("modified") == "true"; }));
    CHECK_FALSE(quit_by_key(s));
    CHECK(s.r.session().notice.find("unsaved changes") != std::string::npos);
    CHECK(s.r.session().notice.find("quit.txt") != std::string::npos);
    focus(s);
    s.r.key(input::scan::kS, input::mod::kCtrl);
    REQUIRE(beat_until(s, [&] { return s.read("modified") == "false"; }));
    CHECK(quit_by_key(s));
}

TEST_CASE("a switch away from Neovim names another modified buffer for consent, and carries the current one") {
    SwitchRig s("nvim-losses");
    NeovimEnvironment env(s.root, NEOVIM_PROGRAM);
    s.open(standard_and_neovim(), nve::kNeovimEditorStem);
    REQUIRE(beat_until(s, [&] { return s.read("ready") == "true"; }));
    REQUIRE(open_through_office(s, "first.txt", "first\n").accepted);
    focus(s);
    s.type("x");
    s.r.key(input::scan::kEscape);
    REQUIRE(beat_until(s, [&] { return s.read("modified") == "true"; }));
    REQUIRE(open_through_office(s, "second.txt", "second\n").accepted);
    REQUIRE(beat_until(s, [&] { return s.read("path") == spelled(s.root / "second.txt"); }));

    const EditorSwitchAnswered asked = switch_live(s, "standard");
    REQUIRE_MESSAGE(asked.outcome == switch_outcome::kNeedsConfirmation, asked.detail);
    REQUIRE(asked.losses.size() == 1);
    CHECK(asked.losses[0].find("first.txt") != std::string::npos);
    CHECK_MESSAGE(asked.detail.find("would lose unsaved changes to ") != std::string::npos, asked.detail);
    const std::size_t before = s.asker->answers.size();
    const std::int64_t op = asked.op;
    const std::string consent = asked.consent;
    s.enqueue([op, consent](SwitchAsker&, loom::Mail& mail) {
        (void)mail.as_role(kSwitchAskerOffice).send_to_role(kEditorSwitchRole, EditorSwitchConfirmed{op, consent});
    });
    REQUIRE(beat_until(s, [&] { return s.asker->answers.size() > before; }));
    const EditorSwitchAnswered done = s.asker->answers.back();
    REQUIRE_MESSAGE(done.outcome == switch_outcome::kSwitched, done.detail);
    CHECK(s.read("path") == spelled(s.root / "second.txt"));
    CHECK(s.read("text") == "second\n");
    CHECK(file_text(s.root / "first.txt") == "first\n"); // the consented loss was not written anywhere
}

TEST_CASE("Neovim ended from inside leaves the office held with no document, said so, and switching back carries nothing") {
    SwitchRig s("nvim-qa");
    NeovimEnvironment env(s.root, NEOVIM_PROGRAM);
    s.open(standard_and_neovim(), nve::kNeovimEditorStem);
    REQUIRE(beat_until(s, [&] { return s.read("ready") == "true"; }));
    focus(s);
    s.type(":qa!");
    s.r.key(input::scan::kReturn);
    REQUIRE(beat_until(s, [&] { return s.read("running") == "false"; }));
    CHECK_MESSAGE(s.shows("Neovim exited"), s.status());
    const EditorSwitchAnswered back = switch_live(s, "standard");
    REQUIRE_MESSAGE(back.outcome == switch_outcome::kSwitched, back.detail);
    CHECK(s.read("path").empty());
}

TEST_CASE("a copy in Neovim reaches the Skin, and a paste in Neovim asks the Skin") {
    SwitchRig s("nvim-clipboard");
    NeovimEnvironment env(s.root, NEOVIM_PROGRAM);
    s.open(standard_and_neovim(), nve::kNeovimEditorStem);
    REQUIRE(beat_until(s, [&] { return s.read("ready") == "true"; }));
    REQUIRE(open_through_office(s, "clip.txt", "copy me\n").accepted);
    focus(s);
    // A LINEWISE COPY is its line and its newline -- once.
    s.type("\"+yy");
    REQUIRE(beat_until(s, [&] { return s.skin->platform == "copy me\n"; }));
    // A PASTE ASKS THE SKIN, once, and what the medium holds lands where Neovim put it.
    s.skin->platform = "pasted";
    const int reads = s.skin->clipboard_reads;
    s.type("\"+P");
    REQUIRE(beat_until(s, [&] { return s.shows("pastedcopy me"); }));
    CHECK(s.skin->clipboard_reads == reads + 1);
}

TEST_CASE("an unfinished command in Neovim is reset by a switch and said, and a prompt refuses the switch until it is answered") {
    SwitchRig s("nvim-pending");
    NeovimEnvironment env(s.root, NEOVIM_PROGRAM);
    s.open(standard_and_neovim(), nve::kNeovimEditorStem);
    REQUIRE(beat_until(s, [&] { return s.read("ready") == "true"; }));
    REQUIRE(open_through_office(s, "pending.txt", "keep\n").accepted);

    // A COUNT NOBODY FINISHED holds every question Neovim is asked; the switch cancels it with
    // Escape, carries the document, and lists the reset.
    focus(s);
    s.type("3");
    (void)beat_until(s, [] { return false; }, 100);
    const EditorSwitchAnswered away = switch_live(s, "standard");
    REQUIRE_MESSAGE(away.outcome == switch_outcome::kSwitched, away.detail);
    bool said = false;
    for (const std::string& reset : away.resets) {
        said = said || reset.find("an unfinished command in Neovim") != std::string::npos;
    }
    CHECK_MESSAGE(said, "the cancelled count was not reported");
    CHECK(s.read("text") == "keep\n");

    // A PROMPT is the maker's to answer: refused, in words, and the switch takes once it is answered.
    REQUIRE(switch_live(s, "neovim").outcome == switch_outcome::kSwitched);
    focus(s);
    s.type(":echo \"one\\ntwo\\nthree\"");
    s.r.key(input::scan::kReturn);
    REQUIRE(beat_until(s, [&] { return s.shows("Press ENTER"); }));
    const EditorSwitchAnswered no = switch_live(s, "standard");
    CHECK(no.outcome == switch_outcome::kRefused);
    CHECK_MESSAGE(no.detail.find("waiting at a prompt") != std::string::npos, no.detail);
    CHECK(s.r.plan_->choice_holder(pane::kEditorPaneRole) == nve::kNeovimEditorStem);
    focus(s);
    s.r.key(input::scan::kReturn);
    (void)beat_until(s, [&] { return !s.shows("Press ENTER"); });
    const EditorSwitchAnswered yes = switch_live(s, "standard");
    REQUIRE_MESSAGE(yes.outcome == switch_outcome::kSwitched, yes.detail);
    CHECK(s.read("text") == "keep\n");
}

TEST_CASE("from a Loom with no Workshop, Neovim listens for a second terminal, says where, and stops only when nothing is lost") {
    // THE BASELINE JOURNEY'S OWN WIRING: the weave loaded by a plan into the Editor's office and
    // nothing else -- no desk, no pane, no switch -- asked the three sentences a console sends.
    TempDir dir("nvim-baseline");
    const std::filesystem::path root = dir.path();
    NeovimEnvironment env(root, NEOVIM_PROGRAM);
    PaneRig r;
    load::LoadPlan plan;
    load::ArtifactIntent seat;
    seat.stem = nve::kNeovimEditorStem;
    seat.weave = load::WeaveIntent{nve::kEditorOffice};
    plan.artifacts.push_back(seat);
    const load::Executed done = r.run_plan(plan);
    REQUIRE_MESSAGE(done.ok, done.refusal);
    auto held = std::make_unique<ConsoleSeat>();
    ConsoleSeat* console = held.get();
    loom::Grant may_ask;
    may_ask.allow_to_role(nve::NeovimStartRequested::zen_name, nve::NeovimStartRequested::zen_version,
                          nve::kEditorOffice);
    may_ask.allow_to_role(nve::NeovimStopRequested::zen_name, nve::NeovimStopRequested::zen_version,
                          nve::kEditorOffice);
    may_ask.allow_to_role(nve::NeovimStatusRequested::zen_name, nve::NeovimStatusRequested::zen_version,
                          nve::kEditorOffice);
    const loom::WeaveId console_id = r.bus.register_weave(std::move(held), std::move(may_ask),
                                                          std::string("zengine.test.console"));
    console->zen_set_self(console_id);
    const auto ask = [&](auto shape) {
        const std::size_t before = console->answers.size();
        console->next = [shape](loom::Mail& mail) {
            (void)mail.as_role("zengine.test.console").send_to_role(nve::kEditorOffice, shape);
        };
        (void)r.bus.send(console_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
        if (console->answers.size() != before + 1) {
            FAIL_CHECK("the ask was never answered");
            return std::string();
        }
        return console->answers.back();
    };
    /// A SECOND NEOVIM, the way a second terminal runs one: `--server <address>` and one remote
    /// argument, and what it printed.
    const auto remote = [](const std::string& address, std::vector<std::string> args) {
        zengine::neovim::LaunchSpec spec;
        spec.program = NEOVIM_PROGRAM;
        spec.args = {"--clean", "--headless", "--server", address};
        for (std::string& a : args) {
            spec.args.push_back(std::move(a));
        }
        spec.unset_env = {"NVIM", "NVIM_LISTEN_ADDRESS"};
        zengine::neovim::ChildStart started = zengine::neovim::start_child(spec);
        REQUIRE_MESSAGE(started.started, started.trouble);
        std::string out;
        std::string in;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        for (;;) {
            const zengine::neovim::Child::Pump p = started.child.pump(out, in, 1u << 16);
            if ((p.exited && p.output_ended) || std::chrono::steady_clock::now() >= deadline) {
                break;
            }
            (void)started.child.wait(10);
        }
        (void)started.child.finish(0);
        return in;
    };

    const std::filesystem::path file = root / "remote.txt";
    put_bytes(file, "far away\n");
    CHECK(ask(nve::NeovimStatusRequested{}).rfind("Neovim is not running", 0) == 0);
    const std::string started = ask(nve::NeovimStartRequested{file.string(), std::string()});
    REQUIRE_MESSAGE(started.find("is listening at ") != std::string::npos, started);
    CHECK(started.find("--remote-ui") != std::string::npos);
    const std::size_t at = started.find("is listening at ") + std::string("is listening at ").size();
    const std::string address = started.substr(at, started.find(' ', at) - at);

    // THE SECOND TERMINAL SEES THE FILE, and edits it.
    const std::string name = remote(address, {"--remote-expr", "expand('%:t')"});
    CHECK_MESSAGE(name.find("remote.txt") != std::string::npos, name);
    (void)remote(address, {"--remote-send", "ggInear <Esc>"});

    const std::string status = ask(nve::NeovimStatusRequested{});
    CHECK_MESSAGE(status.find("remote.txt, modified") != std::string::npos, status);
    const std::string kept = ask(nve::NeovimStopRequested{false});
    CHECK_MESSAGE(kept.rfind("REFUSED: Neovim holds unsaved changes to", 0) == 0, kept);
    CHECK(file_text(file) == "far away\n");
    const std::string stopped = ask(nve::NeovimStopRequested{true});
    CHECK_MESSAGE(stopped.rfind("Neovim ended (status", 0) == 0, stopped);
    CHECK(ask(nve::NeovimStatusRequested{}).rfind("Neovim is not running", 0) == 0);
}

#endif // NEOVIM_PROGRAM

} // TEST_SUITE
