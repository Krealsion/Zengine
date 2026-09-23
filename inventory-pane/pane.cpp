// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "activation/activation.hpp"
#include "inventory/codec.hpp"
#include "inventory/pane_client.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_text.hpp"
#include "component/row_map.hpp"
#include "component/list_window.hpp"
#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include <zen/kernel/export.hpp>
#include <algorithm>

namespace {
namespace ws = zengine::workshop;
namespace inv = zengine::inventory;
namespace input = zengine::input;
namespace surf = zengine::surface;
namespace component = zengine::component;
constexpr const char* office = "zengine.inventory-pane";
constexpr const char* pane = "inventory";
struct InventoryPaneState { ZEN_SHAPE(InventoryPaneState, 1); };
class InventoryPane : public loom::WeaveBase<InventoryPane, InventoryPaneState,
    loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom, ws::PaneButton,
                 ws::v3::PanePressed, ws::PaneDragged, ws::PaneWheel, ws::PaneKey, ws::PaneTextInput,
                 ws::PaneValueDrop, ws::PaneActionRequested, ws::PaneMenuAnswered, ws::PaneOperationAnswered,
                 ws::PaneCarryAnswered, inv::InventoryEntry, inv::InventoryListed, inv::InventoryChanged,
                 loom::Ack, loom::Refused, loom::DispatchRefused>,
    loom::Emit<ws::PaneOffered, ws::PaneActions, ws::v3::PaneContent, ws::PaneMenuRequested,
               ws::PaneKeyboardRequested, ws::PaneOperationRequested, ws::PaneCarryRequested,
               ws::PaneValueCarryRequested, inv::InventoryList, inv::InventoryRead, inv::InventoryAdd,
               inv::InventoryRename, inv::InventoryRemove>> {
public:
    void on(const loom::Activated& a, loom::Mail& m) {
        if (activation_.accept(m, a)) announce(m);
    }
    void on(const ws::PaneCatalogRequested&, loom::Mail& m) {
        if (host(m)) announce(m);
    }
    void on(const ws::PaneRoom& room, loom::Mail& m) {
        if (!host(m) || room.pane != pane) return;
        rows_ = room.rows; columns_ = room.columns; map_.clear();
        refresh(m); draw(m);
    }
    void on(const inv::InventoryChanged&, loom::Mail& m) {
        if (m.authored_from_role(inv::kInventoryRole) && rows_ > 0) refresh(m);
    }
    void on(const inv::InventoryListed& answer, loom::Mail& m) {
        if (!list_.valid() || !m.answers_ask() || m.correlation() != list_ask_) return;
        list_ = {}; entries_ = answer.entries;
        if (selected_.empty() && !entries_.empty()) selected_ = entries_.front().reference.entry;
        if (refresh_again_) { refresh_again_ = false; refresh(m); }
        draw(m);
    }
    void on(const ws::v3::PanePressed& b, loom::Mail& m) {
        if (!host(m) || b.pane != pane || naming_) return;
        remove_armed_ = false;
        const auto* entry = pointed(b.row, b.column, b.picture);
        if (!entry) { client_.notice = "That row moved or is not an entry; try its current row"; draw(m); return; }
        selected_ = entry->reference.entry; target_ = *entry;
        acquire(Mode::drag, m);
    }
    void on(const ws::PaneDragged&, loom::Mail&) {} // Workshop owns value transfer motion.
    void on(const ws::PaneButton& b, loom::Mail& m) {
        if (!host(m) || b.pane != pane || !b.pressed || b.button != 3 || b.lost || naming_) return;
        remove_armed_ = false;
        const auto* entry = pointed(b.row, b.column, b.picture);
        if (!entry) { client_.notice = "Right-click a current inventory entry"; draw(m); return; }
        target_ = *entry; selected_ = entry->reference.entry;
        menu_ = ws::pane_menu::Offer(pane, target_.label).at(b.row, b.column)
            .row("live", "Grab live entry reference").row("copy", "Pick up a copy")
            .row("rename", "Rename entry").row("remove", "Remove entry...").send(m, office);
        draw(m);
    }
    void on(const ws::PaneMenuAnswered& answer, loom::Mail& m) {
        const auto choice = menu_.take(m, answer);
        if (choice == "live") acquire(Mode::reference, m);
        else if (choice == "copy") acquire(Mode::copy, m);
        else if (choice == "rename" || choice == "remove") {
            m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
                ws::PaneKeyboardRequested{pane}, m.correlation());
            if (choice == "rename") begin_name();
            else { remove_armed_ = true; client_.notice = "Press Delete to confirm removing " + target_.label; }
            declare(m); draw(m);
        }
    }
    void on(const ws::PaneActionRequested& action, loom::Mail& m) {
        if (!host(m) || action.pane != pane) return;
        const auto& id = action.id;
        if (naming_) {
            if (id == "inventory.name.cancel") { naming_ = false; client_.notice = "Rename cancelled"; }
            else if (id == "inventory.name.save") {
                if (client_.begin(inv::InventoryRename{target_.reference, target_.revision, line_.text()},
                    pane, office, m, asks_)) { naming_ = false; mode_ = Mode::change; }
            }
        } else if (id == "inventory.up") step(-1);
        else if (id == "inventory.down") step(1);
        else if (id == "inventory.sort") { order_ = (order_ + 1) % 3; client_.notice = "Sort: " + order_name(); }
        else if (id == "inventory.refresh") refresh(m);
        else if (id == "inventory.remove" && remove_armed_) {
            remove_armed_ = false;
            client_.begin(inv::InventoryRemove{target_.reference, target_.revision}, pane, office, m, asks_);
        } else if (const auto* entry = selected()) {
            target_ = *entry;
            if (id == "inventory.copy") acquire(Mode::copy, m);
            else if (id == "inventory.live") acquire(Mode::reference, m);
            else if (id == "inventory.rename") begin_name();
            else if (id == "inventory.remove") {
                remove_armed_ = true; client_.notice = "Press Delete again to remove " + target_.label;
            }
        }
        if (id != "inventory.remove") remove_armed_ = false;
        declare(m); draw(m);
    }
    void on(const ws::PaneWheel& wheel, loom::Mail& m) {
        if (!host(m) || wheel.pane != pane || naming_) return;
        wheel_ += wheel.dy;
        while (wheel_ >= 1) { step(-1); wheel_ -= 1; }
        while (wheel_ <= -1) { step(1); wheel_ += 1; }
        remove_armed_ = false; draw(m);
    }
    void on(const ws::PaneKey& key, loom::Mail& m) {
        if (host(m) && key.pane == pane && naming_ && line_.consume(key.scancode, key.modifiers, clipboard_)) draw(m);
    }
    void on(const ws::PaneTextInput& text, loom::Mail& m) {
        if (!host(m) || text.pane != pane || !naming_) return;
        if (std::all_of(text.text.begin(), text.text.end(), [](unsigned char c) { return c >= 32 && c <= 126; }))
            line_.type(text.text);
        draw(m);
    }
    void on(const ws::PaneValueDrop& value, loom::Mail& m) {
        if (!host(m) || value.pane != pane || naming_) return;
        if (client_.begin(inv::InventoryAdd{value.data, {}}, pane, office, m, asks_)) mode_ = Mode::change;
        draw(m);
    }
    void on(const ws::PaneOperationAnswered& a, loom::Mail& m) { if (client_.hear(a,m)) draw(m); }
    void on(const loom::Ack& a, loom::Mail& m) { if (client_.hear(a,m)) { refresh(m); draw(m); } }
    void on(const loom::Refused& a, loom::Mail& m) {
        if (list_.valid() && m.answers_ask() && m.correlation() == list_ask_) {
            list_ = {}; client_.notice = a.reason; draw(m);
        } else if (client_.hear(a,m)) draw(m);
    }
    void on(const loom::DispatchRefused& a, loom::Mail& m) {
        if (!m.dispatch_refused()) return;
        if (list_.valid() && a.refused_attempt().seq == list_.seq && a.role == inv::kInventoryRole &&
            a.shape == inv::InventoryList::zen_name && a.version == 1 && a.target.empty()) {
            list_ = {}; client_.notice = "Inventory list unavailable: " + a.reason; draw(m);
        } else if (carry_.valid() && a.refused_attempt().seq == carry_.seq &&
            a.shape == carry_shape_ && a.version == 1 && a.role == ws::pane_menu::kWorkshopRole && a.target.empty()) {
            carry_ = {}; client_.notice = "The item could not be picked up: " + a.reason; draw(m);
        } else if (client_.hear(a,m)) draw(m);
    }
    void on(const inv::InventoryEntry& e, loom::Mail& m) {
        if (!client_.hear(e,m) || !client_.result) return;
        client_.result.reset();
        if (mode_ == Mode::change) { client_.notice = "Entry saved"; refresh(m); draw(m); return; }
        try {
            const auto decoded = inv::decode_pair(view(e.pair));
            const auto label = target_.label.empty() ? decoded.item.schema().name() : target_.label;
            if (mode_ == Mode::reference) {
                const auto encoded = inv::encode_pair(loom::to_value(e.reference), {});
                carry_shape_ = ws::PaneCarryRequested::zen_name;
                carry_ = m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
                    ws::PaneCarryRequested{pane, label, loom::Bytes(encoded.begin(), encoded.end())}, client_.gesture);
            } else {
                carry_shape_ = ws::PaneValueCarryRequested::zen_name;
                carry_ = m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
                    ws::PaneValueCarryRequested{pane, label, e.pair, mode_ == Mode::drag}, client_.gesture);
            }
            client_.notice = carry_.valid() ? "Copy acquired; finish the gesture to place it" : "Pickup could not be queued";
        } catch (const std::exception& error) { client_.notice = error.what(); }
        draw(m);
    }
    void on(const ws::PaneCarryAnswered& answer, loom::Mail& m) {
        if (!carry_.valid() || !m.answers_ask() || m.correlation() != client_.gesture) return;
        carry_ = {};
        client_.notice = !answer.carried ? answer.reason : mode_ == Mode::drag
            ? "Drag copies; right-click opens entry actions" : "Click a receiving pane; Escape cancels";
        draw(m);
    }
private:
    enum class Mode { drag, copy, reference, change };
    static bool host(const loom::Mail& m) { return m.authored_from_role(ws::pane_menu::kWorkshopRole); }
    static std::string_view view(const loom::Bytes& b) { return {reinterpret_cast<const char*>(b.data()), b.size()}; }
    void announce(loom::Mail& m) {
        m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
            ws::PaneOffered{pane, "Inventory", "Stored values: drag a copy; right-click for live entry actions"});
        declare(m);
    }
    void declare(loom::Mail& m) {
        using namespace input;
        std::vector<ws::PaneActionRow> rows;
        if (naming_) rows = {
            {"inventory.name.save", "save name", scan::kReturn, mod::kNone},
            {"inventory.name.cancel", "cancel name", scan::kEscape, mod::kNone}};
        else rows = {
            {"inventory.copy", "pick up copy", scan::kReturn, mod::kNone},
            {"inventory.live", "pick up live reference", scan::kReturn, mod::kCtrl},
            {"inventory.up", "previous entry", scan::kUp, mod::kNone},
            {"inventory.down", "next entry", scan::kDown, mod::kNone},
            {"inventory.sort", "cycle sorting", scan::kS, mod::kCtrl},
            {"inventory.refresh", "refresh entries", scan::kR, mod::kCtrl},
            {"inventory.rename", "rename entry", scan::kN, mod::kCtrl},
            {"inventory.remove", "remove entry", scan::kDelete, mod::kNone}};
        m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole, ws::PaneActions{pane, std::move(rows)});
    }
    void refresh(loom::Mail& m) {
        if (list_.valid()) { refresh_again_ = true; return; }
        list_ask_ = ++asks_;
        list_ = m.send_to_role(inv::kInventoryRole, inv::InventoryList{}, list_ask_);
        if (!list_.valid()) client_.notice = "The inventory list request could not be queued";
    }
    const inv::InventorySummary* selected() const {
        for (const auto& e : entries_) if (e.reference.entry == selected_) return &e;
        return nullptr;
    }
    const inv::InventorySummary* pointed(std::int64_t row, std::int64_t column, std::int64_t picture) const {
        if (!map_.current(picture)) return nullptr;
        const auto* id = map_.at(row, column);
        if (id) for (const auto& e : entries_) if (e.reference.entry == *id) return &e;
        return nullptr;
    }
    std::vector<inv::InventorySummary> ordered() const {
        auto rows = entries_;
        if (order_) std::stable_sort(rows.begin(), rows.end(), [&](const auto& a, const auto& b) {
            return (order_ == 1 ? a.label : a.schema) < (order_ == 1 ? b.label : b.schema);
        });
        return rows;
    }
    std::string order_name() const { return order_ == 0 ? "added" : order_ == 1 ? "name" : "type"; }
    void step(int delta) {
        const auto rows = ordered(); if (rows.empty()) return;
        auto it = std::find_if(rows.begin(), rows.end(), [&](const auto& e) { return e.reference.entry == selected_; });
        const auto index = it == rows.end() ? 0 : it - rows.begin();
        const auto next = std::clamp<std::ptrdiff_t>(index + delta, 0, static_cast<std::ptrdiff_t>(rows.size() - 1));
        selected_ = rows[static_cast<std::size_t>(next)].reference.entry;
    }
    void begin_name() { naming_ = true; line_.set(target_.label, target_.label.size()); }
    void acquire(Mode mode, loom::Mail& m) {
        if (carry_.valid()) { client_.notice = "A pickup is still pending"; draw(m); return; }
        if (client_.begin(inv::InventoryRead{target_.reference}, pane, office, m, asks_)) mode_ = mode;
        draw(m);
    }
    void draw(loom::Mail& m) {
        if (rows_ <= 0 || columns_ <= 0) return;
        map_.begin();
        std::vector<surf::SurfaceTextRow> rows;
        const auto push = [&](std::string text, std::int64_t role) {
            if (static_cast<std::int64_t>(rows.size()) < rows_)
                rows.push_back({ws::pane_text::drawable(ws::pane_text::fit(text, columns_)), role, surf::role::kNone});
        };
        push("INVENTORY " + std::to_string(entries_.size()) + " | sort: " + order_name(), surf::role::kAccent);
        if (naming_) {
            push("Name: " + line_.text(), surf::role::kFill);
            push("Enter saves; Escape cancels", surf::role::kMuted);
        } else {
            const auto entries = ordered();
            const auto cursor = std::find_if(entries.begin(), entries.end(), [&](const auto& e) { return e.reference.entry == selected_; });
            const auto index = cursor == entries.end() ? 0u : static_cast<std::size_t>(cursor - entries.begin());
            const auto budget = static_cast<std::size_t>(std::max<std::int64_t>(0, rows_ - (rows_ > 2 ? 2 : 1)));
            const auto window = component::cursor_window(entries.size(), index, index, budget);
            if (window.before && window.marker_rows()) push("... " + std::to_string(window.before) + " earlier", surf::role::kMuted);
            for (std::size_t i = window.first; i < window.end(); ++i) {
                map_.row(static_cast<std::int64_t>(rows.size()), entries[i].reference.entry);
                push(std::string(entries[i].reference.entry == selected_ ? "> " : "  ") + entries[i].label +
                    (entries[i].capture_slot ? " [capture slot]" : "") + " : " + entries[i].schema, surf::role::kFill);
            }
            if (window.after && window.marker_rows()) push("... " + std::to_string(window.after) + " later", surf::role::kMuted);
            if (entries.empty()) push("No entries; capture a value or drop a copy here", surf::role::kMuted);
            push(client_.notice.empty() ? "Drag copies | right-click live | ^N name | ^S sort" : client_.notice, surf::role::kMuted);
        }
        m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
            ws::v3::PaneContent{pane, std::move(rows), 0, map_.settle()});
    }
    zengine::ActivationCursor activation_;
    ws::pane_menu::Asked menu_;
    inv::PaneClient client_;
    component::RowMap<std::string> map_;
    component::TextBox line_;
    component::Clipboard clipboard_;
    std::vector<inv::InventorySummary> entries_;
    inv::InventorySummary target_;
    loom::Ticket carry_, list_;
    std::uint64_t asks_ = 0, list_ask_ = 0;
    std::string selected_, carry_shape_;
    Mode mode_ = Mode::copy;
    int order_ = 0;
    double wheel_ = 0;
    bool refresh_again_ = false, naming_ = false, remove_armed_ = false;
    std::int64_t rows_ = 0, columns_ = 0;
};
}
ZEN_EXPORT_WEAVE(InventoryPane)
