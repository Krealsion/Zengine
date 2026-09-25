// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_STAGING_HPP
#define ZENGINE_WORKSHOP_STAGING_HPP

// Where a built product goes before the Kernel opens it, and how a running image becomes the file
// a restart loads: the host's two rules, written once. A rebuilt product never lands on the
// mapped, loaded file (agents/decisions/a-reload-lands-off-the-loaded-path.md); a reload copies
// to a per-operation path nothing has written before, and a promotion writes sibling-then-rename
// and keeps the bytes it replaces, so a revert still has an image.
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

/// Copy `from` to `to`, removing `to` first: MinGW's same-file test answers (drive, 0) for every
/// file, so `overwrite_existing` would be refused there. The destination is never a mapped file.
inline std::error_code copy_over(const std::filesystem::path& from,
                                 const std::filesystem::path& to) {
    std::error_code ec;
    std::filesystem::remove(to, ec);
    ec.clear();
    std::filesystem::copy_file(from, to, ec);
    return ec;
}

/// How many taken names a fresh per-operation path passes over before it says why not. A bound
/// on a loop over a directory a maker can fill, not a limit on reloads: each pass is one name.
inline constexpr std::size_t kFreshNameTries = 4096;

/// COPY `from` TO A PER-OPERATION PATH IN `reloads` THAT NO FILE HOLDS YET -- `<stem>-<n><tail>`,
/// n counting on from `counter` -- and say where. `copy_file` with no overwrite option refuses
/// an existing destination (`file_exists`, which MinGW answers for every existing file), so a
/// name another process took between the look and the copy is passed over too; any other refusal
/// is the operating system's, in its words. Empty `path` means nothing was copied.
inline std::filesystem::path copy_fresh(const std::filesystem::path& from,
                                        const std::filesystem::path& reloads,
                                        const std::string& stem, const std::string& tail,
                                        std::size_t& counter, std::error_code& refused) {
    for (std::size_t tries = 0; tries < kFreshNameTries; ++tries) {
        ++counter;
        const std::filesystem::path to =
            reloads / (stem + "-" + std::to_string(counter) + tail);
        std::error_code looked;
        if (std::filesystem::exists(to, looked) || looked) {
            continue;
        }
        std::error_code copied;
        std::filesystem::copy_file(from, to, copied);
        if (!copied) {
            refused.clear();
            return to;
        }
        if (copied != std::errc::file_exists) {
            refused = copied;
            return std::filesystem::path();
        }
    }
    refused = std::make_error_code(std::errc::file_exists);
    return std::filesystem::path();
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
        std::error_code refused;
        const std::filesystem::path fresh = copy_fresh(
            source, reloads, stem, plain.extension().string(), host.reloads, refused);
        if (fresh.empty()) {
            out.refusal = "could not copy " + source.generic_string() + " to a new file in " +
                          reloads.generic_string() + ": " + refused.message();
            return out;
        }
        out.ok = true;
        out.path = fresh.generic_string();
        return out;
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

/// The product `recipe` has already made for `stem`, when one is on disk: what `load it` spends
/// once the new row is the frontier, so a product in its workspace loads by the button's own act.
/// A probe, so `stage`'s two refusals are "no product" here.
// WL-AUTH-02 -- agents/workshop/authoring.md
inline std::string product_of(const Host& host, const std::string& stem,
                              const std::string& recipe) {
    if (host.recipes == nullptr) {
        return std::string();
    }
    const builder::RecipeView* view = builder::view_named(host.recipes->views(), recipe);
    if (view == nullptr || view->artifact != stem) {
        return std::string();
    }
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::path(view->path), ec) || ec) {
        return std::string();
    }
    return view->path;
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
        std::error_code aside;
        const std::filesystem::path kept =
            copy_fresh(target, reloads, stem, "-promoted-over" + target.extension().string(),
                       host.reloads, aside);
        if (kept.empty()) {
            out.detail = "could not keep " + target.generic_string() + " aside in " +
                         reloads.generic_string() + ": " + aside.message();
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
