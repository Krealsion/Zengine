// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_STAGING_HPP
#define ZENGINE_WORKSHOP_STAGING_HPP

// WHERE A BUILT PRODUCT GOES BEFORE THE KERNEL OPENS IT, AND HOW A RUNNING IMAGE
// BECOMES THE FILE A RESTART LOADS (RELOAD-1).
//
// TWO RULES, BOTH THE HOST'S, WRITTEN ONCE. The realization owner cannot spell a path
// and must not (its source is read for the attempt); the Workshop host and the build
// witness both wire these into it, and a rule spelled twice is how one host comes to
// stage a file where the other will not find it.
//
// THE PATH RULE. A live artifact's file is MAPPED by this process: Windows refuses a
// writer on it and Linux lets a writer change code under the running program (both
// measured). So a rebuilt product never lands on the loaded file. A single-source
// recipe builds into its own workspace (`recipe_persist::complete_recipes`), a CMake
// target builds wherever its project puts it, and it is THIS rule that copies the
// product to where it will be opened from:
//
//     an initial realization   ->  the file the plan resolves the stem to
//     a reload in place        ->  <host>/<stem>.reloads/<stem>-<n>.<suffix>, n a
//                                  per-process counter; nothing here prunes them
//
// A source that IS its destination is no copy. Every other case copies through an
// `error_code`, and the operating system's words are the refusal.
//
// THE PROMOTION RULE. A reload leaves the plan's file exactly as it was, so a maker
// who quits runs the old code next launch. `promote` writes the running image's
// bytes into that file the way every durable file here is written -- a sibling, then
// a rename -- so a refused write leaves nothing half-written, and it KEEPS the bytes it
// writes over at a per-operation path first, so a revert after a promotion still has an
// image to run. Whether the rename is refused (the old image still mapped, which is
// KERN-05 not honoured by a foreign target) is the operating system's to say, and it
// is said in its words.
// Workshop law: agents/workshop/project.md

#include "load_execute.hpp"
#include "recipes.hpp"

#include "builder/recipe.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>

namespace zengine::workshop::staging {

/// WHAT THE TWO RULES SPEND: the host's artifact directory, the catalog in force (read
/// at the moment of the act, never copied), the host's one rule for spelling a stem as
/// a file, and the per-process reload counter.
struct Host {
    std::string dir;
    const CurrentRecipes* recipes = nullptr;
    CurrentRecipes::ArtifactFile artifact_file = nullptr;
    std::size_t reloads = 0;
};

/// COPY `from` TO `to`, REMOVING `to` FIRST. Unconditional rather than
/// `overwrite_existing`, for `Stage::put`'s measured reason in the load suite: MinGW's
/// same-file test answers (drive, 0) for every file, so an overwrite of an existing
/// destination is refused `file_exists` there and the old bytes stay. The destination
/// is never a mapped file -- an initial realization's target is absent (that is why the
/// row waited) and a reload's target is a fresh per-operation path.
inline std::error_code copy_over(const std::filesystem::path& from,
                                 const std::filesystem::path& to) {
    std::error_code ec;
    std::filesystem::remove(to, ec);
    ec.clear();
    std::filesystem::copy_file(from, to, ec);
    return ec;
}

/// PUT THE PRODUCT OF `recipe` WHERE `stem` WILL BE OPENED FROM. The signature is the
/// owner's `StageArtifact` seam, with the host's own facts in front.
// WL-PROJ-16 -- agents/workshop/project.md
inline load::PlanExecutor::Staged stage(Host& host, const std::string& stem,
                                        const std::string& recipe, bool reload) {
    load::PlanExecutor::Staged out;
    if (host.recipes == nullptr || host.artifact_file == nullptr) {
        out.refusal = "this host wired no catalog into its staging rule";
        return out;
    }
    const builder::RecipeView* view = builder::view_named(host.recipes->views(), recipe);
    if (view == nullptr) {
        out.refusal = "recipe `" + recipe +
                      "` is not in the catalog in force, so its product cannot be found; "
                      "nothing was staged";
        return out;
    }
    if (view->artifact != stem) {
        out.refusal = "recipe `" + recipe + "` produces `" + view->artifact + "`, not `" +
                      stem + "`; nothing was staged";
        return out;
    }
    const std::filesystem::path source(view->path);
    const std::filesystem::path plain(host.artifact_file(host.dir, stem));
    std::filesystem::path destination = plain;
    if (reload) {
        const std::filesystem::path reloads =
            std::filesystem::path(host.dir) / (stem + ".reloads");
        std::error_code made;
        std::filesystem::create_directories(reloads, made);
        if (made) {
            out.refusal = "could not make " + reloads.generic_string() + ": " + made.message();
            return out;
        }
        ++host.reloads;
        destination = reloads / (stem + "-" + std::to_string(host.reloads) +
                                 plain.extension().string());
    }
    if (source.lexically_normal() == destination.lexically_normal()) {
        out.ok = true;
        out.path = destination.generic_string();
        return out;
    }
    const std::error_code copied = copy_over(source, destination);
    if (copied) {
        out.refusal = "could not copy " + source.generic_string() + " to " +
                      destination.generic_string() + ": " + copied.message();
        return out;
    }
    out.ok = true;
    out.path = destination.generic_string();
    return out;
}

/// WRITE `image`'S BYTES INTO THE FILE THE PLAN RESOLVES `stem` TO, sibling then rename --
/// and KEEP THE BYTES IT WRITES OVER, at a per-operation path beside the reload copies, so
/// a revert after a promotion has an honest image to run. The signature is the owner's
/// `PromoteImage` seam, with the host's facts in front; `kept` is where the old default
/// went, or empty when there was none.
// WL-PROJ-16 -- agents/workshop/project.md
inline load::PlanExecutor::Promoted promote(Host& host, const std::string& stem,
                                            const std::string& image) {
    load::PlanExecutor::Promoted out;
    if (host.artifact_file == nullptr) {
        out.detail = "this host wired no artifact rule into its promotion rule";
        return out;
    }
    const std::filesystem::path target(host.artifact_file(host.dir, stem));
    std::error_code looked;
    if (std::filesystem::exists(target, looked) && !looked) {
        const std::filesystem::path reloads =
            std::filesystem::path(host.dir) / (stem + ".reloads");
        std::error_code made;
        std::filesystem::create_directories(reloads, made);
        if (made) {
            out.detail = "could not make " + reloads.generic_string() + ": " + made.message();
            return out;
        }
        ++host.reloads;
        const std::filesystem::path kept =
            reloads / (stem + "-" + std::to_string(host.reloads) + "-promoted-over" +
                       target.extension().string());
        const std::error_code aside = copy_over(target, kept);
        if (aside) {
            out.detail = "could not keep " + target.generic_string() + " aside at " +
                         kept.generic_string() + ": " + aside.message();
            return out;
        }
        out.kept = kept.generic_string();
    }
    const std::filesystem::path sibling(target.string() + ".promoting");
    const std::error_code copied = copy_over(std::filesystem::path(image), sibling);
    if (copied) {
        out.detail = "could not write " + sibling.generic_string() + ": " + copied.message();
        return out;
    }
    std::error_code renamed;
    std::filesystem::rename(sibling, target, renamed);
    if (renamed) {
        std::error_code dropped;
        std::filesystem::remove(sibling, dropped);
        out.detail = "could not replace " + target.generic_string() + ": " + renamed.message();
        return out;
    }
    out.ok = true;
    out.detail = target.generic_string() + " now holds the running image";
    return out;
}

} // namespace zengine::workshop::staging

#endif // ZENGINE_WORKSHOP_STAGING_HPP
