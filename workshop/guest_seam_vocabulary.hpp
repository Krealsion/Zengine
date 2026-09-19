// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_GUEST_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_GUEST_SEAM_VOCABULARY_HPP

// THE GUEST SEAM: what this Workshop says about the other hosts connected to it.
//
// A Workshop launched with `--guests <file>` listens for other Loom hosts. Each connection is
// admitted or refused by the file's rows (workshop/guests.hpp), and an admitted one becomes an
// ordinary participant on this bus -- a proxy the Loom stamps as the sender of everything it
// says, under a grant no wider than the row gave it. Which connections there are, what each
// claimed, what this host established for it, and whether it is still here are facts the
// host's guest door owns (`zengine.guests`) and SAYS, so a pane can show them without holding a
// second copy of the truth.
//
// APPEARING HERE GRANTS NOTHING. A row in this inventory is a fact about a socket and a
// decision; the authority a guest holds is its grant, checked at every send by the bus, and
// disconnection ends the session whatever the inventory last said.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The office the guest door holds. A pane asks it for the inventory; the Input weave accepts a
/// session closed on a guest's behalf only from an office speaking deliberately.
inline constexpr const char* kGuestsRole = "zengine.guests";

/// ONE CONNECTION AS THE DOOR SEES IT.
struct GuestConnection {
    std::int64_t connection = 0;  ///< the door's own number for this connection
    std::string state;            ///< awaiting-hello / awaiting-decision / admitted / refused / closed
    std::string claimed;          ///< what the peer called itself -- its word
    std::string established;      ///< what this host's policy established -- the host's word
    std::string peer;             ///< the socket's far address, where the platform says
    std::int64_t session = 0;     ///< the proxy's WeaveId on this bus, when admitted
    std::string refusal;          ///< why, when refused
    ZEN_SHAPE(GuestConnection, 1, ZEN_FIELD(connection), ZEN_FIELD(state), ZEN_FIELD(claimed),
              ZEN_FIELD(established), ZEN_FIELD(peer), ZEN_FIELD(session), ZEN_FIELD(refusal));
};

/// THE INVENTORY, SAID WHOLE whenever it changes, and answered to whoever asks. `listen` is
/// where this host accepts guests (empty when it does not); `refused` and `shed` are all-time
/// counts, so a refusal that came and went is still countable after its row is gone.
struct GuestConnections {
    std::string listen;
    std::vector<GuestConnection> rows;
    std::int64_t refused = 0;
    std::int64_t shed = 0;
    ZEN_SHAPE(GuestConnections, 1, ZEN_FIELD(listen), ZEN_FIELD(rows), ZEN_FIELD(refused),
              ZEN_FIELD(shed));
};

/// TELL ME THE INVENTORY AS IT IS NOW -- a presenter that has just arrived asks, and is answered.
struct GuestConnectionsRequested {
    ZEN_SHAPE(GuestConnectionsRequested, 1);
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_GUEST_SEAM_VOCABULARY_HPP
