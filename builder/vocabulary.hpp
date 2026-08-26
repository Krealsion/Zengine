// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_VOCABULARY_HPP
#define ZENGINE_BUILDER_VOCABULARY_HPP

// The Builder package's message vocabulary -- the shapes a build conversation is
// made of, and the two offices that hold its two halves.
//
// THE WHOLE POINT OF THIS PACKAGE IS A SPLIT, and the vocabulary is where it is
// visible. Building something means starting a process, which is the first
// effect any Zengine application has asked for that is not "paint" or "open the
// document the host named". So it is split across two weaves that the host
// mounts separately, with different grants:
//
//   the TOOL     holds NAMES. It knows which recipes exist and which ARTIFACT
//                each is expected to produce, remembers how the build of one is
//                going, and publishes that so any presentation can read it. It
//                cannot start a process, and it holds no command.
//   the RUNNER   holds the RECIPES, and is the only thing in the program that
//                turns one into a process. Its catalog comes from the host, it
//                answers a name it does not know with a refusal rather than a
//                guess -- and since ASYNC-1 it HOLDS the processes it started,
//                and says what it sees of them on an ordinary beat.
//
// BLD-1 CHANGED WHAT A NAME NAMES AND NOTHING ELSE ABOUT THAT SPLIT. The tool
// used to hold ONE name, chosen at configure time; it holds a catalog of authored
// recipe identities now, each with the artifact stem it produces. The runner used
// to hold one command, baked in beside it; it holds authored recipes now and
// DERIVES the command. Both halves grew a plural and neither gained a power: no
// shape below carries a program, an argument vector, a working directory or a
// shell line, and the recipe files a maker writes cannot name one either.
//
// So a maker's Build reaches an operating-system process through TWO offices and
// TWO grants, and neither of them is Workshop. Workshop may say `BuildRequested`
// to whoever holds `zengine.builder` and that is the whole of its new reach: it
// cannot order the runner, it cannot name a command, and nothing in this header
// gives it a way to spell one.
//
// ---------------------------------------------------------------------------
// COMMANDS AND OBSERVATIONS ARE DIFFERENT KINDS OF SENTENCE (ASYNC-1), and this
// file now has both. The distinction is not bookkeeping -- BLD-0 paid for
// confusing two of these in its first live run, when a panel announced a build
// that had finished minutes earlier:
//
//   COMMAND      BuildRequested{recipe}   "I want this."         an intent
//                RunBuild{recipe}         "Carry this out."
//                StatusRequested{}        "Say what you are."
//
//   OBSERVATION  BuildStarted{...}        "I saw a process begin."
//                BuildOutput{...}         "I saw it say this."     immutable,
//                BuildFinished{...}       "I saw it exit."         each about
//                BuildNotStarted{...}     "I saw it never begin."  ONE moment
//                ArtifactBuilt{...}       "the artifact is there." (BLD-1)
//                ArtifactRealized{...}    "and the project took it."
//
//   DERIVED      BuildStatus{...}         "Where this stands now." held, and
//                RecipeCatalog{...}       "What can be built here."recomputed
//
// AN OBSERVATION IS NEVER COLLAPSED INTO ANOTHER. `BuildStarted` and
// `BuildFinished` are two facts because they happen at two times, and a maker
// who can see the first is a maker who knows their build is alive. `BuildNotStarted`
// is a third because "there is no compiler" and "the compiler said no" are
// different problems -- which is BLD-0's `started`/`status` split, promoted from
// two fields of one shape into two shapes, now that the two can arrive minutes
// apart.
//
// THE OPERATION IDENTIFIER, and what it is NOT. `op` is a plain payload field,
// minted by the runner, that names one live external operation so that facts
// published after the initiating turn is gone can still be recognised as being
// about the same build. It is deliberately not a `WeaveId` (an operation is not
// a participant: it cannot be registered, granted, killed or revived) and
// deliberately not `Message::correlation` (that identifies a CONVERSATION -- one
// ask and its answer -- and these are later, uncorrelated observations that
// outlive any conversation). One identifier for one thing; a second would be the
// parallel-identifier trap.
//
// ITS SCOPE IS ONE LIVE RUNNER, AND THAT IS WRITTEN DOWN RATHER THAN DISCOVERED.
// The numbers come from a counter in the runner's own memory, so they mean
// something within one incarnation of that weave and nothing across two.
// builder/runner.hpp says why that limit is safe to accept HERE and exactly what
// would have to change before it is not.
//
// `op == 0` IS NOT AN OPERATION. It is the value on a `BuildNotStarted` that
// reports an ask which never became a process at all -- an unknown name, or a
// program that could not even be launched. Nothing was ever held, so there is
// nothing to name, and saying so with an absent number is more honest than
// minting an identity for a thing that never existed.
//
// WHAT IS DELIBERATELY ABSENT, so the absence is a decision:
//
//   - no command text ANYWHERE on the wire. `BuildRequested` and `RunBuild`
//     carry a recipe NAME, and a name is checked against a catalog the host
//     read out of an authored file. There is no shape here whose field is a
//     program, an argument vector, a working directory or a shell line, so "the
//     panel sent a command" is not a sentence this vocabulary can express. That
//     is BLD-0's hard boundary and it is enforced by the type, not by a check.
//     What comes BACK does describe what ran (`command`), because the only
//     honest moment to show a maker a command is after the thing that holds it
//     has run it.
//   - no recipe INPUTS on the wire either (BLD-1). A source path, a build tree,
//     a package prefix and a link list are the runner's to hold and the file's
//     to state; what travels is an identity and an artifact stem. So the widest
//     thing anything on this bus can say is "build the recipe called X", which
//     is exactly what it could say before recipes were authored at all.
//   - no CANCEL. There is no shape here that stops a build, and that absence is
//     ASYNC-1's, not an oversight: a cancel is a COMMAND ("please stop") and an
//     ending is an OBSERVATION ("it stopped"), and a phase that shipped the
//     first without deciding how the second is authored would have taught this
//     vocabulary to lie about the difference.
//   - no TIMEOUT and no "still not done". A running operation is simply running.
//     Nothing here publishes absence, and a maker who wants to know whether a
//     build is alive reads `BuildStatus` -- which says so because the runner
//     genuinely saw it, not because a clock ran out.
//   - no REPLACE, no unload, no reload, no swap. `ArtifactBuilt` says a file
//     exists and `ArtifactRealized` says a project took it; neither can be said
//     about an artifact that is already loaded, and there is no shape here that
//     could ask for one to be exchanged. BLD-1 does not earn hot reload and does
//     not pretend to: an already-loaded artifact is refused, in words.
//   - no build-on-missing, no automatic anything. Every build in this vocabulary
//     begins with a maker saying `BuildRequested`. Nothing here fires on a
//     missing file, a changed source or a failed load.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::builder {

/// The office the TOOL holds. Workshop's grant names this role rather than a
/// WeaveId, so who answers for it can change without Workshop's authority being
/// rewritten -- the same reason `zengine.skin` is a role.
inline constexpr const char* kBuilderRole = "zengine.builder";

/// The office the RUNNER holds -- the only holder of process authority in this
/// program. It is a separate role from the tool's precisely so that a grant can
/// name one without naming the other: the tool may reach the runner, and nothing
/// else may.
inline constexpr const char* kBuildRunnerRole = "zengine.build-runner";

/// The most output one `BuildOutput` carries.
///
/// A BOUND ON A MESSAGE, NOT ON A BUILD. The runner drains everything it can see
/// each time it looks and publishes what it drained; this is what keeps any one
/// fact a reasonable size when a build says a great deal between two looks. The
/// OLDEST characters are the ones dropped -- the end of a build's output is the
/// part that says what went wrong -- and how many were dropped travels with the
/// message rather than being swallowed, because a bounded surface that pretended
/// to be complete would trade a memory lie for an observability one.
inline constexpr std::size_t kMaxOutputChars = 2048u;

/// Where a build stands, as ONE value, so a presentation asks one question.
///
/// The failures are told apart because they are different problems: a maker who
/// asked for a name nobody knows, a build that broke, and a program that is not
/// installed each need a different next action.
namespace outcome {
inline constexpr std::int64_t kNeverBuilt = 0;   ///< nothing has been asked for yet
inline constexpr std::int64_t kAsked = 1;        ///< ordered; no process has been seen yet
inline constexpr std::int64_t kSucceeded = 2;    ///< the process exited 0 AND the artifact is there
inline constexpr std::int64_t kFailed = 3;       ///< the process ran and exited non-zero
inline constexpr std::int64_t kNotStarted = 4;   ///< the process never started at all
inline constexpr std::int64_t kUnknownRecipe = 5; ///< the name asked for is not one anybody knows
/// A process IS RUNNING right now -- the state BLD-0 had no word for, because a
/// build that held the pump could not be asked about while it held it. It is the
/// one value here that is a statement about the present rather than about a
/// finished thing, and it is the answer a panel opened mid-build receives.
inline constexpr std::int64_t kRunning = 6;
/// THE PROCESS SUCCEEDED AND THE ARTIFACT IS NOT THERE (BLD-1).
///
/// A SEVENTH VALUE BECAUSE IT IS A SEVENTH PROBLEM, and folding it into
/// `kSucceeded` is the exact lie this phase exists to refuse: a green build whose
/// product is absent has told a maker something true about a process and
/// something false about their project. It is also not `kFailed` -- nothing
/// failed, and saying so would send a maker to read compiler output that says
/// everything went fine. The usual cause is a recipe whose `artifact` or
/// `artifact_dir` does not describe what its target actually produces.
inline constexpr std::int64_t kNoArtifact = 7;
} // namespace outcome

/// WHERE THE REALIZATION OF A BUILT ARTIFACT STANDS -- a SECOND axis, and the
/// separation is the phase's own law.
///
/// A build outcome and a realization outcome are different truths with different
/// owners: the first is about a process and a file and belongs to the Builder,
/// the second is about a running project and belongs to the realization owner.
/// Collapsing them into one value would make "it built" and "the project took it"
/// indistinguishable exactly where a maker most needs them apart -- a successful
/// build whose load was refused is a completely different situation from a build
/// that failed, and both are ordinary.
namespace realization {
inline constexpr std::int64_t kNotAsked = 0;  ///< a plain BUILD; nothing was to be realized
inline constexpr std::int64_t kAsked = 1;     ///< BUILD & REALIZE, and the build is not done
inline constexpr std::int64_t kOffered = 2;   ///< the artifact was handed to the realization owner
inline constexpr std::int64_t kRealized = 3;  ///< the project took it
inline constexpr std::int64_t kRefused = 4;   ///< it was not taken, and `detail` says why
} // namespace realization

/// The tool's own name for each of those.
inline const char* name_of_realization(std::int64_t value) {
    switch (value) {
    case realization::kNotAsked: return "not asked";
    case realization::kAsked: return "asked";
    case realization::kOffered: return "offered";
    case realization::kRealized: return "realized";
    case realization::kRefused: return "REFUSED";
    default: return "unknown";
    }
}

/// The tool's own name for each of those, so a presentation does not invent a
/// seventh vocabulary for a fact the tool already has a word for.
inline const char* name_of_outcome(std::int64_t value) {
    switch (value) {
    case outcome::kNeverBuilt: return "not built yet";
    case outcome::kAsked: return "asked";
    case outcome::kRunning: return "running";
    case outcome::kSucceeded: return "succeeded";
    case outcome::kFailed: return "FAILED";
    case outcome::kNotStarted: return "did not start";
    case outcome::kUnknownRecipe: return "unknown recipe";
    case outcome::kNoArtifact: return "NO ARTIFACT";
    default: return "unknown";
    }
}

/// Did this outcome produce the artifact its recipe names?
///
/// ONE PLACE, because three readers need the same answer and `outcome == kSucceeded`
/// is exactly the sentence a later value would silently fall outside of. It is the
/// gate on realization: nothing is handed to a project on any other value.
inline bool artifact_produced(std::int64_t value) { return value == outcome::kSucceeded; }

/// Is this a condition the build is still IN, rather than one it ended at?
///
/// Written once, here, because three readers need the same answer and each of
/// them would otherwise carry its own list of which constants mean "not over
/// yet" -- three lists that agree until the day a seventh value is added.
inline bool still_going(std::int64_t value) {
    return value == outcome::kAsked || value == outcome::kRunning;
}

/// Every line there is -- the `how_many` a caller passes to `tail_lines` when it
/// wants the whole of what it handed over, joined, rather than a tail of it.
inline constexpr std::size_t kAllLines = static_cast<std::size_t>(-1);

/// The timer id the runner's observation beat rides, and how often it beats.
///
/// THE CADENCE IS THE GRANULARITY OF WHAT A MAKER SEES, and nothing else: it
/// bounds how soon a build's newest line can appear and how soon its ending can
/// be noticed, and it has no effect whatever on how long the build takes. 100ms
/// is ten looks a second -- fast enough that output reads as arriving, slow
/// enough that a chatty build does not repaint a screen faster than a maker can
/// read it. The Timer's own cap is 10ms (`zengine::timer::kBeatCapMs`), so this
/// is a multiple of the finest beat available rather than a demand for a new one.
///
/// THE BEAT ONLY EXISTS WHILE THERE IS SOMETHING TO LOOK AT. The runner cancels
/// it when it holds nothing and asks for it again when a build starts, so an
/// idle Workshop carries no Builder traffic at all -- which is what keeps "poll
/// inside the custodian" from becoming "poll forever, everywhere".
inline constexpr const char* kLookTimerId = "zengine.builder.look";
inline constexpr std::int64_t kLookBeatMs = 100;

/// LOOK AT WHAT YOU HOLD, NOW -- the same hands the beat opens, on request.
///
/// It exists for suites, diagnostics and hosts with no Timer service, and the
/// precedent is exact: `zengine::input::PumpInput` is the same door on the same
/// kind of weave, for the same reason. A weave that can only be driven by a
/// service that may not be deployed is a weave a test cannot drive at all, and a
/// second code path written for the test would prove something about the second
/// path.
///
/// IT WIDENS NOTHING. Looking is not starting: this cannot create an operation,
/// cannot name a target, and cannot make the runner do anything it was not
/// already holding. All it can cause is that observations the runner would have
/// published a beat later are published now.
struct LookAtBuilds {
    ZEN_SHAPE(LookAtBuilds, 1);
};

/// The last few lines of what a build said.
///
/// The END of the output, because that is where a compiler puts the reason and
/// where a successful build puts the thing it made. Whole lines, so a maker is
/// never shown half a path -- and a bounded number of them, because this is a
/// message that ends up on a panel with a handful of rows.
///
/// IT LIVES IN THE VOCABULARY AND NOT WITH THE RUNNER (moved by ASYNC-1), for a
/// reason worth one sentence: it is pure text arithmetic, the TOOL now needs it
/// to keep a readable tail of a build in flight, and the tool must never include
/// the header that starts processes. Which header a consumer names is the
/// visible half of this package's split, so a helper that both halves need
/// belongs on the side neither of them is refused.
inline std::string tail_lines(const std::string& text, std::size_t how_many) {
    std::size_t end = text.size();
    while (end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r')) {
        --end;
    }
    if (end == 0) {
        return {};
    }
    std::size_t start = end;
    std::size_t taken = 0;
    while (start > 0 && taken < how_many) {
        const std::size_t nl = text.rfind('\n', start - 1);
        if (nl == std::string::npos) {
            start = 0;
            ++taken;
            break;
        }
        start = nl;
        ++taken;
        if (start == 0) {
            break;
        }
    }
    if (start < end && text[start] == '\n') {
        ++start;
    }
    const std::string block = text.substr(start, end - start);
    // ONE LINE, WITH THE LINE BREAKS STILL VISIBLE AS BREAKS. A message that
    // travels as one string still has to say where the build's own lines ended:
    // turning them into spaces produced `Built target SDL3-shared [100%] Built
    // target zengine-snake` in the first live run, which reads as one sentence
    // that never happened. ` | ` is the smallest mark that keeps them apart.
    std::string out;
    std::string line;
    const auto flush = [&out, &line] {
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        if (line.empty()) {
            return;
        }
        if (!out.empty()) {
            out += " | ";
        }
        out += line;
        line.clear();
    };
    for (const char c : block) {
        if (c == '\n') {
            flush();
        } else if (c == '\r' || c == '\t') {
            line += ' ';
        } else {
            line += c;
        }
    }
    flush();
    return out;
}

// ---- commands ---------------------------------------------------------------

/// ASK THE TOOL to build the recipe it knows by this name -- and, optionally, to
/// hand the result to the running project when it works.
///
/// The name is here rather than implied, and that is what makes "the maker's
/// chosen recipe reaches the intended artifact" a checkable claim instead of a
/// hope: a request that names the wrong thing is refused by the tool and says
/// so, and a request that names the right thing can be seen naming it.
///
/// `realize` IS A FIELD AND NOT A SECOND SHAPE, and the asymmetry with
/// `BuildRequested`/`RunBuild` is deliberate. Those two are two shapes because
/// they are two AUTHORITIES -- "may ask for a build" and "may order the machine
/// that runs one" -- and a reader of the host's grants must not be able to
/// mistake one rule for the other. This is one authority with two intentions,
/// asked of one office, by one participant, in one sentence; and the tool has to
/// remember which intention it was for the whole length of a build, which is a
/// thing it can only do if the intention arrived with the ask.
///
/// v2 (BLD-1): `target` became `recipe` and `realize` joined. The rename is not
/// cosmetic -- a recipe now CONTAINS a CMake target, so a field called `target`
/// would name the wrong one of the two things in the room.
struct BuildRequested {
    std::string recipe;
    bool realize = false; ///< BUILD & REALIZE rather than BUILD
    ZEN_SHAPE(BuildRequested, 2, ZEN_FIELD(recipe), ZEN_FIELD(realize));
};

/// ASK THE TOOL TO SAY WHAT IT IS -- no fields, because the question has no
/// parameters: there is one tool at this office and it has one condition.
///
/// It exists because a presentation that has just been opened knows nothing, and
/// the honest way for it to learn is to ASK THE TOOL rather than to be handed
/// the tool's facts by whoever built the presentation. That is the panel/tool
/// split at its smallest: opening a Builder panel sends this, and everything the
/// panel then shows arrived as the tool's own answer -- including, since
/// ASYNC-1, that a build is running right now.
struct StatusRequested {
    ZEN_SHAPE(StatusRequested, 1);
};

/// ORDER THE RUNNER to carry out the recipe it holds for this name.
///
/// A SECOND SHAPE FOR WHAT LOOKS LIKE THE SAME SENTENCE, deliberately. One shape
/// with two destinations would mean one grant rule could be mistaken for the
/// other while reviewing them, and the difference between "may ask for a build"
/// and "may order the machine that runs one" is the entire authority story of
/// this package. Two shapes make the two rules unmistakable in the host.
///
/// IT CARRIES NO OPERATION NUMBER, and the absence is the design: an operation
/// is a live external thing, it does not exist until a process does, and the
/// participant that can see one begin is the only one that can honestly name it.
/// An order is not an operation; it is a request that one be created.
struct RunBuild {
    std::string recipe;
    ZEN_SHAPE(RunBuild, 2, ZEN_FIELD(recipe));
};

// ---- observations -----------------------------------------------------------

/// A PROCESS BEGAN. Reported by the participant that started it, to whoever
/// holds `zengine.builder`.
///
/// `command` travels here rather than being known up front because the tool must
/// not hold a command -- so the first honest moment to say what is running is
/// the moment it started running, said by the thing that started it. BLD-0 could
/// only ever say this at the END; a build a maker can watch has to say it at the
/// beginning.
///
/// v2 (BLD-1): `target` became `recipe` (the identity) and `recipe` became
/// `command` (what is running). Both names moved to the thing they are true of.
struct BuildStarted {
    std::int64_t op = 0;
    std::string recipe;  ///< the authored recipe this operation is carrying out
    std::string command; ///< what is running, as one line
    ZEN_SHAPE(BuildStarted, 2, ZEN_FIELD(op), ZEN_FIELD(recipe), ZEN_FIELD(command));
};

/// A RUNNING PROCESS SAID SOMETHING -- output observed since the last look, and
/// never anything already reported.
///
/// `dropped` is how many characters of THIS observation did not fit in `text`
/// (`kMaxOutputChars` above). It is on the wire rather than swallowed for the
/// same reason `loom::BoundedHistory` exposes its eviction count: a reader that
/// is shown a bounded thing has to be able to tell that it is bounded.
struct BuildOutput {
    std::int64_t op = 0;
    std::string recipe;
    std::string text;
    std::int64_t dropped = 0;
    ZEN_SHAPE(BuildOutput, 2, ZEN_FIELD(op), ZEN_FIELD(recipe), ZEN_FIELD(text),
              ZEN_FIELD(dropped));
};

/// A PROCESS EXITED, and was reaped. The end of one operation.
///
/// It carries no `started` flag, because by the time this can be said the answer
/// is known to be yes -- something that never started gets `BuildNotStarted`
/// instead. That is the same distinction BLD-0 drew with two fields of one
/// shape, drawn now with two shapes, because the two facts no longer arrive at
/// the same moment.
/// ⚠ IT IS ABOUT A PROCESS AND NOT ABOUT AN ARTIFACT. `status == 0` means the
/// build system was satisfied; whether the file the recipe names actually exists
/// is a DIFFERENT question with a different owner, asked by the tool and answered
/// by `ArtifactBuilt` (or by `outcome::kNoArtifact`). Widening this shape to
/// carry the answer would put artifact-domain knowledge in the one participant
/// whose whole discipline is that it holds only process custody.
struct BuildFinished {
    std::int64_t op = 0;
    std::string recipe;
    std::int64_t status = 0; ///< the child's exit status
    ZEN_SHAPE(BuildFinished, 2, ZEN_FIELD(op), ZEN_FIELD(recipe), ZEN_FIELD(status));
};

/// NO PROCESS RAN. The ask reached the runner and nothing was ever built by it.
///
/// `op` is 0 when nothing was ever held -- an unknown name, or a launch that
/// failed where it was attempted. It is a REAL operation number in the one case
/// where a child existed and never became the program it was meant to be: on
/// POSIX a failed `exec` can only report itself as an exit status, so that fact
/// arrives after the operation was already announced (builder/run.hpp explains
/// the platform asymmetry). Saying it with the operation's own number is what
/// keeps the story of one operation readable end to end.
struct BuildNotStarted {
    std::int64_t op = 0;
    std::string recipe;  ///< the authored recipe that was asked for
    std::string command; ///< what would have run, when there was one
    std::string trouble; ///< why nothing did
    ZEN_SHAPE(BuildNotStarted, 2, ZEN_FIELD(op), ZEN_FIELD(recipe), ZEN_FIELD(command),
              ZEN_FIELD(trouble));
};

// ---- derived state ----------------------------------------------------------

/// THE TOOL'S OWN STATE, PUBLISHED -- the shape a presentation reads.
///
/// Published rather than fetched, because a panel is not the tool's owner and
/// must not be its only reader: anything on this bus that accepts this shape
/// sees the same facts, and the Builder panel is simply one of them. It is also
/// what keeps a closed panel honest -- the tool goes on counting whether or not
/// anybody is looking, and `builds` is the field that proves it.
///
/// v2 (ASYNC-1): `op` and `chunks` joined the shape. `op` is which operation
/// this status is about -- 0 when none has ever been held -- and `chunks` is how
/// many times the runner has been heard to say something about it. `chunks` is
/// there because it is the number that makes "this build is alive" VISIBLE
/// rather than asserted: it climbs while a maker does other work, and a build
/// that had frozen would leave it still.
/// v3 (BLD-1): `target` became `recipe`, `recipe` became `command`, and four
/// fields joined -- `artifact` (which artifact this recipe produces), and the
/// three that carry the SECOND axis: `realize` (was this a BUILD & REALIZE?),
/// `realization` (where that stands) and `realized_detail` (its own words). A
/// panel therefore reads two outcomes and never has to derive one from the other.
struct BuildStatus {
    std::string recipe;      ///< the recipe this picture is about; empty before any ask
    std::string artifact;    ///< the artifact stem that recipe produces
    std::int64_t outcome = outcome::kNeverBuilt;
    std::int64_t status = 0; ///< the last process's exit status
    std::string command;     ///< what the runner said it ran, once it has run something
    std::string detail;      ///< the last thing the runner said, or the tool's own refusal
    std::int64_t builds = 0; ///< how many builds this TOOL has been asked for, ever
    std::int64_t op = 0;     ///< which operation this is about; 0 = none has been held
    std::int64_t chunks = 0; ///< output observations folded in for THIS operation
    bool realize = false;    ///< the maker asked for BUILD & REALIZE
    std::int64_t realization = realization::kNotAsked;
    std::string realized_detail; ///< realization's own sentence, when it has one
    ZEN_SHAPE(BuildStatus, 3, ZEN_FIELD(recipe), ZEN_FIELD(artifact), ZEN_FIELD(outcome),
              ZEN_FIELD(status), ZEN_FIELD(command), ZEN_FIELD(detail), ZEN_FIELD(builds),
              ZEN_FIELD(op), ZEN_FIELD(chunks), ZEN_FIELD(realize), ZEN_FIELD(realization),
              ZEN_FIELD(realized_detail));
};

/// ONE ROW OF WHAT CAN BE BUILT HERE.
///
/// TWO FIELDS AND NOT THE RECIPE. A presentation needs to name a recipe and to say
/// what it makes; it does not need the source path, the build tree, the package
/// prefixes or the link list, and handing them to it would put a build procedure on
/// a screen that has no way to act on one. The split is `builder/recipe.hpp`'s
/// `RecipeView`, said on the wire.
struct RecipeSummary {
    std::string recipe;
    std::string artifact;
    ZEN_SHAPE(RecipeSummary, 1, ZEN_FIELD(recipe), ZEN_FIELD(artifact));
};

/// WHAT THIS PROGRAM CAN BUILD -- published by the tool when it is asked what it is.
///
/// A SECOND SHAPE RATHER THAN A LIST ON `BuildStatus`, because they answer questions
/// that change at completely different rates: a status is republished on every line
/// a compiler says, and a catalog is fixed for the life of the process. Folding the
/// second into the first would put the whole catalog on the bus a few hundred times
/// per build.
///
/// IT IS THE TOOL'S OWN VIEW COMING BACK. The host read a file, the host gave the
/// tool the view, and this is the tool saying what it was given -- so a presentation
/// showing three recipes is showing three recipes the tool will actually accept.
struct RecipeCatalog {
    std::vector<RecipeSummary> recipes;
    ZEN_SHAPE(RecipeCatalog, 1, ZEN_FIELD(recipes));
};

/// THE ARTIFACT A RECIPE NAMES IS NOW ON DISK, AND THIS BUILD PUT IT THERE.
///
/// A FIFTH OBSERVATION, AND IT IS NOT A RESTATEMENT OF `BuildFinished`. That one is
/// about a PROCESS: it says a child exited and with what status, which is everything
/// the participant holding process custody can honestly know. This one is about an
/// ARTIFACT: a named file, at a known path, that a recipe said it would produce and
/// that has been looked at since the build ended. Reconstructing it from the other
/// facts would mean every interested party learning the recipe-to-artifact mapping,
/// the host's rule for spelling a stem as a file, and the difference between a build
/// system's idea of success and a file's existence -- three things with owners.
///
/// ⚠ IT IS SAID ONLY WHEN THE MAKER ASKED FOR REALIZATION. A plain BUILD produces a
/// file too, and says so in `BuildStatus`; what this shape carries is an INTENT that
/// something be done with the result, so publishing it after every build would put a
/// standing offer on the bus that nobody made.
///
/// `path` IS HERE SO NOTHING DOWNSTREAM HAS TO SPELL A STEM. The host owns that rule
/// (a directory, a separator, `.so`/`.dll`); a reader that re-derived it would be a
/// second copy of a rule that is deliberately written once.
struct ArtifactBuilt {
    std::int64_t op = 0;  ///< the operation that produced it
    std::string recipe;   ///< the recipe that names it
    std::string artifact; ///< the artifact STEM -- the name a project's plan would use
    std::string path;     ///< the exact file, as the host spells it
    ZEN_SHAPE(ArtifactBuilt, 1, ZEN_FIELD(op), ZEN_FIELD(recipe), ZEN_FIELD(artifact),
              ZEN_FIELD(path));
};

/// WHAT THE RUNNING PROJECT MADE OF A NEWLY BUILT ARTIFACT.
///
/// THE BUILDER DOES NOT SAY THIS ONE. It is realization's own sentence, said by the
/// participant that speaks for the realization owner, and the Builder tool merely
/// HEARS it so that one presentation can show a maker both halves of what they asked
/// for. The shape lives in this file because it is the second half of a conversation
/// this file already holds; a vocabulary organised by speaker rather than by
/// conversation would split six shapes across four headers to say one thing.
///
/// `realized` IS A BOOL AND `detail` IS ALWAYS PRESENT. A refusal without words is
/// the failure mode this whole repository keeps refusing: `detail` carries the
/// deepest layer's own sentence -- the plan's, the catalog's, the loader's -- and on
/// the accepting path it says what participated.
struct ArtifactRealized {
    std::string artifact;
    bool realized = false;
    std::string detail;
    ZEN_SHAPE(ArtifactRealized, 1, ZEN_FIELD(artifact), ZEN_FIELD(realized),
              ZEN_FIELD(detail));
};

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_VOCABULARY_HPP
