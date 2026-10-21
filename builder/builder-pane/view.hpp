// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_PANE_VIEW_HPP
#define ZENGINE_BUILDER_PANE_VIEW_HPP

// The Builder pane's fact view and what every mode shares of its face: the members of
// `BuilderPaneWeave` that seat the facts in the room's budget, pack and draw the control strip,
// and offer and judge the pane's own menu (WL-CTX-09).
// A part of `builder/builder-pane/pane.cpp`, which includes it after `BuilderPaneWeave` and its
// aliases; nothing else includes it.
// Builder law: agents/realization.md

#include "builder/builder-pane/vocabulary.hpp"
#include "builder/vocabulary.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_text.hpp"

#include "graphics/role.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

// ---- The pane's own menu (WL-CTX-09) --------------------------------------------------

/// THE MENU KEY'S ENTRANCE: the mode's own rows, beside the row they are about.
inline void BuilderPaneWeave::offer_here(loom::Mail& mail, std::uint64_t correlation) {
    std::int64_t row = 0;
    if (choosing_.open) {
        const std::int64_t at =
            room().map().row_of(BuilderMeaning{builder_row::kRecipe, 0, {}, choosing_.name});
        row = at < 0 ? 0 : at;
    }
    offer_menu(row, 0, correlation, mail);
}

/// The rows this mode offers: operations this pane already has, spelled with the subject they
/// will act on, and a row not about the weaver's choice says so by name. Every control of the
/// mode has a row here, including an unavailable one: a narrow strip's `+N in menu` is a
/// promise only this function keeps, and a weaver who cannot reach an operation is owed its
/// refusal rather than silence.
inline void BuilderPaneWeave::offer_menu(std::int64_t row, std::int64_t column,
                                         std::uint64_t correlation, loom::Mail& mail) {
    pane_menu::Offer offer(pane::kBuilderPane, menu_subject());
    offer.at(row, column);
    offered_.clear();
    // WHAT EACH ROW ADVERTISES IT WILL ACT ON, kept in the image beside the ask. An answer
    // arrives after any number of the weaver's other acts and after anything that moved
    // this pane's facts, and the row's own promise is the thing to judge it against
    // (`chose`); `menu_subject` establishes only the MODE the menu was opened in.
    const auto say_row = [&](const char* id, std::string label, std::string advertised = {},
                             std::int64_t advertised_op = 0) {
        offer.row(id, std::move(label));
        offered_.push_back(Offered{id, std::move(advertised), advertised_op});
    };
    if (role_.open) {
        say_row(pane::kMenuCommit, "load `" + role_.stem + "` with the role typed");
        say_row(pane::kMenuCancel, "write nothing and load nothing");
    } else if (output_.open) {
        say_row(pane::kMenuOutputUp, "a line up");
        say_row(pane::kMenuOutputDown, "a line down");
        say_row(pane::kMenuOutputFirst, "the first line");
        say_row(pane::kMenuOutputLast, "the last lines");
        say_row(pane::kMenuOutputLeft, "pan left");
        say_row(pane::kMenuOutputRight, "pan right");
        say_row(pane::kMenuOutputOlder, "the older build's output");
        say_row(pane::kMenuOutputNewer, "the newer build's output");
        say_row(pane::kMenuClose, "close this build's output");
    } else if (choosing_.open) {
        const std::size_t at = list_row();
        if (at < known_.recipes.size()) {
            const std::string& named = known_.recipes[at].recipe;
            say_row(pane::kMenuChoose, "choose `" + named + "`", named);
            say_row(pane::kMenuEditSource, "edit `" + named + "`'s source", named);
        }
        say_row(pane::kMenuClose, "leave the choice as it was");
    } else {
        const std::size_t at = cursor_row();
        const std::string chosen =
            at < known_.recipes.size() ? known_.recipes[at].recipe : std::string();
        say_row(pane::kMenuRecipes, "choose a recipe from the list...");
        say_row(pane::kMenuBuild,
                chosen.empty() ? std::string("build the chosen recipe")
                               : "build `" + chosen + "`",
                chosen);
        say_row(pane::kMenuArm, state_.arm ? "turn load-after-build off"
                                           : "turn load-after-build on");
        say_row(pane::kMenuLoadBuilt,
                ready_to_load() ? "load the built `" + shown_.artifact + "` now"
                                : std::string("load what was built"),
                ready_to_load() ? shown_.artifact : std::string(),
                ready_to_load() ? shown_.op : 0);
        say_row(pane::kMenuAddToPlan,
                chosen.empty() ? std::string("add the chosen artifact to the load plan...")
                               : "add `" + chosen + "`'s artifact to the load plan...",
                chosen);
        say_row(pane::kMenuFrontier,
                waiting_ ? "build and load `" + frontier_artifact_ + "`"
                         : std::string("build what the project is waiting on"),
                waiting_ ? frontier_artifact_ : std::string());
        say_row(pane::kMenuPromote,
                standing() ? "promote `" + shown_.artifact + "` -- a restart loads it"
                           : std::string("promote the loaded image"),
                standing() ? shown_.artifact : std::string(),
                standing() ? shown_.op : 0);
        say_row(pane::kMenuRevert,
                standing() ? "revert `" + shown_.artifact + "` -- the previous image runs"
                           : std::string("revert the loaded image"),
                standing() ? shown_.artifact : std::string(),
                standing() ? shown_.op : 0);
        say_row(pane::kMenuEditSource,
                chosen.empty() ? std::string("edit the chosen recipe's source")
                               : "edit `" + chosen + "`'s source",
                chosen);
        say_row(pane::kMenuOutput,
                shown_.op == 0 ? std::string("read what a build said")
                               : "read what build #" + std::to_string(shown_.op) + " said",
                shown_.op == 0 ? std::string() : "#" + std::to_string(shown_.op));
    }
    menu().send_continuing(mail, offer, correlation);
}

/// WHAT THIS ROW ADVERTISED WHEN IT WAS OFFERED, or an empty `Offered` -- read once, when
/// the answer lands. A row nobody offered advertises nothing and is judged by the mode alone.
inline BuilderPaneWeave::Offered BuilderPaneWeave::offered_as(const std::string& id) const {
    for (const Offered& row : offered_) {
        if (row.id == id) {
            return row;
        }
    }
    return Offered{};
}

/// WHAT A MENU IS ABOUT, carried across the seam and established again when it answers: the
/// mode it was opened in, and the row it was opened on where the mode has rows.
inline std::string BuilderPaneWeave::menu_subject() const {
    if (role_.open) {
        return "role:" + role_.stem;
    }
    if (output_.open) {
        return "output:" + std::to_string(output_.op);
    }
    if (choosing_.open) {
        const std::size_t at = list_row();
        return "list:" + (at < known_.recipes.size() ? known_.recipes[at].recipe
                                                     : std::string());
    }
    return "builder";
}

/// WHAT A CHOSEN ROW MEANS -- judged here, against what this pane holds NOW. A menu that
/// was opened in another mode, or about a recipe the catalog has since dropped, acts on
/// nothing: the mode may have changed while the menu stood open, and every row below is
/// about the mode it was written for.
inline void BuilderPaneWeave::chose(const std::string& id, const std::string& subject,
                                    loom::Mail& mail) {
    if (subject != menu_subject()) {
        notice_ = "that menu was about something else -- nothing was done";
        say(mail);
        return;
    }
    static const struct {
        const char* menu;
        const char* action;
    } kRows[] = {{pane::kMenuRecipes, pane::kActionRecipes},
                 {pane::kMenuBuild, pane::kActionBuild},
                 {pane::kMenuArm, pane::kActionArm},
                 {pane::kMenuLoadBuilt, pane::kActionLoadBuilt},
                 {pane::kMenuAddToPlan, pane::kActionLoadIt},
                 {pane::kMenuFrontier, pane::kActionFrontier},
                 {pane::kMenuPromote, pane::kActionPromote},
                 {pane::kMenuRevert, pane::kActionRevert},
                 {pane::kMenuEditSource, pane::kActionEditSource},
                 {pane::kMenuOutput, pane::kActionOutput},
                 {pane::kMenuChoose, pane::kActionRecipeChoose},
                 {pane::kMenuCommit, pane::kActionCommit},
                 {pane::kMenuCancel, pane::kActionCancel},
                 {pane::kMenuOutputUp, pane::kActionOutputUp},
                 {pane::kMenuOutputDown, pane::kActionOutputDown},
                 {pane::kMenuOutputFirst, pane::kActionOutputFirst},
                 {pane::kMenuOutputLast, pane::kActionOutputLast},
                 {pane::kMenuOutputLeft, pane::kActionOutputLeft},
                 {pane::kMenuOutputRight, pane::kActionOutputRight},
                 {pane::kMenuOutputOlder, pane::kActionOutputOlder},
                 {pane::kMenuOutputNewer, pane::kActionOutputNewer}};
    for (const auto& row : kRows) {
        if (id == row.menu) {
            // The row's own promise, established again: a menu stands open across the
            // weaver's other acts and every build that settles under it, so `load the built
            // `a` now` loads `a` or refuses. A choice that begins an edit carries its number
            // to where the edit opens: the role line opens only after the project office
            // answers, so the number rides in its ask's `choice` and the grab is spent there
            // (`on(PlanNames)`), still judged by the host as a continuation of this choice.
            const Offered offered = offered_as(id);
            choice_ = mail.correlation();
            perform_on(row.action, offered.advertised, offered.op, mail);
            choice_ = 0;
            return;
        }
    }
    if (id == pane::kMenuClose) {
        // ONE ROW, TWO MODES, AND THE MODE DECIDES WHICH OPERATION IT IS: the subject
        // above already established that this menu belongs to the mode in force.
        perform(output_.open ? pane::kActionOutputClose : pane::kActionRecipesClose, mail);
    }
}

// ---- The controls a weaver can press ---------------------------------------------

/// The Builder's controls, in the order a weaver reads them: choose, build, decide what
/// happens to what was built, then the two that change what a restart loads, then the two
/// that read. `build`, `add to the load plan` and `edit source` act on the weaver's choice,
/// which the `recipe` row says; `load built ...`, `promote ...` and `revert ...` act on what
/// was built or is standing, which can differ, so those three name what they will touch.
inline std::vector<BuilderPaneWeave::ControlRow> BuilderPaneWeave::builder_controls() const {
    const bool ready = ready_to_load();
    std::vector<ControlRow> controls;
    controls.push_back(ControlRow{pane::kActionMenu, "menu", true});
    controls.push_back(ControlRow{pane::kActionRecipes, "choose a recipe...",
                                  heard_ && !known_.recipes.empty()});
    controls.push_back(
        ControlRow{pane::kActionBuild, "build", heard_ && !known_.recipes.empty()});
    controls.push_back(ControlRow{pane::kActionArm,
                                  state_.arm ? "turn load-after-build off"
                                             : "turn load-after-build on",
                                  !ready});
    controls.push_back(ControlRow{pane::kActionLoadBuilt,
                                  ready ? "load built " + shown_.artifact
                                        : std::string("load what was built"),
                                  ready, ready ? shown_.artifact : std::string(),
                                  ready ? shown_.op : 0});
    controls.push_back(ControlRow{pane::kActionLoadIt, "add to the load plan...",
                                  heard_ && !known_.recipes.empty()});
    controls.push_back(ControlRow{pane::kActionFrontier, "build what is waited on",
                                  heard_ && waiting_});
    controls.push_back(ControlRow{pane::kActionPromote,
                                  standing() ? "promote " + shown_.artifact
                                             : std::string("promote the loaded image"),
                                  standing(),
                                  standing() ? shown_.artifact : std::string(),
                                  standing() ? shown_.op : 0});
    controls.push_back(ControlRow{pane::kActionRevert,
                                  standing() ? "revert " + shown_.artifact
                                             : std::string("revert the loaded image"),
                                  standing(),
                                  standing() ? shown_.artifact : std::string(),
                                  standing() ? shown_.op : 0});
    controls.push_back(
        ControlRow{pane::kActionEditSource, "edit source", heard_ && !known_.recipes.empty()});
    controls.push_back(ControlRow{pane::kActionOutput,
                                  shown_.op == 0
                                      ? std::string("read output")
                                      : "read output #" + std::to_string(shown_.op),
                                  heard_ && shown_.op != 0,
                                  shown_.op == 0 ? std::string()
                                                 : "#" + std::to_string(shown_.op)});
    return controls;
}

/// DRAW THE STRIP (`PaneRowsFacet::push_controls`), every face a target. The controls do not
/// get to eat the pane (`component::strip_budget`): in a six-row room two rows of buttons would
/// cost the `realize` row, the one that says a loaded image is not the file a restart loads.
/// What does not fit is counted and reachable through `[menu]`, and a room too small for one
/// strip row leaves the right press.
inline void BuilderPaneWeave::say_controls(const std::vector<ControlRow>& controls) {
    // The advertised subject and its operation are part of the meaning, so two equal-width
    // faces about different artifacts, or two builds sharing a stem, are different spans: the
    // picture moves when the promise does, and a press queued against the older one is refused
    // by the fence (`perform_on`).
    room().push_controls(controls, fit_row(), [](const ControlRow& control) {
        return BuilderMeaning{builder_row::kControl, 0, control.id, control.subject,
                              control.subject_op, control.available};
    });
}

// ---- The fact view: the facts, seated in the room's budget ------------------------------

/// The three-row block the compiler's answer is wrapped into: wrapped to the width in force,
/// cut to the rows that survived the budget, and marked when cut, so the elision mark tells the
/// truth about this face.
inline std::vector<std::string> labelled_block(const char* label, const std::string& value,
                                               std::size_t rows, std::int64_t width) {
    std::vector<std::string> lines = wrap(labelled_row(label, value), width);
    if (lines.size() > rows) {
        lines.resize(rows);
        lines.back() = fit(lines.back() + " " + kElided, width);
    }
    while (lines.size() < rows) {
        lines.push_back(std::string());
    }
    return lines;
}

/// Keep what the budget seats, in display order. The priorities are distinct, so "the
/// smallest that fit" is one threshold, and the said block is wrapped last, into exactly the
/// rows that survived; a dropped fact is dropped whole. The budget is one row smaller when a
/// notice stands, so the composition is asked for fewer rows rather than losing its last.
template <class Facts>
inline void BuilderPaneWeave::publish(Facts facts, const std::string& said_detail,
                                      const std::vector<ControlRow>& controls) {
    const std::int64_t budget =
        rows() - (notice_.empty() ? 0 : 1) - room().strip_rows(controls);
    if (budget <= 0) {
        say_controls(controls); // the controls outrank the facts: they are the only route
        return;
    }
    std::vector<std::int64_t> priorities;
    priorities.reserve(facts.size());
    for (const auto& f : facts) {
        priorities.push_back(f.priority);
    }
    std::sort(priorities.begin(), priorities.end());
    const std::size_t seats = static_cast<std::size_t>(budget);
    const std::int64_t cut =
        priorities.size() > seats ? priorities[seats] : priorities.back() + 1;
    std::size_t said_kept = 0;
    for (const auto& f : facts) {
        if (f.priority < cut && f.text.empty()) {
            ++said_kept; // a said placeholder: counted now, written below
        }
    }
    std::vector<std::string> said;
    if (said_kept > 0) {
        said = labelled_block("said",
                           said_detail.empty() ? std::string("--")
                                               : ascii_spelling(said_detail),
                           said_kept, columns());
    }
    std::size_t said_at = 0;
    for (auto& f : facts) {
        if (f.priority >= cut) {
            continue;
        }
        std::string text = f.text.empty() ? said[said_at++] : f.text;
        push_row(text, f.role, f.meaning);
    }
    say_controls(controls);
}

/// The facts, composed against the budget: nine facts do not fit five rows, so each carries a
/// survival priority. What a weaver is acting on survives longest -- the office, the live
/// build's activity, the frontier, what a build will do next, the realization outcome, the
/// compiler's words -- and static metadata and the output's tail yield first. The display
/// order never changes with the budget: a shorter face shows the same rows, fewer of them.
inline void BuilderPaneWeave::say_builder() {
    struct Fact {
        std::string text;
        std::int64_t role = zengine::graphics::role::kFill;
        std::int64_t priority = 0; ///< smaller survives longer; all distinct
        BuilderMeaning meaning{};  ///< what a press on this row names, where it names one
    };
    std::vector<Fact> facts; // display order, priorities deciding survival
    // THE CONTROLS ARE ASKED FOR FIRST, because their rows come out of the same budget the
    // facts are seated in: a strip appended after the pane had already filled the room
    // would be the row that silently vanished, and it is the only mouse route there is.
    const std::vector<ControlRow> controls = builder_controls();

    // The header names the office it presents -- the tool's, whose facts these are; this
    // pane's own office is the pane header Workshop draws above the room. It also names the
    // catalog in force, carried by `RecipeCatalog` from its one owner (WL-PROJ-09).
    std::string head = std::string("BUILDER @") + builder::kBuilderRole;
    if (!known_.source.empty()) {
        head += "  " + known_.source;
    }
    facts.push_back(Fact{head, zengine::graphics::role::kAccent, 0});

    if (!heard_) {
        // NOT THE SAME AS "NEVER BUILT", and the pane must not show it as though it were.
        // This is a fact about this pane -- it has asked and is waiting -- and the
        // recipe's own history is not knowable from here until the tool says it.
        facts.push_back(Fact{labelled_row("recipe", "(the Builder has not answered yet)"),
                             zengine::graphics::role::kMuted, 1});
        publish(std::move(facts), std::string(), controls);
        return;
    }

    const builder::BuildStatus& s = shown_;
    // WHAT THE WEAVER HAS PICKED OUT, AND HOW MANY THERE ARE TO PICK FROM. It is the
    // CHOICE and not the last build, and when they differ the choice is the truer row:
    // it is what a build will do next, which is the question a weaver looking at this pane
    // is actually asking.
    const std::size_t held = known_.recipes.size();
    if (held == 0) {
        facts.push_back(Fact{labelled_row("recipe", "(this project has no build recipes)"),
                             zengine::graphics::role::kMuted, 3});
    } else {
        const std::size_t at = cursor_row();
        // THE ROW THAT NAMES THE CHOICE IS ALSO A WAY INTO THE LIST: a press on it opens
        // the catalog. The subject is the recipe it names, so the picture moves when the
        // choice does -- and the row itself reads exactly as it always did, because the
        // control that ADVERTISES the list is `[choose a recipe...]` beside it.
        facts.push_back(Fact{labelled_row("recipe", known_.recipes[at].recipe + " -> " +
                                                       known_.recipes[at].artifact + "  (" +
                                                       std::to_string(at + 1) + "/" +
                                                       std::to_string(held) + ")"),
                             zengine::graphics::role::kFill, 3,
                             BuilderMeaning{builder_row::kRecipe, at, {},
                                            known_.recipes[at].recipe}});
    }
    // What the project is waiting on, while it is: the row exists exactly while the frontier
    // does and costs the third `said` row, which a waiting project has no output for. The
    // artifact and the blocked count are the realization owner's, through the host's
    // read-only door; which recipes produce it is the tool's catalog, joined by stem.
    const std::size_t shift = waiting_ ? 1u : 0u;
    if (waiting_) {
        std::size_t makers = 0;
        const builder::RecipeSummary* maker = nullptr;
        for (const builder::RecipeSummary& known : known_.recipes) {
            if (known.artifact == frontier_artifact_) {
                ++makers;
                maker = &known;
            }
        }
        std::string said = "waiting " + frontier_artifact_ + " (";
        if (makers == 0) {
            said += "no recipe";
        } else if (makers == 1) {
            said += maker->recipe;
        } else {
            said += std::to_string(makers) + " recipes";
        }
        said += ", blocks " + std::to_string(frontier_blocked_) + ")";
        facts.push_back(Fact{labelled_row("project", said), zengine::graphics::role::kAccent, 2});
    }
    // WHAT THIS PANE IS WATCHING beats what it was last told: the tool's last OUTCOME is
    // still the previous build's while a new one runs, and showing that would answer
    // "what happened on the last build" with a sentence about the wrong build.
    const bool named_op = s.op != 0;
    // A BUILD THAT DID NOT PRODUCE ITS ARTIFACT POINTS AT ITS OWN WORDS, by the action's label:
    // the `said` rows keep its last lines, and the reader keeps every line the tool kept.
    const bool unproduced = s.outcome == builder::outcome::kFailed ||
                            s.outcome == builder::outcome::kNoArtifact ||
                            s.outcome == builder::outcome::kNotStarted;
    const std::string carried =
        named_op ? " -- op #" + std::to_string(s.op) + ", " + std::to_string(s.chunks) +
                       " out" + (unproduced ? std::string(" -- read output") : std::string())
                 : std::string();
    const bool unanswered = awaiting_ && s.outcome != builder::outcome::kRunning;
    facts.push_back(
        Fact{unanswered ? labelled_row("last", "asked -- waiting for it to start")
                        : labelled_row("last", std::string(builder::name_of_outcome(s.outcome)) +
                                                  carried),
             unanswered || s.outcome == builder::outcome::kRunning
                 ? zengine::graphics::role::kAccent
                 : (s.outcome == builder::outcome::kFailed ||
                            s.outcome == builder::outcome::kNotStarted ||
                            s.outcome == builder::outcome::kNoArtifact ||
                            s.outcome == builder::outcome::kUnknownRecipe
                        ? zengine::graphics::role::kAlert
                        : zengine::graphics::role::kFill),
             1});
    // THE EXIT STATUS IS ONLY SHOWN WHEN THERE WAS ONE. A `0` printed after a build that
    // never started reads as success, which is the exact wrong answer at the exact moment
    // a weaver most needs the right one. The tool's own counter shares the row, because it
    // is the number that proves the tool outlives its presentation.
    facts.push_back(
        Fact{labelled_row("exit", pad(s.outcome == builder::outcome::kSucceeded ||
                                             s.outcome == builder::outcome::kFailed
                                         ? std::to_string(s.status)
                                         : std::string("--"),
                                     11) +
                                     "asks " + std::to_string(s.builds) + " ever"),
             zengine::graphics::role::kMuted, 6});
    // WHAT WAS ACTUALLY RUN, as the runner reported it. The first fact a constrained
    // budget gives up: it is an echo of the weaver's own act.
    facts.push_back(Fact{labelled_row("ran", s.command.empty()
                                                ? std::string("(nothing has run yet)")
                                                : s.command),
                         zengine::graphics::role::kMuted, 7});
    // A BUILD OUTCOME AND A REALIZATION OUTCOME ARE TWO ANSWERS AND THIS PANE SHOWS TWO.
    // The row has three faces: ARMED (`[x] load after build`), THE BUTTON (an artifact is
    // built, nothing was asked about realizing it, nothing is armed), and OTHERWISE the
    // realization outcome -- with the clause a weaver must not miss, that a realized image
    // which is NOT the file a restart loads says so.
    const bool button = !awaiting_ && !state_.arm && !s.recipe.empty() &&
                        s.outcome == builder::outcome::kSucceeded &&
                        s.realization == builder::realization::kNotAsked;
    std::string realize_face;
    std::int64_t realize_role = zengine::graphics::role::kMuted;
    if (state_.arm && s.realization == builder::realization::kNotAsked) {
        realize_face = "[x] load after build";
        realize_role = zengine::graphics::role::kAccent;
    } else if (button) {
        realize_face = "-- (load-after-build loads " + s.artifact + " now)";
        realize_role = zengine::graphics::role::kAccent;
    } else if (s.realization == builder::realization::kNotAsked) {
        realize_face = "-- (load-after-build arms it)";
    } else {
        // THE OPERATION IT IS ABOUT, named the way the `last` row names its own: two reloads in
        // a row end on the same words, and only the number tells the newer answer from the older.
        realize_face = std::string(builder::name_of_realization(s.realization)) + " -- op #" +
                       std::to_string(s.op);
        if (s.realization == builder::realization::kRealized && !s.default_image) {
            realize_face += ", NOT DEFAULT (promote / revert)";
        }
        if (!s.realized_detail.empty()) {
            realize_face += " -- " + s.realized_detail;
        }
        realize_role = s.realization == builder::realization::kRefused
                           ? zengine::graphics::role::kAlert
                           : (s.realization == builder::realization::kRealized
                                  ? zengine::graphics::role::kFill
                                  : zengine::graphics::role::kMuted);
    }
    facts.push_back(Fact{labelled_row("realize", realize_face), realize_role, 4});
    // THREE ROWS FOR WHAT THE BUILD SAID, because this is the row budget a weaver spends
    // when something has gone wrong, and one row of a compiler's answer is a row of
    // nothing. While the project is WAITING, the `project` row holds the third of them.
    // They are PLACEHOLDERS here: `publish` wraps the detail into exactly the rows that
    // survive the budget, so the elision mark tells the truth about THIS face.
    const std::size_t said_max = 3 - shift;
    const std::int64_t said_priorities[3] = {5, 8, 9};
    for (std::size_t i = 0; i < said_max; ++i) {
        facts.push_back(Fact{std::string(), zengine::graphics::role::kMuted, said_priorities[i]});
    }
    publish(std::move(facts), s.detail, controls);
}

} // namespace

#endif // ZENGINE_BUILDER_PANE_VIEW_HPP
