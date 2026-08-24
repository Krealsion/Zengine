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
#include "load_execute.hpp"
#include "load_persist.hpp"
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
/// `--load-plan <path>`, defaulted to `default-load-plan.json` BESIDE THE
/// EXECUTABLE, is the third of the same shape and is the one this host cannot run
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
    /// Empty means "the one shipped beside this executable", which `main()` resolves
    /// once it knows where that is. It is deliberately NOT defaulted to a bare name
    /// here: a bare name would resolve against whatever directory a maker happened to
    /// launch from, which is right for a document and wrong for a plan naming
    /// artifacts staged beside the binary.
    std::string load_plan;
    std::string log;  ///< empty = keep nothing durably
    std::string dump; ///< empty = write no snapshot of working memory at exit
};

Arguments parse_arguments(int argc, char** argv) {
    Arguments args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--document" || arg == "--setup" || arg == "--load-plan" || arg == "--log" ||
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
                    "                        [--load-plan <path>]\n"
                    "                        [--log <path>] [--dump <path>]\n"
                    "the graphical Workshop is the second plan shipped beside this binary:\n"
                    "  zengine-workshop --load-plan <workshop dir>/%s\n",
                    args.complaint.c_str(), load_persist::kGraphicalLoadPlanName);
        return 2;
    }

    // WHERE THE HOST'S OWN FILES ARE, resolved once and before anything is said
    // about them, because the load plan's default IS this directory and a banner
    // that named a path this host had not resolved would be naming a guess.
    HostContext host;
    host.dir = exe_dir();
    host.document_path = args.document;
    host.setup_path = args.setup;

    const std::string plan_path =
        args.load_plan.empty() ? host.dir + "/" + load_persist::kDefaultLoadPlanName
                               : args.load_plan;

    // The honest line, in plain scrollback, exactly as snake's host prints it:
    // this host isolates nothing.
    std::printf("zengine-workshop - containment: %s\n", loom::Kernel::containment_note());
    std::printf("zengine-workshop - document: %s\n", args.document.c_str());
    std::printf("zengine-workshop - setup: %s\n", args.setup.c_str());
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
    // The catalog below starts EMPTY and this file publishes nothing into it. What
    // fills it is mounting artifacts that say, across a C seam, "I supply these
    // definitions" -- and what this host then owns is the live resolution: which
    // contribution currently satisfies each logical power, which are shadowed
    // beneath it, and what happens to both when a provider goes away.
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
    std::printf("zengine-workshop - terminal: weave #%s (shift+space opens it)\n",
                std::to_string(terminal.id.value).c_str());
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
    std::vector<builder::BuildRecipe> catalog;
    catalog.push_back(builder::BuildRecipe{
        ZENGINE_BUILDER_TARGET, ZENGINE_BUILDER_CMAKE,
        {"--build", ZENGINE_BUILDER_BUILD_DIR, "--target", ZENGINE_BUILDER_TARGET},
        ZENGINE_BUILDER_BUILD_DIR});

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
        bus, std::move(run_builds), builder::kBuildRunnerRole, catalog);

    loom::Grant order_builds;
    order_builds.allow_to_role(builder::RunBuild::zen_name, builder::RunBuild::zen_version,
                               builder::kBuildRunnerRole);
    order_builds.allow_to_any(builder::BuildStatus::zen_name, builder::BuildStatus::zen_version);
    const loom::WeaveId builder_tool = mount_in_office<builder::BuilderWeave>(
        bus, std::move(order_builds), builder::kBuilderRole, std::string(ZENGINE_BUILDER_TARGET));

    // THE RECIPE IS PRINTED IN PLAIN SCROLLBACK, beside the containment note and
    // for the same reason: what a button in this program will actually run is a
    // fact a maker is entitled to before they press it, and the panel cannot
    // show it until the runner has started it (the tool holds no command to
    // show). ASYNC-1 moved "until it has finished" to "until it has started",
    // which is a real improvement and still not "before".
    // Two lines, so the identities are as legible as the command.
    std::printf("zengine-workshop - builder: weave #%s builds `%s` (p opens the panel)\n",
                std::to_string(builder_tool.value).c_str(), ZENGINE_BUILDER_TARGET);
    std::printf("zengine-workshop - build runner: weave #%s runs `%s`\n",
                std::to_string(runner.value).c_str(), catalog.front().as_line().c_str());
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
    speak.allow_to_role(builder::StatusRequested::zen_name,
                        builder::StatusRequested::zen_version, builder::kBuilderRole);
    speak.allow_to_role(builder::BuildRequested::zen_name, builder::BuildRequested::zen_version,
                        builder::kBuilderRole);
    speak.allow_to_any(PaneCatalogRequested::zen_name, PaneCatalogRequested::zen_version);
    speak.allow_to_any(PaneRoom::zen_name, PaneRoom::zen_version);
    speak.allow_to_any(PanePressed::zen_name, PanePressed::zen_version);
    speak.allow_to_any(PaneKey::zen_name, PaneKey::zen_version);
    speak.allow_to_any(PaneTextInput::zen_name, PaneTextInput::zen_version);
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
    // IT NO LONGER SPEAKS A STATUS LINE, and that subtraction is the phase's:
    // the old boot weave printed a refusal AND published one to the screen, because
    // a refused load used to be something this process survived. A refused artifact
    // now stops startup and is reported on stdout by the executor's caller, so a
    // status line published to a painter that may itself be the thing that refused
    // is a message with nowhere to arrive.
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    load::BootAnswers answers;
    const loom::WeaveId booter =
        loom::mount_granted<load::PlanBooter>(bus, std::move(operate), answers);

    // DECLARED AFTER THE KERNEL, which is the second half of the lifetime claim the
    // catalog block above states. It retains the provider identity of every mount it
    // made -- which is what a rollback needs and what nothing else in this process
    // knows -- and it must not be the thing that outlives the artifacts holding them.
    //
    // IT IS HANDED THE ONE RULE THAT SPELLS A STEM AS A FILE, and that is the whole
    // of what the host contributes to resolution. `host.so` is the same function
    // every artifact path in this program has always come from; keeping it here is
    // what makes ONE authored plan legal on Linux and on Windows with no platform
    // field, no suffix in the file and no locator.
    load::PlanExecutor executor(bus, operators, operator_host, booter, manager, answers,
                                [&host](const std::string& stem) { return host.so(stem); });

    // ---- WHAT THIS HOST RESOLVED, ANSWERED TO WHOEVER ASKS (INTR-1) ----------
    //
    // A READ-ONLY OBSERVATION PARTICIPANT AND NOTHING MORE. It holds the authored
    // plan, the executor and the catalog as `const` references it does not own, and
    // it answers two questions with values: what this project asked to participate
    // and what came of it, and which operator powers currently resolve here and
    // whose contribution satisfies each.
    //
    // MOUNTED BEFORE THE PLAN RUNS, AND THE ORDER IS THE HONESTY. The tool that asks
    // is an artifact THIS PLAN LOADS, so a door mounted afterwards would be absent
    // during the very window in which a pane might first be granted room -- and the
    // pane would say `waiting` for a host that was in fact right here. Mounted first,
    // it answers what has resolved SO FAR at any moment it is asked, which is a true
    // sentence at every point on the timeline rather than only at the end of it.
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
    mount_in_office<ArrangementDoor>(bus, std::move(say_resolved), kArrangementRole,
                                     read_plan.plan, executor, operators, plan_path);

    const load::Executed performed = executor.run(read_plan.plan);

    // SAID ARTIFACT BY ARTIFACT, IN THE ORDER IT HAPPENED. What each row of this
    // banner reports is RESOLVED truth -- the identity the artifact declared, how
    // many powers it supplied, the WeaveId this Kernel minted -- none of which is in
    // the plan and none of which is written back to it.
    //
    // SAID AFTER THE RUN RATHER THAN DURING IT, and the honest cost is stated rather
    // than hidden: by the time this loop runs, whichever Skin the plan named is live
    // and painting, so on an interactive terminal these lines are written into
    // scrollback and then repainted over. They are on stdout in full -- a pipe, a
    // redirect or a `--log` keeps every one -- and that is the same condition
    // anything this host says after its Skin is live has always had. The line that
    // must survive is the REFUSAL below, and it does: a refused plan stops the
    // process, so nothing repaints after it.
    for (const load::ResolvedArtifact& done : performed.resolved) {
        std::string said = done.stem;
        if (done.provider_mounted) {
            said += " | provider '" + done.provider + "' supplied " +
                    std::to_string(done.contributed);
        }
        if (done.weave_loaded) {
            said += " | weave #" + std::to_string(done.weave.value) + " as " + done.role;
            said += done.offer == op::OfferOutcome::Offered
                        ? " (this host's operator resolution offered)"
                        : " (ordinary weave: it declares no operator surface)";
        }
        std::printf("zengine-workshop - loaded: %s\n", said.c_str());
    }
    if (!performed.ok) {
        // FAIL VISIBLE, AND NAME WHAT STANDS. The refusal already carries which
        // artifact, which participation step and the deepest layer's own sentence;
        // what this adds is the honest scope of what happened before it, because a
        // maker whose fourth artifact refused needs to know the first three did not.
        std::printf("zengine-workshop - %s\n"
                    "zengine-workshop - the authored plan was not completed; %zu artifact(s) "
                    "participated before it stopped. Exiting.\n",
                    performed.refusal.c_str(), performed.resolved.size());
        std::fflush(stdout);
        return 4;
    }
    std::printf("zengine-workshop - operators: %zu resolvable, from %zu provider(s)\n",
                operators.size(), operators.providers().size());
    std::fflush(stdout);

    // Everything runs inside pump(): the input weave's own beat keeps the queue
    // alive, the Timer service's nap paces it, and `q` stops the bus. A pump
    // that returns with an empty queue means nothing in this process will ever
    // speak again -- say so and leave rather than spin, snake's stance and for
    // the same reason.
    while (!host.quit) {
        bus.pump();
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
    return 0;
}
