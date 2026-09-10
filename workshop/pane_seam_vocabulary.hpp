// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_SEAM_VOCABULARY_HPP

// WHAT A PANE WEAVE ASKS THE HOST, AND WHAT IT HEARS BACK -- the shared half of the pane
// seam, and the shapes the FIRST tenant needed.
//
// ⚠ THE NAME IS THE THIRD TENANT'S DOING. This was `files_seam_vocabulary.hpp` through two
// migrations, because the project browser cut the seam and nobody else was standing in it.
// The Builder arrived and shared two of these names; the Attention pane is the third, and a
// file whose name says `files` while three panes read it is a name that has stopped being
// true. Nothing else about this file changed with the rename: every declaration below is
// where it was, saying what it said. What is genuinely SHARED -- `kProjectRole`, the
// read-only project office, and `SourceOpened`, what opening a source came to -- is what
// makes the file worth a name of its own; what is Files' alone (`kDefaultMarksFileName`,
// `zengine.recipes` and the three recipe shapes) stays here rather than moving, because
// splitting it is a second change and this was one.
//
// The project browser became a loadable weave (Zengine/files/), and the four facts it used
// to read straight off `HostContext` -- where this run began, which places file it owns,
// whether a file it points at is a recipe catalog, and one path to open in the Editor --
// cannot cross into a loaded image as anything but a value. So each becomes an ask to an
// OFFICE and an answer back, the seam `zengine.arrangement` and `zengine.sources` already
// spend.
//
// ---- THREE OFFICES, SPLIT BY WHETHER ANSWERING ACTS ---------------------------
//
//     zengine.project    read-only    ProjectRootRequested  -> ProjectRoot
//                                     RecipeSourceRequested  -> RecipeSourceSaid
//     zengine.recipes    ACTS         RecipeUseRequested     -> RecipeOutcome
//                                     RecipeAuthorRequested  -> RecipeOutcome
//     zengine.editor     ACTS         OpenSourceRequested    -> SourceOpened
//
// The split is `arrangement_vocabulary.hpp`'s own: a question whose answer runs nobody's
// code lives apart from one whose whole purpose is to change the project on disk, so
// "which office can write a maker's files" keeps a one-word answer. `ProjectRoot` reads
// two strings the host captured once; `zengine.recipes` writes recipe catalogs through the
// one authoring writer; and opening a source is the Editor's. It was addressed at
// `zengine.workshop` while the host held the document, with a note that the sentence would
// one day go to `zengine.editor` and only the address would move. It has: the Editor is a
// weave of its own (`Zengine/editor-pane/`), the document lives in it, and the two askers
// spell `kEditorRole` below. Which SOURCE a recipe names stayed with the host, because the
// completed catalog is the host's: that is the read-only door's third question.
//
// ---- WHAT CROSSES, AND WHAT CANNOT --------------------------------------------
//
// VALUES. Every field below is Text, Int, Bool or a List of those -- the ordinary Loom
// wire, admitted at the reader's own schema. No `HostContext&`, no `CurrentRecipes&`, no
// `LocationMarks`, no `Session&` and no host address of any kind. The C++ objects that own
// these facts stay in the host's `main`; a reader gets a picture and can do nothing to
// them but ask again.
//
// AND KNOWLEDGE IS NOT AUTHORITY. A weave that hears `ProjectRoot{...}` has learned two
// paths; it has not been permitted to read, write or delete anything under them. The one
// act each ACTING office performs is spelled by the shape it answers and by nothing else --
// the rule `arrangement_vocabulary.hpp` and `sample_vocabulary.hpp` state one seam over.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// THE PLACES FILE'S SUGGESTED NAME. The host resolves WHERE it lives (its durable-path
/// rule, beside the keymap's and the session's) and answers it through `ProjectRoot`; the
/// pane OWNS the file. The name is here because both sides spell it.
inline constexpr const char* kDefaultMarksFileName = "workshop-marks.json";

/// THE OFFICE THAT ANSWERS WHERE THIS RUN BEGAN AND WHICH PLACES FILE IT OWNS. A ROLE, for
/// `kArrangementRole`'s reason: it survives its holder being replaced, and a loaded
/// artifact names it without ever learning a `WeaveId`. A host that mounts no such door
/// holds no such office and an ask reaches nobody -- the correct answer for a host with no
/// project.
inline constexpr const char* kProjectRole = "zengine.project";

/// THE OFFICE THAT MAY CHANGE WHICH RECIPES THIS PROJECT MEANS. Separate from
/// `zengine.project` because answering it WRITES -- it installs a catalog or appends a
/// recipe row -- and the read-only door must not be the one that can.
inline constexpr const char* kRecipesRole = "zengine.recipes";

/// THE OFFICE THAT HOLDS THE ONE SOURCE DOCUMENT AND OPENS A SOURCE INTO IT. A ROLE, for the
/// two above's reason: a reloaded Editor is still the party a Files row asks. Spelled here
/// rather than taken from `editor-pane/vocabulary.hpp`, for `pane_migration.hpp`'s reason:
/// that header is the weave's own and neither asker links the weave; a case checks the two
/// spellings against each other, which is the seam where a divergence would actually be
/// caught. A host that loads no Editor holds no such office and an ask reaches nobody -- the
/// asker's row stays as it was, and nothing was opened.
inline constexpr const char* kEditorRole = "zengine.editor";

// ---- The project root, read ----------------------------------------------------

/// ASK WHERE THIS RUN BEGAN AND WHERE ITS MARKS LIVE. Carries nothing: a filter would be a
/// policy the asker was never entitled to author, `ArrangementRequested`'s own reason.
struct ProjectRootRequested {
    ZEN_SHAPE(ProjectRootRequested, 1);
};

/// THE TWO FACTS THE BROWSER READS FROM THE HOST AND NOTHING ELSE. `project_dir` is where
/// Workshop was launched -- the origin a fresh browse begins at and what a relative recipe
/// source means; empty is the designed absence (a run that began nowhere). `marks_path` is
/// the machine-local file the pane's marks are durable in; empty is a run that keeps none.
///
/// IT IS NOT THE BROWSING LOCATION. Where the maker is looking is the pane's own state
/// (`FilesState::current_dir`); this says where it STARTS, which is a host fact and does
/// not move when the maker walks.
struct ProjectRoot {
    std::string project_dir;
    std::string marks_path;
    ZEN_SHAPE(ProjectRoot, 1, ZEN_FIELD(project_dir), ZEN_FIELD(marks_path));
};

// ---- The recipe catalog, changed -----------------------------------------------

/// USE THE FILE AT THIS PATH AS THIS SESSION'S RECIPE CATALOG. The path is already the
/// pane's own resolved spelling of a listed row; the host reads the file and answers.
struct RecipeUseRequested {
    std::string path;
    ZEN_SHAPE(RecipeUseRequested, 1, ZEN_FIELD(path));
};

/// AUTHOR ONE RECIPE ROW FROM WHAT A MAKER TYPED. The fields are `HostContext::RecipeDraft`
/// on the wire: the host composes the recipe, checks it by the recipe law, appends it AS
/// WRITTEN to the catalog in force (or a project catalog), saves atomically and installs
/// it -- the one authoring writer, unchanged, behind one sentence. `tree` says which of the
/// two kinds this draft is; the unused half's fields are empty.
struct RecipeAuthorRequested {
    std::string id;                    ///< what the maker calls it
    std::string artifact;              ///< the stem it produces
    std::string source;                ///< single-source: the one .cpp, as the pane spelled it
    std::vector<std::string> packages; ///< single-source: CMAKE_PREFIX_PATH entries
    std::vector<std::string> links;    ///< single-source: exported target names
    std::string build_dir;             ///< cmake-target: the configured tree
    std::string target;                ///< cmake-target: the target in it
    std::string artifact_dir;          ///< cmake-target: where it lands, or empty
    bool tree = false;                 ///< which of the two kinds this draft is
    ZEN_SHAPE(RecipeAuthorRequested, 1, ZEN_FIELD(id), ZEN_FIELD(artifact), ZEN_FIELD(source),
              ZEN_FIELD(packages), ZEN_FIELD(links), ZEN_FIELD(build_dir), ZEN_FIELD(target),
              ZEN_FIELD(artifact_dir), ZEN_FIELD(tree));
};

/// WHAT EITHER RECIPE ACT CAME TO -- `HostContext::RecipeSwap` on the wire. `accepted` with
/// an empty `refusal`, or the owner's own words for why not; `path` is the catalog IN FORCE
/// after the attempt (never a different file from the one running), and `recipes` how many
/// it holds. A refused act leaves the catalog exactly as it was and says so.
struct RecipeOutcome {
    bool accepted = false;
    std::string refusal;
    std::string path;
    std::int64_t recipes = 0;
    ZEN_SHAPE(RecipeOutcome, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(path),
              ZEN_FIELD(recipes));
};

// ---- One source, opened in the Editor ------------------------------------------

/// OPEN THIS PATH IN THE EDITOR. Addressed to `zengine.editor` (`kEditorRole`), the office
/// that holds the one source document; the Editor weave normalizes, reads and judges the
/// file through its one door `open_source(path)` (WL-EDIT-05), installs it, answers, and
/// asks Workshop to reveal the pane. A dirty buffer's refusal reaches the asker as the
/// answer. The sentence used to go to `zengine.workshop`; only the address moved.
struct OpenSourceRequested {
    std::string path;
    ZEN_SHAPE(OpenSourceRequested, 1, ZEN_FIELD(path));
};

/// WHAT OPENING CAME TO. `accepted` with an empty `refusal`, or the door's own sentence --
/// a file that is not there, a name the custody cannot carry, a dirty buffer that must be
/// saved or discarded first. The pane says the refusal in its own row.
///
/// ⚠ ACCEPTED IS NOT PRESENTED. The Editor answers this the moment the document is installed
/// in its own image; whether Workshop then seats its pane is a separate outcome the host says
/// on the notice line (`PaneRevealRequested`, pane_vocabulary.hpp). A refused reveal leaves
/// the document open in the weave, exactly as a removed pane does.
struct SourceOpened {
    bool accepted = false;
    std::string refusal;
    ZEN_SHAPE(SourceOpened, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_SEAM_VOCABULARY_HPP
