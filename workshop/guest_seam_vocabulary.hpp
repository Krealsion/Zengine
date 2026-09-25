// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_GUEST_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_GUEST_SEAM_VOCABULARY_HPP

// The guest seam: what this Workshop says about the other hosts connected to it. Started with
// `--guests <file>`, it admits or refuses each connection by the file's rows (workshop/guests.hpp),
// and an admitted one is an ordinary participant under a grant no wider than its row. Appearing
// here grants nothing: a guest's authority is its grant, checked by the bus at every send.

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
