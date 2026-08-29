// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_RECIPES_HPP
#define ZENGINE_WORKSHOP_RECIPES_HPP

// THE RECIPES THIS RUNNING WORKSHOP CURRENTLY MEANS, AND WHICH FILE THEY CAME FROM
// (PROJ-0, live replacement PROJ-1).
//
// ---- The two facts, and which one lives here ------------------------------------
//
//     authored recipe catalog        a durable FILE a maker wrote and can carry
//             +
//     host/project completion        where this install puts artifacts, where it may
//                                    generate a project, and what a relative source
//                                    is relative to (`recipe_persist::complete_recipes`)
//             |
//             v
//     COMPLETED SESSION RECIPES      what this process is actually going to build,
//                                    edit and look for -- and what this class holds
//
// The FILE stays authored truth and is never rewritten. What this owns is the OUTPUT of
// the one completion law, for as long as this Workshop is running.
//
// ---- Why it is a thing and not a local variable ---------------------------------
//
// It was a local variable, and the local was scattered: the runner took a whole COPY of
// the completed catalog, the tool took a copy of its reduced view, and the host's
// edit-source answer was a closure over a THIRD copy of three fields per recipe. After
// construction nobody held the sentence "these are the recipes this Workshop currently
// means" -- so the completed catalog could not be replaced without finding every
// consumer and re-synchronising it by hand, and the first consumer somebody forgot
// would go on answering from the old project in a way no test could see.
//
// So this holds it, every consumer READS it, and a replacement has ONE place to happen.
//
// ---- THE CATALOG AND ITS SOURCE ARE ONE FACT (PROJ-1) ---------------------------
//
// Once the catalog can CHANGE while Workshop runs, a session has to be able to answer
// two questions that must never be able to disagree:
//
//     which completed recipes are currently in force?
//     which authored file produced them?
//
// So they are held together and written together. `hold()` takes the source path in the
// same call as the rows, there is no second door that writes one without the other, and
// a caller therefore cannot install recipes without saying where they came from. That is
// not tidiness: "the path moved and the recipes did not" and "the recipes moved and the
// path did not" are exactly the two half-replacements this phase exists to make
// unspellable, and the way to make a state unspellable is to give it no spelling.
//
// THE PATH IS PROVENANCE, AND IT IS NOT THE OTHER FOUR THINGS. It is not recipe content
// (the rows are), not project identity (`HostContext::project_dir` is, and selecting a
// catalog never moves it), not a Builder selection, not persistence (nothing writes it to
// a file), and not load authority (a parsed catalog grants nothing).
//
// ---- What this deliberately is NOT ----------------------------------------------
//
// It is not the POLICY of a replacement. `install_recipes` below is the one seam that
// reads a file and installs the result, and it is deliberately the whole of what this
// header decides: what a replacement does to a chosen Builder row, to an in-flight
// build, or to a maker's standing pick is the WEAVE's question, answered where those
// facts live, and nothing here can see any of them.
//
// It is not an authorship surface. There is no non-const access to what is held: a
// consumer may read the catalog and may not edit it, because a recipe a consumer wrote
// into memory would be a build procedure with no author and no file. The authored FILE
// is never rewritten -- completion fills in host facts in MEMORY, and writing them back
// would turn a catalog a project can carry into one machine's.
//
// It is not the Builder's, the Editor's, the browser's or a pane's, and it is not a
// build invocation's. Every one of those is a consumer with a shorter life than the
// answer, which is exactly why the answer needed an owner above all of them.
//
// It is not a discovery mechanism. Nothing here searches, guesses a filename, reads a
// directory, watches a file or reloads on its own: every catalog this object ever holds
// arrived because somebody named a path.
//
// ---- The lifetime rule, which is the whole safety argument ----------------------
//
// CONSUMERS HOLD REFERENCES INTO THIS OBJECT, so this object must outlive them. The host
// declares one BEFORE the HostContext, the bus, the Kernel and every weave, which makes
// reverse-destruction order the proof -- it is the same argument, one file over, that
// lets `HostContext::frontier` read a `PlanExecutor` the host also owns as a local.
//
// A REPLACEMENT REPLACES CONTENTS, NEVER THE OBJECT. `hold()` assigns into the members
// this class already owns, so every reference a consumer took at construction keeps
// naming the live answer. What a consumer must not do is keep a pointer to one ROW
// across a turn -- `recipe_named`/`view_named` hand back element pointers, and those are
// for spending inside the operation that asked, exactly as they always were.

#include "recipe_persist.hpp"

#include "builder/recipe.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The one current completed recipe catalog of a running Workshop, and the authored file
/// it came from.
class CurrentRecipes {
public:
    /// HOW THIS PLATFORM SPELLS AN ARTIFACT STEM AS A FILE, in a named directory.
    ///
    /// This class cannot know it and must not guess it: LOAD-0 makes that rule the
    /// HOST's, written in exactly one place, which is what keeps one authored plan legal
    /// on Linux and on Windows. So the host hands it over -- one function, no state --
    /// exactly as it hands `complete_recipes` its two directories.
    using ArtifactFile = std::string (*)(std::string_view directory, std::string_view stem);

    /// TAKE CUSTODY OF A COMPLETED CATALOG AND OF THE FILE IT CAME FROM.
    ///
    /// `completed` has already been through the one completion law; this does not
    /// re-complete it, because a second party resolving an authored spelling is the
    /// defect that law exists to prevent. What this adds is the tool's REDUCED VIEW of
    /// each row, derived here so the two can never disagree: the Builder tool is
    /// deliberately told an identity, an artifact and the one file that artifact means,
    /// and never a source path, a build tree, a package prefix or a link list (BLD-1).
    ///
    /// ⚠ THE SOURCE PATH IS A PARAMETER AND NOT A SETTER (PROJ-1). It arrives in the same
    /// call as the rows because there is no honest moment at which the two disagree, and
    /// a `set_source()` beside this would be exactly that moment made reachable. Three
    /// members, one writer, one statement.
    ///
    /// CALLING IT AGAIN REPLACES WHAT IS HELD, and that is the seam `install_recipes`
    /// spends: read, parse, complete, hold. Every consumer's reference survives it
    /// because the members below are this object's and only their CONTENTS change.
    void hold(std::string source, std::vector<builder::Recipe> completed,
              ArtifactFile artifact_file) {
        source_ = std::move(source);
        all_ = std::move(completed);
        views_.clear();
        views_.reserve(all_.size());
        for (const builder::Recipe& r : all_) {
            views_.push_back(
                builder::RecipeView{r.id, r.artifact, artifact_file(r.artifact_dir, r.artifact)});
        }
    }

    /// THE AUTHORED FILE THESE RECIPES CAME FROM -- provenance, and never content.
    /// Empty means no catalog is in force, which is an ordinary state: a project with
    /// nothing to build is a project.
    const std::string& source() const noexcept { return source_; }

    /// THE WHOLE COMPLETED CATALOG, for the parties that carry out a build or answer for
    /// a recipe's own inputs -- the runner, and the host's edit-source answer.
    const std::vector<builder::Recipe>& all() const noexcept { return all_; }

    /// THE SAME CATALOG WITH THE BUILD PROCEDURE SUBTRACTED, for the presentation side.
    /// One row per recipe, in the catalog's own order, so an index into one is an index
    /// into the other.
    const std::vector<builder::RecipeView>& views() const noexcept { return views_; }

private:
    std::string source_;
    std::vector<builder::Recipe> all_;
    std::vector<builder::RecipeView> views_;
};

/// READ AN AUTHORED CATALOG FILE AND MAKE IT THIS SESSION'S -- the ONE seam, spent by the
/// launch and by every later maker choice alike (PROJ-1).
///
/// ---- Why there is exactly one of these ------------------------------------------
///
/// The startup catalog and a catalog a maker picks while Workshop is running are the SAME
/// KIND OF EVENT: an authored file becomes the recipes this session means. Two functions
/// doing that would be two policies, and the day they drifted, `--recipes` and the live
/// gesture would complete a relative source differently -- which is EDIT-1's defect
/// exactly, re-created one layer up. So the launch does not have a private path: it calls
/// this, and the live gesture calls this.
///
/// ---- The transaction, which is the phase's central law --------------------------
///
///     read -> parse -> complete -> hold
///
/// Every step before `hold` works on a CANDIDATE that lives in this function's own frame.
/// The owner is not touched, not cleared, not partially written and not asked anything
/// until the candidate is complete, so a refusal at any stage leaves the session holding
/// exactly the catalog it held before -- path, recipes and derived views together. That
/// property is structural rather than careful: there is no reachable state in which the
/// owner has been half-replaced, because the only call that writes it takes everything at
/// once and is the last statement here.
///
/// A VALID EMPTY CATALOG IS A SUCCESSFUL REPLACEMENT. `builder::check_recipes` admits one
/// deliberately (a project with nothing to build is a project), so a file that legitimately
/// names no recipes INSTALLS -- an empty catalog in force, with its own source path -- and
/// is not confused with a parse failure, which installs nothing at all.
///
/// ---- What a refusal can and cannot distinguish ----------------------------------
///
/// The sentence handed back is the OWNER'S OWN, unwrapped and unembellished:
/// `persist::read_file`'s when the file cannot be read, and `recipe_persist::from_text`'s
/// when the bytes are not a catalog this build reads. Completion adds no third kind,
/// because `complete_recipes` is TOTAL -- it fills in host facts and cannot fail, and an
/// authored spelling it cannot complete (a relative source with no project to stand on) is
/// left exactly as authored to be refused later, by name, by whoever tries to spend it.
/// Inventing a completion refusal here would be this seam claiming a distinction its own
/// owners do not make, and would also make the live door STRICTER than the launch -- the
/// second recipe policy this function exists to prevent.
inline Written install_recipes(CurrentRecipes& owner, const std::string& path,
                               const std::string& host_dir, const std::string& project_dir,
                               CurrentRecipes::ArtifactFile artifact_file) {
    recipe_persist::LoadedRecipes read = recipe_persist::load_file(path);
    if (!read.outcome.accepted) {
        return read.outcome; // NOTHING WAS TOUCHED: the candidate never became a catalog
    }
    recipe_persist::complete_recipes(read.recipes, host_dir, project_dir);
    owner.hold(path, std::move(read.recipes), artifact_file);
    return Written::ok();
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_RECIPES_HPP
