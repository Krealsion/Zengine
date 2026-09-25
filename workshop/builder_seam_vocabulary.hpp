// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_BUILDER_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_BUILDER_SEAM_VOCABULARY_HPP

// What the Builder pane asks the host, and what it hears back: `pane_seam_vocabulary.hpp`'s seam
// with a second tenant. Reads go to the read-only `zengine.project`; appending a plan row goes to
// `zengine.plan`, which writes the load plan (which artifacts run) and is not `zengine.recipes`
// (how one is produced). A frontier is a reading, not a power: realizing an artifact still goes
// through `BuildRequested` to the Builder office.

// `kProjectRole` and `SourceOpened` are the files seam's, included and never respelled.
#include "pane_seam_vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::workshop {

/// The office that may change which artifacts this project loads. A host that mounts no such door
/// holds no such office.
inline constexpr const char* kPlanRole = "zengine.plan";

// ---- What the project is waiting on, read ---------------------------------------

/// Ask what project realization is stopped on right now. It carries nothing.
struct ProjectFrontierRequested {
    ZEN_SHAPE(ProjectFrontierRequested, 1);
};

/// `ProjectFrontier` (workshop/panel.hpp) on the wire: derived at the ask and never published,
/// because the owner knows no presentation exists. The pane asks on the beats where it can have
/// moved: its room grant, every `BuildStatus`, and the answer to a plan row it wrote.
struct ProjectFrontierSaid {
    bool waiting = false;       ///< realization is stopped at a row waiting on the maker
    std::string artifact;       ///< the frontier artifact stem; empty when not waiting
    std::int64_t blocked = 0;   ///< authored rows behind the frontier, waiting on it
    ZEN_SHAPE(ProjectFrontierSaid, 1, ZEN_FIELD(waiting), ZEN_FIELD(artifact),
              ZEN_FIELD(blocked));
};

// ---- Whether the plan already names an artifact, read ---------------------------

/// Does the plan in force already name this artifact? Answered from the plan the host holds.
struct PlanNamesRequested {
    std::string stem;
    ZEN_SHAPE(PlanNamesRequested, 1, ZEN_FIELD(stem));
};

/// The answer, and the stem it is about, so a pane whose cursor has moved reads it against the
/// row it asked about.
struct PlanNames {
    std::string stem;
    bool named = false;
    ZEN_SHAPE(PlanNames, 1, ZEN_FIELD(stem), ZEN_FIELD(named));
};

// ---- One plan row, authored -----------------------------------------------------

/// Load this artifact as this role: `append_plan_row`'s arguments, plus the recipe so the answer
/// can say whether its product is built. The host composes, checks, runs and saves the row.
struct PlanRowRequested {
    std::string stem;
    std::string role;
    std::string recipe;
    ZEN_SHAPE(PlanRowRequested, 1, ZEN_FIELD(stem), ZEN_FIELD(role), ZEN_FIELD(recipe));
};

/// What authoring the row came to: `accepted`, or the owner's words. `detail` is what the running
/// project made of it, `frontier` that the row is now waited on, `product` the recipe's product
/// when that row is the frontier and the product exists, `path` the plan file written.
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

// ---- One recipe's source, resolved by the host --------------------------------------

/// Which source file this recipe was authored from, asked of `zengine.project` (WL-PROJ-02). It
/// names a recipe, not a path: a presentation holds a recipe's name, never its procedure. The
/// Builder then takes that one path to the Editor's door itself.
struct RecipeSourceRequested {
    std::string recipe;
    ZEN_SHAPE(RecipeSourceRequested, 1, ZEN_FIELD(recipe));
};

/// What the recipe resolves to: the one absolute `source`, or a refusal in the owner's words.
/// `recipe` rides back so the asker reads the answer against the row it asked about.
struct RecipeSourceSaid {
    std::string recipe;
    bool accepted = false;
    std::string refusal;
    std::string source;
    ZEN_SHAPE(RecipeSourceSaid, 1, ZEN_FIELD(recipe), ZEN_FIELD(accepted), ZEN_FIELD(refusal),
              ZEN_FIELD(source));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_BUILDER_SEAM_VOCABULARY_HPP
