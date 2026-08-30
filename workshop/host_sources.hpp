// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_HOST_SOURCES_HPP
#define ZENGINE_WORKSHOP_HOST_SOURCES_HPP

// THE HOST MAY DESCRIBE ITSELF; IT MAY NOT INVENT PROVIDER POWER (SOURCE-0).
//
// PROV-0's law was, and remains, that a host authors no operator: its catalog starts
// empty, powers arrive from providers, and a tripwire reads `workshop.cpp` for the
// semantic strings a host would have to name in order to manufacture meaning for
// itself. What that law over-reached is the case it was never built for -- a host
// exposing facts it ALREADY OWNS, which is not manufacturing meaning at all. This
// file is the refinement, and the boundary is mechanical rather than promised:
//
//     may     zero-input SOURCES over state or synchronous observation the host
//             legitimately owns -- facts it is already the owner of, made routable
//     may not parameterized application power, provider behaviour, participation
//             authority, or a hidden maker input smuggled in as an authored default
//
// `mount_host_sources` ENFORCES THE SECOND HALF rather than describing it: every
// definition in the batch is judged `op::is_source` before any of them is installed,
// so a host that grew a parameterized operator could not get it into its own catalog
// through its own door. The refusal names the definition and its ports.
//
// ---- IT IS TWO SOURCES, NAMED ONE AT A TIME ------------------------------------
//
//     zengine.project.anchor    the project-relative semantic anchor this host owns
//     zengine.recipes.catalog   which authored recipe catalog is in force, and how
//                               many completed recipes it currently holds
//
// NOTHING IS AUTO-WRAPPED. No field, getter, pane, preference, buffer or schema
// becomes routable because it exists; the host does not become reflectable. Exposure
// is an act, and a fact being true is not a reason to publish it -- the clipboard, the
// editor's buffer, the keymap, the session file and the grant ledger are all true and
// all deliberately absent, exactly as `DelayAuthority::host_backed()` is a diagnostic
// and never a door.
//
// ---- THE BODY READS THE OWNER; IT DOES NOT HOLD AN ANSWER ----------------------
//
// Each native body closes over a REFERENCE to the live owner and reads it at the
// moment of the sample. That is the whole difference between a route and a cached
// answer: `CurrentRecipes` is replaced by CONTENTS (`hold()` assigns into members the
// object already has), so a live catalog swap moves what this Source reports with
// nothing re-registered, and a REFUSED swap leaves the owner untouched and therefore
// leaves the sampled answer where it was. A startup copy taken here would report the
// launch catalog forever and no test of the Source alone could see it.
//
// ⚠ WHICH MAKES THE HOST'S DECLARATION ORDER THIS FILE'S LIFETIME PROOF. The owners
// must outlive the catalog that holds these closures: `CurrentRecipes` and the
// `HostContext` are declared BEFORE `op::Catalog` in `workshop.cpp`, so reverse-order
// destruction drops the closures first. The rvalue overloads below are deleted so a
// temporary cannot be handed over at all -- an owner with no name is an owner that
// dies before the sample.
//
// ---- SAMPLING ONE IS NOT PARTICIPATION -----------------------------------------
//
// Both bodies read memory this process already holds and return. Neither loads,
// realizes, asks another weave to act, waits for a frame, schedules later work, moves
// project selection, chooses a recipe catalog, writes a file, saves, builds or
// acquires authority. If answering ever needed another participant to act, the answer
// would not be a Source from this seat and would belong in a message.
//
// ---- THE SCHEMAS, AND WHY THEY ARE NAMED THE WAY THEY ARE ----------------------
//
//     zengine.project.anchor    in  zengine.project.anchor.in  { }
//                               out zengine.ProjectAnchor      { anchor : Text }
//     zengine.recipes.catalog   in  zengine.recipes.catalog.in { }
//                               out zengine.RecipeCatalog
//                                     { catalog : zengine.RecipeCatalogFacts
//                                                 { source : Text, recipes : Int } }
//
// AN OUTPUT SCHEMA IS A PORT LIST, and `make_operator` names one `<identity>.out`
// because it derives it from a C++ return type that has no name of its own. These are
// authored, so they carry the meaning instead: an enumeration answers *what would
// sampling yield* out of the output schema's identity, and `zengine.recipes.catalog.out`
// would answer only *the output of that thing*.
//
// TWO NAMESPACES, KEPT APART. `zengine.project.anchor` answers WHICH routable
// information source is meant; `zengine.ProjectAnchor` answers WHAT admitted meaning
// a sample yields. Sources are routed by identity and never by schema: a Files
// browsing location is also a path and is a DIFFERENT FACT, so it would carry a
// different schema name, and `content_id` hashes the name -- which is what makes the
// two unsubstitutable at the gate rather than merely unwise to confuse.
//
// ⚠ PUBLISHING A FACT CHANGES NOTHING ABOUT IT. `project_dir` still means what
// `HostContext` says it means: origin is not project, the Files pane's location is not
// project, there is no project membership and no project object, and marks, recipe
// completion and path authority are all exactly where they were. What changed is that
// an already-owned fact is now addressable.

#include "recipes.hpp"

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/source.hpp"

#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHO CONTRIBUTED THESE, said honestly. It is not a loaded artifact and does not
/// pretend to be one: `Catalog::mount` takes a null custody for a provider that is not
/// an image at all, which is exactly what an in-process host is. The name is the
/// ordinary dotted provider identity every other provider carries, so provenance
/// projections carry it with nothing taught about hosts.
inline constexpr const char* kHostProvider = "zengine.workshop.host";

inline constexpr const char* kProjectAnchorSource = "zengine.project.anchor";
inline constexpr const char* kRecipeCatalogSource = "zengine.recipes.catalog";

namespace detail {

/// A Source's input schema: no ports, and an IDENTITY all the same. Two Sources have
/// two different empty schemas because the name is hashed into the content id, which is
/// what makes an empty pack built for one of them refused at the other's door.
inline std::shared_ptr<const loom::Schema> no_inputs(const std::string& identity) {
    return loom::make_schema(identity + ".in", 1, std::vector<loom::Field>());
}

inline const std::shared_ptr<const loom::Schema>& project_anchor_schema() {
    static const std::shared_ptr<const loom::Schema> s =
        loom::SchemaBuilder("zengine.ProjectAnchor", 1).field("anchor", loom::Kind::Text).build();
    return s;
}

inline const std::shared_ptr<const loom::Schema>& recipe_catalog_facts_schema() {
    static const std::shared_ptr<const loom::Schema> s =
        loom::SchemaBuilder("zengine.RecipeCatalogFacts", 1)
            .field("source", loom::Kind::Text)
            .field("recipes", loom::Kind::Int)
            .build();
    return s;
}

inline const std::shared_ptr<const loom::Schema>& recipe_catalog_schema() {
    static const std::shared_ptr<const loom::Schema> s =
        loom::SchemaBuilder("zengine.RecipeCatalog", 1)
            .message("catalog", recipe_catalog_facts_schema())
            .build();
    return s;
}

} // namespace detail

/// THE PROJECT-RELATIVE SEMANTIC ANCHOR THIS HOST OWNS, read from the owner.
///
/// EMPTY IS THE OWNER'S OWN DESIGNED ABSENCE and is carried verbatim. A working
/// directory the platform will not report and one this application cannot say are the
/// same absence in `HostContext`, refused in words wherever it is needed and never
/// guessed -- so this Source preserves it rather than manufacturing a path, because a
/// Source that invented a plausible answer would be worse than one that has none.
inline op::OperatorDef project_anchor_source(const std::string& project_dir) {
    const std::string identity = kProjectAnchorSource;
    return op::OperatorDef(
        identity, detail::no_inputs(identity), detail::project_anchor_schema(),
        [&project_dir](const loom::Value&) { return loom::Cell::text(project_dir); });
}
/// An owner with no name dies before the sample. Refused at compile time.
op::OperatorDef project_anchor_source(std::string&&) = delete;

/// WHICH AUTHORED CATALOG IS IN FORCE AND HOW MUCH IT HOLDS, read from the owner.
///
/// THE TWO FACTS ARE ONE ANSWER, taken in one read of one owner, because `CurrentRecipes`
/// holds them together for exactly that reason: "the path moved and the recipes did not"
/// is a state that header exists to make unspellable, and a Source that read them
/// through two calls would be a place for it to become spellable again.
///
/// It is the SMALLEST standing description and not the catalog: the completed rows,
/// their sources, their build procedures and their artifacts stay unexposed. Being in
/// memory is not a reason to be routable.
inline op::OperatorDef recipe_catalog_source(const CurrentRecipes& recipes) {
    const std::string identity = kRecipeCatalogSource;
    return op::OperatorDef(identity, detail::no_inputs(identity), detail::recipe_catalog_schema(),
                           [&recipes](const loom::Value&) {
                               loom::Value facts(detail::recipe_catalog_facts_schema());
                               facts.set("source", loom::Cell::text(recipes.source()));
                               facts.set("recipes", loom::Cell::integer(static_cast<std::int64_t>(
                                                        recipes.all().size())));
                               return loom::Cell::message(std::move(facts));
                           });
}
/// As above: a temporary owner is not an owner.
op::OperatorDef recipe_catalog_source(CurrentRecipes&&) = delete;

/// EVERY SOURCE THIS HOST EXPOSES, and the list is written out by hand on purpose.
inline std::vector<op::OperatorDef> host_sources(const std::string& project_dir,
                                                 const CurrentRecipes& recipes) {
    std::vector<op::OperatorDef> defs;
    defs.push_back(project_anchor_source(project_dir));
    defs.push_back(recipe_catalog_source(recipes));
    return defs;
}
std::vector<op::OperatorDef> host_sources(std::string&&, const CurrentRecipes&) = delete;
std::vector<op::OperatorDef> host_sources(const std::string&, CurrentRecipes&&) = delete;

/// THE HOST'S ONE DOOR INTO ITS OWN CATALOG, and the only thing it will carry.
///
/// Every definition is judged before any is installed -- the same all-or-nothing shape
/// `Catalog::mount` already keeps -- and the judgement is `op::is_source`. That is the
/// refined PROV-0 boundary as a mechanism: describing yourself is contributing facts
/// you own, and the moment a contribution would take an argument it has stopped being
/// a description and started being application power, whoever wrote it.
///
/// It is not a third registration mechanism. Underneath it is `Catalog::mount` with a
/// null custody, which is what that parameter is already for; there is no
/// `register_source`, no host-only door and no new ABI.
inline op::MountReport mount_host_sources(op::Catalog& into, std::vector<op::OperatorDef> defs) {
    for (const op::OperatorDef& def : defs) {
        if (!op::is_source(def)) {
            std::string ports;
            for (const loom::Field& f : def.inputs()->fields()) {
                ports += ports.empty() ? "" : ", ";
                ports += f.name;
            }
            return op::MountReport{false, "'" + def.identity() + "' takes " + ports +
                                              ", and a host may expose only sources over state it "
                                              "owns -- never parameterized power"};
        }
    }
    return into.mount(kHostProvider, std::move(defs));
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_HOST_SOURCES_HPP
