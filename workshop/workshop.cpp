// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-workshop — the first Workshop surface. One rectangle, selected,
// inspected, edited, refused, moved, resized, typed into.
//
// It is an ordinary Zengine application and holds no privilege snake does not:
// input arrives from the Input package's weave, time from the Timer package's,
// pixels-or-characters from whichever Skin holds `zengine.skin`, and — since
// LOAD-0 — the host does not even own the LIST of which artifacts those are.
// Workshop paints by PUBLISHING intent — a SurfaceCanvas — exactly as the world
// publishes a SnakeVisual, and it never touches the terminal.
//
// THIS FILE IS THE HOST, and only the host: `main()`, the grants, and the
// command-line arguments that say which files this Workshop's document, setup and
// LOAD PLAN live in. Workshop's own weave lives in weave.hpp, where a suite can
// mount it, so `input message -> gesture -> semantic operation` is a chain the
// tests walk end to end instead of a claim a report has to make.
//
// AND IT NAMES NO ARTIFACT. Which providers are mounted and which weaves are
// loaded into which roles is `default-load-plan.json`, read at startup and
// performed by `load_execute.hpp`. What this file still owns is the GRANT the
// plan booter holds — the dangerous one, kernel reach transitively — the one rule
// that spells an artifact stem as a file, and the in-process composition. A
// tripwire in `tests/test_operator_provider.cpp` reads this source and refuses a
// stem. See `docs/reference/load-plan.md`.
//
// THE HOST CHOOSES THE PATH AND THE WEAVE USES IT: where things are is the host's
// business, what to do with them is the application's. Workshop still holds no
// privilege snake does not — it opens an ordinary file with an ordinary
// standard-library call, which is a power every program on this machine already
// has, and it needs no grant, no broker and no capability to do it (see the
// specialness ledger).

#include "arrangement.hpp"
#include "host_sources.hpp"
#include "sample_door.hpp"
#include "load_execute.hpp"
#include "load_persist.hpp"
#include "marks_persist.hpp"
#include "path_admission.hpp"
#include "recipe_persist.hpp"
#include "recipes.hpp"
#include "user_paths.hpp"
#include "weave.hpp"

#include "builder/runner.hpp"
#include "builder/vocabulary.hpp"
#include "builder/weave.hpp"
#include "operator/catalog.hpp"
#include "operator/host_surface.hpp"
#include "surface/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/history/dump.hpp>
#include <zen/history/logger.hpp>
#include <zen/history/recorder.hpp>
#include <zen/host/terminal_wiring.hpp>
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
// THE COMPOSER, THE INPUT PRODUCER AND INTROSPECTION HAVE NO ALIAS HERE ANY MORE,
// and the absence is measurable rather than tidy: since LOAD-0 this host names no
// stem and no role of theirs, so it needs no vocabulary of theirs to name one with.
// What remains is `surface`, `timer` and `builder` -- the shapes this host's own
// GRANTS and log selection are written in, which is authority and observation and
// not a load list. Re-adding an alias here to save a plan row would be re-adding
// the knowledge this phase removed.

/// Mount an in-process weave into an OFFICE, with the grant the host chose.
///
/// `loom::mount_granted` does everything this does except bind a role, and the
/// three-argument `register_weave` is the Loom's own way to bind one — so this
/// is the two of them spelled together rather than a new mechanism. The office
/// is what lets a grant say "may speak to whoever holds this" instead of naming
/// a WeaveId, which is what keeps the authority written here readable: the
/// Builder may reach the build runner, and nothing else may.
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

/// Which file this Workshop saves to and loads from, and which Skin paints it.
///
/// `--document <path>`, defaulted to `workshop.json` in whatever directory the
/// maker started Workshop in. The smallness is the point: there is no picker,
/// no recent list, no project
/// concept and no workspace manager, because none of those is needed to prove
/// that a maker can close Workshop and get their work back. An unknown argument
/// is REFUSED rather than ignored — a mistyped flag that silently saved to the
/// default file is exactly the kind of quiet wrong answer persistence makes
/// expensive.
///
/// `--setup <path>`, defaulted to `workshop-setup.json`, is the SECOND file of
/// the same shape and is a different file on purpose (WS-0). A document is what a
/// maker made; a setup is the arrangement of panes they were looking at while
/// they made it. The same document is worth opening in two arrangements and the
/// same arrangement is worth using over two documents, so folding them into one
/// project container would make both unsayable. Workshop manages ONE active
/// setup path: no catalog, no recent list, no profile manager. An empty path is
/// refused by name, exactly as an empty `--document` is.
///
/// `--session <path>` is the THIRD file of the same shape and is the one nobody types
/// (WUX-0). It holds the LAST SESSION: the desk this Workshop was arranged into, how much
/// room the surface had, and — since WUX-3 — where its window sat on the desktop. It is a
/// different file from `--setup` for the reason that is the whole of the distinction: a
/// setup is a desk a maker deliberately NAMED, and an automatic save that could land on it
/// would rewrite that name's contents every time a window was closed.
///
/// SINCE WUX-3 ITS DEFAULT IS NOT THE LAUNCH DIRECTORY. A session describes the MAKER'S
/// MACHINE — this window, these monitors — not the project, so its default home is the
/// per-user machine-local state root (`user_paths.hpp`: %LOCALAPPDATA% on Windows, the
/// XDG state directory elsewhere), and launching Workshop from two different directories
/// finds the same desk. The document and the setup deliberately keep their launch-
/// directory defaults: project-authored facts follow the project.
///
/// `--keymap <path>` and `--prefs <path>` are the maker-CONFIGURATION pair — the hand and
/// the eyes — and their default home is the per-user configuration root (%APPDATA% on
/// Windows, the XDG config directory elsewhere), for the session's reason with the
/// roaming/local split between them: a preference is meaningful on any machine, a session
/// is not.
///
/// `--marks <path>` is the maker's PLACES (PROJ-2): the directories they said they want to
/// be able to come back to, once the Files browser stopped being confined to the directory
/// Workshop was launched in. It defaults to the per-user MACHINE-LOCAL state root, beside
/// the session and not beside the keymap, by the very criterion that separates the two
/// roots — a mark is an absolute path, so it describes THIS machine's disks exactly as a
/// viewport describes this machine's window. A mark is a destination and nothing else: it
/// confers no authority, states no trust, and cannot move the project.
///
/// `--isolated` is the whole-application refusal of all three defaults (WUX-3): this run
/// reads and writes NONE of the maker's ordinary per-user configuration or session state.
/// It exists because moving the defaults off the launch directory inverts an accident —
/// a witness harness or executor run launched from a scratch directory used to be
/// isolated by its CWD, and after the move that same unflagged launch would touch the
/// real maker's settings — so isolation is explicit now, suitable for executor runs,
/// tests, temporary product witnesses and clean-start diagnosis. Explicit paths outrank
/// it: `--isolated --session s.json` reads and writes exactly `s.json` and nothing else.
/// The precedence, pinned in `user_paths.hpp` and its suite: explicit path, then
/// isolation, then the per-user default.
///
/// THE ONE-TIME LEGACY TRANSITION (WUX-3): pre-WUX-3 builds kept the keymap and session
/// beside the launch directory, and makers may have real settings there. When a per-user
/// default resolves and its file does not exist yet while the old local file does, the
/// host copies the local file's bytes to the user root once, says so in plain words (on
/// this banner and on Workshop's notice line), and NEVER deletes, moves or rewrites the
/// original. A user-root file that already exists always wins — a legacy file's presence
/// can never overwrite it — and once the destination exists the rule never fires again.
///
/// `--load-plan <path>`, defaulted to `default-load-plan.json` BESIDE THE
/// EXECUTABLE, is the fourth of the same shape and is the one this host cannot run
/// without (LOAD-0). It names the authored plan saying which artifacts participate
/// in this project and how -- which provider contributions are mounted, which
/// weaves are loaded into which roles, and in what order. There is no compiled-in
/// fallback plan: a host that could manufacture its own arrangement when the file
/// is missing would make the file decorative, and the file being the source is the
/// whole of what this phase bought.
///
/// IT REPLACED `--skin` AND `--input`, and what those two flags were FOR is what
/// the plan now does better. Their reason was that BUILDING a Skin and CHOOSING one
/// are different acts, and that choosing one by editing a literal in this file is a
/// founder experiment nobody else could repeat. A plan file is that same choice made
/// repeatable, diffable and durable -- so the graphical Workshop is a second SHIPPED
/// PLAN rather than two flags, and a maker who wants a third arrangement copies a
/// file instead of asking this host for a fourth flag.
///
/// THE OTHER HALF OF WHAT THOSE FLAGS SAID IS STILL TRUE AND IS NOW SAID BY THE
/// PLAN. Presentation and input are two dimensions, not one: a backend states what
/// it SAW, and which backend is watching is not deducible from which one is
/// painting. The plan carries them as two independent rows, so nothing here deduces
/// one from the other -- and the banner below prints the whole executed arrangement,
/// which is what makes a run's active reader LEGIBLE rather than inferred.
///
/// EXACTLY ONE READER IS LOADED, which is the other half of that legibility, and
/// it is now a fact about a file rather than about a flag. The Loom would refuse a
/// second anyway -- `zengine.input` is a singleton role -- so a plan naming two
/// input artifacts is refused by the Kernel and reported by artifact and step.
///
/// `--log <path>` and `--dump <path>` are the optional two, and they are TWO
/// because what this host keeps and what it knows are different questions
/// (RTH-1a). `--log` opens the durable JOURNAL: selected facts only, appended as
/// they happen, and it outlives the process. `--dump` writes what the volatile
/// RECORDER still held when Workshop quit -- which is most of a session's story
/// and almost none of what deserves to be permanent.
///
/// Absent both, the recorder still runs in memory (it costs a fraction of one
/// per cent of dispatch) and nothing is written -- so `q` always leaves a live
/// process that could have been asked what happened, and only a maker who asked
/// for a file gets one.
struct Arguments {
    bool ok = true;
    std::string complaint;
    std::string document = zengine::workshop::persist::kDefaultDocumentName;
    /// The setup file, beside the document's and never inside it (WS-0). Same
    /// shape as `--document` for the same reasons; a different file because a
    /// document and the arrangement it is looked at in are different facts.
    std::string setup = zengine::workshop::kDefaultSetupFileName;
    /// The last-session file, written by nobody's gesture (WUX-0). EMPTY MEANS "NOT
    /// EXPLICITLY CHOSEN" since WUX-3 -- `main` resolves the per-user default through
    /// `user_paths.hpp` -- so unlike the document's, this default is not a bare name here:
    /// a bare name would resolve against the launch directory, which is precisely the
    /// behaviour WUX-3 ended for the maker's own files.
    std::string session;
    /// The maker's keymap file (KEY-0), read at startup. An ABSENT file is the defaults,
    /// silently -- deleting it is how a maker resets their bindings -- so unlike the plan
    /// there is nothing to refuse about a path with no file at it. Empty means "not
    /// explicitly chosen", for `session`'s reason.
    std::string keymap;
    /// The maker's presentation preferences (WUX-3), written when they state one (the
    /// pane-title toggle). Empty means "not explicitly chosen", for `session`'s reason.
    std::string prefs;
    /// The maker's location marks (PROJ-2), written when they mark or unmark a place.
    /// Empty means "not explicitly chosen", for `session`'s reason.
    std::string marks;
    /// This run touches none of the maker's ordinary per-user configuration or session
    /// state (WUX-3). Explicit paths above still win over it.
    bool isolated = false;
    /// Empty means "the one shipped beside this executable", which `main()` resolves
    /// once it knows where that is. It is deliberately NOT defaulted to a bare name
    /// here: a bare name would resolve against whatever directory a maker happened to
    /// launch from, which is right for a document and wrong for a plan naming
    /// artifacts staged beside the binary.
    std::string load_plan;
    /// The authored BUILD RECIPES (BLD-1), and the one flag here whose file may be
    /// absent. Empty means "the one shipped beside this executable", resolved by
    /// `main()` for `--load-plan`'s reason exactly.
    ///
    /// ⚠ ABSENT IS NOT REFUSED, and it is the one of the five that is not. A load plan
    /// says what this process RUNS ON and a host with no plan has no arrangement to
    /// perform; recipes say what this project can BUILD, and a project with nothing to
    /// build is an ordinary project. So a missing default file leaves the Builder
    /// holding no recipes and says so in the banner, while a malformed file -- default
    /// or named -- is refused out loud, because silently ignoring an authored file a
    /// maker got wrong is the quiet wrong answer this repository keeps refusing.
    std::string recipes;
    std::string log;  ///< empty = keep nothing durably
    std::string dump; ///< empty = write no snapshot of working memory at exit
};

Arguments parse_arguments(int argc, char** argv) {
    Arguments args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--isolated") {
            // The one flag that takes no path: a whole-run policy, not a file (WUX-3).
            args.isolated = true;
            continue;
        }
        if (arg == "--document" || arg == "--setup" || arg == "--session" ||
            arg == "--keymap" || arg == "--prefs" || arg == "--marks" ||
            arg == "--load-plan" || arg == "--recipes" || arg == "--log" ||
            arg == "--dump") {
            if (i + 1 >= argc) {
                args.ok = false;
                args.complaint = arg + " needs a path";
                return args;
            }
            const std::string value = argv[++i];
            if (arg == "--load-plan") {
                // REFUSED HERE rather than below, because empty is this field's way
                // of saying "the one beside the executable" and a maker who typed an
                // empty path would silently get the default instead of a complaint.
                if (value.empty()) {
                    args.ok = false;
                    args.complaint = "--load-plan needs a path";
                    return args;
                }
                args.load_plan = value;
            } else if (arg == "--recipes") {
                // REFUSED HERE for `--load-plan`'s reason: empty is this field's way of
                // saying "the one beside the executable", so a maker who typed an empty
                // path would silently get the default instead of a complaint.
                if (value.empty()) {
                    args.ok = false;
                    args.complaint = "--recipes needs a path";
                    return args;
                }
                args.recipes = value;
            } else if (arg == "--session" || arg == "--keymap" || arg == "--prefs" ||
                       arg == "--marks") {
                // REFUSED HERE since WUX-3, for the same reason at a different default:
                // empty is now these fields' way of saying "the per-user root decides",
                // and a maker who typed an empty path would silently get that policy
                // instead of a complaint. Turning the file OFF is `--isolated`'s job.
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
            } else if (arg == "--dump") {
                args.dump = value;
            } else if (arg == "--setup") {
                args.setup = value;
            } else {
                args.document = value;
            }
            continue;
        }
        args.ok = false;
        args.complaint = "unknown argument `" + arg + "`";
        return args;
    }
    if (args.document.empty()) {
        args.ok = false;
        args.complaint = "--document needs a path";
    } else if (args.setup.empty()) {
        args.ok = false;
        args.complaint = "--setup needs a path";
    }
    return args;
}

int main(int argc, char** argv) {
    const Arguments args = parse_arguments(argc, argv);
    if (!args.ok) {
        std::printf("zengine-workshop - %s\n"
                    "usage: zengine-workshop [--document <path>] [--setup <path>]\n"
                    "                        [--session <path>] [--keymap <path>]\n"
                    "                        [--prefs <path>] [--marks <path>]\n"
                    "                        [--isolated]\n"
                    "                        [--load-plan <path>]\n"
                    "                        [--recipes <path>]\n"
                    "                        [--log <path>] [--dump <path>]\n"
                    "the graphical Workshop is the second plan shipped beside this binary:\n"
                    "  zengine-workshop --load-plan <workshop dir>/%s\n",
                    args.complaint.c_str(), load_persist::kGraphicalLoadPlanName);
        return 2;
    }

    // ---- THE RECIPES THIS WORKSHOP CURRENTLY MEANS, DECLARED FIRST (PROJ-0) ------
    //
    // EMPTY UNTIL THE CATALOG IS READ, far below -- what is decided HERE is only where
    // it stands in this function, and that position IS the lifetime proof. The runner
    // and the Builder tool read this object for as long as they live, and the host's
    // edit-source answer reads it too; declaring it above the HostContext, the bus, the
    // Kernel and every weave makes reverse-destruction order say what no comment could,
    // which is the argument `HostContext::frontier` already makes for the realization
    // owner one screen down.
    //
    // ⚠ DO NOT MOVE THIS DECLARATION BELOW THE BUS. Everything that reads it is
    // destroyed with the Kernel; a catalog destroyed before them is a catalog they
    // could read on the way out.
    CurrentRecipes current_recipes;

    // WHERE THE HOST'S OWN FILES ARE, resolved once and before anything is said
    // about them, because the load plan's default IS this directory and a banner
    // that named a path this host had not resolved would be naming a guess.
    HostContext host;
    host.dir = exe_dir();
    // AND WHERE THE MAKER IS STANDING, captured in the same breath and for the same
    // reason -- once, before anything is said about it. `exe_dir()` above is where this
    // BINARY lives; this is the PROJECT, which this application has always defined as the
    // launch directory (user_paths.hpp) and has never until now held as a value. Both are
    // resolved here because the host is the party that knows either.
    //
    // THE CAPTURE CANNOT FAIL FATALLY, AND THE WHOLE OF WHY IS `path_admission.hpp`'s: a
    // working directory the platform will not report, and one it reports but this
    // application cannot say, are the SAME absence -- empty, which every consumer already
    // refuses in words and which the banner below states once. No fallback is invented for
    // either, deliberately.
    host.project_dir = launch_project_dir();
    host.document_path = args.document;
    host.setup_path = args.setup;

    // ---- THE MAKER'S OWN FILES, RESOLVED BY THE PINNED PRECEDENCE (WUX-3) --------
    //
    // Explicit path, then isolation, then the per-user default -- `user_paths.hpp` owns
    // the rule and the roots, this host owns calling them, and the weave stays ignorant
    // of every step: it receives one string per file, empty meaning "no persistence",
    // exactly as it always has. An environment that cannot supply a root resolves to
    // that same absence, said below rather than silently falling back to the launch
    // directory -- a quiet location change is the class of wrong answer this repository
    // refuses.
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
    // THE MARKS RIDE THE MACHINE-LOCAL ROOT, and that is a correctness choice rather than a
    // convenience one (PROJ-2). Every other maker-configuration file is meaningful on any
    // machine this person sits at; a mark is an ABSOLUTE PATH, so it describes these disks
    // and nothing else -- the same argument the viewport and the desktop placement already
    // make for riding the session's root instead of the keymap's.
    host.marks_path = user_paths::resolve_durable_path(
        args.marks, args.isolated, state_root, marks_persist::kDefaultMarksFileName);

    // ---- ...AND THE ONE-TIME LEGACY TRANSITION, FOR EXACTLY THE DEFAULTED ONES ----
    //
    // Only a fact that resolved to its per-user DEFAULT can have a pre-WUX-3 local file
    // to inherit: an explicit path is the maker's own answer, and an isolated run touches
    // nothing. The prefs file is new in WUX-3 and has no legacy to import. The rule
    // itself -- import once into an absent destination, never overwrite an existing one,
    // never delete the original, converge by existence -- is `user_paths.hpp`'s, pinned
    // in the suite; what the host owns is the wiring and the words.
    //
    // AND THE TRANSITION PRODUCES TWO KINDS OF FACT, WHICH ARE NOT JOINED WITH A
    // SEMICOLON. An IMPORT happened once, at this launch, and converges by existence so it
    // can never happen again -- an event, and it belongs on the notice row, which is where
    // things that happened go. A SHADOWED file is still shadowed at the next launch and has
    // a standing maker action ("delete it to end this note"); it is a CONDITION, it travels
    // to the weave under a key of its own, and it disappears the day the maker deletes the
    // file rather than the moment something else is said.
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

    const std::string plan_path =
        args.load_plan.empty() ? host.dir + "/" + load_persist::kDefaultLoadPlanName
                               : args.load_plan;

    // The honest line, in plain scrollback, exactly as snake's host prints it:
    // this host isolates nothing. (The `--isolated` below is about the maker's FILES,
    // not about containment -- the two words meet here and mean different things.)
    std::printf("zengine-workshop - containment: %s\n", loom::Kernel::containment_note());
    std::printf("zengine-workshop - document: %s\n", args.document.c_str());
    std::printf("zengine-workshop - setup: %s\n", args.setup.c_str());
    // THE PROJECT, SAID ONCE. It is where Project Files browses from and what a relative
    // source in a build recipe means, so a maker whose shell was somewhere unexpected can
    // read it here rather than deduce it from a listing. An absence is said on the same
    // line rather than left to be discovered later as a refusal.
    //
    // ⚠ THE ABSENCE HAS TWO CAUSES AND ONE SENTENCE, DELIBERATELY. A system that reports no
    // working directory and one that reports a directory this build cannot write down are
    // the same fact to everything downstream, and the sentence has to be true of BOTH the
    // moment it is read -- so it names what is missing (a directory this Workshop can carry)
    // rather than guessing which way it went missing.
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
    // THE PLACES, SAID ON THE SAME TERMS AS THE OTHER FIVE. A maker who wonders why `n`
    // takes them nowhere reads here that this run keeps no marks, rather than deducing it.
    std::printf("zengine-workshop - marks: %s\n", path_or_absence(host.marks_path).c_str());
    if (!transition.empty()) {
        std::printf("zengine-workshop - %s\n", transition.c_str());
    }
    // THE BANNER STILL PRINTS BOTH KINDS. A scrollback launch and a shortcut launch overlap
    // in neither direction, so the host says what it knows here as well -- and it says the
    // standing ones in the same words the weave will show them in, because they are the
    // same conditions and not a second account of them.
    for (const Condition& standing : host.standing_conditions) {
        std::printf("zengine-workshop - %s\n", standing.detail.c_str());
    }
    std::printf("zengine-workshop - load plan: %s\n", plan_path.c_str());
    std::fflush(stdout);

    // ---- THE AUTHORED LOAD PLAN, READ BEFORE ANYTHING IS BUILT (LOAD-0) -------
    //
    // READ FIRST, MOUNTED AND LOADED LATER. A malformed or missing plan is answered
    // before this process has a bus, a Kernel, a catalog, a terminal or a single
    // mounted contribution -- so the one failure that leaves nothing to clean up is
    // the one this host cannot half-perform.
    //
    // THERE IS NO COMPILED-IN FALLBACK, and the absence is the phase. A host that
    // manufactured an arrangement when the file was missing would be the C++ startup
    // code LOAD-0 removed, kept as a spare; and the maker would be told their project
    // loaded when what actually ran was this translation unit's opinion of one.
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

    // ---- THE AUTHORED BUILD RECIPES, READ IN THE SAME BREATH (BLD-1) ----------
    //
    // READ HERE, BESIDE THE PLAN, BECAUSE THEY ARE THE SAME KIND OF THING: durable
    // authored project intent, answered before this process has a bus or a Kernel, so
    // that a file a maker got wrong costs nothing to refuse. What they are NOT is the
    // same DOCUMENT -- a plan row is an execution-authority decision and a recipe is a
    // build procedure, and folding them would bury the first inside the second
    // (workshop/recipe_persist.hpp says this at length).
    //
    // A MISSING DEFAULT IS AN ANSWER AND A MALFORMED FILE IS NOT. See `Arguments`.
    //
    // ---- ...AND THE LAUNCH HAS NO PRIVATE PATH TO THE OWNER (PROJ-1) -------------
    //
    // WHAT AN AUTHORED RECIPE CANNOT SAY IS ANSWERED IN ONE PLACE, and this `main` is not
    // it. Three facts a maker cannot write down -- where this install puts artifacts,
    // where it may write a generated project, and what a relative source is relative to
    // -- are the host's two directories, and `install_recipes` is the one seam that reads
    // a file, gives them and installs the result (workshop/recipes.hpp holds the
    // transaction). What is decided HERE is only WHICH of this host's two directories
    // answers which question: `dir` is INSTALLATION truth and `project_dir` is where the
    // maker is standing, and that decision is made once, in the closure below.
    //
    // SO THE LAUNCH INSTALLS THROUGH THE MAKER'S OWN DOOR. `--recipes` and the shipped
    // default are INITIAL STATE and not a second recipe policy: the flag chooses which
    // file this session STARTS with, and the block below hands that file to the very
    // function a maker's later choice reaches. A launch that completed recipes its own way
    // would be free to complete them DIFFERENTLY, which is EDIT-1's two-files defect
    // waiting one layer up -- and it is why this closure is wired BEFORE the file is read
    // rather than after.
    host.use_recipes = [&host, &current_recipes](const std::string& path) {
        HostContext::RecipeSwap done;
        const Written read = install_recipes(current_recipes, path, host.dir,
                                             host.project_dir, &HostContext::so_in);
        done.accepted = read.accepted;
        done.refusal = read.refusal;
        // WHAT IS IN FORCE, ASKED OF THE OWNER AFTER THE ATTEMPT -- never echoed from the
        // candidate. On acceptance the owner is holding the new catalog and answers with
        // it; on a refusal the owner was never touched and answers with the OLD one, which
        // is exactly the half of the sentence a maker needs to hear. Echoing the candidate
        // here would let a presentation say "still using <the file just refused>" by doing
        // the obvious thing with the obvious field.
        done.path = current_recipes.source();
        done.recipes = current_recipes.all().size();
        return done;
    };
    {
        const bool named = !args.recipes.empty();
        const std::string recipe_path =
            named ? args.recipes : host.dir + "/" + recipe_persist::kDefaultRecipesName;
        if (!named && !std::filesystem::exists(std::filesystem::path(recipe_path))) {
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
            // THE PATH AND THE COUNT COME BACK FROM THE OWNER, so this banner line names
            // the catalog that is actually in force rather than the argument this host
            // passed in a moment ago.
            std::printf("zengine-workshop - build recipes: %s (%zu)\n", read.path.c_str(),
                        read.recipes);
        }
    }
    // ---- THE EDIT-SOURCE SEAM (`HostContext::recipe_source`) ---------------------
    //
    // The host is the one party holding the completed recipes -- the same value the
    // runner builds from -- so the host answers which source file a recipe names,
    // verbatim, and the editor cannot come to open a subtly different join of the
    // same bytes. The kind words are the recipe FILE's own, so a refusal downstream
    // speaks the vocabulary the maker authored in.
    //
    // ⚠ IT ASKS THE OWNER AT THE MOMENT OF THE GESTURE (PROJ-0). This closure used to
    // capture a private copy of three fields per recipe -- a third session-long store
    // of completed truth whose whole reason was that the catalog was about to be handed
    // onward by value. Nothing is handed onward by value any more, so it captures the
    // owner and reads it when asked: the same shape `frontier` has, and the reason the
    // editor's answer cannot outlive the catalog it came from.
    host.recipe_source = [&current_recipes](const std::string& id) {
        HostContext::RecipeSource out;
        const builder::Recipe* found = builder::recipe_named(current_recipes.all(), id);
        if (found != nullptr) {
            out.known = true;
            out.kind = found->single_source.has_value() ? "single_source" : "cmake_target";
            if (found->single_source.has_value()) {
                out.source = found->single_source->source;
            }
        }
        return out;
    };
    std::fflush(stdout);

    loom::Switchboard bus;

    // ---- WHAT THIS HOST REMEMBERS (RTH-1) ------------------------------------
    //
    // FIRST, before the Kernel and before any weave, because a recorder attached
    // later would have a first record that is not the first fact. It is not a
    // participant: it holds no identity, accepts nothing, sends nothing, and
    // cannot be addressed. It is the host's own lens, exactly as a ConsoleEngine
    // would be, and it is here because a Workshop that could not say what just
    // happened has been the recurring cost of every phase since BLD-0 -- a build
    // that finished while its panel was closed finished silently, and a maker
    // had nowhere to look.
    //
    // THE POLICY IS THIS HOST'S, and it is written here rather than defaulted
    // because the numbers that justify it are this application's: RTH-0 measured
    // an idle Zengine app at ~300 deliveries/s, essentially all of it the Timer's
    // own heartbeat, and one interactive SurfaceCanvas at up to 2.75 KiB and
    // ~90% of interactive bytes.
    loom::RecorderPolicy history_policy = loom::default_policy();
    // THE BEATS. A heartbeat is a real fact and this is not a claim that it is
    // not -- it is a claim that four thousand of them are not four thousand
    // pieces of CONTEXT. Unfiltered they cover the recent window in about
    // fourteen seconds, so a maker looking for the build they started a minute
    // ago would find every trace of it gone.
    //
    // RTH-1 said this by making them NotRetained, which also made them
    // UNFINDABLE: a maker could not ask whether a beat had ever arrived at all.
    // The correction is three independent knobs -- keep the last one, take no
    // recent context, keep no bytes -- so `last_of(TimerFired)` still answers and
    // the story of a build is still legible beside it.
    for (const char* shape : {timer::TimerFired::zen_name, timer::Drive::zen_name}) {
        history_policy.rules.push_back(loom::RetentionRule{
            std::string(shape), /*last_n=*/1, /*in_recent=*/false, /*retain_payload=*/false});
    }
    // THE PICTURES. A frame is worth remembering AS AN EVENT -- that Workshop
    // repainted, when, and how long the Skin took over it -- and is not worth
    // remembering as bytes. Retaining them would make this history mostly a
    // screenshot log, which is the other way to lose a build's story.
    history_policy.rules.push_back(loom::RetentionRule{
        std::string(surface::SurfaceCanvas::zen_name), 1, true, false});
    // THE BUILD. Its observations are rare, they arrive in bursts, and they are
    // the thing a maker actually goes looking for. A deep last-call slot of their
    // own means a burst of output cannot push the build's own start out of
    // memory -- and they keep their place in recent context too, because the
    // order they arrived in RELATIVE TO EVERYTHING ELSE is half the story.
    for (const char* shape :
         {builder::BuildStarted::zen_name, builder::BuildOutput::zen_name,
          builder::BuildFinished::zen_name, builder::BuildNotStarted::zen_name,
          builder::RunBuild::zen_name, builder::BuildRequested::zen_name}) {
        history_policy.rules.push_back(loom::RetentionRule{std::string(shape), 512, true, true});
    }
    // ...AND IT IS APPLIED AS A CHANGE, not handed over at construction, so that
    // the FIRST thing this run remembers is what it was told to remember. A
    // recorder whose policy arrived silently could not answer the one question
    // every later absence raises -- "was that never recorded, or was I told not
    // to?" -- and the answer is a record like any other, costing not one message
    // on the bus.
    loom::Recorder history(bus);
    history.apply_policy(std::move(history_policy));

    // ---- ...AND WHAT IT CHOOSES NOT TO FORGET (RTH-1a) ------------------------
    //
    // A SECOND OWNER, not a bigger version of the first. The recorder above will
    // have thrown away almost everything before this process exits, and that is
    // correct: it is working memory. The Logger keeps the handful of facts a
    // maker will want after the fact, for good, and it keeps NOTHING ELSE -- so
    // ordinary Workshop traffic can never consume the horizon a weave
    // replacement or a failed handler needs.
    //
    // Loom's default selection already covers what code is loaded, who may
    // speak, every handler failure and every death. This host adds the two
    // application facts of the same kind: a build that FINISHED and a build that
    // never started. Both are rare, both are what a maker asks about tomorrow,
    // and neither is BuildOutput -- a thousand lines of compiler chatter is
    // working memory, not a record.
    loom::LoggerSelection log_selection = loom::default_selection();
    for (const char* shape :
         {builder::BuildFinished::zen_name, builder::BuildNotStarted::zen_name}) {
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
            journal.info("zengine.workshop", "session started: document " + args.document +
                                                 ", setup " + args.setup + ", load plan " +
                                                 plan_path);
        }
    } else {
        std::printf("zengine-workshop - log: nothing durable (--log <path> to keep one)\n");
    }
    if (!args.dump.empty()) {
        std::printf("zengine-workshop - dump at exit: %s (what memory still held)\n",
                    args.dump.c_str());
    }
    std::fflush(stdout);

    // ---- WHERE THIS PROCESS'S SEMANTIC POWERS COME FROM (PROV-0) ------------
    //
    // POWERS COME FROM PROVIDERS. THIS HOST OWNS ONLY WHICH ONE IS IN FORCE.
    //
    // The catalog below starts EMPTY and this file authors no operator. What fills it
    // is mounting artifacts that say, across a C seam, "I supply these definitions" --
    // and what this host then owns is the live resolution: which contribution
    // currently satisfies each logical power, which are shadowed beneath it, and what
    // happens to both when a provider goes away.
    //
    // THE ONE EXCEPTION IS NOT AN EXCEPTION TO THAT (SOURCE-0), and it is mounted a few
    // lines below: two zero-input SOURCES over facts this process already owns.
    // Describing yourself is not authoring power -- the door that installs them refuses
    // anything that would take an argument -- and no semantic header, no rule and no
    // identity of any of it appears in this file.
    //
    // WHAT THIS REPLACED, TWICE. CAT-0 left one line here that CALLED a package's
    // authoring function to manufacture the process's vocabulary; PROV-0 replaced it
    // with a hard-coded list of two artifacts to mount. LOAD-0 replaced THAT with a
    // file: this host no longer knows which artifacts to mount either. It knows HOW
    // to host what an artifact supplies, and it reads which artifacts to ask from
    // authored project intent.
    //
    // WHICH IS WHY THERE IS NO ARTIFACT STEM IN THIS FILE, and no mount order and no
    // mode. The Timer's artifact used to be named here twice -- once as a mount, once
    // as a boot -- because it is one artifact participating in two ways, and two
    // independent lists could only be maintained so that they happened to agree. It
    // is named once now, in a plan, as one record with two fields; and the law that
    // its provider contribution must be mounted BEFORE its weave is created (a
    // host-backed Timer validates that rule inside its own constructor, CAT-0) is
    // executed by `load_execute.hpp` rather than trusted to the order these lines
    // happen to be written in.
    //
    // A ROLE IS STILL NAMED HERE AND THAT IS A DIFFERENT KIND OF FACT. `kSkinRole`
    // and `kTimerRole` appear below inside GRANTS -- "this participant may say
    // SurfaceText to whoever holds `zengine.skin`" -- which is a statement about who
    // may be spoken to, decided by the party that composed this process. A role
    // cannot become a load: only a STEM can, and there is not one in this file.
    //
    // IT IS NOT A SINGLETON AND MUST NOT BECOME ONE. There is no process-wide
    // registry, no static, no accessor and no service locator: it is a local of
    // the host's own main, and every consumer that spends it does so because
    // this host handed it over during one load. A second Zengine host in this
    // process would own a second one, correctly, and neither would be "the"
    // catalog.
    //
    // ...AND THE DECLARATION ORDER IS THE LIFETIME CLAIM. These are declared
    // BEFORE the Kernel, so destruction -- which runs in reverse -- takes the
    // Kernel down first, and the Kernel destroys every artifact it holds before
    // the surface those artifacts point at goes anywhere. Nothing across the ABI
    // takes shared ownership to enforce that, because a lifetime the host
    // already controls does not need a refcount to be correct; it needs to be
    // stated, and this is where this host states it. The catalog also holds each
    // mounted provider's image open for as long as its contributions are installed,
    // which is the same promise one layer down and is the catalog's to keep.
    //
    // THE PLAN EXECUTOR IS DECLARED AFTER THE KERNEL, further down, and that is the
    // same claim from the other end: it retains the provider identities it mounted,
    // which is what a rollback needs, and it must not be the thing that outlives the
    // artifacts holding them.
    host.request_stop = [&bus] { bus.stop(); };

    op::Catalog operators;

    // ---- ...AND THE ONE THING A HOST MAY PUT IN ITS OWN CATALOG (SOURCE-0) ---
    //
    // THE HOST MAY DESCRIBE ITSELF; IT MAY NOT INVENT PROVIDER POWER. Two facts this
    // process already owns -- the project anchor it was launched into and which recipe
    // catalog is in force -- become routable here, as zero-input SOURCES, through one
    // door that judges every definition `is_source` before installing any of them. So
    // the paragraph above still holds where it matters: this file names no operator
    // semantics, authors no rule, and cannot reach a parameterized definition into the
    // catalog even by trying. `workshop/host_sources.hpp` holds the boundary, the two
    // identities and the schemas; what is decided HERE is only that the exposure
    // happens and that it happens once.
    //
    // ⚠ IT IS MOUNTED BEFORE THE PLAN RUNS, deliberately: the ordinary collision law
    // then answers a provider that would supply one of these identities, in words,
    // rather than letting load order decide which of two answers a maker gets.
    //
    // THE OWNERS OUTLIVE IT BY DECLARATION ORDER. Each Source's body reads
    // `current_recipes` and `host.project_dir` at the moment of the sample -- that is
    // what makes a live catalog swap show up without re-registration -- and both are
    // declared far above this line, so reverse-order destruction drops the catalog
    // holding those closures first.
    const op::MountReport exposed =
        mount_host_sources(operators, host_sources(host.project_dir, current_recipes));
    if (!exposed) {
        std::printf("zengine-workshop - host sources refused: %s\n"
                    "zengine-workshop - nothing was mounted and nothing was loaded.\n",
                    exposed.reason.c_str());
        return 6;
    }

    // ---- ...AND THE ONE READING A DURABLE OWNER TAKES OFF IT (MIG-0) ---------
    //
    // A SESSION FILE WRITTEN BY AN OLDER WORKSHOP NEEDS SOMEBODY TO TRANSLATE IT, and the
    // somebody is whatever conversion this run's plan happened to mount. This line is the
    // whole of the wiring: the persistence owner is handed the catalog to LOOK IN, and
    // looking is all it can do -- `session_persist` performs one `find` and one `evaluate`
    // and holds nothing between calls.
    //
    // ⚠ IT IS SET BEFORE THE PLAN RUNS AND READ LONG AFTER, which is the ordering this seam
    // turns on. What is in the catalog at the moment a session is read is whatever authored
    // realization has put there by then -- and an old file's version claim contributes
    // exactly nothing to that list. A claim selects among powers this host already has; it
    // reaches no load door, and there is nothing on this line that would give it one.
    //
    // THE CATALOG OUTLIVES THE WEAVE BY DECLARATION ORDER, `mount_host_sources`' own claim
    // about the same object one screen up: `operators` is declared above the Kernel, and
    // Workshop's weave is mounted below it.
    host.conversions = &operators;

    op::OperatorHostSurface operator_host(operators);

    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    // ---- The terminal participant Workshop presents (WT-1) -------------------
    //
    // AN ORDINARY WEAVE ON THE BUS THIS PROCESS ALREADY HAS. There is exactly one
    // Switchboard in this program and this participant is mounted on it, beside
    // Workshop's own weave, the boot weave and everything the Manager loads. A
    // second Loom would have made the pane a window onto a different world --
    // useless for looking at THIS one, and a whole second lifetime for the host
    // to own.
    //
    // THE HOST CHOOSES WHAT IT MAY KNOW AND WHAT IT MAY SAY, and the two are
    // separate in every direction. Knowing a shape is type knowledge; being
    // allowed to say one is authority, and it is the Kernel's answer, not this
    // participant's.
    loom::TerminalVocabulary terminal_vocab;
    terminal_vocab
        // The one ordinary verb, deliberately given: a line of text on the
        // screen's score slot. It is real -- whichever Skin holds `zengine.skin`
        // paints it -- and it is small: the participant cannot draw, cannot move
        // an object, cannot save and cannot quit.
        .knows(loom::schema_of<surface::SurfaceText>())
        // ...and the one it must never be able to USE. Knowing SurfaceCanvas is
        // what makes the refusal below a real measurement instead of a shape this
        // participant could not spell: `send * SurfaceCanvas <version>` composes
        // and is then refused by the Kernel, which is exactly the demonstration.
        //
        // THE VERSION IS NOT WRITTEN HERE ANY MORE, and neither is a namespace that
        // never existed. This comment used to read `zengine.SurfaceCanvas 1`, which
        // was wrong in both halves -- `ZEN_SHAPE` stringizes the struct name, so the
        // wire name is `SurfaceCanvas` with no prefix, and the version has since
        // moved twice. Nobody noticed because nothing showed a maker either fact.
        // HD-2's completion list now does, which is exactly why a comment that
        // spells a shape is a comment with a live owner: the pane says the name and
        // the version, out of the catalog, and this line is under no obligation to
        // repeat either.
        .knows(loom::schema_of<surface::SurfaceCanvas>())
        .accepts(loom::schema_of<loom::Ack>())
        .accepts(loom::schema_of<loom::Result>())
        .accepts(loom::schema_of<loom::Refused>());

    // ITS BASELINE IS ONE RULE, target-scoped to an OFFICE rather than to any
    // weave: it may say SurfaceText to whoever holds `zengine.skin` at delivery.
    // Being a terminal confers nothing else -- no allow_any, no observation, no
    // load capability, no reach to the Manager or the control door, and nothing
    // that lets it speak as Workshop.
    loom::Grant terminal_grant;
    terminal_grant.allow_to_role(surface::SurfaceText::zen_name,
                                 surface::SurfaceText::zen_version, surface::kSkinRole);
    const loom::MountedTerminal terminal = loom::host_mount_terminal(
        bus, std::make_unique<loom::TerminalSession>("workshop", std::move(terminal_vocab)),
        std::move(terminal_grant));
    // Non-owning, handed down the way `request_stop` is. The bus owns the
    // participant; Workshop's weave holds a pointer and inherits nothing from it.
    host.terminal = terminal.session;
    // THE BOOT LINE NAMES THE TERMINAL'S EFFECTIVE OPENER, NOT A LITERAL (KEY-0). The old
    // line printed `(shift+space opens it)` into, among others, a POSIX terminal that can
    // never produce shift+space -- the exact independently-authored claim the keymap
    // exists to end. The host reads the maker's keymap through the SAME loader and the
    // same admission the weave uses, so the two cannot disagree: a refused or absent file
    // means the defaults are active, which is precisely what the weave concludes too (and
    // the weave speaks the refusal on its own notice line).
    zengine::workshop::Keymap boot_keymap;
    if (!host.keymap_path.empty() && std::filesystem::exists(host.keymap_path)) {
        const keymap_persist::LoadedKeymap loaded =
            keymap_persist::load_file(host.keymap_path);
        if (loaded.outcome.accepted) {
            boot_keymap = loaded.keymap;
        }
    }
    std::printf("zengine-workshop - terminal: weave #%s (%s opens it)\n",
                std::to_string(terminal.id.value).c_str(),
                zengine::workshop::gesture_text(
                    boot_keymap.gesture_of(zengine::workshop::Act::kTerminalToggle))
                    .c_str());
    // Flushed like the four banner lines above it, and for the reason WT-1a met: a killed
    // process loses whatever is still in the buffer, and the line naming the identity the
    // pane speaks as is exactly the line somebody is reading when they kill it.
    std::fflush(stdout);

    // ---- The Builder tool, and the one thing that may start a process (BLD-0) ----
    //
    // TWO WEAVES, TWO OFFICES, TWO GRANTS, AND THE SPLIT IS THE POINT. Building
    // something is the first effect this repository has asked for that is not
    // "paint" or "open the file the host named", so the authority it needs is
    // written out here rather than acquired by a tool that happens to need it:
    //
    //   the RUNNER holds the catalog -- an absolute cmake, an absolute build
    //     tree and a target name, all decided at configure time -- and is the
    //     only weave in this program that starts a process. Its reach is the
    //     four observations it may report to whoever holds the Builder office,
    //     plus (ASYNC-1) two sentences to the Timer: ask for a beat, and give it
    //     back. It cannot paint, cannot publish, cannot load a weave and cannot
    //     reach the Manager or the control door.
    //
    //     THE TIMER RULES ARE THE PHASE'S ONE GRANT WIDENING, and they are worth
    //     reading as what they are: the right to ask a service for a heartbeat.
    //     A weave holding them can cause itself to be woken; it cannot cause
    //     anything else, cannot address anyone but the Timer with them, and gets
    //     nothing it could not have had by being loaded with a manifest that
    //     declared the same conversation. `EnsureRoleTimer` is in the composed
    //     emit set the binding layer brings and is deliberately NOT granted:
    //     this runner's beat belongs to this incarnation, which is the thing
    //     that holds the children, so the role-addressed form would be an
    //     authority for a promise it does not make.
    //   the TOOL is ordinary. It holds the target's NAME and its history, and
    //     its two rules are: order the runner, and say what it knows to anyone
    //     who accepts a BuildStatus. It holds no command and cannot spell one.
    //
    // WORKSHOP GAINS TWO SENTENCES AND NO POWERS. Its grant below adds the right
    // to ask the Builder what it is and to ask it for a build BY NAME. It cannot
    // reach the runner, and the name it may ask for is the one the tool told it.
    // A maker's Build therefore crosses two offices before it reaches a process,
    // and every hop is one line a reader can check.
    //
    // WHAT THIS IS NOT: containment. An in-process weave shares this process's
    // address space, so the grant bounds what these weaves may SAY, never what
    // they may TOUCH -- any code compiled into this binary could call the same
    // platform functions builder/run.hpp calls. The split buys one reviewable
    // place where process authority lives, which is worth having and is not the
    // same thing as a boundary the operating system enforces. Kernel::
    // containment_note() above already says this host isolates nothing, and this
    // is the same sentence about a different effect.
    //
    // ---- ...AND SINCE BLD-1 THE CATALOG IS A FILE (see the read, far above) ----
    //
    // The runner's catalog used to be one recipe this file constructed from three
    // compile definitions. It is authored project intent now, and what this host still
    // owns is the part that was never the file's: the PROGRAM. `ZENGINE_BUILDER_CMAKE`
    // is THE cmake that configured this tree, by absolute path, and it is handed to the
    // runner at construction where no message, poke or recipe can reach it. A recipe
    // names inputs -- a build tree and a target, or a source file and its packages --
    // and there is no field anywhere in which one could name a program.
    //
    // WHAT THE TOOL GETS IS LESS, AND THE SUBTRACTION IS THE SPLIT. `RecipeView` is an
    // identity, an artifact stem and the one file that stem means; the source paths,
    // build trees, package prefixes and link lists stay with the runner. So the Builder
    // panel can show a maker what this project builds without anything on the
    // presentation side ever holding a build procedure.
    //
    // ⚠ THE SUBTRACTION IS THE OWNER'S NOW, AND SO IS THE CUSTODY (PROJ-0). This file
    // used to derive the views into a local and hand each weave a vector to KEEP, which
    // left two long-lived catalogs beside the one it had just completed. The owner
    // derives the views beside the recipes they come from and both weaves read it, so
    // the split is exactly as wide as it was and there is one truth behind it.
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
    order_builds.allow_to_any(builder::RecipeCatalog::zen_name,
                              builder::RecipeCatalog::zen_version);
    // ---- THE ONE GRANT BLD-1 ADDS, AND EXACTLY WHAT IT IS WORTH ---------------
    //
    // `OfferArtifact` is the Builder tool's third sentence, and it is the only new
    // authority in this phase. What it can cause, at its very widest, is that THE ONE
    // artifact this project's realization is CURRENTLY STOPPED AT is realized now. It
    // cannot name a role, a mount mode or an order (the plan owns all three), it cannot
    // introduce an artifact the plan does not name, it cannot replace one that is
    // already live, it cannot reach a row the plan authors behind the one being waited
    // on (BLD-1a), and the PATH it carries is ignored by the owner that acts on it,
    // which resolves the stem with this host's own rule.
    //
    // ⚠ IT IS AN OFFER AND NOT AN ORDER, and BLD-1a's rename is what makes the grant
    // read as what it is. This tool asks; every eligibility rule and every refusal is
    // the realization owner's, in the owner's own words.
    //
    // WHY THAT IS SMALLER THAN IT LOOKS. The dangerous grant in this process is still
    // exactly one -- `zen.LoadWeave -> manager`, held by the plan booter and written by
    // this file. Nothing here gives the Builder that reach; it gives the Builder a way
    // to make an offer to a participant whose own answer to that offer is bounded by an
    // authored file. A Builder that could say `zen.LoadWeave` would be a different
    // phase, and this is deliberately not it.
    order_builds.allow_to_any(builder::OfferArtifact::zen_name,
                              builder::OfferArtifact::zen_version);
    const loom::WeaveId builder_tool = mount_in_office<builder::BuilderWeave>(
        bus, std::move(order_builds), builder::kBuilderRole, current_recipes.views());

    // THE RECIPE IS PRINTED IN PLAIN SCROLLBACK, beside the containment note and
    // for the same reason: what a button in this program will actually run is a
    // fact a maker is entitled to before they press it, and the panel cannot
    // show it until the runner has started it (the tool holds no command to
    // show). ASYNC-1 moved "until it has finished" to "until it has started",
    // which is a real improvement and still not "before".
    // Two lines, so the identities are as legible as the recipes.
    //
    // ⚠ IT CANNOT PRINT THE COMMAND ANY MORE, and the loss is real and correct. A
    // recipe becomes a command inside the runner, at the moment it is carried out --
    // which is what stopped Builder meaning one target baked in at configure time. What
    // this can still print, and does, is every recipe this project holds and the
    // artifact each is expected to produce; what actually ran reaches the panel the way
    // it has since ASYNC-1, from the participant that ran it, as it starts.
    std::printf("zengine-workshop - builder: weave #%s holds %zu recipe(s) (p opens the "
                "panel)\n",
                std::to_string(builder_tool.value).c_str(), current_recipes.views().size());
    std::printf("zengine-workshop - build runner: weave #%s builds with `%s`\n",
                std::to_string(runner.value).c_str(), ZENGINE_BUILDER_CMAKE);
    for (const builder::RecipeView& r : current_recipes.views()) {
        std::printf("zengine-workshop - recipe: %s -> %s\n", r.id.c_str(), r.path.c_str());
    }
    std::fflush(stdout);

    // Workshop's own reach: the right to SPEAK its screen, to ask the Builder
    // what it is, and to ask it for a build BY NAME -- and nothing else. It
    // commands no lifecycle, loads no weave and reaches no manager -- the boot
    // list below is the host's. A maker tool with a live document does not need
    // the dangerous grant to do its job, and giving it one "because Workshop
    // will eventually need it" is exactly the specialness these phases are
    // supposed to be counting.
    //
    // The two BLD-0 rules are ROLE-SCOPED and the choice of scope is the whole
    // of what Workshop was allowed to become: `to_role(builder)` and not
    // `to_any`, so Workshop can put these two sentences in front of the Builder
    // office and nowhere else -- not to the runner, not to the Manager, not to a
    // stranger who happens to accept the shape.
    //
    // WP-0 ADDED TWO RULES AND NO POWERS, and both are `to_any` for reasons that are
    // written down rather than convenient:
    //
    //   PaneCatalogRequested  the ask is a PUBLICATION, because knowing which offices have
    //                         panes is the very thing it is asking. There is no role to
    //                         scope it to until somebody has answered.
    //   PaneRoom              Workshop still sends this to exactly ONE resolved role -- the
    //                         office the accepted descriptor came in under -- but that role
    //                         is RUNTIME DATA, so no rule written here at boot can name it.
    //
    // SEL-0 ADDED A THIRD, `to_any` FOR `PaneRoom`'S REASON EXACTLY:
    //
    //   PanePressed           the destination is the office that offered the pane a maker
    //                         pressed, resolved at the moment of the press out of a runtime
    //                         catalog row. Same one-resolved-role send, same runtime data,
    //                         same impossibility of naming it here.
    //
    // MSG-0 ADDED A FOURTH AND A FIFTH, `to_any` FOR THE SAME REASON AGAIN:
    //
    //   PaneKey               the destination is the office that offered the pane a maker is
    //   PaneTextInput         typing into, resolved at the moment of the keystroke out of the
    //                         same runtime catalog row. Same one-resolved-role send, same
    //                         runtime data, same impossibility of naming it here.
    //
    // QR-18 ADDED A SIXTH, `to_any` FOR `PanePressed`'S REASON EXACTLY:
    //
    //   PaneWheel             the destination is the office that offered the pane under the
    //                         wheel, resolved at the moment of the notch out of the same
    //                         runtime catalog row. It carries the notches and nothing else.
    //
    // AND IT IS A RULE ABOUT WHAT WORKSHOP MAY SAY, NOT ABOUT WHAT IT MAY REACH. The sentence
    // carries a row and a column of a room Workshop itself granted -- or, since MSG-0, a key
    // a maker pressed while looking at that room; it commands nothing, asks nothing and
    // returns nothing. Being permitted to tell a provider where a hand landed, or what a
    // maker typed at it, is not being permitted to do anything to that provider.
    //
    // Everything else stays exactly as narrow as it was: the two Builder sentences are
    // still `to_role(builder)`, and Workshop still commands no lifecycle, loads no weave,
    // reaches no Manager, holds no observation authority and touches no filesystem,
    // process or network.
    //
    // AND IT IS MOUNTED IN AN OFFICE NOW, which is what makes the pane protocol sayable at
    // all: a provider addresses `zengine.workshop`, and Workshop authors its ask and its
    // grants deliberately AS that office so a provider can verify them. HOLDING THE OFFICE
    // IS NOT A SUPER-GRANT (MSG-07): every one of the rules above is still checked at every
    // send, and `mail.as_role(...)` adds a provenance stamp rather than a capability.
    //
    // THE OFFICE NAME IS `kWorkshopProvider` -- the SAME string the built-in catalog rows
    // carry as their provider key, and deliberately not a second constant that could drift
    // from it. That equality is a fact about the CURRENT trusted host composition (the
    // party that compiled the built-in panes is the party holding the office) and is
    // emphatically not a credential, not a signature, and not a cross-restart author claim.
    loom::Grant speak;
    speak.allow_to_any(surface::SurfaceCanvas::zen_name, surface::SurfaceCanvas::zen_version);
    speak.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    // TEXT-0 ADDED ONE RULE AND NO POWERS: a maker's copy is SAID to the process --
    // `to_any` because the interested parties are the active Skin (which offers the text to
    // the platform's clipboard) and any text-holding pane provider, neither of which this
    // host can name at boot. It carries text a maker already typed and commands nothing.
    speak.allow_to_any(surface::ClipboardCopy::zen_name, surface::ClipboardCopy::zen_version);
    // QR-11 ADDED THE READ, AND BOUND IT TO THE SKIN'S ROLE: clipboard read follows paste
    // intent, so the one thing Workshop may say about the platform's clipboard is a
    // question, asked of the Medium that owns it, when a maker pastes. The payload comes
    // back as the Skin's answer to that ask -- nothing here grants anybody a standing
    // clipboard feed, because none exists any more.
    speak.allow_to_role(surface::ClipboardTextRequested::zen_name,
                        surface::ClipboardTextRequested::zen_version, surface::kSkinRole);
    // WUX-3 ADDED THE PLACEMENT OFFER, BOUND TO THE SKIN'S ROLE FOR THE READ'S REASON:
    // the desktop belongs to the Medium, so the one thing Workshop may say about it is the
    // remembered placement it hands back at restore, addressed to whoever holds the
    // surface. It carries two opaque coordinates and a bool, commands nothing, and the
    // medium's own judgment (validate against live displays, adapt or refuse) is what
    // makes it safe to say at all.
    speak.allow_to_role(surface::SurfacePlacementRemembered::zen_name,
                        surface::SurfacePlacementRemembered::zen_version, surface::kSkinRole);
    speak.allow_to_role(builder::StatusRequested::zen_name,
                        builder::StatusRequested::zen_version, builder::kBuilderRole);
    speak.allow_to_role(builder::BuildRequested::zen_name, builder::BuildRequested::zen_version,
                        builder::kBuilderRole);
    speak.allow_to_any(PaneCatalogRequested::zen_name, PaneCatalogRequested::zen_version);
    speak.allow_to_any(PaneRoom::zen_name, PaneRoom::zen_version);
    speak.allow_to_any(PanePressed::zen_name, PanePressed::zen_version);
    speak.allow_to_any(PaneKey::zen_name, PaneKey::zen_version);
    speak.allow_to_any(PaneTextInput::zen_name, PaneTextInput::zen_version);
    speak.allow_to_any(PaneWheel::zen_name, PaneWheel::zen_version);
    mount_in_office<WorkshopWeave>(bus, std::move(speak), kWorkshopProvider, host);

    // ---- THE PLAN, PERFORMED (LOAD-0) ----------------------------------------
    //
    // THE PLAN BOOTER'S REACH: the Manager, target-scoped. This is the dangerous
    // grant in this process and it is held by the weave whose whole job is one round
    // of loading -- unchanged in kind from the boot weave it replaces, and THE HOST
    // STILL WRITES IT. `load_execute.hpp` performs the plan; it does not mint the
    // authority to perform one, and an executor that mounted its own weave with its
    // own grant would be an orchestration layer granting itself kernel reach.
    //
    // IT NO LONGER SPEAKS A STATUS LINE, and that subtraction is LOAD-0's: the old
    // boot weave printed a refusal AND published one to the screen, because a refused
    // load used to be something this process survived. A refused artifact stops the
    // project and is reported on stdout by the settle notice written below, so a
    // status line published to a painter that may itself be the thing that refused
    // is a message with nowhere to arrive.
    //
    // ---- MOUNTED BY HAND, FOR ONE REASON (BOOT-0) ----------------------------
    //
    // `loom::mount_granted` hands back a WeaveId, and the realization owner needs the
    // PARTICIPANT: an answer arriving at this booter has to reach the host-side owner
    // whose unfinished row it settles, and that wiring is made in the owner's
    // constructor and unmade in its destructor. Everything else here is what
    // `mount_granted` does, written out: construct, register with the host's grant,
    // tell it which weave it is.
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    // ---- ...AND ONE OBSERVATION IT MAY PUBLISH (BLD-1) ------------------------
    //
    // `ArtifactRealized` is realization's own sentence about a maker's BUILD & REALIZE:
    // what the project made of a newly built artifact, in the deepest layer's words,
    // whether it was taken or refused. It is an OBSERVATION and not a power -- nothing
    // in this process acts on it, the Builder tool folds it into what it publishes, and
    // the Builder panel shows it beside the build's own outcome. `to_any` because the
    // interested party is a presentation this host does not name.
    operate.allow_to_any(builder::ArtifactRealized::zen_name,
                         builder::ArtifactRealized::zen_version);
    load::BootAnswers answers;
    auto speaker = std::make_unique<load::PlanBooter>(answers);
    load::PlanBooter& voice = *speaker;
    const loom::WeaveId booter = bus.register_weave(std::move(speaker), std::move(operate));
    voice.zen_set_self(booter);

    // ---- WHAT REALIZES THE PROJECT, AND OUTLIVES EVERY TURN IT TAKES (BOOT-0) -
    //
    // DECLARED AFTER THE KERNEL, which is the second half of the lifetime claim the
    // catalog block above states. It retains the provider identity of every mount it
    // made -- which is what a rollback needs and what nothing else in this process
    // knows -- and since BOOT-0 it also holds the operator handoff it makes around a
    // load ACROSS HOST TURNS, so it must not be the thing that outlives the artifacts
    // holding them. That is also why it is a local of `main` and not a weave: a
    // registered weave is owned by the bus, which is declared BEFORE the catalog and
    // would therefore be destroyed after it. (The type is deliberately not named here:
    // this file does not know how to offer anything, and a tripwire says so.)
    //
    // IT IS HANDED THE ONE RULE THAT SPELLS A STEM AS A FILE, and that is the whole
    // of what the host contributes to resolution. `host.so` is the same function
    // every artifact path in this program has always come from; keeping it here is
    // what makes ONE authored plan legal on Linux and on Windows with no platform
    // field, no suffix in the file and no locator.
    //
    // ...AND THE ONE POLICY THAT IS THIS HOST'S AND NOT REALIZATION'S: what a
    // Workshop does about a project that finished, or stopped. See the lambda.
    bool project_refused = false;
    load::PlanExecutor executor(
        bus, operators, operator_host, voice, manager, answers,
        [&host](const std::string& stem) { return host.so(stem); },
        [&host, &operators, &project_refused](const load::Executed& done) {
            // SAID ARTIFACT BY ARTIFACT, IN THE ORDER IT HAPPENED. What each row of
            // this banner reports is RESOLVED truth -- the identity the artifact
            // declared, how many powers it supplied, the WeaveId this Kernel minted --
            // none of which is in the plan and none of which is written back to it.
            //
            // SAID WHEN REALIZATION SETTLES rather than when `main` gets control back,
            // and since BOOT-0 those are different moments: the last row settles inside
            // an ordinary delivery, with the host loop already running. The honest cost
            // is unchanged and stated rather than hidden -- whichever Skin the plan
            // named is live and painting by now, so on an interactive terminal these
            // lines are written into scrollback and then repainted over. They are on
            // stdout in full; a pipe, a redirect or a `--log` keeps every one.
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
            // ---- AND WHERE THIS RUN STOPPED, NAMED (BLD-1, BLD-1a) --------------
            //
            // A waiting row is not a failure and is not silence either. It is the
            // authored participation this run REACHED AND STOPPED AT, and a maker whose
            // project is short an artifact is entitled to know which one, that nothing
            // went wrong, and that the rest of their project is behind it. Named rather
            // than counted, for the reason the resolved rows above are: a number is not
            // something a maker can act on.
            //
            // ⚠ AND IT IS ONE NAME (BLD-1a). Realization stops at the first row it
            // cannot perform, so there is never a second one to print -- what used to
            // be a list was a list of rows that had been stepped over.
            if (!done.waiting_on.empty()) {
                std::printf("zengine-workshop - waiting to be built: %s (build it, and its "
                            "authored participation is performed then -- every authored row "
                            "after it is waiting on this one)\n",
                            done.waiting_on.c_str());
            }
            if (done.ok) {
                std::printf("zengine-workshop - operators: %zu resolvable, from %zu "
                            "provider(s)\n",
                            operators.size(), operators.providers().size());
                std::fflush(stdout);
                // COMPLETION ENDS NOTHING. A realized project is a fact about the
                // project; this host goes on being a host, which is what the loop
                // below is for.
                return;
            }
            // ---- ...AND NEITHER DOES WAITING (BLD-1a) --------------------------
            //
            // REALIZATION CAME TO REST WITHOUT COMPLETING AND WITHOUT REFUSING. The
            // row above says which artifact and the notice will be said again when a
            // maker's build lets the project finish, so there is nothing more to print
            // and nothing at all to end: a Workshop stopped at an artifact it can build
            // is precisely the Workshop that artifact gets built in.
            //
            // ⚠ THE ORDER OF THESE TWO TESTS IS THE HONESTY. `ok` false is not a
            // failure by itself any more -- a refusal is a refusal because somebody
            // SAID one, and `refusal` is where they said it.
            if (done.refusal.empty()) {
                std::fflush(stdout);
                return;
            }
            // ---- THIS HOST'S FAILURE POLICY, AND IT IS THE HOST'S ------------
            //
            // A REFUSED STARTUP PROJECT ENDS THIS WORKSHOP, exactly as it did before
            // BOOT-0 -- and it is written HERE, in the host, because it is a product
            // decision and not a fact about realization. The owner has no opinion
            // about process lifetime: it recorded which artifact refused, put back
            // what that row had mounted, kept every earlier row, and stopped.
            //
            // WHY THIS ONE IS STILL THE SMALLEST. The shipped plan's second row is
            // the SKIN. A Workshop that survived its refusal would have no painter
            // and no input -- a live process a maker cannot see or quit -- so
            // surviving a refusal is worth strictly less than saying so and leaving,
            // until there is a recovery surface to survive INTO. That is a later
            // phase's, and the owner it would need now exists.
            //
            // FAIL VISIBLE, AND NAME WHAT STANDS. The refusal already carries which
            // artifact, which participation step and the deepest layer's own
            // sentence; what this adds is the honest scope of what happened before
            // it, because a maker whose fourth artifact refused needs to know the
            // first three did not.
            std::printf("zengine-workshop - %s\n"
                        "zengine-workshop - the authored plan was not completed; %zu "
                        "artifact(s) participated before it stopped. Exiting.\n",
                        done.refusal.c_str(), done.resolved.size());
            std::fflush(stdout);
            std::fflush(stdout);
            project_refused = true;
            // THE SAME DOOR `q` LEAVES BY, and deliberately not a second one:
            // `host.quit` is what the loop below reads, and `request_stop` ends the
            // turn in flight -- necessary because that loop is inside
            // `drain_until_idle()` right now, which does not return on its own once
            // anything perpetual is live.
            host.quit = true;
            if (host.request_stop) {
                host.request_stop();
            }
        },
        // ---- IS THIS ROW WAITING ON THE MAKER? (BLD-1) ------------------------
        //
        // THE HOST ANSWERS IT BECAUSE ONLY THE HOST HOLDS BOTH HALVES. The artifact
        // file is absent -- this host owns the rule that spells a stem as a file -- AND
        // some authored recipe says this project can produce that stem. Realization
        // learns only the answer, never either half, and never why.
        //
        // ⚠ THE SECOND HALF IS WHAT KEEPS THIS FROM BEING "SKIP WHAT IS MISSING". An
        // artifact that is not on this disk and that nothing here can build is a broken
        // deployment, and it still refuses the plan by name, exactly as it did before
        // this phase. What changes is only the case where the project itself says how
        // the file is made: then its absence is a build state, and the honest thing is
        // to leave the row for the maker rather than to refuse the Workshop they would
        // have built it in.
        //
        // ⚠ AND IT IS NOT BUILD-ON-MISSING. Nothing here starts a build, asks for one,
        // or remembers to. The maker presses Build.
        //
        // ⚠ AND THE SECOND HALF IS ASKED OF THE OWNER (PROJ-0), at the moment the walk
        // asks -- never of a catalog this closure kept. "Some authored recipe produces
        // this stem" is a question about what this Workshop currently means, and a
        // predicate answering it from an older catalog would leave one row's fate
        // decided by a project nobody is in any more.
        [&host, &current_recipes](const std::string& stem) {
            for (const builder::Recipe& r : current_recipes.all()) {
                if (r.artifact != stem) {
                    continue;
                }
                return !std::filesystem::exists(std::filesystem::path(host.so(stem)));
            }
            return false;
        });

    // ---- WHAT THE PROJECT IS WAITING ON, ANSWERED ALIVE (BLD-2) ---------------
    //
    // THE OWNER DERIVES, THE HOST WIRES, THE WEAVE SPENDS. `waiting_on` and `behind`
    // are the realization owner's own derived answers — the same cursor `state_of`
    // reads, projected two more ways — and this function does nothing but read them
    // at the moment the Builder panel paints or the maker asks for the frontier. No
    // copy is taken anywhere on the path, which is what makes the panel's frontier
    // the owner's frontier at every instant rather than at the instant somebody
    // last remembered to refresh one.
    //
    // IT IS A READING AND NOT A POWER, and deliberately not a message: the weave it
    // is handed to is in-process host composition, wired the way `request_stop` and
    // the terminal pointer are. Nothing here lets a presentation perform a row,
    // start a build, or reorder anything — the one route from a maker's gesture to
    // a realized frontier is still BuildRequested -> the tool -> the runner ->
    // OfferArtifact -> the owner's own eligibility rules.
    host.frontier = [&executor] {
        ProjectFrontier now;
        now.artifact = executor.waiting_on();
        now.waiting = !now.artifact.empty();
        now.blocked = executor.behind();
        return now;
    };

    // ---- WHAT THIS HOST RESOLVED, ANSWERED TO WHOEVER ASKS (INTR-1) ----------
    //
    // A READ-ONLY OBSERVATION PARTICIPANT AND NOTHING MORE. It holds the realization
    // owner and the catalog as `const` references it does not own, and it answers two
    // questions with values: what this project asked to participate and what came of
    // it, and which operator powers currently resolve here and whose contribution
    // satisfies each.
    //
    // MOUNTED BEFORE REALIZATION BEGINS, AND THE ORDER IS THE HONESTY. The tool that
    // asks is an artifact THIS PLAN LOADS, so a door mounted afterwards would be
    // absent during the very window in which a pane might first be granted room --
    // and the pane would say `waiting` for a host that was in fact right here.
    // Mounted first, it answers what has resolved SO FAR at any moment it is asked,
    // which is a true sentence at every point on the timeline rather than only at the
    // end of it. SINCE BOOT-0 THAT SENTENCE HAS SOMETHING TO SAY MID-FLIGHT: the plan
    // is realized through ordinary deliveries, so an ask can genuinely land while a
    // row is loading, and `loading` is what it hears.
    //
    // ITS GRANT IS THE TWO ANSWERS AND NOTHING ELSE. `to_any` for the reason
    // `PaneRoom` is: Loom picks the recipient of an answer -- it is the weave that
    // asked -- so no rule written here at boot can name it. What that buys and what
    // it does not is written out in `workshop/arrangement.hpp`; the short version is
    // that this door can say two sentences, to askers only, and cannot mount,
    // unmount, overlay, evaluate, load or replace anything at all.
    //
    // IT IS NOT A SERVICE FRAMEWORK AND MUST NOT BECOME ONE. There is no registry
    // here, no locator, no second injected capability and no generic host API: a
    // participant that reads three of this host's own locals is the smallest thing
    // that could carry these facts across the boundary into a dynamically loaded
    // image, and it is deliberately shaped like the Weave Manager -- ask an office,
    // hear an answer -- rather than like a new mechanism.
    loom::Grant say_resolved;
    say_resolved.allow_to_any(ResolvedArrangement::zen_name, ResolvedArrangement::zen_version);
    say_resolved.allow_to_any(ResolvedPowers::zen_name, ResolvedPowers::zen_version);
    mount_in_office<ArrangementDoor>(bus, std::move(say_resolved), kArrangementRole, executor,
                                     operators, plan_path);

    // ---- AND THE ONE OFFICE THAT MAY RUN A SOURCE (SOURCE-1) ------------------
    //
    // A SECOND DOOR RATHER THAN A THIRD SENTENCE ON THE FIRST, and the subtraction is
    // the point. `ArrangementDoor`'s own header says it *cannot mount, unmount,
    // overlay, evaluate, load, unload, reload or replace anything*, and that sentence
    // is worth more than the file this one costs: after this line, *which office can
    // cause evaluation* still has a one-word answer, and it is not the office that
    // describes the project.
    //
    // WHAT IT CAN DO IS EXACTLY ONE THING. `op::sample(catalog, identity)` at the
    // spend -- current catalog truth, both gates, the three sentences those layers
    // already own -- and the admitted value rendered to prose HERE, where the schema
    // still is, because the asker is a woven weave whose accept-set could not name
    // the answer's shape. It holds the catalog as a `const` reference (evaluate is
    // `const`), caches no provider, definition, callable or answer, and is not a
    // generic host-RPC door: one shape in, one shape out.
    //
    // THE CATALOG OUTLIVES IT BY DECLARATION ORDER, the same claim `host_sources.hpp`
    // makes one layer in about the Sources' own owners: `operators` is declared far
    // above this line, so reverse-order destruction drops this door first.
    //
    // ITS GRANT IS THE ONE ANSWER. `to_any` for `PaneRoom`'s reason -- Loom picks the
    // recipient of an answer, it is the weave that asked, and no rule written here at
    // boot could name it.
    loom::Grant say_sampled;
    say_sampled.allow_to_any(SourceSampled::zen_name, SourceSampled::zen_version);
    mount_in_office<SampleDoor>(bus, std::move(say_sampled), kSampleRole, operators);

    // ---- BEGIN THE PROJECT, THEN GO AND BE A HOST (BOOT-0) --------------------
    //
    // THIS RETURNS BEFORE THE PROJECT IS REALIZED, and that is the phase. It mounts
    // every provider it can mount straight away and issues the first weave load, and
    // then it hands control back here with a row still in flight. What carries the
    // rest is the ordinary loop below -- the same one that would be running anyway --
    // and the load's own answer, which reaches the owner through the booter.
    //
    // WHAT USED TO BE ON THIS LINE was a straight-line call that performed the WHOLE
    // plan before returning, because it turned the bus itself -- 64 dispatch turns per
    // load -- to hear its own answers. That loop is deleted rather than moved: nothing
    // in this file counts turns for it, and nothing in `load_execute.hpp` does either.
    // The old verb is deliberately not spelled here: a tripwire reads this file for it,
    // which is how the deletion stays deleted.
    //
    // NOTHING BELOW KNOWS THE PLAN. There is no `if (realizing)`, no next-row check
    // and no completion test in the host loop; the owner is woken by an ordinary
    // delivery and reports through the notice written where it is constructed.
    executor.begin(read_plan.plan);

    // Everything runs inside drain_until_idle(): the input weave's own beat keeps
    // the queue alive, the Timer service's nap paces it, and `q` stops the bus.
    // This host wants the drain rather than the bounded turn -- it has nothing of
    // its own to do between turns. A call that returns with an empty queue means
    // nothing in this process will ever speak again -- say so and leave rather
    // than spin, snake's stance and for the same reason.
    //
    // IT IS ALSO WHAT REALIZES THE PROJECT NOW, without being told: the first
    // artifact's load answer is an ordinary delivery like any other, and so is every
    // fact that follows it.
    while (!host.quit) {
        bus.drain_until_idle();
        if (!host.quit && bus.pending() == 0) {
            std::printf("zengine-workshop - the bus went quiet without a quit "
                        "(no timer service deployed?): exiting.\n");
            break;
        }
    }

    // ---- what this run knew, and what it kept --------------------------------
    //
    // TWO LINES, BECAUSE THEY ARE TWO ANSWERS. The first is working memory at the
    // moment of shutdown; the second is what will still be true tomorrow. A
    // single "history" line would be the conflation this phase exists to end.
    //
    // The dump beside them is a WITNESS and not a product: it is the smallest
    // thing that makes the memory readable without a query surface, and a
    // Terminal, a Workshop panel or a debugger would read the same records and
    // format them for itself.
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
    // A REFUSED PROJECT IS STILL EXIT 4, which is what a script that already reads
    // this host's status expects. It is set by the failure policy above rather than
    // returned from a call, because realization settles inside a delivery now and
    // there is no `run()` to return it -- the same fact, carried the one way a
    // process that is already looping can carry it.
    return project_refused ? 4 : 0;
}
