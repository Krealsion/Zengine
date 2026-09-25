// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-workshop: the Workshop host's `main`. It resolves the maker's files, composes the
// process's offices and grants, realizes the authored load plan and runs the bus.
// Workshop law: agents/workshop/project.md (+2 registers; agents/workshop.md routes)

#include "arrangement.hpp"
#include "authoring.hpp"
#include "pane_doors.hpp"
#include "host_sources.hpp"
#include "sample_door.hpp"
#include "load_execute.hpp"
#include "load_persist.hpp"
#include "path_admission.hpp"
#include "recipe_persist.hpp"
#include "recipes.hpp"
#include "staging.hpp"
#include "user_paths.hpp"
#include "flow-host/runtime.hpp"
#include "weave.hpp"
#include <zen/host/grant_wiring.hpp>
#include "host_pump.hpp"      // the host's turn of the bus, and the pump seam it owns
#include "opening.hpp"        // the opening manager this host mounts
#include "editor_switch.hpp"  // the editor switch this host mounts
#include "pane_migration.hpp" // the retired references this host converts, one of which it manages
#include "provenance.hpp"     // what stands behind an office's running code, from three owners
#include "guest_door.hpp"     // the other hosts this Workshop admits, and the door they come through
#include "guests.hpp"         // ...and the file that says who they are
#include "admission.hpp"

#include "builder/runner.hpp"
#include "builder/vocabulary.hpp"
#include "builder/weave.hpp"
#include "operator/catalog.hpp"
#include "operator/host_surface.hpp"
#include "surface/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/bridge/channel.hpp>
#include "workshop/grant.hpp"
#include <iostream>
#include <zen/history/dump.hpp>
#include <zen/history/logger.hpp>
#include <zen/history/recorder.hpp>
#include <zen/host/terminal_wiring.hpp>
#include <zen/kernel/admission.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/terminal/session.hpp>
#include <zen/terminal/vocabulary.hpp>
#include <zen/weave.hpp>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace zengine::workshop;
namespace builder = zengine::builder;
namespace op = zengine::op;
namespace surface = zengine::surface;
namespace timer = zengine::timer;

/// Mount an in-process weave into an office with the grant the host chose: `mount_granted` plus
/// the role binding, so a grant can name whoever holds the office rather than a WeaveId.
template <class Weave, class... Args>
loom::WeaveId mount_in_office(loom::Switchboard& bus, loom::Grant grant, const char* office,
                              Args&&... args) {
    auto weave = std::make_unique<Weave>(std::forward<Args>(args)...);
    Weave* raw = weave.get();
    const loom::WeaveId id =
        bus.register_weave(std::move(weave), std::move(grant), std::string(office));
    raw->zen_set_self(id);
    return id;
}

// WL-PROJ-15 -- agents/workshop/project.md
std::string exe_dir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    const DWORD n = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return ".";
    }
    std::string path(buf, n);
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? "." : path.substr(0, slash);
#else
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return ".";
    }
    buf[n] = '\0';
    std::string path(buf);
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
#endif
}

} // namespace

// WL-SESSION-01, WL-SESSION-02 -- agents/workshop/session.md
struct Arguments {
    bool ok = true;
    std::string complaint;
    /// An object document a launch still names (`--document`), or empty: read so an old launch
    /// line starts, then said once and left alone (WL-DOC-22).
    std::string document;
    /// The setup file: the arrangement a maker saved, a PROJECT file resolved like the others.
    std::string setup = zengine::workshop::kDefaultSetupFileName;
    /// The pane-definition file, a project file `main()` resolves against the project directory.
    std::string pane = zengine::workshop::pane_definition_persist::kDefaultPaneFileName;
    /// The last-session file, written by nobody's gesture.
    // WL-SESSION-02 -- agents/workshop/session.md
    std::string session;
    /// The maker's keymap file, read at startup.
    // WL-KEY-07 -- agents/workshop/keyboard.md
    std::string keymap;
    /// The maker's presentation preferences, written when they state one (the
    /// pane-title toggle). Empty means "not explicitly chosen", for `session`'s reason.
    std::string prefs;
    /// The maker's location marks, written when they mark or unmark a place.
    /// Empty means "not explicitly chosen", for `session`'s reason.
    std::string marks;
    /// This run touches none of the maker's ordinary per-user configuration or session
    /// state. Explicit paths above still win over it.
    bool isolated = false;
    /// Empty means the plan shipped beside this executable, resolved by `main()`: a bare name would
    /// resolve against wherever the maker happened to launch.
    std::string load_plan;
    /// The authored build recipes, whose file may be absent; empty means the one beside this
    /// executable.
    // WL-PROJ-04 -- agents/workshop/project.md
    std::string recipes;
    std::string log;  ///< empty = keep nothing durably
    std::string read_log; ///< render an existing log without starting Workshop
    std::string dump; ///< empty = write no snapshot of working memory at exit
    bool log_refusals = false; ///< explicit diagnostic retention, never additional authority
    bool demo_history = false; ///< bounded delivery metadata for a diagnostic demo run
    /// THE GUESTS FILE: who may connect to this Workshop from another host, and what each may
    /// then say (workshop/guests.hpp). Empty = no listener, so connecting is impossible.
    std::string guests;
};

Arguments parse_arguments(int argc, char** argv) {
    Arguments args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--log-refusals") { args.log_refusals = true; continue; }
        if (arg == "--demo-history") { args.demo_history = true; continue; }
        if (arg == "--isolated") {
            // The one flag that takes no path: a whole-run policy, not a file.
            args.isolated = true;
            continue;
        }
        if (arg == "--document" || arg == "--setup" || arg == "--pane" ||
            arg == "--session" || arg == "--keymap" || arg == "--prefs" || arg == "--marks" ||
            arg == "--load-plan" || arg == "--recipes" || arg == "--log" ||
            arg == "--dump" || arg == "--guests" || arg == "--read-log") {
            if (i + 1 >= argc) {
                args.ok = false;
                args.complaint = arg + " needs a path";
                return args;
            }
            const std::string value = argv[++i];
            if (arg == "--load-plan") {
                // Refused here: empty is this field's way of saying "the one beside the
                // executable".
                if (value.empty()) {
                    args.ok = false;
                    args.complaint = "--load-plan needs a path";
                    return args;
                }
                args.load_plan = value;
            } else if (arg == "--recipes") {
                // Refused here for `--load-plan`'s reason.
                if (value.empty()) {
                    args.ok = false;
                    args.complaint = "--recipes needs a path";
                    return args;
                }
                args.recipes = value;
            } else if (arg == "--session" || arg == "--keymap" || arg == "--prefs" ||
                       arg == "--marks") {
                // Refused here: empty means "the per-user root decides"; turning the file off is
                // `--isolated`'s job.
                if (value.empty()) {
                    args.ok = false;
                    args.complaint = arg + " needs a path";
                    return args;
                }
                if (arg == "--session") {
                    args.session = value;
                } else if (arg == "--keymap") {
                    args.keymap = value;
                } else if (arg == "--marks") {
                    args.marks = value;
                } else {
                    args.prefs = value;
                }
            } else if (arg == "--log") {
                args.log = value;
            } else if (arg == "--read-log") {
                if (value.empty()) {
                    args.ok = false;
                    args.complaint = "--read-log needs a path";
                    return args;
                }
                args.read_log = value;
            } else if (arg == "--dump") {
                args.dump = value;
            } else if (arg == "--guests") {
                if (value.empty()) {
                    args.ok = false;
                    args.complaint = "--guests needs a path";
                    return args;
                }
                args.guests = value;
            } else if (arg == "--setup") {
                args.setup = value;
            } else if (arg == "--pane") {
                args.pane = value;
            } else {
                // `--document` is retired, and read so it can be said; empty is still a complaint.
                if (value.empty()) {
                    args.ok = false;
                    args.complaint = "--document needs a path";
                    return args;
                }
                args.document = value;
            }
            continue;
        }
        args.ok = false;
        args.complaint = "unknown argument `" + arg + "`";
        return args;
    }
    if (args.setup.empty()) {
        args.ok = false;
        args.complaint = "--setup needs a path";
    } else if (args.pane.empty()) {
        args.ok = false;
        args.complaint = "--pane needs a path";
    }
    return args;
}

int main(int argc, char** argv) {
#if defined(_WIN32)
    // A bad artifact is refused in words, never a modal dialog: Windows otherwise raises a hard
    // error the process waits on for a file that is not a valid image. Children inherit the mode,
    // so the Builder's runner is covered.
    ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
#endif
    const Arguments args = parse_arguments(argc, argv);
    if (!args.ok) {
        std::printf("zengine-workshop - %s\n"
                    "usage: zengine-workshop [--setup <path>] [--pane <path>]\n"
                    "                        [--session <path>] [--keymap <path>]\n"
                    "                        [--prefs <path>] [--marks <path>]\n"
                    "                        [--isolated]\n"
                    "                        [--load-plan <path>]\n"
                    "                        [--recipes <path>]\n"
                    "                        [--log <path>] [--log-refusals] [--dump <path>] [--demo-history]\n"
                    "                        [--guests <file>]\n"
                    "       zengine-workshop --read-log <path>\n"
                    "the graphical Workshop is the second plan shipped beside this binary:\n"
                    "  zengine-workshop --load-plan <workshop dir>/%s\n",
                    args.complaint.c_str(), load_persist::kGraphicalLoadPlanName);
        return 2;
    }

    if (!args.read_log.empty()) {
        std::vector<loom::LogRecord> records;
        std::string complaint;
        if (!loom::Logger::read(args.read_log, &records, &complaint)) {
            std::fprintf(stderr, "zengine-workshop - log: %s\n", complaint.c_str());
            return 3;
        }
        loom::dump_log(records, std::cout);
        return 0;
    }
    if (args.log_refusals && args.log.empty()) {
        std::fprintf(stderr, "zengine-workshop - --log-refusals needs --log <path>\n");
        return 2;
    }

    // ---- The recipes this Workshop means, declared first ----------------------------------------
    // Empty until read below. Declared above the HostContext, the bus and the Kernel so reverse
    // destruction outlives every reader: do not move it below the bus.
    CurrentRecipes current_recipes;

    // Where the host's own files are, resolved before anything is said about them.
    HostContext host;
    host.dir = exe_dir();
    // And where the maker is standing: the project is the launch directory (user_paths.hpp),
    // captured once. A directory the platform will not report and one this application cannot
    // say are the same absence, empty, which the banner states (path_admission.hpp).
    host.project_dir = launch_project_dir();
    // The one pane whose presentation this host commits jointly with its document, spelled
    // through the conversion table, the one host-side file that names it.
    host.managed_pane = PaneRef{pane_migration::kEditorProvider, pane_migration::kEditorPane};
    // An object document this launch names, or one under the retired default name in the project:
    // said once at startup and never read (WL-DOC-22).
    if (!args.document.empty()) {
        host.retired_document = args.document;
    } else if (!host.project_dir.empty()) {
        std::error_code ec;
        const std::string old = zengine::workshop::persist::resolved_against(
            host.project_dir, zengine::workshop::persist::kRetiredDocumentName);
        if (std::filesystem::is_regular_file(old, ec)) {
            host.retired_document = old;
        }
    }
    host.setup_path = args.setup;
    // The pane-definition file, resolved against the project once. A project this build cannot
    // carry leaves a relative spelling nowhere to stand, and the pane file is off for the run.
    {
        const std::filesystem::path spelled(args.pane);
        if (spelled.is_absolute() || !host.project_dir.empty()) {
            host.pane_path = persist::resolved_against(host.project_dir, args.pane);
        }
    }

    // ---- The maker's own files, by the pinned precedence ----------------------------------------
    // Explicit path, then isolation, then the per-user default (`user_paths.hpp` owns the rule).
    // An environment with no root resolves to no file, said below -- never a quiet fall-back.
    const user_paths::Environment env = user_paths::host_environment();
    const std::string config_root = user_paths::config_root(env);
    const std::string state_root = user_paths::state_root(env);
    host.keymap_path = user_paths::resolve_durable_path(
        args.keymap, args.isolated, config_root, keymap_persist::kDefaultKeymapFileName);
    host.prefs_path = user_paths::resolve_durable_path(
        args.prefs, args.isolated, config_root, prefs_persist::kDefaultPrefsFileName);
    host.session_path = user_paths::resolve_durable_path(
        args.session, args.isolated, state_root,
        session_persist::kDefaultSessionFileName);
    // The marks ride the machine-local root: a mark is an absolute path, true of these disks only.
    // The Files pane learns this path by asking `zengine.project`.
    const std::string marks_path = user_paths::resolve_durable_path(
        args.marks, args.isolated, state_root, kDefaultMarksFileName);

    // ---- ...and the one-time legacy import, for defaulted files only --------------------------
    // The rule is `user_paths.hpp`'s. An import is an event, said on the notice row; a shadowed
    // file is a standing condition until the maker deletes it.
    std::string transition;
    const auto note_transition = [&](const user_paths::LegacyImport& did, const char* what) {
        if (did.note.empty()) {
            return;
        }
        if (did.shadowed) {
            host.standing_conditions.push_back(
                Condition{std::string(kLegacyShadowedKeyPrefix) + what,
                          std::string("older local ") + what + " file is not read",
                          did.note, surface::role::kAccent, std::string()});
            return;
        }
        if (!transition.empty()) {
            transition += "; ";
        }
        transition += did.note;
    };
    if (args.keymap.empty() && !args.isolated) {
        note_transition(user_paths::import_legacy_file(
                            host.keymap_path, keymap_persist::kDefaultKeymapFileName, "keymap"),
                        "keymap");
    }
    if (args.session.empty() && !args.isolated) {
        note_transition(user_paths::import_legacy_file(
                            host.session_path, session_persist::kDefaultSessionFileName,
                            "session"),
                        "session");
    }
    host.transition_note = transition;

    // The plan in force: `--load-plan`, else the project plan, else the shipped default -- one
    // rule, `load_persist::plan_in_force`, handed the same probe as the recipes' twin rule.
    const auto present = [](const std::string& path) {
        std::error_code ec;
        return std::filesystem::exists(std::filesystem::path(path), ec) && !ec;
    };
    const std::string plan_path =
        load_persist::plan_in_force(args.load_plan, host.project_dir, host.dir, present);

    // The honest line: this host isolates nothing. (`--isolated` is about the maker's files.)
    std::printf("zengine-workshop - containment: %s\n", loom::Kernel::containment_note());
    std::printf("zengine-workshop - document: retired with the object canvas%s%s\n",
                host.retired_document.empty() ? "" : " -- left as it is: ",
                host.retired_document.c_str());
    std::printf("zengine-workshop - setup: %s\n", args.setup.c_str());
    // The pane file as resolved, or its absence and its cause.
    std::printf("zengine-workshop - pane: %s\n",
                host.pane_path.empty()
                    ? "none (no project directory to resolve it under -- an absolute --pane "
                      "<path> names one)"
                    : host.pane_path.c_str());
    // The project, said once. Its absence has two causes and one sentence, true of both.
    std::printf("zengine-workshop - project: %s\n",
                host.project_dir.empty()
                    ? "none (this system gave no working directory this Workshop can write "
                      "down -- Project Files and relative recipe sources are refused)"
                    : host.project_dir.c_str());
    const auto path_or_absence = [&args](const std::string& path) {
        if (!path.empty()) {
            return path;
        }
        return std::string(args.isolated ? "none (--isolated)"
                                         : "none (no per-user root in this environment)");
    };
    std::printf("zengine-workshop - last session: %s (restored at startup, written on quit)\n",
                path_or_absence(host.session_path).c_str());
    std::printf("zengine-workshop - keymap: %s\n", path_or_absence(host.keymap_path).c_str());
    std::printf("zengine-workshop - prefs: %s\n", path_or_absence(host.prefs_path).c_str());
    // The marks, said on the same terms as the other files.
    std::printf("zengine-workshop - marks: %s\n", path_or_absence(marks_path).c_str());
    if (!transition.empty()) {
        std::printf("zengine-workshop - %s\n", transition.c_str());
    }
    // The standing conditions too, in the words the weave will show them in.
    for (const Condition& standing : host.standing_conditions) {
        std::printf("zengine-workshop - %s\n", standing.detail.c_str());
    }
    std::printf("zengine-workshop - load plan: %s\n", plan_path.c_str());
    std::fflush(stdout);

    // ---- The authored load plan, read before anything is built ----------------------------------
    // A bad plan is refused before this process has a bus, a Kernel or a catalog. There is no
    // compiled-in fallback: a manufactured arrangement would not be the maker's project.
    const load_persist::LoadedPlan read_plan = load_persist::load_file(plan_path);
    if (!read_plan.outcome.accepted) {
        std::printf("zengine-workshop - load plan refused: %s\n"
                    "zengine-workshop - nothing was mounted and nothing was loaded.\n",
                    read_plan.outcome.refusal.c_str());
        return 3;
    }
    std::printf("zengine-workshop - load plan: %zu artifact(s) declared\n",
                read_plan.plan.artifacts.size());
    std::fflush(stdout);

    // ---- The authored build recipes, read in the same breath ------------------------------------
    // Project intent like the plan, refused before anything is built, but a different document
    // (workshop/recipe_persist.hpp). The launch installs them through the maker's own door
    // (`install_recipes`), wired before the file is read, so it cannot complete them differently.
    host.use_recipes = [&host, &current_recipes](const std::string& path) {
        HostContext::RecipeSwap done;
        const Written read = install_recipes(current_recipes, path, host.dir,
                                             host.project_dir, &HostContext::so_in);
        done.accepted = read.accepted;
        done.refusal = read.refusal;
        // What is in force, asked of the owner after the attempt -- never echoed from the
        // candidate, so a refusal names the catalog still in force.
        done.path = current_recipes.source();
        done.recipes = current_recipes.all().size();
        return done;
    };
    {
        // The plan's twin (WL-PROJ-15): an absent shipped default is "nothing to build"; an absent
        // named file is a refusal.
        const std::string recipe_path = recipe_persist::recipes_in_force(
            args.recipes, host.project_dir, host.dir, present);
        if (args.recipes.empty() && !present(recipe_path)) {
            std::printf("zengine-workshop - build recipes: none (%s is not there, so this "
                        "Workshop can build nothing)\n",
                        recipe_path.c_str());
        } else {
            const HostContext::RecipeSwap read = host.use_recipes(recipe_path);
            if (!read.accepted) {
                std::printf("zengine-workshop - build recipes refused: %s\n"
                            "zengine-workshop - nothing was mounted and nothing was loaded.\n",
                            read.refusal.c_str());
                return 5;
            }
            // The path and the count come back from the owner.
            std::printf("zengine-workshop - build recipes: %s (%zu)\n", read.path.c_str(),
                        read.recipes);
        }
    }
    // ---- The edit-source seam (`HostContext::recipe_source`) ------------------------------------
    // The host holds the completed recipes, so it answers which source a recipe names, asking the
    // owner at the gesture through the one rule Edit Code reads too.
    host.recipe_source = [&current_recipes](const std::string& id) {
        return provenance::recipe_source_of(current_recipes.all(), id);
    };
    std::fflush(stdout);

    loom::Switchboard bus;

    // ---- What this host remembers ---------------------------------------------------------------
    // First, before the Kernel and any weave, so the first record is the first fact. The recorder
    // is the host's own lens, not a participant; its policy is sized by this application's traffic.
    loom::RecorderPolicy history_policy = loom::default_policy();
    if (args.demo_history) {
        history_policy.recent_capacity = 100000;
        history_policy.default_retain_payload = false;
    }
    // The beats: the last one, no recent context, no bytes -- findable, never flooding the window.
    for (const char* shape : {timer::TimerFired::zen_name, timer::Drive::zen_name}) {
        history_policy.rules.push_back(loom::RetentionRule{
            std::string(shape), /*last_n=*/1, /*in_recent=*/false, /*retain_payload=*/false});
    }
    // The pictures: remembered as events, not bytes.
    history_policy.rules.push_back(loom::RetentionRule{
        std::string(surface::SurfaceCanvas::zen_name), 1, true, false});
    // The build: rare, bursty and what a maker looks for -- a deep slot of its own, and its place
    // in recent context.
    for (const char* shape :
         {builder::BuildStarted::zen_name, builder::BuildOutput::zen_name,
          builder::BuildFinished::zen_name, builder::BuildNotStarted::zen_name,
          builder::RunBuild::zen_name, builder::BuildRequested::zen_name}) {
        history_policy.rules.push_back(loom::RetentionRule{std::string(shape), 512, true, true});
    }
    // Applied as a change, so the first thing this run remembers is its policy.
    loom::Recorder history(bus);
    history.apply_policy(std::move(history_policy));

    // ---- ...and what it chooses not to forget ---------------------------------------------------
    // The Logger keeps a few facts for good: Loom's defaults, a build that finished or never
    // started, and what the project made of an offered artifact -- the one place a long refusal is
    // kept whole (docs/workshop/builder.md). Not `BuildStatus`: it is republished on every chunk.
    loom::LoggerSelection log_selection = loom::default_selection();
    log_selection.log_refusals = args.log_refusals;
    for (const char* shape :
         {builder::BuildFinished::zen_name, builder::BuildNotStarted::zen_name,
          builder::ArtifactRealized::zen_name}) {
        log_selection.shapes.push_back(loom::LogRule{std::string(shape), /*cap=*/0});
    }
    loom::Logger journal(bus, std::move(log_selection));
    if (!args.log.empty()) {
        std::string complaint;
        if (!journal.open(args.log, &complaint)) {
            std::printf("zengine-workshop - log: %s\n", complaint.c_str());
        } else {
            std::printf("zengine-workshop - log: %s (durable, selected facts only)\n",
                        args.log.c_str());
            journal.info("zengine.workshop", "session started: setup " + args.setup +
                                                 ", load plan " + plan_path);
        }
    } else {
        std::printf("zengine-workshop - log: nothing durable (--log <path> to keep one)\n");
    }
    if (!args.dump.empty()) {
        std::printf("zengine-workshop - dump at exit: %s (what memory still held)\n",
                    args.dump.c_str());
    }
    std::fflush(stdout);

    // ---- Where this process's semantic powers come from -----------------------------------------
    // Providers supply them; this host owns only which contribution is in force, and names no
    // artifact, rule or mount order -- the plan does, and `load_execute.hpp` performs it. The
    // catalog is a local of `main`, never a singleton, declared before the Kernel so the Kernel and
    // its artifacts are destroyed first; the plan executor, declared after, must not outlive them.
    host.request_stop = [&bus] { bus.stop(); };

    op::Catalog operators;

    // ---- ...and the one thing a host may put in its own catalog --------------------------------
    // The host may describe itself, never invent provider power: two zero-input Sources over facts
    // it owns, through a door that refuses anything else (workshop/host_sources.hpp). Mounted
    // before the plan runs, so a provider supplying one of them meets the collision law.
    const op::MountReport exposed =
        mount_host_sources(operators, host_sources(host.project_dir, current_recipes));
    if (!exposed) {
        std::printf("zengine-workshop - host sources refused: %s\n"
                    "zengine-workshop - nothing was mounted and nothing was loaded.\n",
                    exposed.reason.c_str());
        return 6;
    }

    // ---- ...and the one reading a durable owner takes off it ------------------------------------
    // An older session file is translated by whatever conversion this run's plan mounted: the
    // persistence owner may look in the catalog, and only look. Set now, read long after.
    host.conversions = &operators;

    op::OperatorHostSurface operator_host(operators);

    // The host decides what each loaded office may say. Inventory keeps its bounded
    // grant; the broader plan-authority policy remains explicit in admission.hpp.
    loom::Kernel kernel(bus, zengine::workshop::artifact_admission());
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    // ---- The terminal participant Workshop presents ---------------------------------------------
    // An ordinary weave on this process's one bus. The host chooses what it may know and what it
    // may say; whether it may say something is the Kernel's answer.
    loom::TerminalVocabulary terminal_vocab;
    terminal_vocab
        // The one ordinary verb: a line of text on the screen's score slot.
        .knows(loom::schema_of<surface::SurfaceText>())
        // ...and one it must never be able to use: knowing it makes the Kernel's refusal real.
        .knows(loom::schema_of<surface::SurfaceCanvas>())
        .accepts(loom::schema_of<loom::Ack>())
        .accepts(loom::schema_of<loom::Result>())
        .accepts(loom::schema_of<loom::Refused>());

    // Its baseline is one rule, scoped to an office: SurfaceText to whoever holds `zengine.skin`.
    // Being a terminal confers nothing else.
    loom::Grant terminal_grant;
    terminal_grant.allow_to_role(surface::SurfaceText::zen_name,
                                 surface::SurfaceText::zen_version, surface::kSkinRole);
    // ...and the four questions a maker asks the editor switch, to that office only (WL-SWITCH-07).
    let_terminal_switch_editors(terminal_vocab, terminal_grant);
    const loom::MountedTerminal terminal = loom::host_mount_terminal(
        bus, std::make_unique<loom::TerminalSession>("workshop", std::move(terminal_vocab)),
        std::move(terminal_grant));
    // Non-owning, handed down the way `request_stop` is. The bus owns the
    // participant; Workshop's weave holds a pointer and inherits nothing from it.
    host.terminal = terminal.session;
    // The boot line names the identity the Terminal pane speaks as.
    std::printf("zengine-workshop - terminal: weave #%s (presented by the Terminal pane)\n",
                std::to_string(terminal.id.value).c_str());
    // Flushed: a killed process loses its buffer, and this is the line read while killing it.
    std::fflush(stdout);

    // ---- The Builder tool, and the one thing that may start a process ---------------------------
    // Two weaves, two offices, two grants. The runner is the only weave that starts a process: it
    // holds the catalog and a program fixed at configure time that no message can reach, reports
    // to the Builder office, and may ask the Timer for a beat. The tool holds names only and orders
    // the runner. The grants bound what these weaves may say, not what they touch: no containment.
    loom::Grant run_builds;
    run_builds.allow_to_role(builder::BuildStarted::zen_name, builder::BuildStarted::zen_version,
                             builder::kBuilderRole);
    run_builds.allow_to_role(builder::BuildOutput::zen_name, builder::BuildOutput::zen_version,
                             builder::kBuilderRole);
    run_builds.allow_to_role(builder::BuildFinished::zen_name,
                             builder::BuildFinished::zen_version, builder::kBuilderRole);
    run_builds.allow_to_role(builder::BuildNotStarted::zen_name,
                             builder::BuildNotStarted::zen_version, builder::kBuilderRole);
    run_builds.allow_to_role(timer::EnsureTimer::zen_name, timer::EnsureTimer::zen_version,
                             timer::kTimerRole);
    run_builds.allow_to_role(timer::CancelTimer::zen_name, timer::CancelTimer::zen_version,
                             timer::kTimerRole);
    const loom::WeaveId runner = mount_in_office<builder::BuildRunnerWeave>(
        bus, std::move(run_builds), builder::kBuildRunnerRole, current_recipes.all(),
        std::string(ZENGINE_BUILDER_CMAKE));

    loom::Grant order_builds;
    order_builds.allow_to_role(builder::RunBuild::zen_name, builder::RunBuild::zen_version,
                               builder::kBuildRunnerRole);
    order_builds.allow_to_any(builder::BuildStatus::zen_name, builder::BuildStatus::zen_version);
    // ...and what became of each ask it heard (`BuildAsked`), for whoever follows one ask.
    order_builds.allow_to_any(builder::BuildAsked::zen_name, builder::BuildAsked::zen_version);
    order_builds.allow_to_any(builder::RecipeCatalog::zen_name,
                              builder::RecipeCatalog::zen_version);
    // `OfferArtifact`, an offer and not an order: at its widest it realizes the one artifact
    // realization is stopped at; every rule and refusal is the owner's, and its path is ignored.
    // The one dangerous grant is still `zen.LoadWeave -> manager`, held by the plan booter.
    order_builds.allow_to_any(builder::OfferArtifact::zen_name,
                              builder::OfferArtifact::zen_version);
    // ...and its answer to "what did operation #n say": one bounded page, reaching no runner, file
    // or process (WL-OUT-02).
    order_builds.allow_to_any(builder::BuildOutputSaid::zen_name,
                              builder::BuildOutputSaid::zen_version);
    const loom::WeaveId builder_tool = mount_in_office<builder::BuilderWeave>(
        bus, std::move(order_builds), builder::kBuilderRole, current_recipes.views(),
        current_recipes.source());

    // What a button will run is printed before it is pressed: every recipe and the artifact it
    // produces. The Builder's pane opens from the Pane Manager, whose key is the desktop's.
    std::printf("zengine-workshop - builder: weave #%s holds %zu recipe(s) (open its pane from "
                "the Pane Manager)\n",
                std::to_string(builder_tool.value).c_str(), current_recipes.views().size());
    std::printf("zengine-workshop - build runner: weave #%s builds with `%s`\n",
                std::to_string(runner.value).c_str(), ZENGINE_BUILDER_CMAKE);
    for (const builder::RecipeView& r : current_recipes.views()) {
        std::printf("zengine-workshop - recipe: %s -> %s\n", r.id.c_str(), r.path.c_str());
    }
    std::fflush(stdout);

    // Workshop's own grant (`workshop_grant`, workshop/grant.cpp): its screen, its answers, and the
    // pane sentences to offices resolved at runtime -- no lifecycle, no Manager. It is mounted in
    // `kWorkshopProvider`, the built-in rows' own provider string rather than a credential, so a
    // provider can verify its asks; holding the office is not a super-grant (Loom MSG-07).
    host.role_holder = [&bus](std::string_view role) { return bus.role_holder(role); };
    host.input_authority = [&bus](loom::WeaveId actor) {
        return bus.alive(actor)
            ? loom::host_grant_authority(bus, actor, loom::LiveAuthority::nothing())
            : loom::GrantAuthority{};
    };
    host.holder_accepts = [&bus](std::string_view role, const loom::Schema& shape) {
        return holder_accepts_on(bus, role, shape);
    };
    host.destinations = [&bus, &host] {
        return bus_destinations(bus, host.terminal != nullptr ? host.terminal->id() : loom::WeaveId{});
    };
    auto speak = workshop_grant();
    const loom::WeaveId workshop_id =
        mount_in_office<WorkshopWeave>(bus, std::move(speak), kWorkshopProvider, host);

    // ---- The guest door: other hosts, admitted deliberately (workshop/guest_door.hpp) -----------
    // Only with a `--guests` file: without one this Workshop cannot be connected to, the honest
    // default for a bridge with no transport security. The file's rows are the policy. Mounted
    // before the plan runs, so the Timer's beat finds it.
    std::string guests_listen;
    if (!args.guests.empty()) {
        guests::GuestsFile file;
        std::string complaint;
        if (!guests::read_guests_file(args.guests, &file, &complaint)) {
            std::printf("zengine-workshop - guests: %s\n"
                        "zengine-workshop - nothing was mounted and nothing was loaded.\n",
                        complaint.c_str());
            return 7;
        }
        std::string listen_host;
        std::uint16_t listen_port = 0;
        (void)guests::split_listen(file.listen, &listen_host, &listen_port);
        if (!loom::bridge_net_init(&complaint)) {
            std::printf("zengine-workshop - guests: the network could not be initialised: %s\n",
                        complaint.c_str());
            return 7;
        }
        const loom::socket_t listener = loom::bridge_listen_tcp(listen_port, &complaint);
        if (listener == loom::kInvalidSocket) {
            std::printf("zengine-workshop - guests: cannot listen on %s: %s\n",
                        file.listen.c_str(), complaint.c_str());
            return 7;
        }
        guests_listen = "127.0.0.1:" + std::to_string(loom::bridge_socket_port(listener));
        if (!file.port_file.empty()) {
            // The port the OS chose, where the file said, for a script that launched with port 0.
            std::ofstream port_out(file.port_file, std::ios::trunc);
            port_out << loom::bridge_socket_port(listener) << '\n';
        }
        auto door = std::make_unique<GuestDoor>(bus, listener, guests_listen,
                                                guests::admission_of(file));
        GuestDoor* raw_door = door.get();
        const loom::WeaveId door_id = bus.register_weave(std::move(door), guest_door_grant(),
                                                         std::string(kGuestsRole));
        raw_door->zen_set_self(door_id);
        std::printf("zengine-workshop - guests: listening on %s for %zu guest(s) named in %s "
                    "(door: weave #%s; the Connections pane lists them)\n",
                    guests_listen.c_str(), file.rows.size(), args.guests.c_str(),
                    std::to_string(door_id.value).c_str());
        // ---- THE OBSERVATION RELAY, beside the door (workshop/guest_door.hpp says how) --------
        // What a guest may OBSERVE is its row's `observe` list and nothing else. No maker control
        // calls the relay's `revoke` yet -- like `decide` for an "ask" row, it is a host seam.
        (void)mount_observation(bus, *raw_door, file);
        std::size_t observers = 0;
        for (const guests::GuestRow& row : file.rows) {
            observers += row.observe.empty() ? 0u : 1u;
        }
        std::printf("zengine-workshop - observe: relay at %s (weave #%s); %zu guest(s) may "
                    "observe what their rows list\n",
                    loom::observe::kObserveRole,
                    std::to_string(bus.role_holder(loom::observe::kObserveRole).value).c_str(),
                    observers);
    } else {
        std::printf("zengine-workshop - guests: none (this Workshop listens for no other host; "
                    "--guests <file> to admit one)\n");
    }
    std::fflush(stdout);

    // ---- The quit's undeliverable questions (WL-SESSION-19) -------------------------------------
    // A refusal notice never reaches a publication's author, so this host watches its tap for the
    // quit's refused deliveries and writes them where Workshop reads (`quit_delivery.hpp`).
    // Declared after the bus and the `HostContext`, so it goes first.
    const QuitDeliveryWatch quit_watch(bus, workshop_id, host.undelivered_quits);

    // ---- The opening manager (WL-OPEN-01, WL-OPEN-08) -------------------------------------------
    // Mounted in its own office with exactly the conversation it carries. Its joint authority is
    // minted by this host for its own id: one replaced at that address commits nothing it did not
    // begin.
    loom::Grant arrange_openings;
    arrange_openings.allow_to_role(PresentationTrialRequested::zen_name,
                                   PresentationTrialRequested::zen_version, kWorkshopProvider);
    arrange_openings.allow_to_role(PresentationAdmitRequested::zen_name,
                                   PresentationAdmitRequested::zen_version, kWorkshopProvider);
    arrange_openings.allow_to_role(ManagedOpenProgress::zen_name, ManagedOpenProgress::zen_version,
                                   kWorkshopProvider);
    arrange_openings.allow_to_role(ManagedOpenSettled::zen_name, ManagedOpenSettled::zen_version,
                                   kWorkshopProvider);
    arrange_openings.allow_to_role(ManagedOpenSettled::zen_name, ManagedOpenSettled::zen_version,
                                   kEditorRole);
    // ...and the `apply` word to the Editor too: the delivery that shows it its published claim.
    arrange_openings.allow_to_role(ManagedOpenProgress::zen_name, ManagedOpenProgress::zen_version,
                                   kEditorRole);
    arrange_openings.allow_to_role(PrepareSourceRequested::zen_name,
                                   PrepareSourceRequested::zen_version, kEditorRole);
    arrange_openings.allow_to_any(SourceOpened::zen_name, SourceOpened::zen_version);
    // ...and it answers pokes: the operation, its stage and whom it waits on are readable.
    loom::allow_poke_answers(arrange_openings);
    {
        auto opener = std::make_unique<OpeningManager>(std::string(kEditorRole),
                                                       std::string(kWorkshopProvider),
                                                       host.managed_pane);
        OpeningManager* raw = opener.get();
        const loom::WeaveId opening = bus.register_weave(
            std::move(opener), std::move(arrange_openings), std::string(kOpeningRole));
        raw->zen_set_self(opening);
        raw->set_authority(bus.mint_joint_authority(
            opening, {std::string(kEditorRole), std::string(kWorkshopProvider)}));
    }

    // ---- The plan, performed --------------------------------------------------------------------
    // The plan booter's reach -- the Manager, target-scoped -- is the dangerous grant in this
    // process, and the host writes it: an executor that granted itself would be orchestration
    // minting kernel reach. Mounted by hand, because the owner wires the participant, not its id.
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    // ...and the Manager's other lifecycle op on the same terms: a reload in place, so a maker need
    // not restart to see an edit. A tripwire reads these two lines and refuses a third.
    operate.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
    // ...and two observations it may publish: what the project made of a maker's build, and
    // whether a promotion landed. Observations, not powers.
    operate.allow_to_any(builder::ArtifactRealized::zen_name,
                         builder::ArtifactRealized::zen_version);
    operate.allow_to_any(builder::ArtifactPromoted::zen_name,
                         builder::ArtifactPromoted::zen_version);
    load::BootAnswers answers;
    auto speaker = std::make_unique<load::PlanBooter>(answers);
    load::PlanBooter& voice = *speaker;
    const loom::WeaveId booter = bus.register_weave(std::move(speaker), std::move(operate));
    voice.zen_set_self(booter);

    // ---- What realizes the project --------------------------------------------------------------
    // Declared after the Kernel, the other half of the catalog's lifetime claim: it holds provider
    // identities and an operator handoff across turns, so it must not outlive the artifacts, and
    // it is a local rather than a weave. The host hands it the one stem-to-file rule, and keeps
    // its own failure policy (the lambda).
    bool project_refused = false;
    staging::Host staging_host{host.dir, &current_recipes, &HostContext::so_in, 0};
    load::PlanExecutor executor(
        bus, operators, operator_host, voice, manager, answers,
        [&host](const std::string& stem) { return host.so(stem); },
        [&host, &operators, &project_refused](const load::Executed& done) {
            // Said artifact by artifact, in order: resolved truth, never written back to the plan.
            // Said when realization settles, inside a delivery, so a painting Skin may draw over
            // these lines; stdout keeps them all.
            for (const load::ResolvedArtifact& row : done.resolved) {
                std::string said = row.stem;
                if (row.provider_mounted) {
                    said += " | provider '" + row.provider + "' supplied " +
                            std::to_string(row.contributed);
                }
                if (row.weave_loaded) {
                    said += " | weave #" + std::to_string(row.weave.value) + " as " + row.role;
                    said += row.offer == op::OfferOutcome::Offered
                                ? " (this host's operator resolution offered)"
                                : " (ordinary weave: it declares no operator surface)";
                }
                std::printf("zengine-workshop - loaded: %s\n", said.c_str());
            }
            // And where this run stopped, named: one row, never a count.
            if (!done.waiting_on.empty()) {
                std::printf("zengine-workshop - waiting to be built: %s (build it, and its "
                            "authored participation is performed then -- every authored row "
                            "after it is waiting on this one)\n",
                            done.waiting_on.c_str());
            }
            // The tools that are not here, named: an optional row that refused is an unavailable
            // tool, said with its reason and kept as a standing condition for the run (WL-ATTN-01).
            // The compact row is what a terminal maker can still read.
            for (std::size_t i = 0; i < done.unavailable.size(); ++i) {
                std::printf("zengine-workshop - unavailable: %s\n", done.unavailable[i].c_str());
                host.standing_conditions.push_back(zengine::workshop::unavailable_tool(
                    done.unavailable_stems[i], done.unavailable[i]));
                ++host.conditions_generation;
            }
            if (done.ok) {
                std::printf("zengine-workshop - operators: %zu resolvable, from %zu "
                            "provider(s)\n",
                            operators.size(), operators.providers().size());
                std::fflush(stdout);
                // Completion ends nothing: the host goes on being a host.
                return;
            }
            // Nor does waiting. A refusal is one because somebody said it, so `refusal` is tested,
            // not `ok`.
            if (done.refusal.empty()) {
                std::fflush(stdout);
                return;
            }
            // This host's failure policy: a refused startup project ends this Workshop. The
            // shipped plan's second row is the Skin, so surviving a refusal would leave a process a
            // maker cannot see or quit. Say what stood before it.
            std::printf("zengine-workshop - %s\n"
                        "zengine-workshop - the authored plan was not completed; %zu "
                        "artifact(s) participated before it stopped. Exiting.\n",
                        done.refusal.c_str(), done.resolved.size());
            std::fflush(stdout);
            std::fflush(stdout);
            project_refused = true;
            // The door `q` leaves by: `host.quit`, and `request_stop` to end the turn in flight.
            host.quit = true;
            if (host.request_stop) {
                host.request_stop();
            }
        },
        // Is this row waiting on the maker? Only the host holds both halves: the file is absent,
        // and a recipe in force produces the stem -- asked of the owner at the walk. Without the
        // second half an absent artifact is a broken deployment and still refuses. Nothing here
        // starts a build.
        [&host, &current_recipes](const std::string& stem) {
            for (const builder::Recipe& r : current_recipes.all()) {
                if (r.artifact != stem) {
                    continue;
                }
                return !std::filesystem::exists(std::filesystem::path(host.so(stem)));
            }
            return false;
        },
        // The two disk acts realization cannot perform: the host's rules (`workshop/staging.hpp`),
        // over the catalog in force at the act.
        [&staging_host](const std::string& stem, const std::string& recipe, bool reload) {
            return staging::stage(staging_host, stem, recipe, reload);
        },
        [&staging_host](const std::string& stem, const std::string& image) {
            return staging::promote(staging_host, stem, image);
        });

    // ---- What the project is waiting on, answered alive -----------------------------------------
    // The owner derives, the host wires, the weave spends: read at each ask, never copied. A
    // reading, not a power.
    host.frontier = [&executor] {
        ProjectFrontier now;
        now.artifact = executor.waiting_on();
        now.waiting = !now.artifact.empty();
        now.blocked = executor.behind();
        return now;
    };

    // ---- What stands behind an office's running code --------------------------------------------
    // Three owners read at the ask (`provenance.hpp`): the bus, the realization owner and the
    // catalog in force. It opens nothing, and chooses nothing when several recipes answer.
    host.code_source = [&bus, &executor, &current_recipes](const std::string& office) {
        return provenance::code_source_of(office, bus, executor, current_recipes);
    };
    // ...and whether an office is still to come: pending in the inventory, never unavailable.
    host.office_pending = [&executor](std::string_view office) {
        return executor.office_pending(office);
    };

    // ---- The two authored files gain a writer: the maker's own act ------------------------------
    // `workshop/authoring.hpp`'s rules, wired over the owners this host holds and read at the act.
    // A plan row is written only after the running project took it.
    authoring::RecipeAuthor recipe_author{host.dir, host.project_dir, &current_recipes,
                                          host.use_recipes};
    host.author_recipe = [&recipe_author](const HostContext::RecipeDraft& draft) {
        return authoring::author_recipe(recipe_author, draft);
    };
    authoring::PlanAuthor plan_author{
        host.project_dir.empty() ? std::string()
                                 : host.project_dir + "/" + load_persist::kProjectLoadPlanName,
        read_plan.plan, &executor, &staging_host};
    host.append_plan_row = [&plan_author](const std::string& stem, const std::string& role,
                                          const std::string& recipe) {
        return authoring::append_plan_row(plan_author, stem, role, recipe);
    };
    host.plan_names = [&plan_author](const std::string& stem) {
        return authoring::plan_names(plan_author, stem);
    };

    // ---- What this host resolved, answered to whoever asks --------------------------------------
    // A read-only observation door over the realization owner and the catalog
    // (workshop/arrangement.hpp), mounted before realization begins: the tool that asks is loaded
    // by this plan and must find it. Its grant is its two answers, to any: Loom picks recipients.
    loom::Grant say_resolved;
    say_resolved.allow_to_any(ResolvedArrangement::zen_name, ResolvedArrangement::zen_version);
    say_resolved.allow_to_any(v2::ResolvedArrangement::zen_name,
                              v2::ResolvedArrangement::zen_version);
    say_resolved.allow_to_any(ResolvedPowers::zen_name, ResolvedPowers::zen_version);
    mount_in_office<ArrangementDoor>(
        bus, std::move(say_resolved), kArrangementRole, executor, operators, plan_path,
        [&bus](std::string_view role, const loom::Schema& shape) {
            return holder_accepts_on(bus, role, shape);
        });

    // ---- ...and the one office that may run a Source --------------------------------------------
    // A second door, so which office can cause evaluation keeps a one-word answer. One shape in and
    // one out; it caches nothing, and the catalog, declared far above, outlives it.
    loom::Grant say_sampled;
    say_sampled.allow_to_any(SourceSampled::zen_name, SourceSampled::zen_version);
    mount_in_office<SampleDoor>(bus, std::move(say_sampled), kSampleRole, operators);

    // ---- ...and the three doors the pane weaves ask (workshop/pane_doors.hpp) -------------------
    // Three, because answering `zengine.project` reads while `zengine.recipes` and `zengine.plan`
    // write; each acting door holds a closure already wired (WL-PROJ-04, WL-AUTH-01, WL-AUTH-02).
    // The read-only door captures `host.frontier`, `host.plan_names` and `host.recipe_source` by
    // value, so it is mounted after they are wired.
    loom::Grant say_project;
    say_project.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
    say_project.allow_to_any(ProjectFrontierSaid::zen_name, ProjectFrontierSaid::zen_version);
    say_project.allow_to_any(PlanNames::zen_name, PlanNames::zen_version);
    say_project.allow_to_any(RecipeSourceSaid::zen_name, RecipeSourceSaid::zen_version);
    mount_in_office<ProjectDoor>(bus, std::move(say_project), kProjectRole, host.project_dir,
                                 marks_path, host.frontier, host.plan_names,
                                 host.recipe_source);

    loom::Grant say_recipes;
    say_recipes.allow_to_any(RecipeOutcome::zen_name, RecipeOutcome::zen_version);
    mount_in_office<RecipesDoor>(bus, std::move(say_recipes), kRecipesRole, host.use_recipes,
                                 host.author_recipe);

    loom::Grant say_plan;
    say_plan.allow_to_any(PlanRowWritten::zen_name, PlanRowWritten::zen_version);
    mount_in_office<PlanDoor>(bus, std::move(say_plan), kPlanRole, host.append_plan_row);

    // ---- The editor switch (WL-SWITCH-03, WL-SWITCH-07) -----------------------------------------
    // One native coordinator in its own office, switching the managed pane's office between the
    // choices the plan authors. Handed the bus, the Kernel, the office and four readings of the
    // realization owner, it names no artifact; its grant is its conversation.
    {
        EditorSwitchHost switch_host;
        switch_host.bus = &bus;
        switch_host.kernel = &kernel;
        switch_host.office = kEditorRole;
        switch_host.choices = [&executor] { return executor.plan().choices; };
        switch_host.holder = [&executor] { return executor.choice_holder(kEditorRole); };
        switch_host.image_of = [&executor](const std::string& stem) { return executor.image_of(stem); };
        switch_host.record = [&executor](const std::string& stem, loom::WeaveId weave,
                                         const std::string& image) {
            const load::PlanExecutor::Recorded done =
                executor.record_choice_holder(kEditorRole, stem, weave, image);
            return done.accepted ? std::string() : done.refusal;
        };
        auto switcher = std::make_unique<EditorSwitchCoordinator>(std::move(switch_host));
        EditorSwitchCoordinator* raw = switcher.get();
        const loom::WeaveId switching = bus.register_weave(
            std::move(switcher), editor_switch_grant(kEditorRole), std::string(kEditorSwitchRole));
        raw->zen_set_self(switching);
    }

    // ---- Begin the project, then go and be a host -----------------------------------------------
    // This returns before the project is realized: the loop below and the load's own answers carry
    // the rest, and nothing below knows the plan.
    zengine::flow_host::RuntimeHost flow_runtime(bus, operators);
    flow_runtime.mount();

    executor.begin(read_plan.plan);

    // The loop: the input beat keeps the queue alive, the Timer paces it, and `q` stops the bus. A
    // turn that ends with an empty queue means nothing will speak again: say so and leave. A
    // native owner's showing runs inside the host's boundary (workshop/host_pump.hpp), so a throw
    // there is Loom's Failed, told here in the owner's words.
    while (!host.quit) {
        const ServedTurn served =
            serve_until_idle(bus, host.showings, [&journal](const std::string& said) {
                journal.info("zengine.workshop", said);
                std::printf("zengine-workshop - %s\n", said.c_str());
                std::fflush(stdout);
            });
        (void)served;
        if (!host.quit && bus.pending() == 0) {
            std::printf("zengine-workshop - the bus went quiet without a quit "
                        "(no timer service deployed?): exiting.\n");
            break;
        }
    }

    // ---- What this run knew, and what it kept: working memory and the durable record ----------
    // The dump beside them is a witness, not a product.
    const loom::RecorderBounds b = history.bounds();
    const loom::RecorderCounters c = history.counters();
    std::printf("zengine-workshop - knew: %zu retained (%zu recent, %zu protected, %zu last-call "
                "over %zu shapes), %llu forgotten, %llu observed, %llu declined by policy\n",
                b.retained, b.recent_held, b.protected_held, b.last_call_held, b.shapes_observed,
                static_cast<unsigned long long>(b.forgotten),
                static_cast<unsigned long long>(c.observed),
                static_cast<unsigned long long>(c.declined_by_policy));
    const loom::LoggerCounters lc = journal.counters();
    std::printf("zengine-workshop - kept: %llu of %llu observed selected, %llu appended, "
                "%llu bytes%s\n",
                static_cast<unsigned long long>(lc.selected),
                static_cast<unsigned long long>(lc.observed),
                static_cast<unsigned long long>(lc.appended),
                static_cast<unsigned long long>(lc.bytes),
                args.log.empty() ? " (no --log: nothing was written)" : "");
    if (!args.dump.empty()) {
        std::ofstream dump(args.dump);
        if (dump) {
            loom::DumpOptions opts;
            opts.payloads = false;
            loom::dump_history(history, dump, opts);
        }
    }
    journal.close();
    // A refused project is still exit 4, set by the failure policy above.
    return project_refused ? 4 : 0;
}
