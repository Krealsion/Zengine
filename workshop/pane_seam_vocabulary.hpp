// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_SEAM_VOCABULARY_HPP

// What a pane weave asks the host, and what it hears back: the shared half of the pane seam. Each
// host fact a loaded image needs is an ask to an office and an answer, split by whether answering
// acts. Values only: knowing a path is not permission to read, write or delete under it.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The places file's suggested name, spelled here because both sides say it. The host resolves
/// where it lives and answers it through `ProjectRoot`; the pane owns the file.
inline constexpr const char* kDefaultMarksFileName = "workshop-marks.json";

/// The office that answers where this run began and which places file it owns. A host that
/// mounts no such door holds no such office, and an ask reaches nobody.
inline constexpr const char* kProjectRole = "zengine.project";

/// The office that may change which recipes this project means -- separate from
/// `zengine.project` because answering it writes.
inline constexpr const char* kRecipesRole = "zengine.recipes";

/// The office that holds the one source document. A literal rather than
/// `editor-pane/vocabulary.hpp`'s constant: neither asker links the weave, and a case checks the
/// two spellings agree.
inline constexpr const char* kEditorRole = "zengine.editor";

// ---- The project root, read ----------------------------------------------------

/// Ask where this run began and where its marks live. It carries nothing.
struct ProjectRootRequested {
    ZEN_SHAPE(ProjectRootRequested, 1);
};

/// The two facts the browser reads from the host: `project_dir`, where Workshop was launched (empty
/// for a run that began nowhere), and `marks_path`, where the pane's marks are durable (empty for
/// none). Where the maker is browsing is the pane's own state, not this.
struct ProjectRoot {
    std::string project_dir;
    std::string marks_path;
    ZEN_SHAPE(ProjectRoot, 1, ZEN_FIELD(project_dir), ZEN_FIELD(marks_path));
};

// ---- The recipe catalog, changed -----------------------------------------------

/// Use the file at this path as this session's recipe catalog.
struct RecipeUseRequested {
    std::string path;
    ZEN_SHAPE(RecipeUseRequested, 1, ZEN_FIELD(path));
};

/// Author one recipe row from what a maker typed: the host composes it, checks it by the recipe
/// law, appends it as written, saves atomically and installs it. `tree` says which of the two
/// kinds this draft is; the other kind's fields are empty.
struct RecipeAuthorRequested {
    std::string id;                    ///< what the maker calls it
    std::string artifact;              ///< the stem it produces
    std::string source;                ///< single-source: the one .cpp, as the pane spelled it
    std::vector<std::string> packages; ///< single-source: CMAKE_PREFIX_PATH entries
    std::vector<std::string> links;    ///< single-source: exported target names
    std::string build_dir;             ///< cmake-target: the configured tree
    std::string target;                ///< cmake-target: the target in it
    std::string artifact_dir;          ///< cmake-target: where it lands, or empty
    std::string config;                ///< cmake-target: a multi-config generator's
                                        ///< configuration, asked when the tree has several; empty
                                        ///< everywhere else
    bool tree = false;                 ///< which of the two kinds this draft is
    // Version 2 since `config` joined (WL-AUTH-01). For a pane built apart from its host, see
    // docs/workshop/develop-workshop.md's "a pane's messages change".
    ZEN_SHAPE(RecipeAuthorRequested, 2, ZEN_FIELD(id), ZEN_FIELD(artifact), ZEN_FIELD(source),
              ZEN_FIELD(packages), ZEN_FIELD(links), ZEN_FIELD(build_dir), ZEN_FIELD(target),
              ZEN_FIELD(artifact_dir), ZEN_FIELD(config), ZEN_FIELD(tree));
};

/// What either recipe act came to: `accepted`, or the owner's words for why not. `path` is the
/// catalog in force after the attempt; a refused act leaves it exactly as it was.
struct RecipeOutcome {
    bool accepted = false;
    std::string refusal;
    std::string path;
    std::int64_t recipes = 0;
    ZEN_SHAPE(RecipeOutcome, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(path),
              ZEN_FIELD(recipes));
};

// ---- One source, opened in the Editor ------------------------------------------

/// Open this path as the source document: asked of `zengine.opening`, or of `zengine.editor`,
/// which relays it to the managed opening (WL-OPEN-01).
struct OpenSourceRequested {
    std::string path;
    ZEN_SHAPE(OpenSourceRequested, 1, ZEN_FIELD(path));
};

/// What opening came to: `accepted`, or the door's own sentence, which the pane says in its own
/// row. What `accepted` establishes is WL-OPEN-02's.
struct SourceOpened {
    bool accepted = false;
    std::string refusal;
    ZEN_SHAPE(SourceOpened, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal));
};

// ---- A pane's code, reached from the pane ---------------------------------------

/// The source behind a pane's running code is open, because a maker asked from that pane: said
/// once, when the open Edit Code asked for was accepted and the host still names the same code.
/// Published to any, as Workshop's office. A reading, not an order: it starts no build.
struct PaneSourceOpened {
    std::string office;   ///< the pane's provider office, as Loom stamped its offer
    std::string pane;     ///< the pane key, in that office's namespace
    std::string name;     ///< what the maker sees the pane called
    std::string artifact; ///< the artifact the office's running code was realized from
    std::string recipe;   ///< the one authored recipe that produces it
    std::string source;   ///< the file now open: that recipe's single source
    ZEN_SHAPE(PaneSourceOpened, 1, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(name),
              ZEN_FIELD(artifact), ZEN_FIELD(recipe), ZEN_FIELD(source));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_SEAM_VOCABULARY_HPP
