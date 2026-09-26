// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_VOCABULARY_HPP
#define ZENGINE_INTROSPECTION_VOCABULARY_HPP

// The Introspection tool's durable names: its office, its pane keys, the lines a maker reads,
// the Powers pane's action ids, and its one shape, `LoadedSelected`. Here rather than only in
// the .cpp, because a `PaneRef` is a promise to a saved setup and a listener is a stranger to
// the tool that says the fact.
// Pane law: agents/panes.md

#include <zen/weave/shape.hpp>

#include <string>

namespace zengine::introspection {

/// The office this tool holds, the durable half of its `PaneRef` and the only address anything
/// reaches it by: a role, so a reloaded provider (another WeaveId) is the same office.
inline constexpr const char* kIntrospectionRole = "zengine.introspection";

/// The pane key, in this office's namespace. It names the facts rather than the tool, so a
/// second Introspection pane has somewhere to go.
inline constexpr const char* kLoadedPane = "loaded";

/// The two lines a maker reads about the pane (its name in the Pane Manager, its summary in
/// Info), written short within Workshop's admission bounds. `Loaded` rather than `Weaves`: the
/// list is what the kernel loaded, and `Weaves` would promise the population it cannot see.
inline constexpr const char* kLoadedPaneName = "Loaded";
inline constexpr const char* kLoadedPaneSummary = "what the kernel has loaded, and each one's role";

/// The second and third panes; `loaded` is not widened to hold them. Three populations, three
/// owners, three currencies: `loaded` (the Kernel's map), `arrangement` (the resolved plan) and
/// `powers` (the host's catalog). A provider is in `arrangement` and not in `loaded`, correctly:
/// no Kernel loads it.
inline constexpr const char* kArrangementPane = "arrangement";
inline constexpr const char* kPowersPane = "powers";

/// The lines for the two, short enough that neither arrives cut.
inline constexpr const char* kArrangementPaneName = "Project";
inline constexpr const char* kArrangementPaneSummary =
    "what this project asked for, and what resolved";
inline constexpr const char* kPowersPaneName = "Powers";
inline constexpr const char* kPowersPaneSummary = "which operators resolve, and who supplies each";

/// The Powers pane's four declared actions (`PaneActions`): ids a maker's keymap names, durable
/// as the pane key is. The pane is told the id, never the key; the query's editing is not among
/// them, since a component's gestures are the component's.
inline constexpr const char* kPowersActionView = "powers.view";
inline constexpr const char* kPowersActionUp = "powers.up";
inline constexpr const char* kPowersActionDown = "powers.down";
inline constexpr const char* kPowersActionSample = "powers.sample";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable
/// the way the two keys above are -- it is a file name, and it is here because the
/// host's boot list and the suite's loader must agree on it.
inline constexpr const char* kIntrospectionStem = "zengine-introspection";

/// A maker selected one of the entries this pane was showing: a fact, not a command, and nothing
/// here listens. It carries data and no authority: a listener hearing a library and its role may
/// not thereby send it anything, load, unload or read it -- values flow, authority does not.
/// `library` is the name the kernel loaded it under (not a WeaveId, not proof it is alive);
/// `role` is the office bound at load, empty meaning none; `pane` names the pane. Whose fact it
/// is, is `mail.authored_role()`. No previous value, row, timestamp or grant.
struct LoadedSelected {
    std::string pane;    ///< the pane key this selection happened in -- `loaded`
    std::string library; ///< the loaded-library name the selected row named
    std::string role;    ///< the role observed at load; EMPTY means the kernel bound none
    ZEN_SHAPE(LoadedSelected, 1, ZEN_FIELD(pane), ZEN_FIELD(library), ZEN_FIELD(role));
};

} // namespace zengine::introspection

#endif // ZENGINE_INTROSPECTION_VOCABULARY_HPP
