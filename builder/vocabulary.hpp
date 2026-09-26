// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_VOCABULARY_HPP
#define ZENGINE_BUILDER_VOCABULARY_HPP

// The Builder's vocabulary: the shapes of one build conversation, across two offices. The tool
// (`kBuilderRole`) holds recipe names and the artifact each makes and follows a build; it holds
// no command and starts nothing. The runner (`kBuildRunnerRole`) holds the recipes and the
// processes it starts. Workshop may only ask the tool. No shape carries a program, an argument,
// a directory or a recipe input, and there is no cancel and no timeout.
// Builder law: agents/realization.md

// Commands (`BuildRequested`, `RunBuild`, `StatusRequested`, `OfferArtifact`, `PromoteArtifact`,
// `RevertArtifact`) are intents; observations each report one moment and are never collapsed.
// `op`, minted by the runner, names one live operation so later facts say which build they are
// about: not a WeaveId (an operation is no participant), not a correlation (it outlives any
// conversation), meaningful within one runner's life (builder/runner.hpp); `op == 0` is none.
// Reference: docs/reference/builder.md.

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

/// The office the REALIZATION OWNER'S voice holds: the participant that hears `OfferArtifact`,
/// `PromoteArtifact` and `RevertArtifact` and says what came of each (Workshop's `PlanBooter`).
/// An office so that its words can be followed by name -- an observer names the office it
/// follows, never a WeaveId -- and it grants nobody anything to say to it.
inline constexpr const char* kRealizationRole = "zengine.realization";

/// The most output one `BuildOutput` carries, in bytes: a bound on a message, not on a build.
/// The runner says all it drained, in as many messages as that takes, in order.
// WL-OUT-01 -- agents/workshop/build-output.md
inline constexpr std::size_t kMaxOutputChars = 2048u;

/// Where a build stands, as one value. The failures are told apart because each needs a
/// different next action.
namespace outcome {
inline constexpr std::int64_t kNeverBuilt = 0;   ///< nothing has been asked for yet
inline constexpr std::int64_t kAsked = 1;        ///< ordered; no process has been seen yet
inline constexpr std::int64_t kSucceeded = 2;    ///< the process exited 0 AND the artifact is there
inline constexpr std::int64_t kFailed = 3;       ///< the process ran and exited non-zero
inline constexpr std::int64_t kNotStarted = 4;   ///< the process never started at all
inline constexpr std::int64_t kUnknownRecipe = 5; ///< the name asked for is not one anybody knows
/// A process is running now: the one value about the present, and the answer a panel opened
/// mid-build receives.
inline constexpr std::int64_t kRunning = 6;
/// The process succeeded and the artifact is not there: its own problem, neither a success (the
/// project has no product) nor a failure (nothing failed). Usually a recipe whose `artifact` or
/// `artifact_dir` does not describe what its target produces.
inline constexpr std::int64_t kNoArtifact = 7;
} // namespace outcome

/// Where the realization of a built artifact stands: a second axis with its own owner. "It
/// built" and "the project took it" stay apart; a build whose load was refused is not a build
/// that failed.
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

/// Did this outcome produce the artifact its recipe names? The gate on realization, written in
/// one place so a later outcome value cannot fall outside it silently.
inline bool artifact_produced(std::int64_t value) { return value == outcome::kSucceeded; }

/// Is the build still in this condition rather than ended at it? One list, for every reader.
inline bool still_going(std::int64_t value) {
    return value == outcome::kAsked || value == outcome::kRunning;
}

/// Every line there is -- the `how_many` a caller passes to `tail_lines` when it
/// wants the whole of what it handed over, joined, rather than a tail of it.
inline constexpr std::size_t kAllLines = static_cast<std::size_t>(-1);

/// The runner's observation beat: the granularity of what a maker sees, never of how long a
/// build takes (ten looks a second, a multiple of the Timer's 10ms cap). The runner holds the
/// beat only while it holds a process, so an idle Workshop carries no Builder traffic.
inline constexpr const char* kLookTimerId = "zengine.builder.look";
inline constexpr std::int64_t kLookBeatMs = 100;

/// Look at what you hold, now: the beat's work on request, for suites, diagnostics and hosts
/// with no Timer (as `input::PumpInput` is). It widens nothing: it cannot start an operation,
/// only publish now what the next beat would have.
struct LookAtBuilds {
    ZEN_SHAPE(LookAtBuilds, 1);
};

/// The last lines of what a build said, whole and non-blank, joined with ` | `: the end is where
/// a compiler puts the reason. In the vocabulary because the tool needs it and must never
/// include the header that starts processes.
inline std::string tail_lines(const std::string& text, std::size_t how_many) {
    // Blank lines are not "the last line": a compiler's error block ends in two, and counting
    // them spent the whole tail on nothing.
    std::vector<std::string> kept;
    std::size_t end = text.size();
    while (end > 0 && kept.size() < how_many) {
        const std::size_t nl = text.rfind('\n', end - 1);
        const std::size_t start = nl == std::string::npos ? 0 : nl + 1;
        std::string line;
        for (std::size_t i = start; i < end; ++i) {
            const char c = text[i];
            line += (c == '\r' || c == '\t') ? ' ' : c;
        }
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        if (!line.empty()) {
            kept.push_back(std::move(line));
        }
        if (nl == std::string::npos) {
            break;
        }
        end = nl;
    }
    // One line with the breaks still visible: spaces made two build lines read as one sentence
    // that never happened.
    std::string out;
    for (std::size_t i = kept.size(); i > 0; --i) {
        if (!out.empty()) {
            out += " | ";
        }
        out += kept[i - 1];
    }
    return out;
}

// ---- commands ---------------------------------------------------------------

/// Ask the tool to build the recipe it knows by this name, and optionally to offer the result
/// to the running project. `realize` is a field, not a second shape: one authority with two
/// intentions, which the tool must remember for the whole build.
struct BuildRequested {
    std::string recipe;
    bool realize = false; ///< BUILD & REALIZE rather than BUILD
    ZEN_SHAPE(BuildRequested, 2, ZEN_FIELD(recipe), ZEN_FIELD(realize));
};

/// Ask the tool to say what it is: a presentation just opened learns everything it shows from
/// the tool's own answer, a build running now included.
struct StatusRequested {
    ZEN_SHAPE(StatusRequested, 1);
};

/// Ask the tool where it stands: its `BuildStatus`, answered to this asker alone and published
/// to nobody (`StatusRequested` republishes to every listener). The baseline a late or
/// returning observer joins: subscribe first, then ask. Within one operation a build and its
/// realization only move forward, so the further along of the answer and any publication about
/// the same operation stands.
struct BuildStatusRequested {
    ZEN_SHAPE(BuildStatusRequested, 1);
};

/// Order the runner to carry out the recipe it holds for this name. A second shape for the same
/// sentence on purpose, so "may ask for a build" and "may order the machine that runs one" are
/// two grant rules nobody can mistake. It carries no `op`: an order is not yet an operation.
struct RunBuild {
    std::string recipe;
    ZEN_SHAPE(RunBuild, 2, ZEN_FIELD(recipe));
};

// ---- observations -----------------------------------------------------------

/// What the tool did with one `BuildRequested`: taken as ask number `ask` (the `builds` its
/// statuses carry from then on), or refused in its own words -- so whoever pressed Build learns
/// what became of that ask from the owner, never from `builds` moving. The operation it became
/// is the one `BuildStatus` names beside that `builds`.
struct BuildAsked {
    std::int64_t ask = 0;  ///< the number this ask became; 0 when it was not taken
    std::string recipe;    ///< the recipe it named, as it named it
    bool realize = false;  ///< it asked for BUILD & REALIZE
    bool taken = false;
    std::string refusal;   ///< why it was not taken, in the tool's words; empty when taken
    ZEN_SHAPE(BuildAsked, 1, ZEN_FIELD(ask), ZEN_FIELD(recipe), ZEN_FIELD(realize),
              ZEN_FIELD(taken), ZEN_FIELD(refusal));
};

/// A process began, reported by the runner that started it. `command` travels here because the
/// tool must not hold a command: the first honest moment to say what runs is when it runs.
struct BuildStarted {
    std::int64_t op = 0;
    std::string recipe;  ///< the authored recipe this operation is carrying out
    std::string command; ///< what is running, as one line
    ZEN_SHAPE(BuildStarted, 2, ZEN_FIELD(op), ZEN_FIELD(recipe), ZEN_FIELD(command));
};

/// A running process said something: its next bytes, in order, line breaks included, never
/// anything already reported. Consecutive messages are one stream; a line longer than
/// `kMaxOutputChars` continues in the next.
// WL-OUT-01 -- agents/workshop/build-output.md
struct BuildOutput {
    std::int64_t op = 0;
    std::string recipe;
    std::string text;
    ZEN_SHAPE(BuildOutput, 3, ZEN_FIELD(op), ZEN_FIELD(recipe), ZEN_FIELD(text));
};

/// A process exited and was reaped: the end of one operation. It is about a process, not an
/// artifact: `status == 0` is the build system satisfied, and whether the file is there is the
/// tool's question (`OfferArtifact`, `outcome::kNoArtifact`), not the process custodian's.
struct BuildFinished {
    std::int64_t op = 0;
    std::string recipe;
    std::int64_t status = 0; ///< the child's exit status
    ZEN_SHAPE(BuildFinished, 2, ZEN_FIELD(op), ZEN_FIELD(recipe), ZEN_FIELD(status));
};

/// No process ran. `op` is 0 when nothing was ever held (an unknown name, a failed launch), and
/// a real number when a child existed but never became the program: on POSIX a failed `exec`
/// reports itself as an exit status, after the operation was announced (builder/run.hpp).
struct BuildNotStarted {
    std::int64_t op = 0;
    std::string recipe;  ///< the authored recipe that was asked for
    std::string command; ///< what would have run, when there was one
    std::string trouble; ///< why nothing did
    ZEN_SHAPE(BuildNotStarted, 2, ZEN_FIELD(op), ZEN_FIELD(recipe), ZEN_FIELD(command),
              ZEN_FIELD(trouble));
};

// ---- derived state ----------------------------------------------------------

/// The tool's own state, published for any presentation to read: the tool goes on counting
/// whether anybody looks, and `builds` proves it; `chunks` climbing makes a live build visible.
/// It carries two outcomes, the build's and realization's, so a panel never derives one from
/// the other, and `default_image` says whether a restart loads the realized image.
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
    bool default_image = false;  ///< the realized image is the file a restart loads
    ZEN_SHAPE(BuildStatus, 4, ZEN_FIELD(recipe), ZEN_FIELD(artifact), ZEN_FIELD(outcome),
              ZEN_FIELD(status), ZEN_FIELD(command), ZEN_FIELD(detail), ZEN_FIELD(builds),
              ZEN_FIELD(op), ZEN_FIELD(chunks), ZEN_FIELD(realize), ZEN_FIELD(realization),
              ZEN_FIELD(realized_detail), ZEN_FIELD(default_image));
};

// ---- what one operation said, read back --------------------------------------------------

/// How many operations' output the tool keeps: the one it is following and the ones before
/// it, newest last. A fifth build forgets the first's words, and a reader asking for them is
/// told so rather than handed another operation's.
inline constexpr std::size_t kKeptOperations = 4u;

/// The most of one operation's output the tool keeps, in bytes: its FIRST lines up to
/// `kKeptHeadBytes` and its LAST lines up to `kKeptTailBytes`, with what fell between them
/// counted. Both ends, because a compiler's first error is near the start of a failure and
/// a build tool's verdict at the end; neither is a guess about which line matters.
inline constexpr std::size_t kKeptHeadBytes = 32u * 1024u;
inline constexpr std::size_t kKeptTailBytes = 64u * 1024u;

/// The longest one kept line may be, in bytes. A longer one keeps this much and counts the
/// rest (`BuildOutputSaid::cut`): a single command echo can run to several kilobytes, and one
/// such line must not spend a whole end of the record.
inline constexpr std::size_t kMaxKeptLineBytes = 4096u;

/// The most lines, and bytes, one answer carries -- a page, never the record.
inline constexpr std::size_t kMaxOutputPageLines = 64u;
inline constexpr std::size_t kMaxOutputPageBytes = 16u * 1024u;

/// Ask the tool for a page of one operation's output, by operation and never by recipe: a
/// reader bound to op 7 reads 7's words whatever happens after. `from` counts lines from 1
/// (0 asks for the last page); `lines` is the reader's room, bounded by the tool.
// WL-OUT-02 -- agents/workshop/build-output.md
struct BuildOutputRequested {
    std::int64_t op = 0;
    std::int64_t from = 1;
    std::int64_t lines = 0;
    ZEN_SHAPE(BuildOutputRequested, 1, ZEN_FIELD(op), ZEN_FIELD(from), ZEN_FIELD(lines));
};

/// One page of what the tool keeps of an operation's output. `kept` false: nothing held for
/// `op` (never built here, or forgotten) and only `ops` is filled. `text` is whole lines numbered
/// from `first`, of `said` so far; lines no longer kept are `omitted` from `omitted_from`, and a
/// page stops before that gap. `cut` counts the bytes over-long lines on this page lost; `ended`
/// says the operation will say no more.
// WL-OUT-02 -- agents/workshop/build-output.md
struct BuildOutputSaid {
    std::int64_t op = 0;
    bool kept = false;
    std::string recipe;
    std::string artifact;
    std::int64_t outcome = outcome::kNeverBuilt;
    std::int64_t status = 0;
    bool ended = false;
    std::int64_t said = 0;
    std::int64_t first = 0;
    std::vector<std::string> text;
    std::int64_t omitted = 0;
    std::int64_t omitted_from = 0;
    std::int64_t cut = 0;
    std::vector<std::int64_t> ops; ///< every operation the tool keeps, oldest first
    ZEN_SHAPE(BuildOutputSaid, 1, ZEN_FIELD(op), ZEN_FIELD(kept), ZEN_FIELD(recipe),
              ZEN_FIELD(artifact), ZEN_FIELD(outcome), ZEN_FIELD(status), ZEN_FIELD(ended),
              ZEN_FIELD(said), ZEN_FIELD(first), ZEN_FIELD(text), ZEN_FIELD(omitted),
              ZEN_FIELD(omitted_from), ZEN_FIELD(cut), ZEN_FIELD(ops));
};

/// One row of what can be built here: a recipe's name and what it makes, never its procedure
/// (`RecipeView`, builder/recipe.hpp, on the wire).
struct RecipeSummary {
    std::string recipe;
    std::string artifact;
    ZEN_SHAPE(RecipeSummary, 1, ZEN_FIELD(recipe), ZEN_FIELD(artifact));
};

/// What this program can build, published when the tool is asked what it is: the tool's own
/// view, so a presentation shows recipes the tool will accept. A shape of its own because a
/// catalog changes when a file is installed, not on every line a compiler says. `source` is
/// provenance -- the authored file in force, or empty -- set with the rows by the one seam that
/// installs a catalog, and still not the procedure.
struct RecipeCatalog {
    std::vector<RecipeSummary> recipes;
    std::string source; ///< the authored file in force, or empty; provenance only
    ZEN_SHAPE(RecipeCatalog, 2, ZEN_FIELD(recipes), ZEN_FIELD(source));
};

/// Take this artifact, if the project wants it: a maker's intent, said only for BUILD & REALIZE,
/// with the facts that justify it. An offer, not an order: every eligibility rule and refusal is
/// the realization owner's, answered as `ArtifactRealized`. Not `BuildFinished` again: that is
/// about a process, this about a file seen since. `path` spares readers spelling a stem, and the
/// realization owner ignores it -- it resolves the stem by the host's rule, so a message naming
/// a path cannot redirect a load.
struct OfferArtifact {
    std::int64_t op = 0;  ///< the operation that produced it
    std::string recipe;   ///< the recipe that names it
    std::string artifact; ///< the artifact STEM -- the name a project's plan would use
    std::string path;     ///< the exact file, as the host spells it
    ZEN_SHAPE(OfferArtifact, 1, ZEN_FIELD(op), ZEN_FIELD(recipe), ZEN_FIELD(artifact),
              ZEN_FIELD(path));
};

/// What the running project made of an offered artifact: realization's sentence, said for the
/// realization owner; the tool only hears it. `detail` is always present -- the deepest layer's
/// own words, or what participated. `default_image` says whether the running image is the file
/// a restart loads (a reload runs a per-operation copy); `ask` names the `RealizationAsked` it
/// answers, since a revert settles late and other asks about the artifact can be answered first.
struct ArtifactRealized {
    std::string artifact;
    bool realized = false;
    std::string detail;
    bool default_image = false; ///< the running image is the file a restart loads
    std::int64_t ask = 0;       ///< the ask this answers; 0 for an ask the owner did not take
    ZEN_SHAPE(ArtifactRealized, 3, ZEN_FIELD(artifact), ZEN_FIELD(realized),
              ZEN_FIELD(detail), ZEN_FIELD(default_image), ZEN_FIELD(ask));
};

/// Make the running image the one a restart loads, after a reload in place (which runs a copy
/// and leaves the file a stem resolves to as it was). An offer to the realization owner, which
/// writes the file by the host's durable-file discipline and answers `ArtifactPromoted`.
struct PromoteArtifact {
    std::string artifact; ///< the artifact STEM
    ZEN_SHAPE(PromoteArtifact, 1, ZEN_FIELD(artifact));
};

/// Run the image before the last reload again: a reload through the same arm -- same WeaveId,
/// state kept -- answered as `ArtifactRealized`; a refusal is the owner's own sentence.
struct RevertArtifact {
    std::string artifact; ///< the artifact STEM
    ZEN_SHAPE(RevertArtifact, 1, ZEN_FIELD(artifact));
};

/// What came of a promotion: the file a restart loads now holds the running image, or `detail`
/// says why not in the operating system's words. The realization owner's sentence; `ask` joins
/// it to its `RealizationAsked`.
struct ArtifactPromoted {
    std::string artifact;
    bool promoted = false;
    std::string detail;
    std::int64_t ask = 0; ///< the promotion ask this answers (`RealizationAsked::ask`)
    ZEN_SHAPE(ArtifactPromoted, 2, ZEN_FIELD(artifact), ZEN_FIELD(promoted),
              ZEN_FIELD(detail), ZEN_FIELD(ask));
};

/// The acts a realization ask names (`RealizationAsked::act`).
namespace realization_act {
inline constexpr const char* kOffer = "offer";     ///< `OfferArtifact`: realize a new build
inline constexpr const char* kPromote = "promote"; ///< `PromoteArtifact`
inline constexpr const char* kRevert = "revert";   ///< `RevertArtifact`
} // namespace realization_act

/// What the realization owner did with one offer, promotion or revert, said before anything
/// else about it: taken as ask number `ask`, or refused in its words. Several asks can be about
/// one artifact at once and a revert is answered only when its reload settles, so an answer
/// names this number and never relies on arrival order. A promotion is always taken; the count
/// is the owner's for its own life.
struct RealizationAsked {
    std::string artifact; ///< the artifact STEM the ask named
    std::string act;      ///< one of `realization_act`
    std::int64_t ask = 0; ///< the number this ask became; 0 when it was not taken
    bool taken = false;
    std::string refusal;  ///< why it was not taken, in the owner's words; empty when taken
    ZEN_SHAPE(RealizationAsked, 1, ZEN_FIELD(artifact), ZEN_FIELD(act), ZEN_FIELD(ask),
              ZEN_FIELD(taken), ZEN_FIELD(refusal));
};

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_VOCABULARY_HPP
