// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The app host as `zengine-app` composes it (app/docs/app.md), over the tally app's folder: each
// plan row waiting on the console until decided in Loom's `authority` grammar, a provider among
// them; the panel drawn in the window the app's weave arranges, typed into and pressed there, its
// flow answering; the window's close ending the run, no other office's. The suites' pixel window
// (`window_backends.hpp`) stands for the plan's window row and the suite turns its beat for the
// Timer, so the plan here is the probe's without those two rows.

#include "doctest.h"

#include "app/files.hpp"
#include "app/host.hpp"
#include "examples/tally-app/tally.hpp"
#include "lifecycle_door.hpp"
#include "window_backends.hpp"

#include "operator/primitives.hpp"
#include "timer/vocabulary.hpp"

#include <zen/host/authority.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace app = zengine::app;
namespace gfx = zengine::graphics;
namespace gw = zengine::graphics::window;
namespace in = zengine::input;
namespace fs = std::filesystem;
using zengine::testing::window_backends::FakeSdlBackend;
using zengine::testing::window_backends::FakeSdlWindow;
using zengine::testing::window_backends::LightLog;

constexpr const char* kSuffix =
#if defined(_WIN32)
    ".dll";
#elif defined(__APPLE__)
    ".dylib";
#else
    ".so";
#endif

std::string text_of(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool has(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

/// The probe's plan without its window and Timer rows, which the suite stands in for.
std::string suite_plan() {
    const std::string probe = text_of(fs::path(TALLY_APP_DIR) / app::kPlanName);
    const zengine::workshop::load_persist::LoadedPlan read =
        zengine::workshop::load_persist::from_text(probe);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    zengine::workshop::load::LoadPlan plan = read.plan;
    std::erase_if(plan.artifacts, [](const auto& row) {
        return row.stem == "zengine-window-sdl" || row.stem == "zengine-timer";
    });
    REQUIRE(plan.artifacts.size() == 3);
    return zengine::workshop::load_persist::to_text(plan);
}

/// A copy of the tally app's folder of its own, with the suite's plan, and no decisions in it.
struct AppFolder {
    fs::path dir;
    explicit AppFolder(const std::string& name) {
        dir = fs::temp_directory_path() /
              ("zengine-app-host-" + name + "-" +
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(dir);
        for (const char* file : {app::kAppFileName, "tally.view", "tally.flow"}) {
            fs::copy_file(fs::path(TALLY_APP_DIR) / file, dir / file);
        }
        std::ofstream(dir / app::kPlanName, std::ios::binary) << suite_plan();
    }
    ~AppFolder() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    std::string path() const { return dir.generic_string(); }
};

struct AppHostSuiteEars {
    std::int64_t heard = 0;
    ZEN_SHAPE(AppHostSuiteEars, 1, ZEN_FIELD(heard));
};

struct AppHostSuiteCloser {
    std::int64_t said = 0;
    ZEN_SHAPE(AppHostSuiteCloser, 1, ZEN_FIELD(said));
};

/// Says a close as whatever office it holds, and hears nothing it acts on.
class Stranger : public loom::WeaveBase<Stranger, AppHostSuiteCloser, loom::Accept<gfx::WindowShown>,
                                        loom::Emit<gfx::SurfaceCloseRequested>> {
public:
    void on(const gfx::WindowShown&, loom::Mail&) {}
};

/// Hears what the window shows.
class Ears : public loom::WeaveBase<Ears, AppHostSuiteEars, loom::Accept<gfx::WindowShown>,
                                    loom::Emit<gfx::WindowShowAsked>> {
public:
    void on(const gfx::WindowShown& s, loom::Mail&) {
        ++state_.heard;
        shown.push_back(s);
    }
    std::vector<gfx::WindowShown> shown;
};

/// One run of the app host over a folder: the bus, the host, what it says, and the window the
/// suite stands where the plan's window would.
struct Run {
    loom::Switchboard bus;
    LightLog light;
    std::ostringstream said;
    std::unique_ptr<app::AppHost> host;
    loom::WeaveId window{};
    Ears* ears = nullptr;
    loom::WeaveId ears_id{};

    explicit Run(const AppFolder& folder) {
        host = std::make_unique<app::AppHost>(
            bus, app::AppPlace{folder.path(), APP_ARTIFACTS_DIR, kSuffix}, said);
    }
    ~Run() { host.reset(); }

    bool open() {
        std::string why;
        const bool opened = host->open(&why);
        if (!opened) said << why;
        return opened;
    }

    /// Turns of the bus until nothing waits, and the host's after-turn step.
    void turn() {
        for (int n = 0; n < 256 && bus.pending() != 0; ++n) {
            bus.pump_pending();
            host->turned();
        }
    }

    void command(const std::string& line) {
        host->command(line);
        turn();
    }

    std::string waiting_on() const { return host->executor().waiting_on(); }

    /// The OS window over the suite's pixel backend, at its office, activated as Loom's control
    /// door activates a loaded weave: it offers itself.
    void mount_window() {
        auto w = std::make_unique<FakeSdlWindow>(FakeSdlBackend{&light});
        FakeSdlWindow* raw = w.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        window = bus.register_weave(std::move(w), std::move(g),
                                    std::string(gfx::kWindowOfficePrefix) + gfx::kSdlWindow);
        raw->zen_set_self(window);
        const loom::WeaveId door = zengine::testing::mount_door(bus);
        zengine::testing::order_activation(bus, door, window, 1);
        auto e = std::make_unique<Ears>();
        ears = e.get();
        loom::Grant asking;
        asking.allow_to_any(gfx::WindowShowAsked::zen_name, gfx::WindowShowAsked::zen_version);
        ears_id = bus.register_weave(std::move(e), std::move(asking));
        ears->zen_set_self(ears_id);
        turn();
        pump();
    }

    /// The window's own beat, with what the suite queued for it to read.
    void pump() {
        for (int n = 0; n < 3; ++n) {
            bus.send(window, loom::Message(loom::to_value(
                                 zengine::timer::TimerFired{gw::kPumpTimerId})));
            turn();
        }
    }

    /// What the window shows now, row by row.
    std::string shown() {
        bus.send_as_to_role(ears_id, std::string(gfx::kWindowOfficePrefix) + gfx::kSdlWindow,
                            loom::Message(loom::to_value(gfx::WindowShowAsked{}), ears_id, ears_id));
        turn();
        std::string out;
        if (!ears->shown.empty())
            for (const std::string& row : ears->shown.back().rows) out += row + "\n";
        return out;
    }

    void key(std::int64_t scancode) {
        light.queued.events.emplace_back(in::KeyPressed{scancode, "", in::mod::kNone});
        pump();
    }
    void text(const std::string& typed) {
        light.queued.events.emplace_back(in::TextEntered{typed});
        pump();
    }
    /// A left press and its release at a pixel of the window.
    void click(std::int64_t x, std::int64_t y) {
        for (const bool down : {true, false}) {
            light.queued.events.emplace_back(
                in::PointerButton{1, down, x, y, in::space::kPixels, in::mod::kNone});
        }
        pump();
    }
    /// Each field reached by Tab, emptied and typed into, then Count pressed: all in the window.
    void count(const std::string& start, const std::string& limit, const std::string& step) {
        for (const std::string& typed : {start, limit, step}) {
            key(in::scan::kTab);
            for (int i = 0; i < 4; ++i) key(in::scan::kBackspace);
            text(typed);
        }
        key(in::scan::kEscape);
        press_count();
    }
    /// A press on Count, at its middle: the panel's area stands at (8, 8), as `app.json` says.
    void press_count() {
        const zengine::view::Element b = zengine::examples::tally_app::panel().elements[3];
        click(8 + b.x + b.w / 2, 8 + b.y + b.h / 2);
    }
};

} // namespace

TEST_CASE("the tally app's view and flow are the bytes its tool writes from the suites' own tally") {
    zengine::op::Catalog catalog;
    zengine::op::publish_primitives(catalog);
    CHECK(text_of(fs::path(TALLY_APP_DIR) / "tally.view") ==
          zengine::view::description_bytes(zengine::examples::tally_app::panel()));
    CHECK(text_of(fs::path(TALLY_APP_DIR) / "tally.flow") ==
          zengine::flow::project_bytes(zengine::examples::tally_app::project(catalog)));
    const app::ReadApp read = app::read_app((fs::path(TALLY_APP_DIR) / app::kAppFileName).string());
    REQUIRE_MESSAGE(read.ok, read.refusal);
    CHECK(read.app.name == "tally");
    CHECK(read.app.window.skin == "day");
    REQUIRE(read.app.views.size() == 1);
    CHECK(read.app.views[0].file == "tally.view");
    CHECK(read.app.views[0].x == 8);
    REQUIRE(read.app.flows.size() == 1);
    CHECK(read.app.flows[0].session == "tally");
}

TEST_CASE("app.json names files in the app's own folder, and a file it cannot read is refused in words") {
    const auto with_view = [](const std::string& file) {
        return std::string(R"({"zen":1,"schema":"ZengineAppFile","version":1,"fields":{"name":"x",)"
                           R"("window":{"title":"","width":"0","height":"0","skin":"day"},)"
                           R"("views":[{"file":")") + file +
               R"(","session":"s","x":"0","y":"0"}],"flows":[]}})";
    };
    CHECK(app::app_from_text(with_view("ok.view")).ok);
    for (const char* outside : {"../x.view", "a/b.view", "C:x.view", "..", ""}) {
        const app::ReadApp read = app::app_from_text(with_view(outside));
        CHECK_FALSE(read.ok);
        CHECK(has(read.refusal, "app.json view file"));
    }
    CHECK(has(app::app_from_text("{}").refusal, "app.json is not an app"));
    CHECK_FALSE(app::read_app("no-such-folder/app.json").ok);
}

TEST_CASE("each row of the app's plan waits on its console until decided there, a provider among them, and the walk goes on row by row after each yes") {
    AppFolder folder("consent");
    Run run(folder);
    REQUIRE(run.open());
    run.host->begin();
    run.turn();

    // THE FIRST ROW, A PROVIDER, WAITS: nothing is mounted, and the console says what answers it.
    CHECK(run.waiting_on() == "zengine-operators-basic");
    CHECK(run.host->operators().size() == 0);
    CHECK(has(run.said.str(), "waiting on this console: zengine-operators-basic"));
    CHECK(has(run.said.str(), "authority trust zengine-operators-basic"));
    CHECK(run.host->executor().resolved().empty());

    // ...a yes to a row behind it waits its turn: the plan's order is the walk's.
    run.command("authority trust zengine-app-weave");
    CHECK(run.waiting_on() == "zengine-operators-basic");
    CHECK(run.host->operators().size() == 0);

    // ...a refusal at the console keeps it waiting.
    run.command("authority deny zengine-operators-basic");
    CHECK(run.waiting_on() == "zengine-operators-basic");
    CHECK(has(run.said.str(), "is denied by a standing decision"));
    CHECK(run.host->operators().size() == 0);

    // DECIDED: the provider is mounted, the app's weave, decided before, loads behind it, and the
    // walk waits on the directory.
    run.command("authority trust zengine-operators-basic");
    CHECK(run.host->operators().size() > 0);
    CHECK(run.host->executor().state_of("zengine-app-weave") ==
          zengine::workshop::load::RowState::Resolved);
    CHECK(run.waiting_on() == "zengine-graphics");
    CHECK(has(run.said.str(), "waiting on this console: zengine-graphics"));

    run.command("authority trust zengine-graphics");
    CHECK(run.host->executor().state() == zengine::workshop::load::Realization::Complete);
    CHECK(has(run.said.str(), "running: 3 of 3 artifacts"));
    CHECK_FALSE(run.host->ended());

    // THE DECISIONS ARE LOOM'S STORE, in the app's folder: each artifact may run, its build pinned.
    loom::host::AuthorityStore reread;
    std::string why;
    run.host.reset();
    REQUIRE(reread.open((folder.dir / app::kAuthorityName).string(), &why));
    for (const char* stem : {"zengine-operators-basic", "zengine-app-weave", "zengine-graphics"}) {
        const loom::host::AuthorityRule* rule = reread.find(stem);
        REQUIRE(rule != nullptr);
        CHECK(rule->may_run);
        CHECK_FALSE(rule->content_id.empty());
    }
}

TEST_CASE("a second start whose builds are unchanged asks nothing; a build the console did not see is asked again; and two hosts never run one app") {
    AppFolder folder("again");
    {
        Run first(folder);
        REQUIRE(first.open());
        // TWO OF THEM: a second host on the same app is refused, by name, while the first runs.
        Run second(folder);
        CHECK_FALSE(second.open());
        CHECK(has(second.said.str(), "already running"));
        first.host->begin();
        for (const char* stem : {"zengine-operators-basic", "zengine-app-weave", "zengine-graphics"})
            first.command(std::string("authority trust ") + stem);
        REQUIRE(first.host->executor().state() == zengine::workshop::load::Realization::Complete);
    }
    {
        Run again(folder);
        REQUIRE(again.open());
        again.host->begin();
        again.turn();
        CHECK(again.host->executor().state() == zengine::workshop::load::Realization::Complete);
        CHECK_FALSE(has(again.said.str(), "waiting on this console"));
    }
    {
        // A BUILD THE DECISION WAS NOT MADE ABOUT: the pin names other bytes, so the row waits.
        loom::host::AuthorityStore store;
        std::string why;
        REQUIRE(store.open((folder.dir / app::kAuthorityName).string(), &why));
        loom::host::AuthorityRule rule = *store.find("zengine-graphics");
        rule.content_id = std::string(32, '0');
        REQUIRE(store.put(rule, &why));
    }
    Run changed(folder);
    REQUIRE(changed.open());
    changed.host->begin();
    changed.turn();
    CHECK(changed.waiting_on() == "zengine-graphics");
    CHECK(has(changed.said.str(), "'zengine-graphics' changed since it was approved"));
}

TEST_CASE("the app's weave runs its flow and its view, and its panel is drawn in the window it arranges, typed into and pressed through it; the flow answers, and its fold's refusal is said") {
    AppFolder folder("window");
    Run run(folder);
    REQUIRE(run.open());
    run.host->begin();
    for (const char* stem : {"zengine-operators-basic", "zengine-app-weave", "zengine-graphics"})
        run.command(std::string("authority trust ") + stem);
    REQUIRE(run.host->executor().state() == zengine::workshop::load::Realization::Complete);
    run.mount_window();

    // DRAWN in its area, placed where app.json says, painted by the skin it names.
    const LightLog::Area* panel = run.light.area("tally.panel.view");
    REQUIRE(panel != nullptr);
    CHECK(panel->x == 8);
    CHECK(panel->y == 8);
    CHECK(has(run.shown(), "Total: waiting"));

    // COUNT AS IT STARTS: 0 to 10 by 1.
    run.press_count();
    CHECK(has(run.shown(), "Total: 45"));

    // TYPED IN THE WINDOW: only keys typed there make 30.
    run.count("10", "0", "-2");
    CHECK(has(run.shown(), "Total: 30"));

    // THE FOLD'S REFUSAL is said on the view's notice rows, and the total stays.
    run.count("0", "10", "0");
    const std::string after = run.shown();
    CHECK(has(after, "refused"));
    CHECK(has(after, "Total: 30"));
    CHECK_FALSE(run.host->ended());
}

TEST_CASE("a field of the app's view is clicked into in its window and typed into, and Count gives what was typed") {
    // MUTATION: the app's weave placing its views deaf to the pointer -- the click focuses nothing.
    AppFolder folder("click");
    Run run(folder);
    REQUIRE(run.open());
    run.host->begin();
    for (const char* stem : {"zengine-operators-basic", "zengine-app-weave", "zengine-graphics"})
        run.command(std::string("authority trust ") + stem);
    REQUIRE(run.host->executor().state() == zengine::workshop::load::Realization::Complete);
    run.mount_window();
    REQUIRE(has(run.shown(), "Total: waiting"));

    // STEP CLICKED, emptied and typed into: no Tab, only the press put it in focus. 0 to 10 by 2.
    const zengine::view::Element step = zengine::examples::tally_app::panel().elements[2];
    REQUIRE(step.id == "step");
    run.click(8 + step.x + step.w / 2, 8 + step.y + step.h / 2);
    for (int i = 0; i < 4; ++i) run.key(in::scan::kBackspace);
    run.text("2");
    CHECK(has(run.shown(), "step: 2"));
    run.press_count();
    CHECK(has(run.shown(), "Total: 20"));
}

TEST_CASE("the app's window tells the app's weave nothing it does not take: its showings, a press and "
          "typing put no refused notice on the bus") {
    // MUTATION: the window telling every arranger what an area shows settled -- the app's weave,
    // which places its areas and takes no such word, refuses it at each showing.
    AppFolder folder("quiet");
    Run run(folder);
    REQUIRE(run.open());
    run.host->begin();
    for (const char* stem : {"zengine-operators-basic", "zengine-app-weave", "zengine-graphics"})
        run.command(std::string("authority trust ") + stem);
    REQUIRE(run.host->executor().state() == zengine::workshop::load::Realization::Complete);
    std::vector<std::string> refused;
    const loom::ObserverId tap = run.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Refused && run.window.valid() && ev.sender == run.window &&
            ev.addressed_role == app::kAppRole)
            refused.push_back(ev.schema_name);
    });
    run.mount_window();
    REQUIRE(has(run.shown(), "Total: waiting"));
    run.press_count();
    run.count("10", "0", "-2");
    CHECK(has(run.shown(), "Total: 30"));
    run.bus.remove_observer(tap);
    std::string said;
    for (const std::string& r : refused) said += r + "\n";
    CHECK_MESSAGE(refused.empty(), said);
}

TEST_CASE("the close of the app's own window ends the run, exit 0; a close any other office says ends nothing") {
    AppFolder folder("close");
    Run run(folder);
    REQUIRE(run.open());
    run.host->begin();
    for (const char* stem : {"zengine-operators-basic", "zengine-app-weave", "zengine-graphics"})
        run.command(std::string("authority trust ") + stem);
    run.mount_window();
    REQUIRE_FALSE(run.host->ended());

    // ANOTHER OFFICE'S CLOSE, and one with no office, end nothing.
    auto s = std::make_unique<Stranger>();
    const loom::WeaveId stranger =
        run.bus.register_weave(std::move(s), loom::Grant{}.allow_any(), "zengine.window.stranger");
    (void)run.bus.office_publish_as(stranger, "zengine.window.stranger",
                                    loom::Message(loom::to_value(gfx::SurfaceCloseRequested{}), stranger));
    (void)run.bus.publish(loom::Message(loom::to_value(gfx::SurfaceCloseRequested{})));
    run.turn();
    CHECK_FALSE(run.host->ended());

    // ITS OWN WINDOW'S CLOSE ends it.
    run.light.queued.close = true;
    run.pump();
    CHECK(run.host->ended());
    CHECK(run.host->exit_code() == 0);
    CHECK(has(run.said.str(), "ending: its window was closed"));
}

TEST_CASE("an app.json the app's weave cannot read is said on the app's own line at the window's foot, and nothing runs") {
    AppFolder folder("unreadable");
    std::ofstream(folder.dir / app::kAppFileName, std::ios::binary | std::ios::trunc) << "{}";
    Run run(folder);
    REQUIRE(run.open());
    run.host->begin();
    for (const char* stem : {"zengine-operators-basic", "zengine-app-weave", "zengine-graphics"})
        run.command(std::string("authority trust ") + stem);
    run.mount_window();
    const std::string shown = run.shown();
    CHECK(has(shown, "[app] app.json is not an app"));
    CHECK_FALSE(has(shown, "skin"));
    CHECK(run.light.area("tally.panel.view") == nullptr);
    const LightLog::Area* line = run.light.area(app::kAppRole);
    REQUIRE(line != nullptr);
    CHECK(line->y > 0);
}

TEST_CASE("the app's window takes the title and the size its app.json says, once a window, and the app's line stands at its new foot") {
    // MUTATION: the app's weave saying nothing of its window -- it stays 1600 by 900, untitled.
    AppFolder folder("shaped");
    Run run(folder);
    REQUIRE(run.open());
    run.host->begin();
    for (const char* stem : {"zengine-operators-basic", "zengine-app-weave", "zengine-graphics"})
        run.command(std::string("authority trust ") + stem);
    run.mount_window();
    const app::ReadApp file = app::read_app((folder.dir / app::kAppFileName).generic_string());
    REQUIRE(file.ok);
    REQUIRE(run.light.shaped.size() == 1);
    CHECK(run.light.shaped[0].first == file.app.window.title);
    CHECK(run.light.shaped[0].second == gw::PixelSize{file.app.window.width, file.app.window.height});
    CHECK(run.light.size == gw::PixelSize{file.app.window.width, file.app.window.height});
    // THE APP'S LINE at the shaped window's foot, and the window not grown back past it.
    const LightLog::Area* line = run.light.area(app::kAppRole);
    REQUIRE(line != nullptr);
    CHECK(line->y + line->h == file.app.window.height);
    CHECK(run.light.grown.empty());
    // ...and the next beats shape it no more.
    run.pump();
    CHECK(run.light.shaped.size() == 1);
}
