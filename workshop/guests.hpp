// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_GUESTS_HPP
#define ZENGINE_WORKSHOP_GUESTS_HPP

// WHO MAY CONNECT TO THIS WORKSHOP, AND WHAT EACH MAY THEN SAY -- the guests file, and the
// admission policy it becomes.
//
// A maker who wants an agent's host to drive this Workshop writes one file:
//
//     {
//       "listen": "127.0.0.1:0",
//       "port_file": "workshop-guests.port",
//       "guests": [
//         { "name": "agent", "credential": "open-sesame", "may": ["input", "capture", "inspect"] },
//         { "name": "watcher", "credential": "later", "admit": "ask", "may": ["inspect"] }
//       ]
//     }
//
// and launches with `--guests <file>`. No file, no listener: connecting is impossible rather
// than merely refused. `listen` is a loopback address and a port (0 lets the OS choose, and
// `port_file` is where the chosen port is written for a script to read). Each row is one
// guest THIS HOST knows: the name is the host's word for it -- what the policy ESTABLISHES,
// whatever the peer claims -- the credential is what the peer must present, and `may` is the
// whole of what the session may then say, as explicit powers:
//
//     input      open an input session, inject moments, close it        -> zengine.input
//     capture    take a picture of the surface and fetch it by chunk    -> zengine.skin
//     inspect    ask any participant what it accepts (zen.DescribeAccepted), and the guest
//                door for the connection inventory                     -> zengine.guests
//     inventory  set/get/capture the pair; locate/read a live entry and save by revision
//                                                                    -> zengine.inventory
//     toolbox    explicit toolbox file save/restore                   -> zengine.inventory-pane
//     open       open a source through the managed opening              -> zengine.opening
//
// `open` is what lets a guest's own gesture reopen a saved file location dropped on the Editor:
// the Editor asks Workshop to approve `OpenSourceRequested` at the opening office for the
// gesture, and the approval asks the injecting actor's own grant, exactly as a carry asks for
// `inventory`. It opens through the managed opening only -- the Editor's unsaved-work floor and
// the opening's own refusals stand -- and it carries no input: the gesture still needs `input`.
// `demo` reaches the optional demo control service, setup application and three view-reset
// doors. It includes no input, capture or inventory authority.
// `inventory` is deliberately its own power, never folded into `inspect`: inspecting what a
// participant accepts is read-only discovery, while Set (and CaptureDescribe, which Sets)
// replaces this Workshop's one stored pair -- a different kind of authority, named separately
// (inventory/vocabulary.hpp, inventory/weave.hpp).
// Inventory UI operations also check the originating injected actor's inventory power;
// a click is not a grant. See docs/reference/inventory.md for that interaction.
//
// `admit` is "now" (the default) or "ask": an "ask" row is admitted by nobody until the host
// decides -- today at the console or by a test, tomorrow by the popup the founder expects --
// and its connection waits, able to act on nothing. That is the decision seam, enforced.
//
// `observe` is what a guest may OBSERVE, one entry per shape of one office's publications:
//
//     "observe": [ { "producer": "zengine.builder", "shape": "BuildStatus", "version": "4" } ]
//
// judged by the observation relay this Workshop mounts beside its door (`loom.observe`). A row
// without it observes nothing and may not even ask; with it, the session may ask the relay and is
// told yes only for what is listed. No power in `may` implies it -- input, capture and inspect are
// not observation -- and observing grants nothing to say.
//
// WHAT THIS IS NOT. Not an identity system: a credential is a shared secret between a maker
// and their own file, and the file is as private as the maker keeps it. Not network security:
// the listener is loopback and the bridge carries no transport security (Loom's bridge
// reference says so). Not a grant of anything the row does not name: a guest that can inject
// input cannot read the bus, load a weave, or speak to any office its powers omit.

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
/// `may` implies it, and it grants nothing to say.
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
