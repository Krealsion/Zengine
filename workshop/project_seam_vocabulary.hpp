// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PROJECT_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PROJECT_SEAM_VOCABULARY_HPP

// The project door's conversation: where this run began and which places file it owns, read; and
// which recipes this project means, changed. Every ask goes to `zengine.project`, the host's
// in-process door, and is answered by it. Values only: knowing a path is not permission to read,
// write or delete under it.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The project role: it answers where this run began and which places file it owns, and it is
/// the one role that may change which recipes this project means and which artifacts it loads.
/// A host that mounts no such door holds no such role, and an ask reaches nobody.
inline constexpr const char* kProjectRole = "zengine.project";

// ---- The project root, read ----------------------------------------------------

/// Ask where this run began and where its marks live. It carries nothing.
struct ProjectRootRequested {
    ZEN_SHAPE(ProjectRootRequested, 1);
};

/// The two facts the browser reads from the host: `project_dir`, where Workshop was launched (empty
/// for a run that began nowhere), and `marks_path`, where the pane's marks are durable (empty for
/// none). Where the weaver is browsing is the pane's own state, not this.
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

/// Author one recipe row from what a weaver typed: the host composes it, checks it by the recipe
/// law, appends it as written, saves atomically and installs it. `tree` says which of the two
/// kinds this draft is; the other kind's fields are empty.
struct RecipeAuthorRequested {
    std::string id;                    ///< what the weaver calls it
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
    std::string makes = {};            ///< single-source: native, boxed or both
    // Version 3 since `makes` joined (WL-AUTH-01). For a pane built apart from its host, see
    // workshop/docs/develop-workshop.md's "a pane's messages change".
    ZEN_SHAPE(RecipeAuthorRequested, 3, ZEN_FIELD(id), ZEN_FIELD(artifact), ZEN_FIELD(source),
              ZEN_FIELD(packages), ZEN_FIELD(links), ZEN_FIELD(build_dir), ZEN_FIELD(target),
              ZEN_FIELD(artifact_dir), ZEN_FIELD(config), ZEN_FIELD(tree), ZEN_FIELD(makes));
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

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PROJECT_SEAM_VOCABULARY_HPP
