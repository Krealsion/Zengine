// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "activation/activation.hpp"
#include "inventory/codec.hpp"
#include "inventory/pane_client.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_text.hpp"
#include "input/vocabulary.hpp"
#include <zen/kernel/export.hpp>

namespace {
namespace ws = zengine::workshop;
namespace inv = zengine::inventory;
namespace input = zengine::input;
namespace surf = zengine::surface;
constexpr const char* office = "zengine.inventory-pane";
constexpr const char* pane = "inventory";
struct InventoryPaneState { ZEN_SHAPE(InventoryPaneState, 1); };
class InventoryPane : public loom::WeaveBase<InventoryPane, InventoryPaneState,
    loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom, ws::PaneButton,
                 ws::PaneActionRequested, ws::PaneMenuAnswered, ws::PaneOperationAnswered,
                 ws::PaneCarryAnswered, inv::InventoryEntry, loom::Refused, loom::DispatchRefused>,
    loom::Emit<ws::PaneOffered, ws::PaneActions, ws::PaneContent, ws::PaneMenuRequested,
               ws::PaneOperationRequested, ws::PaneCarryRequested, inv::InventoryLocate>> {
public:
    void on(const loom::Activated& a, loom::Mail& m) {
        if (activation_.accept(m, a)) announce(m);
    }
    void on(const ws::PaneCatalogRequested&, loom::Mail& m) {
        if (m.authored_from_role(ws::pane_menu::kWorkshopRole)) announce(m);
    }
    void on(const ws::PaneRoom& room, loom::Mail& m) {
        if (!m.authored_from_role(ws::pane_menu::kWorkshopRole) || room.pane != pane) return;
        rows_ = room.rows; columns_ = room.columns; draw(m);
    }
    void on(const ws::PaneButton& b, loom::Mail& m) {
        if (!m.authored_from_role(ws::pane_menu::kWorkshopRole) || b.pane != pane || !b.pressed ||
            b.button != 3 || b.lost) return;
        menu_ = ws::pane_menu::Offer(pane, "slot").at(b.row, b.column)
            .row("grab", "Grab live entry reference").send(m, office);
    }
    void on(const ws::PaneActionRequested& action, loom::Mail& m) {
        if (!m.authored_from_role(ws::pane_menu::kWorkshopRole) || action.pane != pane ||
            action.id != "inventory.grab") return;
        grab(m);
    }
    void on(const ws::PaneMenuAnswered& answer, loom::Mail& m) {
        if (menu_.take(m, answer) == "grab") grab(m);
    }
    void on(const ws::PaneOperationAnswered& a, loom::Mail& m) { if (client_.hear(a,m)) draw(m); }
    void on(const loom::Refused& a, loom::Mail& m) { if (client_.hear(a,m)) draw(m); }
    void on(const loom::DispatchRefused& a, loom::Mail& m) {
        if (carry_.valid() && m.dispatch_refused() && a.refused_attempt().seq == carry_.seq &&
            a.shape == ws::PaneCarryRequested::zen_name && a.version == ws::PaneCarryRequested::zen_version &&
            a.role == ws::pane_menu::kWorkshopRole && a.target.empty()) {
            carry_ = {};
            client_.notice = "The reference could not be picked up: " + a.reason;
            draw(m);
        } else if (client_.hear(a,m)) draw(m);
    }
    void on(const inv::InventoryEntry& e, loom::Mail& m) {
        if (!client_.hear(e,m) || !client_.result) return;
        try {
            const auto decoded = inv::decode_pair(std::string_view(
                reinterpret_cast<const char*>(e.pair.data()), e.pair.size()));
            label_ = decoded.item.schema().name();
            const auto encoded = inv::encode_pair(loom::to_value(e.reference), {});
            carry_ = m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
                ws::PaneCarryRequested{pane, label_, loom::Bytes(encoded.begin(), encoded.end())},
                client_.gesture);
            client_.notice = "Picking up the reference";
        } catch (const std::exception& error) { client_.notice = error.what(); }
        draw(m);
    }
    void on(const ws::PaneCarryAnswered& answer, loom::Mail& m) {
        if (!carry_.valid() || !m.answers_ask() || m.correlation() != client_.gesture) return;
        carry_ = {};
        client_.notice = answer.carried ? "Click Info to place the reference; Escape cancels" : answer.reason;
        draw(m);
    }
private:
    void announce(loom::Mail& m) {
        m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
            ws::PaneOffered{pane, "Inventory", "One stored item; grab its live reference"});
        m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
            ws::PaneActions{pane, {{"inventory.grab", "grab reference", input::scan::kReturn,
                                    input::mod::kNone}}});
    }
    void grab(loom::Mail& m) {
        if (carry_.valid()) { client_.notice = "The reference is still being picked up"; draw(m); return; }
        client_.begin(inv::InventoryLocate{}, pane, office, m, asks_); draw(m);
    }
    void draw(loom::Mail& m) {
        if (rows_ <= 0 || columns_ <= 0) return;
        std::vector<surf::SurfaceTextRow> rows;
        const auto push = [&](std::string text, std::int64_t role) {
            if (static_cast<std::int64_t>(rows.size()) < rows_)
                rows.push_back({ws::pane_text::drawable(ws::pane_text::fit(text, columns_)), role,
                                surf::role::kNone});
        };
        if (!client_.notice.empty()) push(client_.notice, surf::role::kAlert);
        push("INVENTORY - one slot", surf::role::kAccent);
        push(label_.empty() ? "Right-click the slot or press Enter" : "Last acquired: " + label_, surf::role::kFill);
        push("Grab a live reference, then click Info", surf::role::kMuted);
        m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole, ws::PaneContent{pane, std::move(rows)});
    }
    zengine::ActivationCursor activation_;
    ws::pane_menu::Asked menu_;
    inv::PaneClient client_;
    loom::Ticket carry_;
    std::uint64_t asks_ = 0;
    std::string label_;
    std::int64_t rows_ = 0, columns_ = 0;
};
}
ZEN_EXPORT_WEAVE(InventoryPane)
