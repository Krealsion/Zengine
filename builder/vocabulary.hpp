// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_VOCABULARY_HPP
#define ZENGINE_BUILDER_VOCABULARY_HPP

// The Builder package's message vocabulary — the shapes a build conversation is
// made of, and the two offices that hold its two halves.
//
// THE WHOLE POINT OF THIS PACKAGE IS A SPLIT, and the vocabulary is where it is
// visible. Building something means starting a process, which is the first
// effect any Zengine application has asked for that is not "paint" or "open the
// document the host named". So it is split across two weaves that the host
// mounts separately, with different grants:
//
//   the TOOL     holds a NAME. It knows one target, remembers what happened to
//                it last time, and publishes that so any presentation can read
//                it. It cannot start a process, and it holds no command.
//   the RUNNER   holds the COMMANDS. It is the only thing in the program that
//                starts a process, its catalog comes from the host, and it
//                answers a name it does not know with a refusal rather than a
//                guess.
//
// So a maker's Build reaches an operating-system process through TWO offices and
// TWO grants, and neither of them is Workshop. Workshop may say `BuildRequested`
// to whoever holds `zengine.builder` and that is the whole of its new reach: it
// cannot order the runner, it cannot name a command, and nothing in this header
// gives it a way to spell one.
//
// WHAT IS DELIBERATELY ABSENT, so the absence is a decision:
//
//   - no command text ANYWHERE on the wire. `BuildRequested` and `RunBuild`
//     carry a target NAME, and a name is checked against a catalog the host
//     wrote. There is no shape here whose field is a program, an argument
//     vector, a working directory or a shell line, so "the panel sent a
//     command" is not a sentence this vocabulary can express. That is BLD-0's
//     hard boundary and it is enforced by the type, not by a check.
//   - no build QUEUE, no cancel, no progress. BLD-0's runner builds inside its
//     own handler and answers when the process exits, so there is no in-flight
//     state for a shape to describe. The cost of that is real and measured
//     rather than hidden (the report says what a maker sees while it runs).
//   - no artifact, no load, no replace. What a build PRODUCED is not in
//     `BuildOutcome`, because the next thing anybody would do with it is load
//     it, and Build+Load is a different phase with a different threat model.
//   - no second target, no target list on the wire. The tool knows one name and
//     says which; a picker over targets is a thing a later phase can add
//     without any shape here changing meaning.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::builder {

/// The office the TOOL holds. Workshop's grant names this role rather than a
/// WeaveId, so who answers for it can change without Workshop's authority being
/// rewritten — the same reason `zengine.skin` is a role.
inline constexpr const char* kBuilderRole = "zengine.builder";

/// The office the RUNNER holds — the only holder of process authority in this
/// program. It is a separate role from the tool's precisely so that a grant can
/// name one without naming the other: the tool may reach the runner, and nothing
/// else may.
inline constexpr const char* kBuildRunnerRole = "zengine.build-runner";

/// What the tool's last build came to. One value, so a presentation asks one
/// question — and the two failures are told apart, because a maker who asked for
/// a name nobody knows has a different problem from one whose build broke.
namespace outcome {
inline constexpr std::int64_t kNeverBuilt = 0;   ///< nothing has been asked for yet
inline constexpr std::int64_t kAsked = 1;        ///< ordered; no answer has come back
inline constexpr std::int64_t kSucceeded = 2;    ///< the process ran and exited 0
inline constexpr std::int64_t kFailed = 3;       ///< the process ran and exited non-zero
inline constexpr std::int64_t kNotStarted = 4;   ///< the process never started at all
inline constexpr std::int64_t kUnknownTarget = 5; ///< the name asked for is not one anybody knows
} // namespace outcome

/// The tool's own name for each of those, so a presentation does not invent a
/// sixth vocabulary for a fact the tool already has a word for.
inline const char* name_of_outcome(std::int64_t value) {
    switch (value) {
    case outcome::kNeverBuilt: return "not built yet";
    case outcome::kAsked: return "asked";
    case outcome::kSucceeded: return "succeeded";
    case outcome::kFailed: return "FAILED";
    case outcome::kNotStarted: return "did not start";
    case outcome::kUnknownTarget: return "unknown target";
    default: return "unknown";
    }
}

/// ASK THE TOOL to build the target it knows by this name.
///
/// The name is here rather than implied, and that is what makes "the configured
/// build action reaches the intended target" a checkable claim instead of a
/// hope: a request that names the wrong thing is refused by the tool and says
/// so, and a request that names the right thing can be seen naming it.
struct BuildRequested {
    std::string target;
    ZEN_SHAPE(BuildRequested, 1, ZEN_FIELD(target));
};

/// ASK THE TOOL TO SAY WHAT IT IS — no fields, because the question has no
/// parameters: there is one tool at this office and it has one condition.
///
/// It exists because a presentation that has just been opened knows nothing, and
/// the honest way for it to learn is to ASK THE TOOL rather than to be handed
/// the tool's facts by whoever built the presentation. That is the panel/tool
/// split at its smallest: opening a Builder panel sends this, and everything the
/// panel then shows arrived as the tool's own answer.
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
struct RunBuild {
    std::string target;
    ZEN_SHAPE(RunBuild, 1, ZEN_FIELD(target));
};

/// What the runner saw. Reported to whoever holds `zengine.builder` and to
/// nobody else.
///
/// `recipe` is the runner telling the truth about what it actually ran, in one
/// readable line. It travels back rather than being known up front because the
/// tool must not hold a command — so the only honest moment for a maker to be
/// shown one is after it has been run, by the thing that ran it.
struct BuildOutcome {
    std::string target;
    bool started = false;  ///< did a process begin at all
    std::int64_t status = 0; ///< its exit status, meaningful only when it started
    std::string recipe;    ///< what was run, as one line
    std::string detail;    ///< the tail of what it said, or why it never started
    ZEN_SHAPE(BuildOutcome, 1, ZEN_FIELD(target), ZEN_FIELD(started), ZEN_FIELD(status),
              ZEN_FIELD(recipe), ZEN_FIELD(detail));
};

/// THE TOOL'S OWN STATE, PUBLISHED — the shape a presentation reads.
///
/// Published rather than fetched, because a panel is not the tool's owner and
/// must not be its only reader: anything on this bus that accepts this shape
/// sees the same facts, and the Builder panel is simply one of them. It is also
/// what keeps a closed panel honest — the tool goes on counting whether or not
/// anybody is looking, and `builds` is the field that proves it.
struct BuildStatus {
    std::string target;      ///< the one name this tool knows
    std::int64_t outcome = outcome::kNeverBuilt;
    std::int64_t status = 0; ///< the last process's exit status
    std::string recipe;      ///< what the runner said it ran, once it has run something
    std::string detail;      ///< the last thing the runner said, or the tool's own refusal
    std::int64_t builds = 0; ///< how many builds this TOOL has been asked for, ever
    ZEN_SHAPE(BuildStatus, 1, ZEN_FIELD(target), ZEN_FIELD(outcome), ZEN_FIELD(status),
              ZEN_FIELD(recipe), ZEN_FIELD(detail), ZEN_FIELD(builds));
};

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_VOCABULARY_HPP
