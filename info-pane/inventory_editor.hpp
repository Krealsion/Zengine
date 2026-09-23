// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INFO_INVENTORY_EDITOR_HPP
#define ZENGINE_INFO_INVENTORY_EDITOR_HPP
#include "vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "component/text_box.hpp"
#include "inventory/codec.hpp"
#include "inventory/pane_client.hpp"
#include "message-draft/draft.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_text.hpp"
#include <algorithm>
#include <optional>

namespace zengine::info_pane {
class InventoryEditor {
public:
    bool active = false;
    bool has_entry() const { return item_.has_value(); }
    inventory::PaneClient client;

    void open(const workshop::PaneValueDrop& drop, loom::Mail&, std::uint64_t&) {
        if (client.busy() || dirty_ || editing_) {
            client.notice = "Save or discard this draft before opening another value"; return;
        }
        try {
            auto decoded = inventory::decode_pair(view(drop.data));
            item_.emplace(decoded.item); metadata_ = std::move(decoded.metadata);
            saved_ = {{}, 0, drop.data}; detached_ = true; active = true;
            selected_ = 0; discard_armed_ = false;
            client.notice = "Independent copy; Ctrl+S saves a new inventory entry";
        } catch (const std::exception& e) { client.notice = e.what(); }
    }

    void open(const workshop::PaneDrop& drop, loom::Mail& mail, std::uint64_t& asks) {
        if (client.busy() || dirty_ || editing_) {
            client.notice = "Save or fetch a fresh copy before replacing this draft";
            return;
        }
        try {
            const auto pair = inventory::decode_pair(view(drop.data));
            if (!loom::same_identity(pair.item.schema(), *loom::schema_of<inventory::InventoryReference>()))
                throw std::invalid_argument("Info does not yet inspect this kind of reference");
            const auto ref = loom::from_value<inventory::InventoryReference>(pair.item);
            if (client.begin(inventory::InventoryRead{ref}, kInfoPane, kInfoPaneRole, mail, asks)) {
                active = true;
                saving_ = false;
                sent_edit_ = edits_;
            }
        } catch (const std::exception& e) { client.notice = e.what(); }
    }

    template<class Answer>
    bool hear(const Answer& answer, loom::Mail& mail) {
        if (!client.hear(answer, mail)) return false;
        if (client.result) {
            const auto result = std::move(*client.result);
            client.result.reset();
            try {
                auto decoded = inventory::decode_pair(view(result.pair));
                if (edits_ != sent_edit_) {
                    if (saving_) {
                        saved_ = result;
                        detached_ = false;
                        dirty_ = true;
                        client.notice = "Saved the submitted draft; newer edits remain unsaved";
                    } else {
                        client.notice = "Fresh copy arrived after new edits; draft retained. Fetch again to replace it";
                    }
                } else {
                    item_.emplace(decoded.item);
                    metadata_ = std::move(decoded.metadata);
                    saved_ = result;
                    dirty_ = false;
                    editing_ = false;
                    client.notice = saving_ ? (detached_ ? "Saved as a new inventory entry" : "Saved to inventory")
                                            : "Fresh copy from inventory";
                    detached_ = false;
                    selected_ = 0;
                }
            } catch (const std::exception& e) { client.notice = e.what(); }
        }
        return true;
    }

    std::vector<workshop::PaneActionRow> actions() const {
        using namespace input;
        if (editing_) return {
            {"inventory.field.accept", "keep field edit", scan::kReturn, mod::kNone},
            {"inventory.field.cancel", "cancel field edit", scan::kEscape, mod::kNone}};
        return {{"inventory.up", "previous field", scan::kUp, mod::kNone},
                {"inventory.down", "next field", scan::kDown, mod::kNone},
                {"inventory.edit", "edit field", scan::kReturn, mod::kNone},
                {"inventory.save", "save entry", scan::kS, mod::kCtrl},
                {"inventory.fresh", "fetch fresh copy", scan::kR, mod::kCtrl},
                {"inventory.discard", "discard local edits", scan::kD, mod::kCtrl},
                {"inventory.panes", "pane properties", scan::kI, mod::kCtrl}};
    }

    void act(const std::string& id, loom::Mail& mail, std::uint64_t& asks) {
        bool declared = false;
        for (const auto& row : actions()) if (row.id == id) declared = true;
        if (!declared) return;
        if (id != "inventory.fresh") discard_armed_ = false;
        if (id == "inventory.panes") { active = false; return; }
        if (id == "inventory.discard" && item_) {
            const auto decoded = inventory::decode_pair(view(saved_.pair));
            item_.emplace(decoded.item); metadata_ = decoded.metadata;
            dirty_ = false; ++edits_;
            client.notice = "Local edits discarded; this is the last saved/read copy";
            return;
        }
        if (id == "inventory.field.cancel") { editing_ = false; line_.clear(); return; }
        if (id == "inventory.field.accept") {
            try {
                item_->set_text(field_, line_.text());
                dirty_ = true; ++edits_; editing_ = false; line_.clear();
                client.notice = "Field changed in draft; Ctrl+S saves the entry";
            } catch (const std::exception& e) { client.notice = e.what(); }
            return;
        }
        const auto fields = rows();
        if (id == "inventory.up" && selected_ > 0) --selected_;
        else if (id == "inventory.down" && selected_ + 1 < fields.size()) ++selected_;
        else if (id == "inventory.edit" && selected_ < fields.size()) {
            const auto& row = fields[selected_];
            if (row.metadata || row.row.type.kind == loom::Kind::Message ||
                row.row.type.kind == loom::Kind::List || row.row.type.kind == loom::Kind::Bytes) {
                client.notice = row.metadata ? "Capture metadata is read-only" :
                    "Select a scalar field inside this value; byte fields are read-only here";
                return;
            }
            field_ = row.row.path;
            line_.set(row.row.present ? row.row.summary : "", row.row.summary.size());
            editing_ = true; ++edits_;
        } else if (id == "inventory.save" && item_) {
            try {
                const auto encoded = inventory::encode_pair(item_->snapshot(), metadata_);
                const loom::Bytes bytes(encoded.begin(), encoded.end());
                const bool begun = detached_
                    ? client.begin(inventory::InventoryAdd{bytes, {}}, kInfoPane, kInfoPaneRole, mail, asks)
                    : client.begin(inventory::InventoryWrite{saved_.reference, saved_.revision, bytes},
                                   kInfoPane, kInfoPaneRole, mail, asks);
                if (begun) {
                    saving_ = true; sent_edit_ = edits_;
                }
            } catch (const std::exception& e) { client.notice = e.what(); }
        } else if (id == "inventory.fresh" && item_) {
            if (dirty_ && !discard_armed_) {
                discard_armed_ = true;
                client.notice = "Unsaved draft: press Ctrl+R again to discard it and fetch a fresh copy";
                return;
            }
            discard_armed_ = false;
            if (detached_) {
                const auto decoded = inventory::decode_pair(view(saved_.pair));
                item_.emplace(decoded.item); metadata_ = decoded.metadata; dirty_ = false; ++edits_;
                client.notice = "Restored the received copy; the source entry is independent"; return;
            }
            if (client.begin(inventory::InventoryRead{saved_.reference}, kInfoPane, kInfoPaneRole, mail, asks)) {
                saving_ = false; sent_edit_ = edits_;
            }
        }
    }

    void key(const workshop::PaneKey& key) {
        if (editing_ && line_.consume(key.scancode, key.modifiers, clipboard_)) ++edits_;
    }
    void text(const std::string& text) {
        if (editing_ && text.find_first_of("\r\n\0", 0, 3) == std::string::npos) {
            line_.type(text); ++edits_;
        }
    }
    void press(std::int64_t row) {
        if (editing_) return;
        for (const auto& placed : placed_) if (placed.first == row) selected_ = placed.second;
        discard_armed_ = false;
    }

    std::vector<surface::SurfaceTextRow> draw(std::int64_t height, std::int64_t width) {
        std::vector<surface::SurfaceTextRow> out;
        placed_.clear();
        const auto push = [&](std::string text, std::int64_t role = surface::role::kFill) {
            if (static_cast<std::int64_t>(out.size()) < height)
                out.push_back({workshop::pane_text::drawable(workshop::pane_text::fit(std::move(text), width)),
                               role, surface::role::kNone});
        };
        push(item_ ? std::string(detached_ ? "COPY " : "LIVE ENTRY ") + item_->schema()->name() + " v" + std::to_string(item_->schema()->version()) +
            (dirty_ ? " * unsaved" : "") : "INVENTORY (waiting)", surface::role::kAccent);
        push(client.notice, surface::role::kAlert);
        if (editing_) {
            push("Edit " + message_draft::path_label(field_));
            line_.keep_caret_visible(width - 2);
            push("> " + line_.visible(width - 2), surface::role::kAccent);
            push("Enter keeps field edit; Escape cancels", surface::role::kMuted);
            return out;
        }
        push("Enter edit | ^S save | ^R fresh | ^D discard | ^I panes", surface::role::kMuted);
        const auto fields = rows();
        if (fields.empty()) return out;
        selected_ = std::min(selected_, fields.size() - 1);
        const auto count = static_cast<std::size_t>(std::max<std::int64_t>(0, height - 3));
        const auto first = count && selected_ >= count ? selected_ - count + 1 : 0;
        for (std::size_t i = first; i < fields.size() && i < first + count; ++i) {
            placed_.emplace_back(static_cast<std::int64_t>(out.size()), i);
            push(std::string(i == selected_ ? "> " : "  ") + fields[i].label + ": " + fields[i].row.summary,
                 fields[i].metadata ? surface::role::kMuted : surface::role::kFill);
        }
        return out;
    }
private:
    struct Field { message_draft::Row row; std::string label; bool metadata = false; };
    static std::string_view view(const loom::Bytes& bytes) {
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }
    std::vector<Field> rows() const {
        std::vector<Field> result;
        if (item_) for (auto row : item_->rows()) {
            auto label = row.label; result.push_back({std::move(row), std::move(label), false});
        }
        for (std::size_t i = 0; i < metadata_.size(); ++i)
            for (auto row : message_draft::Draft(metadata_[i]).rows()) {
                auto label = "metadata[" + std::to_string(i) + "]." + row.label;
                result.push_back({std::move(row), std::move(label), true});
            }
        return result;
    }
    std::optional<message_draft::Draft> item_;
    std::vector<loom::Value> metadata_;
    inventory::InventoryEntry saved_;
    component::TextBox line_;
    component::Clipboard clipboard_;
    message_draft::Path field_;
    std::vector<std::pair<std::int64_t, std::size_t>> placed_;
    std::size_t selected_ = 0;
    std::uint64_t edits_ = 0, sent_edit_ = 0;
    bool dirty_ = false, editing_ = false, saving_ = false, discard_armed_ = false;
    bool detached_ = false;
};
} // namespace zengine::info_pane
#endif
