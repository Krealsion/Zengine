// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The guests file, read through the gate; the powers as grants; the rows as a policy.

#include "guests.hpp"

#include "attention_seam_vocabulary.hpp"
#include "desktop_seam_vocabulary.hpp"
#include "guest_seam_vocabulary.hpp"
#include "inspection_seam_vocabulary.hpp"
#include "open_seam_vocabulary.hpp"
#include "pane_seam_vocabulary.hpp"
#include "pane_view.hpp"
#include "pane_carry.hpp"
#include "terminal_seam_vocabulary.hpp"
#include "setup_control.hpp"
#include "demo-control/vocabulary.hpp"

#include "builder/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory-pane/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/describe.hpp>
#include <zen/weave/poke.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace zengine::workshop::guests {

namespace {

std::shared_ptr<const loom::Schema> scope_schema() {
    static const auto s = loom::SchemaBuilder("zen.WorkshopGuestObserves", 1)
                              .field("producer", loom::Kind::Text)
                              .field("shape", loom::Kind::Text)
                              .field("version", loom::Kind::Int)
                              .build();
    return s;
}

std::shared_ptr<const loom::Schema> row_schema() {
    static const auto s = loom::SchemaBuilder("zen.WorkshopGuest", 1)
                              .field("name", loom::Kind::Text)
                              .field("credential", loom::Kind::Text)
                              .list("may", loom::type_of(loom::Kind::Text), /*required=*/false)
                              .field("admit", loom::Kind::Text, /*required=*/false)
                              .list("observe", loom::type_message(scope_schema()),
                                    /*required=*/false)
                              .build();
    return s;
}

// Every version's words in one envelope; which of them a file may use is its own `version`'s,
// judged after the gate.
std::shared_ptr<const loom::Schema> file_schema() {
    static const auto s = loom::SchemaBuilder("zen.WorkshopGuests", 1)
                              .field("version", loom::Kind::Int, /*required=*/false)
                              .field("host", loom::Kind::Text, /*required=*/false)
                              .field("listen", loom::Kind::Text, /*required=*/false)
                              .field("port_file", loom::Kind::Text, /*required=*/false)
                              .list("guests", loom::type_message(row_schema()))
                              .build();
    return s;
}

std::string text_or(const loom::Value& v, const char* field, const char* fallback) {
    const loom::Cell* c = v.get(field);
    return (c == nullptr) ? std::string(fallback) : c->as_text();
}

bool row_may(const GuestRow& row, const char* power) {
    return std::find(row.may.begin(), row.may.end(), power) != row.may.end();
}

// WHAT CARRIES A PANE'S OR WORKSHOP'S WORDS, OR THE PICTURE, by its name and from any office: a
// row observes it only with `capture`, whatever its observe list names.
bool carries_the_desk(const std::string& shape) {
    for (const char* name : {surface::SurfaceCanvas::zen_name, surface::SurfaceText::zen_name,
                             TranscriptShown::zen_name, PaneSubjectShown::zen_name,
                             PaneInventory::zen_name, KeymapShown::zen_name,
                             StandingConditions::zen_name}) {
        if (shape == name) return true;
    }
    return false;
}

// ...AND WHAT FOLLOWS THEM: a notice that the desk moved names every pane's picture as a reading
// does, so it is observed under the same power.
bool follows_the_desk(const std::string& shape) { return shape == DeskStamps::zen_name; }

} // namespace

bool split_listen(const std::string& listen, std::string* host, std::uint16_t* port) {
    const std::size_t colon = listen.rfind(':');
    if (colon == std::string::npos || colon + 1 >= listen.size()) {
        return false;
    }
    unsigned long p = 0;
    try {
        p = std::stoul(listen.substr(colon + 1));
    } catch (...) {
        return false;
    }
    if (p > 65535) {
        return false;
    }
    *host = colon == 0 ? std::string("127.0.0.1") : listen.substr(0, colon);
    *port = static_cast<std::uint16_t>(p);
    return true;
}

bool read_guests_file(const std::string& path, GuestsFile* out, std::string* error) {
    *out = GuestsFile{};
    out->path = path;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        *error = "guests file '" + path + "' cannot be read";
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    // The bare form a person writes, wrapped in the envelope the host knows -- the same
    // courtesy the supplied host extends to its own two files.
    const std::string wrapped = "{\"zen\":1,\"schema\":\"" + file_schema()->name() +
                                "\",\"version\":" + std::to_string(file_schema()->version()) +
                                ",\"fields\":" + ss.str() + "}";
    loom::Unverified u = loom::compat::parse(wrapped);
    loom::Admission a = loom::admit(u, file_schema());
    if (!a.ok()) {
        *error = "guests file '" + path + "' refused: " + a.first_error().message();
        return false;
    }
    const loom::Value& v = a.value();
    if (const loom::Cell* version = v.get("version")) {
        out->version = version->as_int();
        if (out->version < 1 || out->version > kGuestsFileVersion) {
            *error = "guests file '" + path + "' names version " + std::to_string(out->version) +
                     ", which this Workshop does not read (it reads versions 1 to " +
                     std::to_string(kGuestsFileVersion) + ")";
            return false;
        }
    }
    // WHAT A FILE OF VERSION 1 IS TOLD when it says a later version's word: whether it named its
    // version or was read as 1 for naming none.
    const std::string read_as = v.get("version") != nullptr
        ? "it names version 1 (name \"version\": \"2\")"
        : "it names no version, so it is read as version 1 (add \"version\": \"2\")";
    if (const loom::Cell* host = v.get("host")) {
        if (out->version < 2) {
            *error = "guests file '" + path + "' names a host, a word of the file's version 2; " + read_as;
            return false;
        }
        out->host = host->as_text();
        if (out->host != kHostWeaver && out->host != kHostDevelopment) {
            *error = "guests file '" + path + "': host '" + out->host + "' must be '" +
                     kHostWeaver + "' or '" + kHostDevelopment + "'";
            return false;
        }
    }
    out->listen = text_or(v, "listen", "127.0.0.1:0");
    out->port_file = text_or(v, "port_file", "");
    std::string host;
    std::uint16_t port = 0;
    if (!split_listen(out->listen, &host, &port)) {
        *error = "guests file '" + path + "': listen '" + out->listen + "' is not host:port";
        return false;
    }
    if (host != "127.0.0.1" && host != "localhost") {
        // The bridge carries no transport security; a listener anywhere but loopback would be
        // a weaver's credentials on the wire. Refused by name rather than warned about.
        *error = "guests file '" + path + "': listen must be loopback (127.0.0.1:<port>)";
        return false;
    }
    for (const loom::Cell& c : v.get("guests")->as_list()) {
        const loom::Value& r = *c.as_message();
        GuestRow row;
        row.name = r.get("name")->as_text();
        row.credential = r.get("credential")->as_text();
        if (row.name.empty()) {
            *error = "guests file '" + path + "': every guest needs a name";
            return false;
        }
        if (row.credential.empty()) {
            *error = "guests file '" + path + "': guest '" + row.name +
                     "' has no credential, and an empty credential admits nobody";
            return false;
        }
        for (const GuestRow& other : out->rows) {
            if (other.credential == row.credential) {
                *error = "guests file '" + path + "': guests '" + other.name + "' and '" +
                         row.name + "' share a credential, so neither could be told apart";
                return false;
            }
        }
        if (const loom::Cell* may = r.get("may")) {
            for (const loom::Cell& p : may->as_list()) {
                const std::string power = p.as_text();
                if (power == kPowerBuild && out->version < 2) {
                    *error = "guests file '" + path + "': guest '" + row.name + "' may '" + power +
                             "', a power of the file's version 2; " + read_as;
                    return false;
                }
                if (power != kPowerInput && power != kPowerCapture && power != kPowerInspect &&
                    power != kPowerInventory && power != kPowerToolbox && power != kPowerDemo &&
                    power != kPowerOpen && power != kPowerBuild) {
                    *error = "guests file '" + path + "': guest '" + row.name +
                             "' may '" + power + "', which is not a power this host grants "
                             "(input, capture, inspect, inventory, toolbox, demo, open, build)";
                    return false;
                }
                row.may.push_back(power);
            }
        }
        if (const loom::Cell* observe = r.get("observe")) {
            for (const loom::Cell& entry : observe->as_list()) {
                const loom::Value& o = *entry.as_message();
                ObserveScope scope;
                scope.producer = o.get("producer")->as_text();
                scope.shape = o.get("shape")->as_text();
                scope.version = o.get("version")->as_int();
                if (scope.producer.empty() || scope.shape.empty() || scope.version <= 0) {
                    *error = "guests file '" + path + "': guest '" + row.name +
                             "' observes an entry without a producer office, a shape and a "
                             "version above 0";
                    return false;
                }
                row.observe.push_back(std::move(scope));
            }
        }
        const std::string admit = text_or(r, "admit", "now");
        if (admit == "ask") {
            row.ask = true;
        } else if (admit != "now") {
            *error = "guests file '" + path + "': guest '" + row.name + "' has admit '" + admit +
                     "'; it must be 'now' or 'ask'";
            return false;
        }
        out->rows.push_back(std::move(row));
    }
    return true;
}

loom::Grant grant_for(const GuestRow& row) {
    loom::Grant g;
    // EVERY ROW MAY ASK FOR ITSELF, whatever its powers: the door answers with the row it admitted
    // the asking session under, and nothing of another's.
    g.allow_to_role(GuestRowDescribedRequested::zen_name, GuestRowDescribedRequested::zen_version,
                    kGuestsRole);
    if (!row.observe.empty()) {
        // THE RIGHT TO ASK THE RELAY, and only that: whether an ask is answered yes is the
        // relay's policy (`observation_of`), which reads this row's list again at every ask.
        for (const char* shape : {loom::observe::Subscribe::zen_name,
                                  loom::observe::Release::zen_name,
                                  loom::observe::Acknowledge::zen_name,
                                  loom::observe::StatusRequested::zen_name}) {
            g.allow_to_role(shape, 1, loom::observe::kObserveRole);
        }
    }
    for (const ObserveScope& s : row.observe) {
        // A ROW THAT MAY SEE THE BUILDER'S WHOLE PICTURE MAY ASK FOR THE CURRENT ONE: the baseline a
        // returning observer joins (builder/vocabulary.hpp, `BuildStatusRequested`), answered to it
        // alone -- a read of what it may already see, and no power to act on the Builder. It is
        // answered with this version's picture, and the relay admits no other, so nor does this.
        if (s.producer == builder::kBuilderRole && s.shape == builder::BuildStatus::zen_name &&
            s.version == builder::BuildStatus::zen_version) {
            g.allow_to_role(builder::BuildStatusRequested::zen_name,
                            builder::BuildStatusRequested::zen_version, builder::kBuilderRole);
        }
    }
    for (const std::string& power : row.may) {
        if (power == kPowerInput) {
            g.allow_to_role(input::InputSessionRequested::zen_name,
                            input::InputSessionRequested::zen_version, input::kInputRole);
            g.allow_to_role(input::PointerMotionRequested::zen_name, input::PointerMotionRequested::zen_version,
                            input::kInputRole);
            g.allow_to_role(input::InjectInput::zen_name, input::InjectInput::zen_version,
                            input::kInputRole);
            g.allow_to_role(input::InputSessionClosed::zen_name,
                            input::InputSessionClosed::zen_version, input::kInputRole);
        } else if (power == kPowerCapture) {
            g.allow_to_role(PaneViewRequested::zen_name, PaneViewRequested::zen_version, "zengine.workshop");
            g.allow_to_role(PanePointRequested::zen_name, PanePointRequested::zen_version, "zengine.workshop");
            g.allow_to_role(DeskViewRequested::zen_name, DeskViewRequested::zen_version, "zengine.workshop");
            g.allow_to_role(v2::PaneViewRequested::zen_name, v2::PaneViewRequested::zen_version, "zengine.workshop");
            g.allow_to_role(v2::PanePointRequested::zen_name, v2::PanePointRequested::zen_version, "zengine.workshop");
            g.allow_to_role(v3::PaneViewRequested::zen_name, v3::PaneViewRequested::zen_version, "zengine.workshop");
            g.allow_to_role(v3::PanePointRequested::zen_name, v3::PanePointRequested::zen_version, "zengine.workshop");
            g.allow_to_role(v2::DeskViewRequested::zen_name, v2::DeskViewRequested::zen_version, "zengine.workshop");
            // THE DESK SAID WHOLE: the desk and its panes in one turn, a pane's page, the inventory
            // of panes and the keymap, each asked of Workshop's office.
            g.allow_to_role(DeskReadRequested::zen_name, DeskReadRequested::zen_version,
                            "zengine.workshop");
            g.allow_to_role(v4::PaneViewRequested::zen_name, v4::PaneViewRequested::zen_version,
                            "zengine.workshop");
            g.allow_to_role(v2::DeskReadRequested::zen_name, v2::DeskReadRequested::zen_version,
                            "zengine.workshop");
            g.allow_to_role(v5::PaneViewRequested::zen_name, v5::PaneViewRequested::zen_version,
                            "zengine.workshop");
            g.allow_to_role(PaneInventoryRequested::zen_name, PaneInventoryRequested::zen_version,
                            "zengine.workshop");
            g.allow_to_role(KeymapRequested::zen_name, KeymapRequested::zen_version,
                            "zengine.workshop");
            g.allow_to_role(surface::SurfaceCaptureRequested::zen_name,
                            surface::SurfaceCaptureRequested::zen_version, surface::kSkinRole);
            g.allow_to_role(surface::SurfaceCaptureChunkRequested::zen_name,
                            surface::SurfaceCaptureChunkRequested::zen_version,
                            surface::kSkinRole);
        } else if (power == kPowerInspect) {
            g.allow_to_any(loom::DescribeAccepted::zen_name, loom::DescribeAccepted::zen_version);
            // A weave's exposed structure, the self-description floor Info's Sample source asks for.
            g.allow_to_any(loom::PokeDescribe::zen_name, loom::PokeDescribe::zen_version);
            g.allow_to_role(GuestConnectionsRequested::zen_name,
                            GuestConnectionsRequested::zen_version, kGuestsRole);
            g.allow_to_role(v2::GuestConnectionsRequested::zen_name,
                            v2::GuestConnectionsRequested::zen_version, kGuestsRole);
        } else if (power == kPowerDemo) {
            for (const char* shape : {"DemoServiceOpened", "DemoServiceClosed", "DemoWorkRequested",
                                      "DemoWorkFinished", "DemoResetRequested", "DemoStatusRequested", "DemoReadyRequested"})
                g.allow_to_role(shape, 1, zengine::demo::kRole);
            g.allow_to_role(SetupApplyRequested::zen_name, 1, "zengine.workshop");
            g.allow_to_role(WorkshopQuitRequested::zen_name, 1, "zengine.workshop");
            for (const char* role : {"zengine.info", "zengine.composer", "zengine.inventory-pane"})
                g.allow_to_role(PaneResetRequested::zen_name, 1, role);
        } else if (power == kPowerOpen) {
            // THE MANAGED OPENING, and nothing beside it: no document door, no save, no build.
            g.allow_to_role(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version, kOpeningRole);
        } else if (power == kPowerBuild) {
            // NOTHING TO SAY: `build` is an action class the Builder asks for a gesture's actor
            // (`actor_scope.hpp`), never a shape a guest sends.
        } else if (power == kPowerToolbox) {
            g.allow_to_role(zengine::inventory_pane::InventoryToolboxSave::zen_name, 1, zengine::inventory_pane::kRole);
            g.allow_to_role(zengine::inventory_pane::InventoryToolboxRestore::zen_name, 1, zengine::inventory_pane::kRole);
        } else if (power == kPowerInventory) {
            g.allow_to_role(TerminalValueRequested::zen_name, 1, "zengine.workshop");
            g.allow_to_role(PaneValueCarryRequested::zen_name, 1, "zengine.workshop");
            g.allow_to_role(zengine::inventory_pane::InventoryViewEdit::zen_name, 1, zengine::inventory_pane::kRole);
            g.allow_to_role(zengine::inventory_pane::InventoryViewsRequested::zen_name, 1, zengine::inventory_pane::kRole);
            g.allow_to_role(zengine::inventory::InventoryAdd::zen_name, 1, zengine::inventory::kInventoryRole);
            g.allow_to_role(zengine::inventory::InventoryList::zen_name, 1, zengine::inventory::kInventoryRole);
            g.allow_to_role(zengine::inventory::InventoryRename::zen_name, 1, zengine::inventory::kInventoryRole);
            g.allow_to_role(zengine::inventory::InventoryRemove::zen_name, 1, zengine::inventory::kInventoryRole);
            g.allow_to_role(zengine::inventory::InventoryCaptureAdd::zen_name, 1, zengine::inventory::kInventoryRole);
            // Organizing is inventory authority: named folders and entry membership, never a
            // snapshot or restore (those stay behind the separate toolbox power at the pane).
            namespace inventory = zengine::inventory;
            g.allow_to_role(inventory::v2::InventoryAdd::zen_name, 2, inventory::kInventoryRole);
            g.allow_to_role(inventory::v2::InventoryList::zen_name, 2, inventory::kInventoryRole);
            for (const char* shape : {inventory::InventoryFile::zen_name, inventory::InventoryFolderCreate::zen_name,
                                      inventory::InventoryFolderRename::zen_name, inventory::InventoryFolderMove::zen_name,
                                      inventory::InventoryFolderRemove::zen_name})
                g.allow_to_role(shape, 1, inventory::kInventoryRole);
            g.allow_to_role(zengine::inventory::InventoryLocate::zen_name,
                            zengine::inventory::InventoryLocate::zen_version,
                            zengine::inventory::kInventoryRole);
            g.allow_to_role(zengine::inventory::InventoryRead::zen_name,
                            zengine::inventory::InventoryRead::zen_version,
                            zengine::inventory::kInventoryRole);
            g.allow_to_role(zengine::inventory::InventoryWrite::zen_name,
                            zengine::inventory::InventoryWrite::zen_version,
                            zengine::inventory::kInventoryRole);
            g.allow_to_role(zengine::inventory::InventorySet::zen_name,
                            zengine::inventory::InventorySet::zen_version,
                            zengine::inventory::kInventoryRole);
            g.allow_to_role(zengine::inventory::InventoryGet::zen_name,
                            zengine::inventory::InventoryGet::zen_version,
                            zengine::inventory::kInventoryRole);
            g.allow_to_role(zengine::inventory::InventoryCaptureDescribe::zen_name,
                            zengine::inventory::InventoryCaptureDescribe::zen_version,
                            zengine::inventory::kInventoryRole);
        }
    }
    return g;
}

scope::GuestRowFacts facts_of(const GuestRow& row, const GuestsFile& file) {
    scope::GuestRowFacts f;
    f.admitted = true;
    f.name = row.name;
    f.powers = row.may;
    f.version = file.version;
    return f;
}

scope::HostFact host_fact_of(const GuestsFile& file) {
    scope::HostFact h;
    h.development = file.development();
    std::error_code ec;
    const std::filesystem::path absolute = std::filesystem::absolute(std::filesystem::path(file.path), ec);
    h.guests_file = ec ? file.path : absolute.lexically_normal().string();
    return h;
}

std::vector<std::string> losses_of(const GuestRow& row, const GuestsFile& file) {
    const scope::GuestRowFacts f = facts_of(row, file);
    std::vector<std::string> out;
    if (f.may(kPowerInput) && !f.may(kPowerBuild)) {
        out.push_back(file.version < 2 ? "the Builder's builds and loads: `build` is a version-2 power"
                                       : "the Builder's builds and loads: the row has no `build`");
    }
    if (file.development()) return out;
    if (f.may(kPowerInput)) {
        out.push_back("file writes: this is a weaver's host");
        out.push_back("the editor, the Terminal and the Hotkeys pane: they answer only the weaver's hand here");
        out.push_back("typed text: a guest's text rests in no pane here");
        // ...and a carry, which only `inventory` begins.
        if (f.may(kPowerInventory)) {
            out.push_back("carrying an item: what a guest would carry rests in no pane here");
        }
    }
    if (f.may(kPowerToolbox)) out.push_back("saving a toolbox file: a file write");
    if (f.may(kPowerDemo)) out.push_back("quitting Workshop: quitting writes the last session");
    if (f.may(kPowerInput)) out.push_back("opening the guests file");
    if (f.may(kPowerOpen)) {
        out.push_back("the `open` power: the editor it reopens a location in answers only the "
                      "weaver's hand here");
    }
    return out;
}

// WL-GUEST-11 -- agents/workshop/guests.md
std::vector<std::string> gains_of(const GuestRow& row, const GuestsFile& file) {
    const scope::GuestRowFacts f = facts_of(row, file);
    std::vector<std::string> out;
    if (f.may(kPowerCapture)) {
        out.push_back("the desk read whole: `capture` also reads the desk's words, the pane "
                      "inventory and the keymap");
    }
    return out;
}

std::vector<Condition> conditions_of(const GuestsFile& file) {
    std::vector<Condition> out;
    if (file.development()) out.push_back(development_host());
    for (std::size_t i = 0; i < file.rows.size(); ++i) {
        const std::vector<std::string> losses = losses_of(file.rows[i], file);
        if (!losses.empty()) out.push_back(guest_row_losses(i, file.rows[i].name, file.version, losses));
        const std::vector<std::string> gains = gains_of(file.rows[i], file);
        if (!gains.empty()) out.push_back(guest_row_gains(i, file.rows[i].name, file.version, gains));
    }
    return out;
}

loom::BridgeAdmission admission_of(const GuestsFile& file, std::shared_ptr<AdmittedRows> admitted) {
    const std::vector<GuestRow> rows = file.rows;
    return [rows, admitted](const loom::ConnectionRequest& r) {
        if (r.credential.empty()) {
            return loom::ConnectionVerdict::refuse("this Workshop admits guests by credential, "
                                                   "and none was presented");
        }
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const GuestRow& row = rows[i];
            if (row.credential != r.credential) {
                continue;
            }
            // THE ROW IS RECORDED AGAINST THE CONNECTION before any verdict, so whatever later
            // asks which row a session holds reads this row, whatever its name.
            if (admitted) admitted->record(r.connection, i);
            if (row.ask) {
                return loom::ConnectionVerdict::defer();
            }
            loom::ConnectionAdmitted a;
            a.grant = grant_for(row);
            a.accept = loom::AcceptMode::AnyRegistered;
            a.established_name = row.name;
            a.observe = false;
            return loom::ConnectionVerdict::admit(std::move(a));
        }
        return loom::ConnectionVerdict::refuse("no guest of this Workshop presents that credential");
    };
}

// WL-GUEST-11 -- agents/workshop/guests.md
loom::observe::ObservePolicy observation_of(
    const GuestsFile& file, std::function<std::optional<std::size_t>(loom::WeaveId)> row_of) {
    const std::vector<GuestRow> rows = file.rows;
    return [rows, row_of](const loom::observe::ObserveRequest& r) {
        const std::optional<std::size_t> index = row_of ? row_of(r.subscriber) : std::nullopt;
        const GuestRow* row = (index && *index < rows.size()) ? &rows[*index] : nullptr;
        if (row == nullptr) {
            return loom::observe::ObserveVerdict::refuse(
                "this Workshop lets only a guest its guests file names observe, and the asker "
                "is not one");
        }
        for (const loom::observe::ShapeRef& asked : r.shapes) {
            // THE INVENTORY THAT SAYS EVERY ROW'S POWERS is this host's own: a guest asks for its own
            // row's, and observes the inventory only as version 1 says it.
            if (r.producer == kGuestsRole && asked.name == v2::GuestConnections::zen_name &&
                asked.version >= v2::GuestConnections::zen_version) {
                return loom::observe::ObserveVerdict::refuse(
                    "guest '" + row->name + "' may not observe " + asked.name + " v" +
                    std::to_string(asked.version) + ": it says every row's powers; ask for your own "
                    "with GuestConnectionsRequested v2");
            }
            // THE DESK'S WORDS AND ITS PICTURE are observed only under `capture`, from whichever
            // office publishes them; the weaver's copied text needs a power of its own.
            if (carries_the_desk(asked.name) && !row_may(*row, kPowerCapture)) {
                return loom::observe::ObserveVerdict::refuse(
                    "guest '" + row->name + "' may not observe " + asked.name + " v" +
                    std::to_string(asked.version) + " from " + r.producer +
                    ": it carries the desk's words or its picture, which a row observes only "
                    "with `capture`");
            }
            if (follows_the_desk(asked.name) && !row_may(*row, kPowerCapture)) {
                return loom::observe::ObserveVerdict::refuse(
                    "guest '" + row->name + "' may not observe " + asked.name + " v" +
                    std::to_string(asked.version) + " from " + r.producer +
                    ": it says when the desk's words or its picture moved, which a row follows "
                    "only with `capture`");
            }
            if (asked.name == surface::ClipboardCopy::zen_name) {
                return loom::observe::ObserveVerdict::refuse(
                    "guest '" + row->name + "' may not observe " + asked.name + " v" +
                    std::to_string(asked.version) + " from " + r.producer +
                    ": it carries the weaver's copied text, which needs `clipboard`, a power "
                    "this Workshop's guests file does not grant yet");
            }
            bool listed = false;
            for (const ObserveScope& s : row->observe) {
                listed = listed || (s.producer == r.producer && s.shape == asked.name &&
                                    s.version == asked.version);
            }
            if (!listed) {
                return loom::observe::ObserveVerdict::refuse(
                    "guest '" + row->name + "' may not observe " + asked.name + " v" +
                    std::to_string(asked.version) + " from " + r.producer +
                    ": its row's observe list does not name it");
            }
        }
        return loom::observe::ObserveVerdict::allow();
    };
}

} // namespace zengine::workshop::guests
