// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_BUILDER_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_BUILDER_SEAM_VOCABULARY_HPP

// WHAT THE BUILDER PANE ASKS THE HOST, AND WHAT IT HEARS BACK. The Builder panel became a
// loadable weave (Zengine/builder-pane/), and the three facts it used to read straight off
// `HostContext` -- what project realization is waiting on, whether the plan in force
// already names an artifact, and the source file a recipe was authored from -- plus the one
// act it used to perform through a host closure -- appending a plan row -- cannot cross into
// a loaded image as anything but values. So each becomes an ask to an OFFICE and an answer
// back, `pane_seam_vocabulary.hpp`'s seam with a second tenant.
//
// ---- THREE OFFICES, THE SAME SPLIT --------------------------------------------
//
//     zengine.project    read-only    ProjectFrontierRequested -> ProjectFrontierSaid
//                                     PlanNamesRequested       -> PlanNames
//     zengine.plan       ACTS         PlanRowRequested         -> PlanRowWritten
//     zengine.workshop   ACTS         RecipeSourceRequested    -> SourceOpened
//
// ⚠ THE TWO PLAN QUESTIONS ARE AT TWO OFFICES ON PURPOSE, and the line is
// `pane_seam_vocabulary.hpp`'s own: a question whose answer runs nobody's code lives apart
// from one whose whole purpose is to change the project on disk. "Does the plan already name
// `zengine-snake`?" reads a vector the host already holds; "append this row" composes a plan
// row, hands it to the running project, and saves a file. Putting them at one address would
// make "which office can write a maker's files" stop having a one-word answer -- which is
// the property that split bought, and it is not worth spending to save one constant.
//
// AND `zengine.plan` IS NOT `zengine.recipes`. Both write a file a maker authored, and they
// write DIFFERENT files under different laws: the recipe catalog says how an artifact is
// produced, the load plan says which artifacts this project runs. One office that could do
// both would be an office whose one act is two acts.
//
// ---- WHAT CROSSES, AND WHAT CANNOT --------------------------------------------
//
// VALUES. Every field below is Text, Int or Bool -- the ordinary Loom wire, admitted at the
// reader's own schema. No `HostContext&`, no realization owner, no `Session&`, no
// `CurrentRecipes&` and no host address of any kind. The C++ objects that own these facts
// stay in the host's `main`; a reader gets a picture and can do nothing to them but ask again.
//
// ⚠ AND A FRONTIER IS A READING, NOT A POWER. A pane that hears `ProjectFrontierSaid` has
// learned which artifact the running project is stopped on and how many rows are behind it.
// It has not been permitted to perform a row, to realize anything, or to reorder the plan:
// the one route from a maker's gesture to a realized artifact is still `BuildRequested` to
// the Builder office, then the tool, the runner, the tool's offer, and the realization
// owner's own eligibility rules -- the sentence `workshop.cpp` writes over
// `HostContext::frontier`, and the tripwire this file is deliberately INSIDE the population
// of: `"BLD-2: the presentation holds no realization or build-runner reach"` reads every
// presentation source under `workshop/`, and a seam header that cannot spell the five
// forbidden names is a seam header that cannot leak one.

// ⚠ THE TWO NAMES THIS SEAM SHARES WITH THE FILES SEAM ARE INCLUDED, NEVER RESPELLED.
// `kProjectRole` is THE read-only project office -- one address that answers where this run
// began, what realization is waiting on, and whether the plan names an artifact -- and
// `SourceOpened` is what OPENING A SOURCE came to, which is one act with one answer no matter
// which pane asked for it. A second constant for the same office would be two names for one
// door, and a second answer shape for the same act would make "did it open" a question with
// two grammars. So this header includes that one and adds what is new, which is what makes
// the two seams one seam with two tenants rather than two seams that happen to agree.
#include "pane_seam_vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::workshop {

/// THE OFFICE THAT MAY CHANGE WHICH ARTIFACTS THIS PROJECT LOADS. A ROLE, for
/// `kRecipesRole`'s reason exactly: it survives its holder being replaced, and a loaded
/// artifact names it without ever learning a `WeaveId`. A host that mounts no such door
/// holds no such office and an ask reaches nobody -- the correct answer for a host whose
/// project has no plan to write.
inline constexpr const char* kPlanRole = "zengine.plan";

// ---- What the project is waiting on, read ---------------------------------------

/// ASK WHAT PROJECT REALIZATION IS STOPPED ON, RIGHT NOW. Carries nothing, for
/// `ProjectRootRequested`'s reason: a filter would be a policy the asker was never entitled
/// to author.
struct ProjectFrontierRequested {
    ZEN_SHAPE(ProjectFrontierRequested, 1);
};

/// `ProjectFrontier` (workshop/panel.hpp) ON THE WIRE -- the realization owner's own derived
/// answer, read at the moment of the ask and held by nobody.
///
/// IT IS DERIVED AT THE ASK AND NOT PUBLISHED. The owner has no idea a presentation exists,
/// and a publication would make every advance of the plan a broadcast about a pane. So the
/// pane asks on the beats where the answer can have moved -- its room grant, every
/// `BuildStatus`, and the answer to a plan row it wrote -- and is told nothing in between.
/// A picture that is one beat old is what a presentation always had; a picture nobody can
/// tell is stale is what this shape is arranged against.
struct ProjectFrontierSaid {
    bool waiting = false;       ///< realization is stopped at a row waiting on the maker
    std::string artifact;       ///< the frontier artifact stem; empty when not waiting
    std::int64_t blocked = 0;   ///< authored rows behind the frontier, waiting on it
    ZEN_SHAPE(ProjectFrontierSaid, 1, ZEN_FIELD(waiting), ZEN_FIELD(artifact),
              ZEN_FIELD(blocked));
};

// ---- Whether the plan already names an artifact, read ---------------------------

/// DOES THE PLAN IN FORCE ALREADY NAME THIS ARTIFACT? `HostContext::plan_names` on the wire.
/// Asked at the gesture, answered from the plan the host holds, and stored nowhere.
struct PlanNamesRequested {
    std::string stem;
    ZEN_SHAPE(PlanNamesRequested, 1, ZEN_FIELD(stem));
};

/// THE ANSWER, AND THE STEM IT IS ABOUT. The stem rides back because an answer that said
/// only `true` would be an answer a pane had to remember what it asked to read -- and a pane
/// whose maker moved the cursor between the ask and the answer would read it against the
/// wrong row. Provenance settles WHO answered; this settles WHAT they answered about.
struct PlanNames {
    std::string stem;
    bool named = false;
    ZEN_SHAPE(PlanNames, 1, ZEN_FIELD(stem), ZEN_FIELD(named));
};

// ---- One plan row, authored -----------------------------------------------------

/// LOAD THIS ARTIFACT AS THIS ROLE (LOAD-IT). The three fields are `append_plan_row`'s own
/// arguments: the artifact stem the chosen recipe produces, the role the maker typed, and
/// the recipe -- carried so the answer can say whether the row's product is already built.
/// The host composes the row, checks it by the plan's law, hands it to the running project,
/// seeds and saves a project plan, and answers.
struct PlanRowRequested {
    std::string stem;
    std::string role;
    std::string recipe;
    ZEN_SHAPE(PlanRowRequested, 1, ZEN_FIELD(stem), ZEN_FIELD(role), ZEN_FIELD(recipe));
};

/// WHAT AUTHORING THE ROW CAME TO -- `HostContext::PlanAppend` on the wire, field for field.
/// `accepted` with an empty `refusal`, or the owner's own words for why not; `detail` is what
/// the running project made of the row; `frontier` says the row is now what the project is
/// waiting on; `product` is the recipe's product on disk when the row is the frontier and the
/// product is there, so the pane can finish the maker's intent without a second gesture; and
/// `path` is the plan file written, when one was.
struct PlanRowWritten {
    bool accepted = false;
    std::string refusal;
    std::string detail;
    bool frontier = false;
    std::string product;
    std::string path;
    ZEN_SHAPE(PlanRowWritten, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(detail),
              ZEN_FIELD(frontier), ZEN_FIELD(product), ZEN_FIELD(path));
};

// ---- One recipe's source, opened in the Editor ----------------------------------

/// OPEN THE SOURCE THIS RECIPE WAS AUTHORED FROM. Addressed to `zengine.workshop`, the office
/// that holds the Editor today, and answered with `SourceOpened` -- the same answer
/// `OpenSourceRequested` gets, because the two asks end in the same act.
///
/// ⚠ IT NAMES A RECIPE AND NOT A PATH, and that is the whole reason it is a second shape
/// rather than a use of `OpenSourceRequested`. `RecipeSummary` is `{recipe, artifact}` on
/// purpose: a presentation does not receive source paths, build trees, package prefixes or
/// link lists, because handing them over would put a build procedure on a screen that has no
/// way to act on one. So the pane says the only thing it holds -- the recipe's own name --
/// and the host resolves it through `HostContext::recipe_source` against the catalog IT owns.
/// A pane that could spell the path would already have been given the procedure.
///
/// AND EVERY REFUSAL IS THE HOST'S. A name the project's recipes do not hold, a recipe kind
/// that names no single source, a file that is not there, a dirty buffer that must be saved
/// or discarded first: all four reach the pane as `SourceOpened::refusal`, in the words their
/// owner used, and the pane says them in its own row.
struct RecipeSourceRequested {
    std::string recipe;
    ZEN_SHAPE(RecipeSourceRequested, 1, ZEN_FIELD(recipe));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_BUILDER_SEAM_VOCABULARY_HPP
