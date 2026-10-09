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

namespace v2 {
/// ONE CONNECTION, AND THE ROW THAT ADMITTED IT: v1's words, then the row's powers, the guests
/// file's `host` and `version`, and what those powers do not reach on this host (its losses). A
/// connection no row has admitted names none of them.
struct GuestConnection {
    std::int64_t connection = 0;
    std::string state;
    std::string claimed;
    std::string established;
    std::string peer;
    std::int64_t session = 0;
    std::string refusal;
    std::vector<std::string> powers; ///< the row's `may`
    std::string host;                ///< the file's `host`: `weaver` or `development`
    std::int64_t version = 0;        ///< the file's `version`; 0 when no row admitted it
    std::vector<std::string> losses; ///< what the row's powers do not reach here
    ZEN_SHAPE(GuestConnection, 2, ZEN_FIELD(connection), ZEN_FIELD(state), ZEN_FIELD(claimed),
              ZEN_FIELD(established), ZEN_FIELD(peer), ZEN_FIELD(session), ZEN_FIELD(refusal),
              ZEN_FIELD(powers), ZEN_FIELD(host), ZEN_FIELD(version), ZEN_FIELD(losses));
};

/// THE INVENTORY WITH EACH ROW'S POWERS AND LOSSES. Every row's to this host's own participants; to
/// a guest that asks, every connection as v1 says it and the powers and losses of its own alone.
struct GuestConnections {
    std::string listen;
    std::vector<GuestConnection> rows;
    std::int64_t refused = 0;
    std::int64_t shed = 0;
    ZEN_SHAPE(GuestConnections, 2, ZEN_FIELD(listen), ZEN_FIELD(rows), ZEN_FIELD(refused),
              ZEN_FIELD(shed));
};

/// ASK FOR THE INVENTORY AT VERSION 2.
struct GuestConnectionsRequested {
    ZEN_SHAPE(GuestConnectionsRequested, 2);
};
} // namespace v2

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_GUEST_SEAM_VOCABULARY_HPP
