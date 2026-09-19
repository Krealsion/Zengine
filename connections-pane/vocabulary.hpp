// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_CONNECTIONS_PANE_VOCABULARY_HPP
#define ZENGINE_CONNECTIONS_PANE_VOCABULARY_HPP

// The Connections pane's DURABLE NAMES -- the office it holds, the one pane it offers, and the
// stem a host boots.
//
// WHAT THIS PANE IS. A view of the other hosts connected to this Workshop: each row is one
// connection as the guest door (`zengine.guests`, workshop/guest_seam_vocabulary.hpp) reports
// it -- what the peer claimed, what this host established, whether it is admitted, waiting,
// refused or gone. It holds no copy of the truth: every row is the door's last publication,
// replaced whole, and a pane opened before the door has spoken says it is waiting.
//
// WHAT IT IS NOT. Not Info: Info inspects a real pane's properties through their owners, and
// this shows a socket inventory; putting one inside the other would make Info the place every
// kind of host fact accumulates. Not an admission surface: it can show a connection awaiting a
// decision and cannot make one -- that seam is the door's, and the popup the founder expects
// attaches there, not here. Not a menu: it declares no actions and takes no keys.

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
