// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-workshop — the first Workshop surface. One rectangle, selected,
// inspected, edited, refused, moved, resized, typed into.
//
// It is an ordinary Zengine application and holds no privilege snake does not:
// input arrives from the Input package's weave, time from the Timer package's,
// pixels-or-characters from whichever Skin holds `zengine.skin`, and the host
// owns the boot list and nothing else. Workshop paints by PUBLISHING intent — a
// SurfaceCanvas — exactly as the world publishes a SnakeVisual, and it never
// touches the terminal.
//
// THIS FILE IS THE HOST, and only the host: the boot weave, `main()`, the two
// grants, and the command-line arguments that say which file this Workshop's
// document lives in and which Skin and reader paint and watch it. Workshop's own
// weave lives in weave.hpp, where a suite can mount it, so
// `input message -> gesture -> semantic operation` is a chain the tests walk end
// to end instead of a claim a report has to make.
//
// THE HOST CHOOSES THE PATH AND THE WEAVE USES IT, which is the same division
// the boot list already follows: where things are is the host's business, what
// to do with them is the application's. Workshop still holds no privilege snake
// does not — it opens an ordinary file with an ordinary standard-library call,
// which is a power every program on this machine already has, and it needs no
// grant, no broker and no capability to do it (see the specialness ledger).

#include "weave.hpp"

#include "builder/runner.hpp"
#include "builder/vocabulary.hpp"
#include "builder/weave.hpp"
#include "input/vocabulary.hpp"
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
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace timer = zengine::timer;

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

/// The boot weave: it asks the Weave Manager to load the packages Workshop runs
/// on, and — the part that matters — it HEARS THE ANSWERS.
///
/// It exists because the first live run of this host had no such weave, and the
/// cost was immediate and instructive. "Failures are values": the Manager answers
/// its asker with zen.Result or zen.Refused. A root send carries no asker, so
/// every one of those answers was addressed to nobody, and a Workshop whose Skin
/// had refused to load looked exactly like a Workshop whose Skin had loaded — a
/// blank terminal and a host that exited saying the bus went quiet. The
/// diagnosis was not in the program at all.
///
/// So the asking is a weave with a grant, the way snake's operator is. It holds
/// the reach to the Manager — which is kernel reach, transitively, and is the
/// dangerous grant — and Workshop's own weave deliberately does not. Two
/// responsibilities, two grants: this one operates, that one authors.
struct BootState {
    std::int64_t answered = 0;
    std::int64_t refused = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(BootState, 1, ZEN_FIELD(answered), ZEN_FIELD(refused));
};

class BootWeave : public loom::WeaveBase<BootWeave, BootState,
                                        loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                        loom::Emit<loom::LoadWeave, surface::SurfaceText>> {
public:
    void on(const loom::Result&, loom::Mail&) { ++state_.answered; }
    void on(const loom::Ack&, loom::Mail&) { ++state_.answered; }

    /// A refusal is the only thing here worth saying out loud, and it is said
    /// twice on purpose: as status intent for whoever is painting, and on plain
    /// stdout — because the most likely thing to have been refused is the Skin,
    /// and a message about a missing painter delivered to the painter is not a
    /// message.
    void on(const loom::Refused& r, loom::Mail& mail) {
        ++state_.answered;
        ++state_.refused;
        std::printf("zengine-workshop - boot refused: %s\n", r.reason.c_str());
        std::fflush(stdout);
        mail.publish(surface::SurfaceText{surface::kSlotStatus,
                                          "[workshop] boot refused: " + r.reason});
    }
};

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
/// `--skin <stem>`, defaulted to the classic terminal Skin, is deliberately the
/// same shape. BUILDING a Skin and CHOOSING one are different acts:
/// `ZENGINE_SDL_SKIN=ON` only makes the window Skin exist, and without this flag
/// running Workshop against it would mean editing the literal below — a founder
/// experiment nobody else could repeat.
///
/// It is a WEAVE STEM and nothing more: the host already resolves stems to files
/// beside the executable (`host.so`), and the boot weave already reports a load
/// that is refused. So a wrong stem is answered by the machinery that exists,
/// which is why this is one string and not a skin registry, a config file, a
/// browser or a hot-swap. Choosing at launch is host policy; Workshop's own weave
/// still does not know which Skin holds the role, and must not.
///
/// `--input <stem>`, defaulted to the terminal/console reader, is the third of
/// the same shape rather than anything cleverer.
///
/// IT IS A SEPARATE FLAG FROM `--skin` ON PURPOSE, and the temptation it refuses
/// is real: `--skin zengine-skin-sdl` could obviously imply `zengine-input-sdl`,
/// and the graphical Workshop would then be one flag instead of two. But
/// presentation and input are two dimensions, not one. A backend states what it
/// saw, and which backend is watching is not
/// deducible from which one is painting. Coupling them here would write that
/// deduction into the host as a permanent architecture on the first day two
/// backends existed. Two flags say what is actually happening, and the banner
/// below prints both, which is what makes a run's active reader LEGIBLE rather
/// than inferred.
///
/// EXACTLY ONE READER IS LOADED, which is the other half of that legibility. The
/// host boots one input stem, so the terminal reader and the SDL reader are never
/// both live and one physical action cannot become two semantic messages. The
/// Loom would refuse the second anyway -- `zengine.input` is a singleton role --
/// so the guarantee is doubled rather than assumed, and neither half rests on OS
/// focus deciding who hears what.
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
    std::string skin = "zengine-skin-tui-classic";
    std::string input = "zengine-input";
    std::string log;  ///< empty = keep nothing durably
    std::string dump; ///< empty = write no snapshot of working memory at exit
};

Arguments parse_arguments(int argc, char** argv) {
    Arguments args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--document" || arg == "--setup" || arg == "--skin" || arg == "--input" ||
            arg == "--log" || arg == "--dump") {
            if (i + 1 >= argc) {
                args.ok = false;
                args.complaint =
                    arg + " needs a " +
                    (arg == "--skin" || arg == "--input" ? "weave stem" : "path");
                return args;
            }
            const std::string value = argv[++i];
            if (arg == "--skin") {
                args.skin = value;
            } else if (arg == "--input") {
                args.input = value;
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
    } else if (args.skin.empty()) {
        args.ok = false;
        args.complaint = "--skin needs a weave stem";
    } else if (args.input.empty()) {
        args.ok = false;
        args.complaint = "--input needs a weave stem";
    }
    return args;
}

int main(int argc, char** argv) {
    const Arguments args = parse_arguments(argc, argv);
    if (!args.ok) {
        std::printf("zengine-workshop - %s\n"
                    "usage: zengine-workshop [--document <path>] [--setup <path>]\n"
                    "                        [--skin <weave stem>] [--input <weave stem>]\n"
                    "                        [--log <path>] [--dump <path>]\n"
                    "the graphical Workshop is:\n"
                    "  zengine-workshop --skin zengine-skin-sdl --input zengine-input-sdl\n",
                    args.complaint.c_str());
        return 2;
    }

    // The honest line, in plain scrollback, exactly as snake's host prints it:
    // this host isolates nothing.
    std::printf("zengine-workshop - containment: %s\n", loom::Kernel::containment_note());
    std::printf("zengine-workshop - document: %s\n", args.document.c_str());
    std::printf("zengine-workshop - setup: %s\n", args.setup.c_str());
    std::printf("zengine-workshop - skin: %s\n", args.skin.c_str());
    std::printf("zengine-workshop - input: %s\n", args.input.c_str());
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
            journal.info("zengine.workshop",
                         "session started: document " + args.document + ", setup " +
                             args.setup + ", skin " + args.skin + ", input " + args.input);
        }
    } else {
        std::printf("zengine-workshop - log: nothing durable (--log <path> to keep one)\n");
    }
    if (!args.dump.empty()) {
        std::printf("zengine-workshop - dump at exit: %s (what memory still held)\n",
                    args.dump.c_str());
    }
    std::fflush(stdout);

    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    HostContext host;
    host.dir = exe_dir();
    host.document_path = args.document;
    host.setup_path = args.setup;
    host.request_stop = [&bus] { bus.stop(); };

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
    mount_in_office<WorkshopWeave>(bus, std::move(speak), kWorkshopProvider, host);

    // The boot weave's reach: the Manager, target-scoped, plus the right to speak
    // a status line. This is the dangerous grant in this process, and it is held
    // by the weave whose whole job is one round of loading.
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    operate.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    const loom::WeaveId booter = loom::mount_granted<BootWeave>(bus, std::move(operate));

    // Boot, as ordinary LoadWeave commands sent AS the boot weave -- so the
    // Manager's answers come back to something that can hear them, and a refused
    // load is a fact this program can report instead of a silence it exits on.
    // The Skin is first: loading it claims the terminal -- which includes asking
    // the terminal to report its POINTER, because terminal modes are output and
    // the output stream is claimed on the Skin's own lifetime -- and its hello is
    // what makes Workshop paint.
    const auto boot = [&](const char* stem, const char* role) {
        bus.send_as(booter, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{stem, host.so(stem), role}),
                                  booter, booter));
    };
    boot(args.skin.c_str(), surface::kSkinRole);
    boot(args.input.c_str(), input::kInputRole);
    boot("zengine-timer", timer::kTimerRole);

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
