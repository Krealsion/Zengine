// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_CONNECTIONS_PANE_VOCABULARY_HPP
#define ZENGINE_CONNECTIONS_PANE_VOCABULARY_HPP

// The Connections pane's durable names: the office it holds, the one pane it offers, and the
// stem a host boots. Each row is one connection as the guest door (`zengine.guests`,
// workshop/guest_seam_vocabulary.hpp) reports it, replaced whole on each publication. Not Info
// (a socket inventory is not a pane's properties), not an admission surface (a decision is the
// door's seam), and not a menu: it declares no actions and takes no keys.
// Pane law: agents/panes.md

#include <zen/weave/shape.hpp>

#include <string>

namespace zengine::connections_pane {

inline constexpr const char* kConnectionsPaneRole = "zengine.connections";
inline constexpr const char* kConnectionsPane = "connections";
inline constexpr const char* kConnectionsPaneName = "Connections";
inline constexpr const char* kConnectionsPaneSummary = "the other hosts connected here";
inline constexpr const char* kConnectionsPaneStem = "zengine-connections-pane";

/// Nothing this pane keeps crosses a reload: the inventory is the door's and is asked for again.
struct ConnectionsPaneState {
    std::int64_t said = 0; ///< contents published, for a poke
    ZEN_SHAPE(ConnectionsPaneState, 1, ZEN_FIELD(said));
};

} // namespace zengine::connections_pane

#endif // ZENGINE_CONNECTIONS_PANE_VOCABULARY_HPP
