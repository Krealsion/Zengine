// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The guests file, read through the gate; the powers as grants; the rows as a policy.

#include "guests.hpp"

#include "guest_seam_vocabulary.hpp"
#include "open_seam_vocabulary.hpp"
#include "pane_seam_vocabulary.hpp"
#include "pane_view.hpp"
#include "pane_carry.hpp"
#include "terminal_seam_vocabulary.hpp"
#include "setup_control.hpp"
#include "demo-control/vocabulary.hpp"

#include "input/vocabulary.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory-pane/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/describe.hpp>
#include <zen/weave/poke.hpp>

#include <fstream>
#include <sstream>
#include <utility>

namespace zengine::workshop::guests {

namespace {

std::shared_ptr<const loom::Schema> row_schema() {
    static const auto s = loom::SchemaBuilder("zen.WorkshopGuest", 1)
                              .field("name", loom::Kind::Text)
                              .field("credential", loom::Kind::Text)
                              .list("may", loom::type_of(loom::Kind::Text), /*required=*/false)
                              .field("admit", loom::Kind::Text, /*required=*/false)
                              .build();
    return s;
}

std::shared_ptr<const loom::Schema> file_schema() {
    static const auto s = loom::SchemaBuilder("zen.WorkshopGuests", 1)
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
        // a maker's credentials on the wire. Refused by name rather than warned about.
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
                if (power != kPowerInput && power != kPowerCapture && power != kPowerInspect &&
                    power != kPowerInventory && power != kPowerToolbox && power != kPowerDemo &&
                    power != kPowerOpen) {
                    *error = "guests file '" + path + "': guest '" + row.name +
                             "' may '" + power + "', which is not a power this host grants "
                             "(input, capture, inspect, inventory, toolbox, demo, open)";
                    return false;
                }
                row.may.push_back(power);
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
        } else if (power == kPowerDemo) {
            for (const char* shape : {"DemoServiceOpened", "DemoServiceClosed", "DemoWorkRequested",
                                      "DemoWorkFinished", "DemoResetRequested", "DemoStatusRequested", "DemoReadyRequested"})
                g.allow_to_role(shape, 1, zengine::demo::kRole);
            g.allow_to_role(SetupApplyRequested::zen_name, 1, "zengine.workshop");
            for (const char* role : {"zengine.info", "zengine.composer", "zengine.inventory-pane"})
                g.allow_to_role(PaneResetRequested::zen_name, 1, role);
        } else if (power == kPowerOpen) {
            // THE MANAGED OPENING, and nothing beside it: no document door, no save, no build.
            g.allow_to_role(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version, kOpeningRole);
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

loom::BridgeAdmission admission_of(const GuestsFile& file) {
    const std::vector<GuestRow> rows = file.rows;
    return [rows](const loom::ConnectionRequest& r) {
        if (r.credential.empty()) {
            return loom::ConnectionVerdict::refuse("this Workshop admits guests by credential, "
                                                   "and none was presented");
        }
        for (const GuestRow& row : rows) {
            if (row.credential != r.credential) {
                continue;
            }
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

} // namespace zengine::workshop::guests
