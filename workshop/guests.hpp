// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_GUESTS_HPP
#define ZENGINE_WORKSHOP_GUESTS_HPP

// Who may connect to this Workshop, and what each may then say: the guests file and the admission
// policy it becomes (docs/workshop/external-host.md). No file, no listener. Not an identity system
// (a credential is a secret between a maker and their own file), not network security (loopback,
// no transport security), and not a grant of anything a row does not name.
// A row's `observe` list is what the observation relay beside the door lets it follow; no power
// in `may` implies it (docs/workshop/external-host.md).

#include <zen/bridge/server.hpp>
#include <zen/observe/relay.hpp>
#include <zen/switchboard/grant.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace zengine::workshop::guests {

inline constexpr const char* kPowerInput = "input";
inline constexpr const char* kPowerCapture = "capture";
inline constexpr const char* kPowerInspect = "inspect";
inline constexpr const char* kPowerInventory = "inventory";
inline constexpr const char* kPowerToolbox = "toolbox";
inline constexpr const char* kPowerDemo = "demo";
inline constexpr const char* kPowerOpen = "open";

/// ONE THING A GUEST MAY OBSERVE: publications of one shape, at its exact version, by whoever
/// holds one office. Observation is its own decision (loom's zen/observe/relay.hpp): no power in
/// `may` implies it, and it grants nothing to say but one read -- observing the Builder's
/// `BuildStatus` lets the guest ask the Builder for the current one (`BuildStatusRequested`),
/// answered to it alone, which is what it may already see.
struct ObserveScope {
    std::string producer;
    std::string shape;
    std::int64_t version = 0;
};

struct GuestRow {
    std::string name;
    std::string credential;
    std::vector<std::string> may;
    std::vector<ObserveScope> observe; ///< what it may observe; empty: nothing
    bool ask = false; ///< admitted only when the host decides (`"admit": "ask"`)
};

struct GuestsFile {
    std::string path;
    std::string listen = "127.0.0.1:0";
    std::string port_file;
    std::vector<GuestRow> rows;
};

/// Read and gate the file. A missing file is an error here -- a maker who named one is owed
/// its absence -- and every refusal names the row and the word.
bool read_guests_file(const std::string& path, GuestsFile* out, std::string* error);

/// THE GRANT ONE ROW'S POWERS ARE WORTH, and nothing else. Spelled once, here, so the policy
/// and a suite that pins what a power reaches agree by construction.
loom::Grant grant_for(const GuestRow& row);

/// Split "host:port" into its halves; false when it is not one.
bool split_listen(const std::string& listen, std::string* host, std::uint16_t* port);

/// THE POLICY: the file's rows as a `loom::BridgeAdmission`. A presented credential that
/// matches a row admits AS THAT ROW (its name, its grant, no observation); a row marked
/// "ask" defers; anything else is refused in words. An empty credential never matches.
loom::BridgeAdmission admission_of(const GuestsFile& file);

/// WHAT EACH GUEST MAY OBSERVE, as the relay's policy: a subscriber is judged by the row its
/// session was admitted as (`established` answers which row that is, by session), and every shape
/// it asks for must be in that row's `observe` list for that producer. Anything else -- a local
/// participant, a session no row admitted, one shape too many -- is refused whole, in words.
loom::observe::ObservePolicy observation_of(
    const GuestsFile& file, std::function<std::string(loom::WeaveId)> established);

} // namespace zengine::workshop::guests

#endif // ZENGINE_WORKSHOP_GUESTS_HPP
