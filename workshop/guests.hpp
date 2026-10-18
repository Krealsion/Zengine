// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_GUESTS_HPP
#define ZENGINE_WORKSHOP_GUESTS_HPP

// Who may connect to this Workshop, and what each may then say: the guests file and the admission
// policy it becomes (external-host/docs/external-host.md). No file, no listener. Not an identity
// system, not network security (loopback only), and no grant of anything a row does not name. Its
// versions' words and what each row reaches are the law's: agents/workshop/guests.md (WL-GUEST)
// and agents/workshop/guide.md (WL-GUIDE).

#include "actor_scope.hpp"
#include "attention.hpp"

#include <zen/bridge/server.hpp>
#include <zen/observe/relay.hpp>
#include <zen/switchboard/grant.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
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
/// The Builder's builds, realizations and loads, judged as an action class (`scope::kBuild`) and
/// never granted as a shape: no guest is granted `BuildRequested`. A version-2 word.
inline constexpr const char* kPowerBuild = "build";

/// A mark the weaver sees on one control or part (`GuideRequested`), within the row's `guide` doors,
/// and nothing else: a pointer, never a press. A version-3 word.
inline constexpr const char* kPowerGuide = "guide";
/// Hearing the weaver's input as it happens: an `observe` entry naming `AttributedInput`,
/// `KeyPressed` or `TextEntered`, from any office, needs it. It grants nothing to say. A version-3
/// word.
inline constexpr const char* kPowerKeystrokes = "keystrokes";
/// Reading and writing one pane's settings on the live layout by message (`PaneSettingsRequested`,
/// `PaneSettingCommitRequested`), Workshop judging each ask by the row as well as Loom by its grant.
/// A version-3 word.
inline constexpr const char* kPowerSettings = scope::kPowerSettings;

/// The newest version of the guests file this Workshop reads; a file names its own in `version`.
inline constexpr std::int64_t kGuestsFileVersion = 3;
/// The version whose rows read and point by door: from it, `capture` reads only the panes the row's
/// `capture` field names, and `guide` marks only within its `guide` field.
inline constexpr std::int64_t kDoorsVersion = 3;

/// THE ROW WORD FOR EVERY PANE, in a `capture` or `guide` field: written, never assumed.
inline constexpr const char* kEveryPane = "*";

/// A ROLE, a version-3 word: a bundle of powers written once here and named by a row, which may add
/// powers of its own (`may`) and sets its scopes (`capture`, `guide`). Expanded at launch; the row's
/// reach is said in full beside it. A role's bundle never changes under a file of its version.
struct RoleBundle {
    const char* name;
    std::vector<const char*> may;
};

/// The Tutor: reads the panes its lesson is about and points; never presses, types, opens, writes
/// or hears the weaver's keys.
inline const std::vector<RoleBundle>& role_bundles() {
    static const std::vector<RoleBundle> bundles = {
        {"tutor", {"capture", "guide"}},
    };
    return bundles;
}

/// The file's `host`, a version-2 word: whose machine this Workshop runs on. A weaver's host, the
/// default, keeps its file writes, typed text, editor, Terminal and Hotkeys pane for the weaver's
/// hand; a development host is an agent's own, where a guest with `input` reaches them.
inline constexpr const char* kHostWeaver = "weaver";
inline constexpr const char* kHostDevelopment = "development";

/// ONE THING A GUEST MAY OBSERVE: publications of one shape, at its exact version, by whoever
/// holds one office. Observation is its own decision (loom's zen/observe/relay.hpp): no power in
/// `may` implies it, and it grants nothing to say but one read -- observing the Builder's
/// `BuildStatus` at the version it publishes lets the guest ask for the current one
/// (`BuildStatusRequested`), answered to it alone, which is what it may already see.
struct ObserveScope {
    std::string producer;
    std::string shape;
    std::int64_t version = 0;
};

struct GuestRow {
    std::string name;
    std::string credential;
    std::vector<std::string> may;      ///< the row's powers, its role's expanded among them
    std::vector<ObserveScope> observe; ///< what it may observe; empty: nothing
    bool ask = false; ///< admitted only when the host decides (`"admit": "ask"`)
    std::string role;                  ///< the role it names, a version-3 word; empty: none
    /// WHETHER IT READS AND POINTS BY DOOR, a row of version 3 or later: then `capture` reads only
    /// the panes `capture` names and `guide` marks only within `guide`; a row of an earlier version
    /// reads the desk whole and points nowhere.
    bool by_door = false;
    std::vector<std::string> capture;  ///< `<office>/<pane>`, an office's every pane, or `*` alone
    std::vector<std::string> guide;    ///< the same, each within `capture`

    /// Reads by door and not every pane: judged pane by pane, refused the reads that carry every pane.
    bool reads_by_pane() const { return by_door && !(capture.size() == 1 && capture[0] == kEveryPane); }
    bool guides_by_pane() const { return by_door && !(guide.size() == 1 && guide[0] == kEveryPane); }
};

struct GuestsFile {
    std::string path;
    std::int64_t version = 1;       ///< the file's own `version`; absent is 1
    std::string host = kHostWeaver; ///< the file's `host`; version 1 names none, and is a weaver's
    std::string listen = "127.0.0.1:0";
    std::string port_file;
    std::vector<GuestRow> rows;

    bool development() const { return host == kHostDevelopment; }
};

/// Read and gate the file. A missing file is an error here -- a weaver who named one is owed
/// its absence -- and every refusal names the row and the word.
bool read_guests_file(const std::string& path, GuestsFile* out, std::string* error);

/// THE GRANT ONE ROW'S POWERS ARE WORTH, and nothing else. Spelled once, here, so the policy
/// and a suite that pins what a power reaches agree by construction.
loom::Grant grant_for(const GuestRow& row);

/// WHETHER THIS ROW ALREADY SHOWS OTHER ROWS' NAMES: the desk (`capture`) or the door's
/// inventory (`inspect`). Only such a row is told which plan rows an answer names.
bool reads_row_names(const GuestRow& row);

/// One row as the action classes read it (`actor_scope.hpp`): its name, powers and the file's
/// version, for a session the door admitted under it.
scope::GuestRowFacts facts_of(const GuestRow& row, const GuestsFile& file);

/// The host fact the file states: development or a weaver's, and which file names the guests.
scope::HostFact host_fact_of(const GuestsFile& file);

/// WHAT THIS ROW'S POWERS DO NOT REACH ON THIS HOST, said beside the row at launch, on Attention
/// and in Connections: the Builder's builds and loads without `build`, and on a weaver's host every
/// file write, typed text, the editor, the Terminal and the Hotkeys pane, the guests file's
/// opening, a quit by message and the `open` power. Empty when the row reaches all its powers name.
std::vector<std::string> losses_of(const GuestRow& row, const GuestsFile& file);

/// WHAT THIS ROW'S POWERS REACH PAST WHAT THEIR NAMES SAY: a widening, in words to stand beside
/// the row's losses. `capture` reads the desk whole, its words, the pane inventory and the keymap
/// included. Empty when no power of the row is widened.
std::vector<std::string> gains_of(const GuestRow& row, const GuestsFile& file);

/// WHAT A ROW READS AND POINTS AT, AND ITS ROLE, in words to stand beside it: the role and the
/// powers it expanded to, the `capture` doors it reads, the `guide` doors it points within. Empty
/// for a row of an earlier version that names no role.
std::vector<std::string> scopes_of(const GuestRow& row);

/// WHAT THE FILE STANDS AS ON ATTENTION for the whole run: a development host, and each row whose
/// powers do not reach everything here, keyed by its place in the file.
std::vector<Condition> conditions_of(const GuestsFile& file);

/// Split "host:port" into its halves; false when it is not one.
bool split_listen(const std::string& listen, std::string* host, std::uint16_t* port);

/// WHICH ROW ADMITTED EACH CONNECTION, written by the policy as it decides: a session's row is read
/// from here, never found by name, so two rows of one name stay two rows. A connection the server
/// let go is forgotten (`forget`), so the record holds no more than the connections alive. Shared
/// by the policy and the door, both of which the server calls inside its service.
class AdmittedRows {
public:
    void record(std::uint64_t connection, std::size_t row) { rows_[connection] = row; }
    std::optional<std::size_t> row_of(std::uint64_t connection) const {
        const auto it = rows_.find(connection);
        if (it == rows_.end()) return std::nullopt;
        return it->second;
    }
    void forget(std::uint64_t connection) { rows_.erase(connection); }
    std::size_t size() const { return rows_.size(); }

private:
    std::map<std::uint64_t, std::size_t> rows_;
};

/// THE POLICY: the file's rows as a `loom::BridgeAdmission`. A presented credential that
/// matches a row admits AS THAT ROW (its name, its grant, no observation) and records the row
/// against the connection in `admitted`; a row marked "ask" defers, recorded the same way; anything
/// else is refused in words. An empty credential never matches.
loom::BridgeAdmission admission_of(const GuestsFile& file, std::shared_ptr<AdmittedRows> admitted);

/// WHAT EACH GUEST MAY OBSERVE, as the relay's policy: a subscriber is judged by the row its
/// session was admitted under (`row_of` answers which row that is, by session, from the record the
/// policy kept), and every shape it asks for must be in that row's `observe` list for that
/// producer; one carrying the desk's words or picture needs `capture` too, and `ClipboardCopy` is
/// observed by no row. Anything else -- a local participant, a session no row admitted, one shape
/// too many -- is refused whole, in words.
loom::observe::ObservePolicy observation_of(
    const GuestsFile& file, std::function<std::optional<std::size_t>(loom::WeaveId)> row_of);

} // namespace zengine::workshop::guests

#endif // ZENGINE_WORKSHOP_GUESTS_HPP
