// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_RUNNER_HPP
#define ZENGINE_BUILDER_RUNNER_HPP

// The build runner: the one weave in this program that holds a command, and the
// one that starts a process.
//
// IT IS A WEAVE AND NOT A HELPER, and that is the whole design. A free function
// the Workshop called would be authority handed over by a pointer — invisible in
// the host, ungated at the bus, and impossible to state in a sentence about who
// may do what. As a weave it has an identity, an office, and a grant a reader can
// see in one place: it may say `BuildOutcome` to whoever holds `zengine.builder`,
// and it may say nothing else to anybody. It cannot paint, cannot load a weave,
// cannot reach the Manager and cannot answer a stranger.
//
// WHAT IT WILL AND WILL NOT DO WITH A NAME. Its catalog comes from the host at
// construction. A `RunBuild` naming something in that catalog is carried out; a
// `RunBuild` naming anything else is REFUSED, by name, and nothing runs. There is
// no fallback, no PATH search of the name, no "if it looks like a target" — the
// catalog is the complete set of things this program can build, and it is a set
// somebody wrote down.
//
// WHAT THIS BOUNDARY IS AND IS NOT WORTH. An in-process weave shares the host's
// address space: the grant bounds what it may SAY, never what it may TOUCH
// (the Loom's own capabilities reference -- named rather than linked, because a
// comment's `*.md` path resolves against THIS repository's root and this repository
// is verified as a standalone clone with no sibling to look at), so any weave
// compiled into this program could call the same platform functions `run.hpp`
// calls. This split
// therefore buys reviewability rather than containment — one place to look for
// process authority, one grant to read, one refusal to test — and calling it
// containment would be exactly the overclaim the surrounding phases exist to
// refuse. Containment of a build is the isolation host's kind of question, and
// this is not that.

#include "builder/run.hpp"
#include "builder/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::builder {

/// The last few lines of what a build said.
///
/// The END of the output, because that is where a compiler puts the reason and
/// where a successful build puts the thing it made. Whole lines, so a maker is
/// never shown half a path — and a bounded number of them, because this is a
/// message that ends up on a panel with a handful of rows.
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

/// The runner's own books. Nothing here is a capability — the capability is the
/// catalog, and the catalog is not state a poke can reach (see below).
struct RunnerState {
    std::int64_t ran = 0;      ///< processes actually started
    std::int64_t refused = 0;  ///< names this runner does not hold a recipe for
    ZEN_EXPOSE();
    ZEN_SHAPE(RunnerState, 1, ZEN_FIELD(ran), ZEN_FIELD(refused));
};

class BuildRunnerWeave
    : public loom::WeaveBase<BuildRunnerWeave, RunnerState, loom::Accept<RunBuild>,
                             loom::Emit<BuildOutcome>> {
public:
    /// THE CATALOG ARRIVES AT CONSTRUCTION, FROM THE HOST, and it is a plain
    /// member rather than part of the weave's state.
    ///
    /// That placement is deliberate and it is the same distinction Workshop
    /// draws between its document and its session: `ZEN_SHAPE` state is
    /// poke-writable by design (the operator's door), and a poke that could
    /// write a new program path into this vector would be a door onto arbitrary
    /// execution wearing an inspection tool's clothes. So the recipes live where
    /// the substrate has no words for them, and what IS exposed is a tally that
    /// tells an operator how often this runner has run and refused.
    explicit BuildRunnerWeave(std::vector<BuildRecipe> catalog) : catalog_(std::move(catalog)) {}

    void on(const RunBuild& order, loom::Mail& mail) {
        const BuildRecipe* recipe = find(order.target);
        if (recipe == nullptr) {
            ++state_.refused;
            // A refusal answers with the SAME shape a success answers with, and
            // says which of its fields carry the refusal: `started` is false and
            // there is no status to read. A separate refusal shape would give the
            // tool two code paths for one question.
            (void)mail.send_to_role(kBuilderRole,
                                    BuildOutcome{order.target, false, 0, std::string(),
                                                 "no recipe here is called `" + order.target +
                                                     "`"});
            return;
        }
        ++state_.ran;
        const RunResult run = run_recipe(*recipe);
        (void)mail.send_to_role(
            kBuilderRole,
            BuildOutcome{recipe->target, run.started, run.status, recipe->as_line(),
                         run.started ? tail_lines(run.output, 3) : run.trouble});
    }

    /// What this runner can build, for a host that wants to say so in its banner.
    /// Read-only, and it is the host's own list coming back — no weave learns it
    /// this way.
    const std::vector<BuildRecipe>& catalog() const { return catalog_; }

    /// How many processes this runner has started, and how many names it has
    /// turned down. The two numbers a suite needs in order to assert that
    /// something did NOT run — "the outcome was a refusal" is a weaker claim
    /// than "no process began", and only the second one is the guarantee.
    std::int64_t ran() const { return state_.ran; }
    std::int64_t refused() const { return state_.refused; }

private:
    const BuildRecipe* find(const std::string& target) const {
        for (const BuildRecipe& r : catalog_) {
            if (r.target == target) {
                return &r;
            }
        }
        return nullptr;
    }

    std::vector<BuildRecipe> catalog_;
};

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_RUNNER_HPP
