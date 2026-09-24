// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INFO_VALUE_VIEW_HPP
#define ZENGINE_INFO_VALUE_VIEW_HPP

// ONE INFO VALUE VIEW: a pane key this weave offers and everything that view owns -- its draft
// (copy or linked entry), structural field selection, picture, notice, and every request it is
// waiting on (owner read/write, field pickup, observation lease and cycle, source sample). Views
// share code and the weave's correlation counter, never mutable subjects: an answer settles only
// the record in the view that asked, under Loom's answer provenance and that record's number.
//
// AN ANSWER CAN MATCH ITS REQUEST AND STILL NOT BE SAFE FOR THE DRAFT, so the view decides that
// second question per operation: a save leaves the draft editable and keeps newer edits; Refresh,
// Link and Sample replace a draft the maker already agreed to replace, so it is frozen until they
// answer, are refused or the maker stops waiting. Every way out of a watch ends it at Workshop.
// agents/inventory.md owns the law; docs/workshop/info-views.md is the maker's guide.

#include "vocabulary.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include "inventory/codec.hpp"
#include "inventory/observation.hpp"
#include "inventory/pane_client.hpp"
#include "message-draft/transfer.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <zen/weave/poke.hpp>
#include <zen/weave/role_request.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <optional>
#include <vector>

namespace zengine::info_pane {

/// A FIELD BY STRUCTURE, never by row number: `metadata` is -1 for the item, else the index of a
/// read-only metadata value; `path` is the draft's structural path inside it.
struct FieldRef {
    std::int64_t metadata = -1;
    message_draft::Path path;
    bool operator==(const FieldRef&) const = default;
    /// Is this field `outer` or inside it?
    bool within(const FieldRef& outer) const {
        return metadata == outer.metadata && path.size() >= outer.path.size() &&
               std::equal(outer.path.begin(), outer.path.end(), path.begin());
    }
};

/// WHAT ONE PLACE OF THE PICTURE MEANS: a control (by action id) or a field.
struct Meaning {
    enum Kind { kNone, kField, kControl } kind = kNone;
    std::string control;
    FieldRef field;
    bool operator==(const Meaning&) const = default;
};

/// WHAT AN ACT MAY SPEND BEYOND ITS VIEW: the delivery that brought it and the weave's one
/// correlation counter.
struct ViewContext {
    loom::Mail& mail;
    std::uint64_t& asks;
};

/// WHAT AN ACT ASKS THE WEAVE TO DO THAT NO VIEW OWNS.
struct ActResult {
    bool reoffer = false; ///< the title changed: the slot's offer names it
    bool leave = false;   ///< the default view returns to pane properties
};

inline std::string clock_text() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char out[16] = {};
    if (const std::tm* local = std::localtime(&now)) std::strftime(out, sizeof out, "%H:%M:%S", local);
    return out;
}

class ValueView {
public:
    ValueView(std::string key, std::size_t slot) : key_(std::move(key)), slot_(slot), title_(default_title()) {}

    const std::string& key() const noexcept { return key_; }
    std::size_t slot() const noexcept { return slot_; }
    bool is_default() const noexcept { return slot_ == 1; }
    std::string default_title() const { return is_default() ? "Info" : "Info " + std::to_string(slot_); }
    const std::string& title() const noexcept { return title_; }
    bool allocated() const noexcept { return allocated_; }
    std::uint64_t incarnation() const noexcept { return incarnation_; }
    bool on_desk() const noexcept { return on_desk_; }
    bool has_entry() const noexcept { return item_.has_value(); }
    bool dirty() const noexcept { return dirty_; }
    bool editing() const noexcept { return editing_ || renaming_; }
    bool watching() const noexcept { return watch_.state != Watch::State::off; }
    bool saving() const noexcept {
        return op_ && op_->purpose != Operation::Purpose::refresh && op_->purpose != Operation::Purpose::link;
    }
    /// A request this view is still waiting on for a maker's act (not the watch cycle).
    bool busy() const noexcept {
        return client_.busy() || pickup_ != Pickup::idle || sample_.phase != Sample::Phase::idle;
    }
    std::string busy_reason() const {
        if (op_) return saving() ? "a save is waiting for its answer" : "this view is " + doing();
        if (pickup_ != Pickup::idle) return "a field pickup is pending";
        if (sample_.phase != Sample::Phase::idle) return "a source sample is waiting for its answer";
        return {};
    }
    /// THE REPLACEMENT IN FLIGHT -- a Refresh, Link or Sample whose answer will replace this draft
    /// -- in words, or empty. While there is one the draft is frozen: nothing changes what the
    /// answer will replace, and a maker who stops waiting keeps the draft as it is.
    std::string replacing() const {
        if (op_ && !saving()) return doing();
        if (sample_.phase != Sample::Phase::idle) return "sampling " + sample_.role;
        return {};
    }
    void say(std::string notice) { notice_ = std::move(notice); }
    const std::string& notice() const noexcept { return notice_; }
    std::int64_t rows() const noexcept { return rows_; }
    std::int64_t columns() const noexcept { return columns_; }
    bool granted() const noexcept { return granted_ && rows_ > 0 && columns_ > 0; }
    /// The default view shows its value instead of pane properties.
    bool active = false;
    /// This slot was offered to Workshop in this image. Offers are never withdrawn, so a slot is
    /// offered on first use and reused after: the catalog never holds more than four Info panes.
    bool offered = false;

    // ---- lifecycle -----------------------------------------------------------------------

    void allocate() { allocated_ = true; ++incarnation_; close_armed_ = false; }
    /// FORGET EVERYTHING THIS INCARNATION HELD: its records go with it, so a late answer to one
    /// of them matches nothing, and the next incarnation mints new numbers. Inventory data and the
    /// slot's offer are untouched; the picture keeps numbering upward so no old press matches.
    /// The one record the slot keeps is a watch request still waiting for Workshop: its approval
    /// may yet grant a lease, and ending that lease is this slot's, whoever holds it next.
    bool retire(ViewContext& c, std::string reason = "view closed") {
        stop_watch(c, std::move(reason));
        const bool renamed = title_ != default_title();
        ValueView fresh(key_, slot_);
        fresh.incarnation_ = incarnation_ + 1;
        fresh.ending_ = std::move(ending_);
        fresh.map_ = std::move(map_);
        fresh.rows_ = rows_; fresh.columns_ = columns_; fresh.granted_ = granted_;
        fresh.on_desk_ = on_desk_;
        fresh.offered = offered;
        fresh.allocated_ = is_default();
        *this = std::move(fresh);
        return renamed;
    }
    /// A FORK IS THE LOCAL DRAFT AND NOTHING THAT WAS WAITING: no pending request, observation
    /// lease, watch, held newer data or sample in flight. Linked stays linked -- a reference is
    /// not authority, and every later save or read needs its own permission.
    void fork_from(const ValueView& o) {
        item_ = o.item_; metadata_ = o.metadata_; saved_ = o.saved_; detached_ = o.detached_;
        preset_ = o.preset_; preset_title_ = o.preset_title_; label_ = o.label_;
        dirty_ = o.dirty_; sampled_ = o.sampled_; edited_ = o.edited_; stale_ = o.stale_;
        selected_ = o.selected_; lost_ = o.lost_; hint_ = o.hint_;
        set_title(o.title_ + " fork");
        notice_ = "Forked from " + o.title_ + ": an independent local draft; watch is off";
    }
    bool set_title(std::string title) {
        title.erase(0, title.find_first_not_of(' '));
        while (!title.empty() && title.back() == ' ') title.pop_back();
        if (title.empty()) title = default_title();
        if (title.size() > kMaxViewTitle) title.resize(kMaxViewTitle);
        for (char& ch : title) if (static_cast<unsigned char>(ch) < 32 || static_cast<unsigned char>(ch) > 126) ch = '?';
        const bool changed = title != title_;
        title_ = std::move(title);
        return changed;
    }

    // ---- the room, and whether the desk shows it -------------------------------------------

    void room(std::int64_t rows, std::int64_t columns) { rows_ = rows; columns_ = columns; granted_ = true; }
    /// THE DESK SAYS WHETHER THIS VIEW IS SHOWN. A hidden view keeps its subject and draft, but a
    /// watch never runs unseen: it ends, and a new one needs a new gesture.
    void desk(bool open, ViewContext& c) {
        on_desk_ = open;
        if (!open && watching()) stop_watch(c, "Watch ended: the view was hidden", true);
    }

    // ---- opening a subject -----------------------------------------------------------------

    /// A VALUE DROPPED ON THIS VIEW. A typed field copy fills the field row it was dropped on;
    /// any other value keeps its open-subject meaning and never replaces unsaved work.
    void drop_value(const workshop::PaneValueDrop& drop, ViewContext& c) {
        try {
            auto decoded = inventory::decode_pair(view(drop.data));
            if (message_draft::is_field_value(decoded.item)) { fill_field(drop, decoded.item); return; }
            if (!allocated_) allocate();
            if (busy() || dirty_ || editing()) {
                notice_ = dirty_ ? "Unsaved edits kept: a whole value would replace this draft. Save, Save copy or "
                                   "discard first -- or drag a single field onto a field row"
                                 : "Finish the pending work (" + busy_reason() + ") before opening another value";
                return;
            }
            // A NEW SUBJECT: the old one's watch ends at Workshop too, not only in this view.
            const bool watched = watching();
            stop_watch(c, {});
            load(decoded, true);
            saved_ = {{}, 0, drop.data}; detached_ = true; label_.clear();
            forget_subject();
            active = true; select_first();
            notice_ = std::string(watched ? "Watch ended: this view now holds an independent copy. " : "Independent copy; ") +
                      "Save stores it as a new entry, Save copy keeps it independent";
        } catch (const std::exception& e) { notice_ = std::string("Drop refused: ") + e.what(); }
    }

    /// A LIVE REFERENCE DROPPED ON THIS VIEW: read the entry it names, under this gesture. Until
    /// it answers, the view keeps showing -- and watching -- what it showed, frozen.
    void drop_reference(const workshop::PaneDrop& drop, ViewContext& c) {
        if (!allocated_) allocate();
        if (busy() || dirty_ || editing()) {
            notice_ = "Save, Save copy or discard this draft before linking another entry";
            return;
        }
        try {
            const auto pair = inventory::decode_pair(view(drop.data));
            if (!loom::same_identity(pair.item.schema(), *loom::schema_of<inventory::InventoryReference>()))
                throw std::invalid_argument("Info does not yet inspect this kind of reference");
            const auto ref = loom::from_value<inventory::InventoryReference>(pair.item);
            std::string label;
            for (const auto& meta : pair.metadata)
                if (loom::same_identity(meta.schema(), *loom::schema_of<inventory::InventorySummary>()))
                    label = loom::from_value<inventory::InventorySummary>(meta).label;
            if (begin(Operation::Purpose::link, inventory::InventoryRead{ref}, c, std::move(label))) active = true;
        } catch (const std::exception& e) { notice_ = e.what(); }
    }

    // ---- gestures ------------------------------------------------------------------------

    /// A PRESS ON THE PICTURE THIS VIEW PUBLISHED. Returns the control a press chose, for the
    /// weave to route through the same act a key or menu row reaches.
    std::string press(std::int64_t row, std::int64_t column, std::int64_t picture, std::uint64_t gesture) {
        press_ = {};
        if (!map_.current(picture)) { notice_ = "This view changed -- press again"; return {}; }
        const Meaning* at = map_.at(row, column);
        if (!at) return {};
        if (at->kind == Meaning::kControl) return at->control;
        if (at->kind == Meaning::kField && !editing()) {
            selected_ = at->field; lost_ = false; observed_.clear(); close_armed_ = false; discard_armed_ = false;
            press_ = {true, false, row, column, gesture, at->field};
        }
        return {};
    }
    /// THE HAND MOVED WITH THE BUTTON DOWN after a press on a field row: the field's typed copy
    /// is acquired now, under that press, so a click that never moved asks nothing.
    void dragged(std::int64_t row, std::int64_t column, ViewContext& c) {
        if (!press_.armed || press_.started || (row == press_.row && column == press_.column)) return;
        press_.started = true;
        begin_pickup(press_.field, press_.gesture, true, c);
    }
    /// THE WHEEL WALKS THE FIELD SELECTION, one field per notch, fractions carried; the window
    /// follows the selection, so every field of a small room stays reachable by pointer alone.
    bool wheel(double dy) {
        if (editing() || !item_) return false;
        wheel_ += dy;
        const auto fields = field_list();
        bool moved = false;
        while (wheel_ >= 1.0) { step(fields, -1); wheel_ -= 1.0; moved = true; }
        while (wheel_ <= -1.0) { step(fields, 1); wheel_ += 1.0; moved = true; }
        return moved;
    }
    void key(const workshop::PaneKey& key) {
        if (pickup_ != Pickup::idle || !editing()) return;
        if (line_.consume(key.scancode, key.modifiers, clipboard_) && editing_) ++edits_;
    }
    void text(const std::string& text) {
        if (pickup_ != Pickup::idle || !editing() || text.find_first_of("\r\n\0", 0, 3) != std::string::npos) return;
        line_.type(text);
        if (editing_) ++edits_;
    }

    // ---- the acts ------------------------------------------------------------------------

    /// WHY AN ACT CANNOT RUN NOW, or empty. Buttons render from this and acts refuse with it,
    /// so a control and its key never disagree.
    std::string unavailable(const std::string& id) const {
        const bool linked = item_ && !detached_ && !saved_.reference.entry.empty();
        if (id == kActionViewNew || id == kActionViewRename || id == kActionViewMenu ||
            id == kActionViewClose || id == kActionViewUse)
            return {};
        if (id == kActionStop) return replacing().empty() ? "nothing is waiting to replace this draft" : std::string();
        if (!item_) return "open a value first: drag an Inventory entry here";
        // THE FREEZE: while a replacement waits, nothing changes the draft it will replace.
        if (id == kValueEdit || id == kActionUnset || id == kActionDiscard || id == kActionPreset ||
            id == kActionRefresh || id == kActionSample || id == kActionSave || id == kActionSaveCopy)
            if (const auto what = replacing(); !what.empty()) return frozen(what);
        if (id == kActionSave) {
            if (busy()) return "wait: " + busy_reason();
            if (linked && !dirty_) return "nothing to save: this draft matches rev " + std::to_string(saved_.revision);
            return {};
        }
        if (id == kActionSaveCopy || id == kActionPreset) return busy() ? "wait: " + busy_reason() : std::string();
        if (id == kActionRefresh) return client_.busy() ? "wait: " + busy_reason() : std::string();
        if (id == kActionWatch) {
            if (watching()) return {};
            if (op_ && op_->purpose == Operation::Purpose::link) return frozen(doing());
            if (ending_.pending()) return "wait: Workshop has not yet answered this view's last watch request";
            if (!linked) return "an independent copy has no linked entry to watch; link one (right-click Grab live)";
            if (stale_) return "this link is stale: its entry is no longer here";
            return {};
        }
        if (id == kActionSample) {
            if (busy()) return "wait: " + busy_reason();
            if (preset_) return "a preset is a draft to finish, not an observation of a source";
            if (!edited_.empty() || editing()) return "unsaved edits: save, save a copy or discard before sampling";
            return sample_route().first.empty() ? sample_route().second : std::string();
        }
        if (id == kActionViewFork) return {};
        return {};
    }

    ActResult act(const std::string& id, ViewContext& c) {
        ActResult out;
        if (id != kActionRefresh) discard_armed_ = false;
        if (id != kActionViewClose) close_armed_ = false;
        observed_.clear();
        if (renaming_) {
            if (id == kActionRenameAccept) { renaming_ = false; out.reoffer = set_title(line_.text()); line_.clear(); notice_ = "View renamed " + title_; }
            else if (id == kActionRenameCancel) { renaming_ = false; line_.clear(); }
            return out;
        }
        if (editing_) {
            if (id == kActionFieldCancel) { editing_ = false; line_.clear(); }
            else if (id == kActionFieldAccept) {
                try {
                    const auto before = item_->get(field_) ? message_draft::summary(item_->get(field_)) : std::string("absent");
                    item_->set_text(field_, line_.text());
                    touched({-1, field_});
                    editing_ = false; line_.clear();
                    notice_ = "Field changed in this draft (was " + before + "); Save stores it";
                } catch (const std::exception& e) { notice_ = e.what(); }
            }
            return out;
        }
        if (id == kActionViewRename) {
            renaming_ = true; line_.set(title_, title_.size());
            notice_ = "Name this view; Enter keeps it, Escape cancels";
            return out;
        }
        if (id == kActionPanes) {
            // THE VIEW LEAVES THE DESK'S SIGHT: a watch never runs unseen.
            stop_watch(c, "Watch ended: this view left for pane properties");
            out.leave = true;
            return out;
        }
        if (const auto why = unavailable(id); !why.empty()) {
            notice_ = label_of(id) + " unavailable: " + why;
            return out;
        }
        if (id == kActionStop) { stop_waiting(); return out; }
        if (pickup_ != Pickup::idle) { notice_ = "A field pickup is pending"; return out; }
        const auto fields = field_list();
        if (id == kValueUp || id == kValueDown) { step(fields, id == kValueUp ? -1 : 1); return out; }
        if (id == kValueEdit) { begin_edit(fields); return out; }
        if (id == kActionPreset) {
            preset_ = true; detached_ = true; dirty_ = true; sampled_ = false; ++edits_;
            preset_title_ = item_->schema()->name() + " preset"; label_.clear();
            stop_watch(c, {});
            newer_.reset(); stale_ = false; // what was held for the linked entry is not this copy's
            notice_ = "Independent preset copy; Ctrl+U unsets a field, Save stores it";
            return out;
        }
        if (id == kActionDiscard) {
            const auto decoded = inventory::decode_pair(view(saved_.pair));
            load(decoded, false);
            dirty_ = false; sampled_ = false; edited_.clear(); ++edits_;
            reselect(fields);
            notice_ = "Local edits discarded; this is the last saved/read copy";
            return out;
        }
        if (id == kActionUnset || id == kActionGrab) {
            const Field* selected = find(fields, selected_);
            if (!selected || lost_) { notice_ = "Choose a field first"; return out; }
            try {
                if (id == kActionUnset) {
                    if (!preset_ || selected->metadata)
                        throw std::invalid_argument("Make a preset copy with Ctrl+B before unsetting item fields");
                    item_->unset(selected->ref.path); touched(selected->ref);
                    notice_ = "Field unset; the schema is unchanged. Save stores the preset";
                } else {
                    begin_pickup(selected->ref, c.mail.correlation(), false, c);
                }
            } catch (const std::exception& e) { notice_ = e.what(); }
            return out;
        }
        if (id == kActionSave || id == kActionSaveCopy) { save(id == kActionSaveCopy, c); return out; }
        if (id == kActionRefresh) { refresh(c); return out; }
        if (id == kActionWatch) {
            if (watching()) stop_watch(c, "Watch paused; nothing is read until Watch is pressed again", true);
            else start_watch(c);
            return out;
        }
        if (id == kActionSample) { start_sample(c); return out; }
        return out;
    }

    // ---- answers -------------------------------------------------------------------------
    //
    // Each returns true when the answer settled a record of THIS view. The weave asks every
    // view in turn; correlations are unique across views, so at most one can match.

    bool hear(const workshop::PaneOperationAnswered& answer, ViewContext& c) {
        if (pickup_ == Pickup::permission && c.mail.answers_ask() && c.mail.correlation() == pickup_ask_) {
            if (!answer.allowed) { finish_pickup("Field pickup refused: " + answer.reason); return true; }
            pickup_ = Pickup::carry;
            pickup_ticket_ = c.mail.as_role(kInfoPaneRole).send_to_role("zengine.workshop",
                workshop::PaneValueCarryRequested{key_, pickup_label_, pickup_bytes_, pickup_drag_}, pickup_gesture_);
            if (!pickup_ticket_.valid()) finish_pickup("Field pickup could not be queued");
            return true;
        }
        if (sample_.phase == Sample::Phase::authorizing && sample_.permission.matches_answer(c.mail)) {
            sample_.permission.forget();
            if (!answer.allowed) {
                sample_.phase = Sample::Phase::idle;
                sample_.last_attempt = clock_text() + " refused: " + answer.reason;
                notice_ = "Sample refused: " + answer.reason;
                return true;
            }
            const auto correlation = ++c.asks;
            if (!sample_.ask.send_to_role(c.mail.as_role(kInfoPaneRole), sample_.role, loom::PokeDescribe{}, correlation)) {
                sample_.phase = Sample::Phase::idle;
                notice_ = "Sample could not be queued to " + sample_.role;
                return true;
            }
            sample_.phase = Sample::Phase::asking;
            sample_.since = clock_text();
            notice_.clear();
            return true;
        }
        if (!client_.hear(answer, c.mail)) return false;
        return after_client(c);
    }

    bool hear(const workshop::PaneObservationAnswered& answer, ViewContext& c) {
        if (ending_.matches_answer(c.mail)) {
            // A WATCH STOPPED BEFORE WORKSHOP ANSWERED IT: a lease granted now is ended now, and
            // nothing here starts watching again.
            ending_.forget();
            if (answer.allowed && answer.lease)
                (void)c.mail.as_role(kInfoPaneRole).send_to_role("zengine.workshop",
                    workshop::PaneObservationEnded{key_, answer.lease});
            return true;
        }
        if (!watch_.ask.matches_answer(c.mail)) return false;
        watch_.ask.forget();
        if (!answer.allowed) {
            const bool started = watch_.state == Watch::State::requesting;
            stop_watch(c, (started ? "Watch refused: " : "Watch ended: ") + answer.reason, false);
            return true;
        }
        if (watch_.state == Watch::State::requesting) {
            watch_.state = Watch::State::on;
            watch_.lease = answer.lease;
            notice_ = "Watching rev " + std::to_string(saved_.revision) + "; changes appear here, your edits are never replaced";
        }
        // THE CONTINUATION IS APPROVED: one read, under this view's own number.
        const auto correlation = ++c.asks;
        if (!watch_.read.send_to_role(c.mail.as_role(kInfoPaneRole), inventory::kInventoryRole,
                                      inventory::InventoryRead{watch_.reference}, correlation))
            stop_watch(c, "Watch ended: the read could not be queued", true);
        return true;
    }

    bool hear(const inventory::InventoryEntry& entry, ViewContext& c) {
        if (watch_.read.matches_answer(c.mail)) {
            watch_.read.forget();
            if (entry.reference.owner != watch_.reference.owner || entry.reference.entry != watch_.reference.entry) {
                stop_watch(c, "Watch ended: inventory answered about a different entry", true);
                return true;
            }
            ++watch_.observations;
            watch_.last_good = clock_text();
            watch_.last_failure.clear();
            observe(entry);
            if (watch_.again) { watch_.again = false; cycle(c); }
            return true;
        }
        if (!client_.hear(entry, c.mail)) return false;
        return after_client(c);
    }

    bool hear(const loom::Refused& refused, ViewContext& c) {
        if (watch_.read.matches_answer(c.mail)) {
            watch_.read.forget();
            watch_.last_failure = clock_text() + " " + refused.reason;
            if (refused.reason.find("no longer here") != std::string::npos) stale_ = true;
            stop_watch(c, "Watch ended: " + refused.reason + (stale_ ? " -- the draft is kept; Save copy stores it" : ""));
            return true;
        }
        if (sample_.phase == Sample::Phase::asking && sample_.ask.matches_answer(c.mail)) {
            sample_.ask.forget();
            sample_.phase = Sample::Phase::idle;
            sample_.last_attempt = clock_text() + " refused: " + refused.reason;
            notice_ = "Sample refused by " + sample_.role + ": " + refused.reason;
            return true;
        }
        if (!client_.hear(refused, c.mail)) return false;
        return after_client(c);
    }

    bool hear(const loom::DispatchRefused& refused, ViewContext& c) {
        const auto* shape = pickup_ == Pickup::permission ? workshop::PaneOperationRequested::zen_name
                                                         : workshop::PaneValueCarryRequested::zen_name;
        if (pickup_ != Pickup::idle && c.mail.dispatch_refused() && pickup_ticket_.valid() &&
            refused.refused_attempt().seq == pickup_ticket_.seq && refused.shape == shape &&
            refused.version == 1 && refused.role == "zengine.workshop" && refused.target.empty()) {
            finish_pickup("Field pickup was not delivered: " + refused.reason);
            return true;
        }
        if (ending_.matches_refusal(refused, c.mail)) { ending_.forget(); return true; } // it granted nothing
        if (watch_.ask.matches_refusal(refused, c.mail) || watch_.read.matches_refusal(refused, c.mail)) {
            // AN UNDELIVERED CONTINUATION OR READ: the lease Workshop approved is still in its book,
            // so it is ended there too. A request refused before it arrived holds no lease.
            if (watch_.state == Watch::State::requesting) watch_.ask.forget();
            watch_.read.forget();
            watch_.last_failure = clock_text() + " not delivered: " + refused.reason;
            stop_watch(c, "Watch ended: its request was not delivered (" + refused.reason + ")");
            return true;
        }
        if (sample_.permission.matches_refusal(refused, c.mail)) {
            sample_.permission.forget(); sample_.phase = Sample::Phase::idle;
            notice_ = "Sample permission request was not delivered: " + refused.reason;
            return true;
        }
        if (sample_.ask.matches_refusal(refused, c.mail)) {
            sample_.ask.forget(); sample_.phase = Sample::Phase::idle;
            sample_.unavailable = true;
            sample_.last_attempt = clock_text() + " unavailable: " + refused.reason;
            notice_ = "Source " + sample_.role + " is unavailable (" + refused.reason + "); showing " +
                      (sample_.last_good.empty() ? "the stored observation" : "the sample from " + sample_.last_good) +
                      ", now stale";
            return true;
        }
        if (!client_.hear(refused, c.mail)) return false;
        return after_client(c);
    }

    bool hear(const workshop::PaneCarryAnswered& answer, loom::Mail& mail) {
        if (pickup_ != Pickup::carry || !mail.answers_ask() || mail.correlation() != pickup_gesture_) return false;
        finish_pickup(!answer.carried ? "Field pickup refused: " + answer.reason
                      : pickup_drag_ ? std::string() : "Carrying field copy; click a receiving field, or Escape");
        return true;
    }

    bool hear(const loom::PokeStructure& structure, ViewContext& c) {
        if (sample_.phase != Sample::Phase::asking || !sample_.ask.matches_answer(c.mail)) return false;
        sample_.ask.forget();
        sample_.phase = Sample::Phase::idle;
        sample_.unavailable = false;
        const auto responder = std::to_string(c.mail.sender().value);
        const auto original = sample_route().second.empty() ? recorded_responder() : std::string();
        try {
            // A NEW OBSERVATION: the answer this delivery carried, recorded with Loom's attested
            // sender and this process's clock. Nothing stored changes until Save or Save copy.
            const auto context = inventory::observed_structure(sample_.role, c.mail.sender());
            const auto encoded = inventory::encode_pair(loom::to_value(structure), {loom::to_value(context)});
            const auto before = field_list();
            load(inventory::decode_pair(encoded), true); // the draft was frozen: nothing local is lost
            sampled_ = true; dirty_ = true; ++edits_;
            marks_from(before);
            reselect(before);
            sample_.last_good = "#" + responder + " at " + clock_text();
            // THE TENSE AND THE WHO FIRST: a narrow room cuts the tail, never the claim. A WeaveId is
            // this process's number: a different one proves another incarnation answered, an equal
            // one proves nothing (the stored capture may come from an earlier process).
            notice_ = !original.empty() && original != responder
                ? "Different incarnation: #" + responder + " answered " + sample_.role + " (stored capture: #" + original + "); unsaved"
                : "Sampled " + sample_.role + ": #" + responder + " answered now" +
                  (original.empty() ? std::string() : "; stored #" + original + " may be an earlier process") + "; unsaved";
        } catch (const std::exception& e) { notice_ = std::string("Sample could not be read: ") + e.what(); }
        return true;
    }

    /// INVENTORY SAID SOMETHING CHANGED -- an invalidation, not a value. A watching view runs one
    /// observation cycle; a trigger during a cycle is remembered once, never queued.
    void changed(ViewContext& c) {
        if (watch_.state != Watch::State::on) return;
        if (watch_.ask.pending() || watch_.read.pending()) { watch_.again = true; return; }
        cycle(c);
    }

    // ---- presentation ------------------------------------------------------------------

    std::vector<workshop::PaneActionRow> actions() const {
        using namespace input;
        if (!allocated_) return {{kActionViewUse, "use this view", scan::kReturn, mod::kNone}};
        if (editing_) return {{kActionFieldAccept, "keep field edit", scan::kReturn, mod::kNone},
                              {kActionFieldCancel, "cancel field edit", scan::kEscape, mod::kNone}};
        if (renaming_) return {{kActionRenameAccept, "rename view", scan::kReturn, mod::kNone},
                               {kActionRenameCancel, "cancel rename", scan::kEscape, mod::kNone}};
        std::vector<workshop::PaneActionRow> rows = {
            {kValueUp, "previous field", scan::kUp, mod::kNone},
            {kValueDown, "next field", scan::kDown, mod::kNone},
            {kValueEdit, "edit field", scan::kReturn, mod::kNone},
            {kActionSave, "save entry", scan::kS, mod::kCtrl},
            {kActionSaveCopy, "save a new copy", scan::kS, mod::kCtrl | mod::kShift},
            {kActionRefresh, "refresh from inventory", scan::kR, mod::kCtrl},
            {kActionWatch, "watch or pause", scan::kL, mod::kCtrl},
            {kActionSample, "sample the source", scan::kE, mod::kCtrl},
            {kActionPreset, "make independent preset copy", scan::kB, mod::kCtrl},
            {kActionUnset, "unset preset field", scan::kU, mod::kCtrl},
            {kActionGrab, "pick up typed field copy", scan::kG, mod::kCtrl},
            {kActionDiscard, "discard local edits", scan::kD, mod::kCtrl},
            {kActionViewNew, "new Info view", scan::kN, mod::kCtrl},
            {kActionViewFork, "fork this view", scan::kN, mod::kCtrl | mod::kShift},
            {kActionViewRename, "rename this view", scan::kM, mod::kCtrl},
            {kActionViewMenu, "all view actions", scan::kPeriod, mod::kCtrl}};
        if (is_default()) rows.push_back({kActionPanes, "pane properties", scan::kI, mod::kCtrl});
        else rows.push_back({kActionViewClose, "close this view", scan::kW, mod::kCtrl | mod::kShift});
        if (!replacing().empty()) rows.push_back({kActionStop, "stop waiting", scan::kEscape, mod::kNone});
        return rows;
    }
    bool answers(const std::string& id) const {
        for (const auto& row : actions()) if (row.id == id) return true;
        return false;
    }
    /// EVERY ACT THE MENU OFFERS, in the order the controls read.
    std::vector<std::pair<std::string, std::string>> menu_rows() const {
        std::vector<std::pair<std::string, std::string>> out;
        for (const auto& row : actions())
            if (row.id != kValueUp && row.id != kValueDown && row.id != kActionViewMenu)
                out.emplace_back(row.id, (row.id == kActionSample && !sample_route().first.empty()
                                              ? "Sample source " + sample_route().first : label_of(row.id)) +
                                             (unavailable(row.id).empty() ? "" : " (unavailable)"));
        return out;
    }

    /// THE PICTURE: one composition of rows and their meanings. `can_create` says whether the
    /// weave has a free slot, which only the weave knows.
    workshop::v3::PaneContent draw(bool can_create) {
        workshop::v3::PaneContent out;
        out.pane = key_;
        map_.begin();
        const auto width = columns_;
        const auto push = [&](std::string text, std::int64_t role, std::int64_t ground = surface::role::kNone) {
            if (static_cast<std::int64_t>(out.rows.size()) < rows_)
                out.rows.push_back({workshop::pane_text::drawable(workshop::pane_text::fit(std::move(text), width)), role, ground});
        };
        can_create_ = can_create;
        if (!allocated_) {
            push(default_title() + " -- unused view slot", surface::role::kAccent, surface::role::kMuted);
            controls({{kActionViewUse, "Use this view"}}, out, 1);
            push("Or drop an Inventory value here. Views close with Close; data stays in Inventory.", surface::role::kMuted);
            out.picture = map_.settle();
            return out;
        }
        push(title_ + " | " + subject_text(), surface::role::kAccent, surface::role::kMuted);
        if (rows_ >= 4) push(state_text(), attention() ? surface::role::kAlert : surface::role::kMuted);
        if (editing_ || renaming_) {
            if (!notice_.empty()) push(notice_, surface::role::kAlert);
            push(editing_ ? "Edit " + message_draft::path_label(field_) : std::string("View title"), surface::role::kFill);
            line_.keep_caret_visible(std::max<std::int64_t>(0, width - 2));
            push("> " + line_.visible(std::max<std::int64_t>(0, width - 2)), surface::role::kAccent);
            push("Enter keeps it; Escape cancels", surface::role::kMuted);
            out.picture = map_.settle();
            return out;
        }
        const std::int64_t control_rows = rows_ >= 10 ? 2 : 1;
        controls(control_list(), out, control_rows);
        if (!notice_.empty()) push(notice_, surface::role::kAlert);
        if (rows_ >= 12) {
            const auto detail = detail_text();
            if (!detail.empty()) push(detail, surface::role::kMuted);
        }
        const auto fields = field_list();
        if (!item_) {
            push("Drag an Inventory entry here for a copy; right-click an entry, Grab live, then click here to link it.",
                 surface::role::kMuted);
            out.picture = map_.settle();
            return out;
        }
        const auto budget = static_cast<std::size_t>(std::max<std::int64_t>(0, rows_ - static_cast<std::int64_t>(out.rows.size())));
        const std::size_t at = selected_index(fields);
        const auto window = component::cursor_window(fields.size(), at, hint_, budget);
        hint_ = window.first;
        if (window.before && window.marker_rows())
            push(workshop::pane_text::omitted_text(window.before, "earlier"), surface::role::kMuted);
        for (std::size_t i = window.first; i < window.end(); ++i) {
            const auto& f = fields[i];
            const bool here = !lost_ && f.ref == selected_;
            const char mark = edited(f.ref) ? '*' : observed(f.ref) ? '~' : ' ';
            map_.row(static_cast<std::int64_t>(out.rows.size()), Meaning{Meaning::kField, {}, f.ref});
            push(std::string(here ? ">" : " ") + mark + f.label + ": " + f.row.summary +
                     (!f.row.present && f.row.required ? " (required)" : ""),
                 f.metadata ? surface::role::kMuted : surface::role::kFill);
        }
        if (window.after && window.marker_rows())
            push(workshop::pane_text::omitted_text(window.after, "more"), surface::role::kMuted);
        out.picture = map_.settle();
        return out;
    }

    /// THE MEANING UNDER A PLACE, for the weave's menu placement and tests.
    const Meaning* meaning(std::int64_t row, std::int64_t column) const { return map_.at(row, column); }
    std::int64_t picture() const noexcept { return map_.picture(); }
    static std::string label_of(const std::string& id) {
        static const std::vector<std::pair<std::string, std::string>> names = {
            {kActionSave, "Save"}, {kActionSaveCopy, "Save copy"}, {kActionRefresh, "Refresh"},
            {kActionWatch, "Watch"}, {kActionSample, "Sample source"}, {kActionPreset, "Make preset copy"},
            {kActionUnset, "Unset field"}, {kActionGrab, "Pick up field"}, {kActionDiscard, "Discard edits"},
            {kActionViewNew, "New view"}, {kActionViewFork, "Fork view"}, {kActionViewRename, "Rename view"},
            {kActionViewClose, "Close view"}, {kActionViewMenu, "Actions"}, {kActionPanes, "Pane properties"},
            {kValueEdit, "Edit field"}, {kActionViewUse, "Use this view"}, {kActionStop, "Stop waiting"}};
        for (const auto& [key, name] : names) if (key == id) return name;
        return id;
    }
    bool close_armed() const noexcept { return close_armed_; }
    void arm_close() { close_armed_ = true; }

    // Evidence doors for loaded tests: read-only views of what this view holds.
    std::int64_t lease() const noexcept { return watch_.lease; }
    std::uint64_t observations() const noexcept { return watch_.observations; }
    bool cycle_pending() const noexcept { return watch_.ask.pending() || watch_.read.pending(); }
    bool again() const noexcept { return watch_.again; }
    std::int64_t base_revision() const noexcept { return saved_.revision; }
    const std::optional<message_draft::Draft>& draft() const noexcept { return item_; }

private:
    /// WHAT THE ONE OWNER OPERATION IN FLIGHT IS FOR. Set when it begins and settled on every
    /// outcome in `after_client`, so a read, write, add or link never inherits another's meaning.
    struct Operation {
        enum class Purpose { refresh, link, save, save_new, save_copy } purpose = Purpose::refresh;
        std::uint64_t sent_edit = 0; ///< the edit generation a save covers
        std::string label;           ///< the entry's name: a link's summary, a copy's new label
    };
    enum class Pickup { idle, permission, carry };
    struct Field {
        message_draft::Row row;
        std::string label;
        bool metadata = false;
        FieldRef ref;
    };
    struct Watch {
        enum class State { off, requesting, on } state = State::off;
        std::int64_t lease = 0;
        std::string subject;
        inventory::InventoryReference reference;
        loom::RoleRequest ask;  ///< PaneObservationRequested, then each PaneObservationContinued
        loom::RoleRequest read; ///< the approved InventoryRead
        bool again = false;     ///< a change during a cycle: one more cycle, never a queue
        std::uint64_t observations = 0;
        std::string last_good, last_failure;
    };
    struct Sample {
        enum class Phase { idle, authorizing, asking } phase = Phase::idle;
        std::string role;
        loom::RoleRequest permission; ///< PaneOperationRequested for zen.PokeDescribe at `role`
        loom::RoleRequest ask;        ///< the PokeDescribe itself
        bool unavailable = false;
        std::string since, last_good, last_attempt, original;
    };
    struct Press {
        bool armed = false, started = false;
        std::int64_t row = 0, column = 0;
        std::uint64_t gesture = 0;
        FieldRef field;
    };

    static std::string_view view(const loom::Bytes& bytes) {
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }

    std::vector<Field> field_list() const {
        std::vector<Field> result;
        if (item_) for (auto row : item_->rows()) {
            auto label = row.label;
            FieldRef ref{-1, row.path};
            result.push_back({std::move(row), std::move(label), false, std::move(ref)});
        }
        for (std::size_t i = 0; i < metadata_.size(); ++i)
            for (auto row : message_draft::Draft(metadata_[i]).rows()) {
                auto label = "meta[" + std::to_string(i) + "]." + row.label;
                FieldRef ref{static_cast<std::int64_t>(i), row.path};
                result.push_back({std::move(row), std::move(label), true, std::move(ref)});
            }
        return result;
    }
    static const Field* find(const std::vector<Field>& fields, const FieldRef& ref) {
        for (const auto& f : fields) if (f.ref == ref) return &f;
        return nullptr;
    }
    std::size_t selected_index(const std::vector<Field>& fields) const {
        for (std::size_t i = 0; i < fields.size(); ++i) if (!lost_ && fields[i].ref == selected_) return i;
        return std::min(hint_, fields.empty() ? std::size_t(0) : fields.size() - 1);
    }
    void select_first() {
        const auto fields = field_list();
        selected_ = fields.empty() ? FieldRef{} : fields.front().ref;
        lost_ = false; hint_ = 0;
    }
    /// KEEP THE SELECTED FIELD BY ITS PATH ACROSS NEW DATA. A field the new data lacks is LOST,
    /// never silently the row that now sits at its number.
    void reselect(const std::vector<Field>&) {
        const auto fields = field_list();
        if (!find(fields, selected_)) lost_ = !fields.empty() && !(selected_ == FieldRef{});
        if (selected_ == FieldRef{} && !fields.empty()) select_first();
    }
    void step(const std::vector<Field>& fields, int by) {
        if (fields.empty()) return;
        std::size_t at = selected_index(fields);
        if (!lost_) {
            if (by < 0 && at > 0) --at;
            else if (by > 0 && at + 1 < fields.size()) ++at;
        }
        selected_ = fields[at].ref; lost_ = false;
    }
    void begin_edit(const std::vector<Field>& fields) {
        const Field* f = find(fields, selected_);
        if (!f || lost_) { notice_ = lost_ ? "The selected field is no longer here; choose a field" : "Choose a field first"; return; }
        if (f->metadata || f->row.type.kind == loom::Kind::Message || f->row.type.kind == loom::Kind::List ||
            f->row.type.kind == loom::Kind::Bytes) {
            notice_ = f->metadata ? "Capture metadata is read-only"
                                  : "Select a scalar field inside this value; byte fields are read-only here";
            return;
        }
        field_ = f->ref.path;
        line_.set(f->row.present ? f->row.summary : "", f->row.summary.size());
        editing_ = true;
    }
    void touched(const FieldRef& ref) {
        edited_.push_back(ref);
        dirty_ = true; ++edits_;
    }
    bool edited(const FieldRef& ref) const {
        return std::any_of(edited_.begin(), edited_.end(), [&](const FieldRef& e) { return ref.within(e); });
    }
    bool observed(const FieldRef& ref) const {
        return std::find(observed_.begin(), observed_.end(), ref) != observed_.end();
    }
    /// MARK WHAT AN OBSERVATION CHANGED against what the view showed before it.
    void marks_from(const std::vector<Field>& before) {
        observed_.clear();
        for (const auto& now : field_list()) {
            const Field* was = find(before, now.ref);
            if (!was || was->row.summary != now.row.summary || was->row.present != now.row.present)
                observed_.push_back(now.ref);
        }
    }

    void load(const inventory::DecodedPair& pair, bool reset_marks) {
        const auto stored = message_draft::is_stored_draft(pair.item)
            ? std::optional(message_draft::read_draft(pair.item)) : std::nullopt;
        auto draft = stored ? stored->draft : message_draft::Draft(pair.item);
        item_ = std::move(draft); preset_ = stored.has_value();
        preset_title_ = stored ? stored->title : "";
        metadata_ = pair.metadata;
        if (reset_marks) { edited_.clear(); observed_.clear(); }
    }

    /// A NEWER REVISION OF THE LINKED ENTRY ARRIVED. A clean, idle draft adopts it and marks what
    /// changed; a draft with edits keeps every character and only says newer data is waiting.
    /// The base revision advances only with the data it describes.
    void observe(const inventory::InventoryEntry& entry) {
        if (entry.revision <= saved_.revision) return;
        if (dirty_ || editing() || client_.busy()) {
            newer_ = entry;
            return;
        }
        adopt(entry, true);
    }
    void adopt(const inventory::InventoryEntry& entry, bool mark) {
        try {
            const auto before = field_list();
            load(inventory::decode_pair(view(entry.pair)), true);
            saved_ = entry; dirty_ = false; sampled_ = false; detached_ = false; stale_ = false; ++edits_;
            if (mark) marks_from(before);
            reselect(before);
            if (newer_ && newer_->revision <= saved_.revision) newer_.reset();
        } catch (const std::exception& e) { notice_ = std::string("New data could not be read: ") + e.what(); }
    }

    /// BEGIN ONE OWNER OPERATION FOR ONE PURPOSE. Its record exists exactly while the client is
    /// busy, so no later operation can find this one's meaning left behind.
    template <class Request>
    bool begin(Operation::Purpose purpose, Request request, ViewContext& c, std::string label = {}) {
        const bool begun = client_.begin(std::move(request), key_, kInfoPaneRole, c.mail, c.asks);
        if (begun) op_ = Operation{purpose, edits_, std::move(label)};
        notice_ = client_.notice;
        return begun;
    }
    /// THE CLIENT HEARD AN ANSWER TO THIS VIEW'S OPERATION. Still in flight, it says where it is.
    /// Over -- answered, refused by the owner, denied, undelivered or never queued -- its record is
    /// settled here, once, before anything is applied.
    bool after_client(ViewContext& c) {
        if (client_.busy() || !op_) { notice_ = client_.notice; return true; }
        const Operation op = std::move(*op_);
        op_.reset();
        if (!client_.result) { failed(op, client_.notice); return true; }
        const auto result = std::move(*client_.result);
        client_.result.reset();
        succeeded(op, result, c);
        return true;
    }

    /// WHAT AN OPERATION THAT HAPPENED DOES TO THIS VIEW, by what it was for.
    void succeeded(const Operation& op, const inventory::InventoryEntry& result, ViewContext& c) {
        try {
            switch (op.purpose) {
            case Operation::Purpose::save_copy:
                // A NEW INDEPENDENT ENTRY. This view keeps its link, its draft and its base.
                notice_ = "Saved a new entry '" + op.label + "'; this view still holds " +
                          (detached_ ? "its independent copy" : "its link to rev " + std::to_string(saved_.revision));
                return;
            case Operation::Purpose::save:
            case Operation::Purpose::save_new: {
                // THE WRITE COVERED WHAT IT SENT: the base advances to it (a copy now links its new
                // entry). The draft takes the stored value only if nothing was edited since the send,
                // and an edit opened meanwhile stays open over it.
                const bool newer = edits_ != op.sent_edit;
                if (op.purpose == Operation::Purpose::save_new) label_ = op.label;
                const auto before = field_list();
                if (!newer) load(inventory::decode_pair(view(result.pair)), true);
                saved_ = result; detached_ = false; stale_ = false; dirty_ = newer;
                sampled_ = false; // a sample it covered is stored now, whatever was typed since
                if (!newer) reselect(before);
                if (newer_ && newer_->revision <= saved_.revision) newer_.reset();
                notice_ = newer ? "Saved the submitted draft as rev " + std::to_string(result.revision) + "; newer edits remain unsaved"
                        : op.purpose == Operation::Purpose::save_new ? std::string("Saved as a new inventory entry; this view now links it")
                        : "Saved rev " + std::to_string(result.revision);
                return;
            }
            case Operation::Purpose::refresh: {
                // THE CONFIRMED DISCARD, NOW: the draft was frozen, so this read is what replaces it.
                const auto before = field_list();
                load(inventory::decode_pair(view(result.pair)), true);
                saved_ = result; dirty_ = false; sampled_ = false; stale_ = false;
                marks_from(before); reselect(before);
                if (newer_ && newer_->revision <= saved_.revision) newer_.reset();
                notice_ = "Read rev " + std::to_string(result.revision) + " from inventory";
                return;
            }
            case Operation::Purpose::link: {
                // THE SUBJECT CHANGES HERE, WHOLE: entry, revision, metadata, label and selection --
                // and the old subject's watch ends, since it observed what this view no longer shows.
                const bool watched = watching();
                stop_watch(c, {});
                load(inventory::decode_pair(view(result.pair)), true);
                saved_ = result; detached_ = false; dirty_ = false; label_ = op.label;
                forget_subject();
                select_first();
                notice_ = "Linked rev " + std::to_string(result.revision) +
                          (watched ? "; the watch on the entry shown before ended" : "");
                return;
            }
            }
        } catch (const std::exception& e) { notice_ = e.what(); }
    }
    /// AN OPERATION THAT DID NOT HAPPEN -- refused by the owner, not permitted to the actor, never
    /// delivered or never queued. The draft, its base, its link and its watch stay as they were.
    void failed(const Operation& op, const std::string& reason) {
        const bool gone = reason.find("no longer here") != std::string::npos;
        switch (op.purpose) {
        case Operation::Purpose::refresh:
            if (gone) stale_ = true; // this view's own entry left
            notice_ = "Read refused: " + reason + "; the draft is kept";
            return;
        case Operation::Purpose::link:
            notice_ = "Link refused: " + reason + "; this view still shows what it showed";
            return;
        case Operation::Purpose::save:
        case Operation::Purpose::save_new:
            if (gone) stale_ = true;
            notice_ = "Save refused: " + reason;
            if (reason.find("changed") != std::string::npos)
                notice_ += " -- your text is kept. Refresh replaces it; Save copy stores it as a new entry";
            return;
        case Operation::Purpose::save_copy:
            notice_ = "Save copy refused: " + reason + "; nothing was stored";
            return;
        }
    }

    void save(bool copy, ViewContext& c) {
        try {
            const auto item = preset_ ? message_draft::store_draft(preset_title_, *item_) : item_->snapshot();
            const auto encoded = inventory::encode_pair(item, metadata_);
            const loom::Bytes bytes(encoded.begin(), encoded.end());
            if (copy) {
                std::string label = preset_ ? preset_title_ : label_.empty() ? item_->schema()->name() : label_;
                if (label.size() > 75) label.resize(75);
                label += " copy";
                begin(Operation::Purpose::save_copy, inventory::InventoryAdd{bytes, label}, c, label);
            } else if (detached_) {
                const std::string label = preset_ ? preset_title_ : "";
                begin(Operation::Purpose::save_new, inventory::InventoryAdd{bytes, label}, c, label);
            } else {
                begin(Operation::Purpose::save, inventory::InventoryWrite{saved_.reference, saved_.revision, bytes}, c);
            }
        } catch (const std::exception& e) { notice_ = e.what(); }
    }

    void refresh(ViewContext& c) {
        if ((dirty_ || editing_) && !discard_armed_) {
            discard_armed_ = true;
            notice_ = "Unsaved draft: press Refresh again to discard it and read the entry again";
            return;
        }
        discard_armed_ = false;
        if (detached_) {
            const auto decoded = inventory::decode_pair(view(saved_.pair));
            const auto before = field_list();
            load(decoded, true); dirty_ = false; sampled_ = false; editing_ = false; ++edits_;
            reselect(before);
            notice_ = "Restored the received copy; the source entry is independent";
            return;
        }
        editing_ = false;
        // AN EXPLICIT REFRESH IS WHERE HELD NEWER DATA IS ACCEPTED -- when its read answers. Until
        // then the draft stays exactly as it was, dirty included and frozen, so a refused read or a
        // stopped wait loses nothing.
        begin(Operation::Purpose::refresh, inventory::InventoryRead{saved_.reference}, c);
    }

    /// STOP WAITING for the replacement in flight: forget its records, keep the draft exactly as
    /// it is. Not a verdict on the owner or the source, which may still answer; that answer is
    /// ignored. A save is never stopped here: a delivered write cannot be recalled.
    void stop_waiting() {
        std::string what = replacing();
        if (op_ && !saving()) { client_.abandon(); op_.reset(); }
        if (sample_.phase != Sample::Phase::idle) {
            sample_.permission.forget(); sample_.ask.forget();
            sample_.phase = Sample::Phase::idle;
            sample_.last_attempt = clock_text() + " stopped waiting";
        }
        notice_ = "Stopped waiting (" + what + "); this draft is unchanged, and a late answer is ignored";
    }
    /// THE SUBJECT CHANGED: what was known about the old one -- a stale link, newer data held for
    /// it, a sample's history -- is not about this one.
    void forget_subject() {
        stale_ = false; sampled_ = false; newer_.reset();
        sample_ = Sample{};
    }
    static std::string frozen(const std::string& what) {
        return "wait: this view is " + what + " -- Stop keeps this draft";
    }
    /// WHAT THE OWNER OPERATION IN FLIGHT IS DOING, in words.
    std::string doing() const {
        if (!op_) return {};
        switch (op_->purpose) {
        case Operation::Purpose::refresh: return "reading its entry again";
        case Operation::Purpose::link: return op_->label.empty() ? "opening another entry" : "opening '" + op_->label + "'";
        case Operation::Purpose::save: case Operation::Purpose::save_new: return "saving";
        case Operation::Purpose::save_copy: return "saving a copy";
        }
        return {};
    }

    // ---- watch ---------------------------------------------------------------------------

    std::string subject() const { return saved_.reference.owner + ":" + saved_.reference.entry; }
    void start_watch(ViewContext& c) {
        // ONE OBSERVATION REQUEST PER SLOT, live or being ended, so the book stays one record deep.
        if (watch_.ask.pending() || ending_.pending()) { notice_ = "Watch is already being approved"; return; }
        watch_ = Watch{};
        watch_.reference = saved_.reference;
        watch_.subject = subject();
        const auto correlation = ++c.asks;
        const bool queued = watch_.ask.send_to_role(c.mail.as_role(kInfoPaneRole), "zengine.workshop",
            workshop::PaneObservationRequested{key_, inventory::kInventoryRole, inventory::InventoryRead::zen_name,
                inventory::InventoryRead::zen_version, static_cast<std::int64_t>(c.mail.correlation()), watch_.subject},
            correlation);
        if (!queued) { watch_ = Watch{}; notice_ = "Watch request could not be queued"; return; }
        watch_.state = Watch::State::requesting;
        notice_ = "Asking to watch this entry";
    }
    void cycle(ViewContext& c) {
        const auto correlation = ++c.asks;
        if (!watch_.ask.send_to_role(c.mail.as_role(kInfoPaneRole), "zengine.workshop",
                workshop::PaneObservationContinued{key_, watch_.lease, watch_.subject}, correlation))
            stop_watch(c, "Watch ended: its observation could not be queued", true);
    }
    /// END THE WATCH, AT WORKSHOP TOO. A known lease is ended now; a request Workshop has not yet
    /// answered moves to `ending_`, so a lease it grants later is ended on arrival and restarts
    /// nothing. Forgetting the read here is what keeps a late answer off whatever comes next.
    /// `tell` is false only where Workshop has already forgotten the lease (it refused).
    void stop_watch(ViewContext& c, std::string reason, bool tell = true) {
        if (watch_.state == Watch::State::off && !watch_.ask.pending()) return;
        if (watch_.state == Watch::State::requesting && watch_.ask.pending()) ending_ = watch_.ask;
        else if (tell && watch_.lease)
            (void)c.mail.as_role(kInfoPaneRole).send_to_role("zengine.workshop",
                workshop::PaneObservationEnded{key_, watch_.lease});
        const auto kept_good = watch_.last_good, kept_failure = watch_.last_failure;
        watch_ = Watch{};
        watch_.last_good = kept_good; watch_.last_failure = kept_failure;
        if (!reason.empty()) notice_ = std::move(reason);
    }

    // ---- sample --------------------------------------------------------------------------

    /// THE ONE SUPPORTED ROUTE, or why there is none: {role, ""} or {"", reason}.
    std::pair<std::string, std::string> sample_route() const {
        if (const auto ctx = inventory::recorded_structure_observation(metadata_)) return {ctx->requested_role, {}};
        for (const auto& meta : metadata_) {
            const auto& name = meta.schema().name();
            if (name == "TerminalCaptureFacts")
                return {{}, "this value came from a Terminal transcript; sampling never re-runs a command"};
            if (name == "ComposerCommandContext")
                return {{}, "this is a stored command; sampling never runs commands"};
        }
        return {{}, "no supported source is recorded (only captured structure descriptions can be sampled again)"};
    }
    std::string recorded_responder() const {
        const auto ctx = inventory::recorded_structure_observation(metadata_);
        return ctx ? ctx->answered_by : std::string();
    }
    void start_sample(ViewContext& c) {
        const auto route = sample_route();
        sample_.role = route.first;
        sample_.original = recorded_responder();
        const auto correlation = ++c.asks;
        if (!sample_.permission.send_to_role(c.mail.as_role(kInfoPaneRole), "zengine.workshop",
                workshop::PaneOperationRequested{key_, sample_.role, loom::PokeDescribe::zen_name,
                    loom::PokeDescribe::zen_version, static_cast<std::int64_t>(c.mail.correlation())}, correlation)) {
            notice_ = "Sample permission request could not be queued";
            return;
        }
        sample_.phase = Sample::Phase::authorizing;
        notice_ = "Checking authority to ask " + sample_.role + " to describe its structure again";
    }

    // ---- field pickup --------------------------------------------------------------------

    void begin_pickup(const FieldRef& ref, std::uint64_t gesture, bool drag, ViewContext& c) {
        try {
            if (client_.busy()) throw std::invalid_argument("Wait for the inventory operation before picking up a field");
            const auto fields = field_list();
            const Field* f = find(fields, ref);
            if (!f) throw std::invalid_argument("That field is no longer here");
            const auto value = f->metadata
                ? message_draft::grab_field(message_draft::Draft(metadata_[static_cast<std::size_t>(ref.metadata)]), ref.path)
                : message_draft::grab_field(*item_, ref.path);
            const auto encoded = inventory::encode_pair(value, {});
            if (encoded.size() > 65536) throw std::invalid_argument("Field copy exceeds the carry limit");
            pickup_bytes_.assign(encoded.begin(), encoded.end());
            pickup_label_ = (title_ + ": " + f->label).substr(0, 128);
            pickup_gesture_ = gesture; pickup_ask_ = ++c.asks; pickup_drag_ = drag;
            pickup_ = Pickup::permission;
            pickup_ticket_ = c.mail.as_role(kInfoPaneRole).send_to_role("zengine.workshop",
                workshop::PaneOperationRequested{key_, "zengine.workshop",
                    workshop::PaneValueCarryRequested::zen_name, 1, static_cast<std::int64_t>(pickup_gesture_)}, pickup_ask_);
            if (!drag) notice_ = "Checking field acquisition authority";
            if (!pickup_ticket_.valid()) finish_pickup("Field permission request could not be queued");
        } catch (const std::exception& e) { notice_ = e.what(); }
    }
    void finish_pickup(std::string notice) {
        pickup_ = Pickup::idle; pickup_ticket_ = {}; pickup_bytes_.clear();
        if (!notice.empty()) notice_ = std::move(notice);
    }

    /// A TYPED FIELD COPY DROPPED ON THIS VIEW: fill the field row under the drop, or refuse
    /// with the draft exactly as it was.
    void fill_field(const workshop::PaneValueDrop& drop, const loom::Value& value) {
        if (!item_) { notice_ = "Open a value here before dropping a field into it"; return; }
        if (!map_.current(drop.picture)) { notice_ = "This view changed during the drag; nothing was filled -- drop again"; return; }
        if (editing()) { notice_ = "Finish the open edit before filling a field"; return; }
        if (const auto what = replacing(); !what.empty()) {
            notice_ = "Field drop refused, nothing changed: " + frozen(what);
            return;
        }
        const Meaning* at = map_.at(drop.row, drop.column);
        if (!at || at->kind != Meaning::kField) { notice_ = "Drop a field onto a field row of this view"; return; }
        if (at->field.metadata >= 0) { notice_ = "Capture metadata is read-only; drop onto an item field"; return; }
        try {
            const auto field = message_draft::read_field(value);
            message_draft::require_type(item_->type(at->field.path), field.type);
            const auto* before = item_->get(at->field.path);
            const auto was = before ? message_draft::summary(before) : std::string("absent");
            item_->set(at->field.path, field.cell);
            touched(at->field);
            selected_ = at->field; lost_ = false;
            notice_ = "Filled " + message_draft::path_label(at->field.path) + " (was " + was + "); Save stores it";
        } catch (const std::exception& e) {
            notice_ = std::string("Field drop refused, nothing changed: ") + e.what();
        }
    }

    // ---- composition ---------------------------------------------------------------------

    struct ControlItem { std::string id; std::string label; };
    std::vector<ControlItem> control_list() const {
        std::vector<ControlItem> out;
        // WHILE A REPLACEMENT WAITS, the way out of waiting comes first: it is the one act that
        // matters until the answer arrives, and a narrow room must not wrap it away.
        if (!replacing().empty()) out.push_back({kActionStop, "Stop"});
        for (const ControlItem& item : std::vector<ControlItem>{{kActionSave, "Save"}, {kActionSaveCopy, "Save copy"},
                                                                {kActionRefresh, "Refresh"},
                                                                {kActionWatch, watching() ? "Pause" : "Watch"},
                                                                {kActionViewNew, "New"}, {kActionViewFork, "Fork"},
                                                                {kActionSample, "Sample"}, {kActionViewRename, "Rename"}})
            out.push_back(item);
        if (!is_default()) out.push_back({kActionViewClose, "Close"});
        else out.push_back({kActionPanes, "Panes"});
        return out;
    }
    bool control_available(const std::string& id) const {
        if (id == kActionViewNew) return can_create_;
        if (id == kActionViewFork) return can_create_ && item_.has_value();
        return unavailable(id).empty();
    }
    /// LAY OUT CONTROLS GREEDILY OVER `lines` ROWS, keeping `[...]` last so every act stays one
    /// press away. A control the width cannot hold whole is not drawn, so it is never a target.
    void controls(const std::vector<ControlItem>& items, workshop::v3::PaneContent& out, std::int64_t lines) {
        const bool menu = items.size() > 1;
        const std::int64_t reserve = menu ? 6 : 0; // room for " [...]" on whichever line is last
        std::size_t next = 0;
        std::int64_t last_row = -1;
        std::string last_text;
        for (std::int64_t line = 0; line < lines && static_cast<std::int64_t>(out.rows.size()) < rows_; ++line) {
            std::string text;
            const auto row = static_cast<std::int64_t>(out.rows.size());
            while (next < items.size()) {
                const bool on = control_available(items[next].id);
                std::string word = on ? "[" + items[next].label + "]" : "(" + items[next].label + ")";
                const auto start = static_cast<std::int64_t>(text.size()) + (text.empty() ? 0 : 1);
                if (start + static_cast<std::int64_t>(word.size()) > columns_ - reserve) break;
                if (!text.empty()) text += ' ';
                map_.span(row, start, static_cast<std::int64_t>(word.size()), columns_, Meaning{Meaning::kControl, items[next].id, {}});
                text += word;
                ++next;
            }
            out.rows.push_back({workshop::pane_text::drawable(text), surface::role::kFill, surface::role::kNone});
            last_row = row; last_text = text;
            if (next >= items.size()) break;
        }
        if (menu && last_row >= 0) {
            // EVERY ACT, ONE PRESS AWAY: the menu lists what the width could not show.
            const auto start = static_cast<std::int64_t>(last_text.size()) + (last_text.empty() ? 0 : 1);
            if (start + 5 <= columns_) {
                if (!last_text.empty()) last_text += ' ';
                map_.span(last_row, start, 5, columns_, Meaning{Meaning::kControl, kActionViewMenu, {}});
                last_text += "[...]";
                out.rows[static_cast<std::size_t>(last_row)].text = workshop::pane_text::drawable(last_text);
            }
        }
    }
    std::string subject_text() const {
        if (!item_) return "empty";
        std::string mode = detached_ ? "COPY" : stale_ ? "LINK STALE" : "LINKED";
        if (preset_) mode = "PRESET " + mode;
        std::string what = item_->schema()->name() + " v" + std::to_string(item_->schema()->version());
        if (!label_.empty() && !detached_) what = "'" + label_ + "' " + what;
        return mode + " " + what;
    }
    bool attention() const {
        return newer_.has_value() || stale_ || sample_.unavailable || (dirty_ && !sampled_);
    }
    std::string state_text() const {
        std::vector<std::string> parts;
        if (op_) parts.push_back(doing() + (client_.phase == inventory::PaneClient::Phase::authorizing
            ? ": checking authority" : "..."));
        if (pickup_ != Pickup::idle) parts.push_back("picking up field");
        if (sample_.phase == Sample::Phase::authorizing) parts.push_back("sample: checking authority");
        if (sample_.phase == Sample::Phase::asking) parts.push_back("sampling " + sample_.role + " since " + sample_.since);
        if (!item_) parts.push_back("no value");
        else if (sampled_ && edited_.empty()) parts.push_back("UNSAVED SAMPLE");
        else if (dirty_) parts.push_back("UNSAVED " + std::to_string(std::max<std::size_t>(1, edited_.size())) + " edit(s)");
        else if (detached_) parts.push_back("not stored");
        else parts.push_back("saved rev " + std::to_string(saved_.revision));
        if (newer_) parts.push_back("NEWER rev " + std::to_string(newer_->revision) + " waiting (Refresh accepts)");
        if (item_ && !detached_) {
            if (watch_.state == Watch::State::on) parts.push_back("watch ON" + (watch_.last_good.empty() ? std::string() : " read " + watch_.last_good));
            else if (watch_.state == Watch::State::requesting) parts.push_back("watch: asking");
            else parts.push_back("watch off");
        }
        if (stale_) parts.push_back("entry no longer here");
        if (sample_.unavailable) parts.push_back("source unavailable");
        std::string out;
        for (const auto& p : parts) out += (out.empty() ? "" : " | ") + p;
        return out;
    }
    std::string detail_text() const {
        std::vector<std::string> parts;
        if (item_ && !detached_ && !saved_.reference.entry.empty())
            parts.push_back("entry " + saved_.reference.entry.substr(0, 8) + " rev " + std::to_string(saved_.revision));
        if (item_) {
            const auto route = sample_route();
            if (!route.first.empty()) {
                std::string text = "source " + route.first;
                if (const auto ctx = inventory::recorded_structure_observation(metadata_)) text += " (answered by #" + ctx->answered_by + ")";
                parts.push_back(text);
            }
        }
        if (!sample_.last_good.empty()) parts.push_back("last sample " + sample_.last_good);
        if (!sample_.last_attempt.empty()) parts.push_back("latest sample attempt " + sample_.last_attempt);
        if (!watch_.last_failure.empty()) parts.push_back("last watch attempt " + watch_.last_failure);
        std::string out;
        for (const auto& p : parts) out += (out.empty() ? "" : " | ") + p;
        return out;
    }

    std::string key_;
    std::size_t slot_ = 1;
    std::string title_;
    bool allocated_ = false, on_desk_ = false, can_create_ = false;
    std::uint64_t incarnation_ = 0;
    std::int64_t rows_ = 0, columns_ = 0;
    bool granted_ = false;
    component::RowMap<Meaning> map_;
    std::string notice_;

    inventory::PaneClient client_;
    std::optional<Operation> op_; ///< present exactly while `client_` is busy
    std::optional<message_draft::Draft> item_;
    std::vector<loom::Value> metadata_;
    inventory::InventoryEntry saved_;
    std::optional<inventory::InventoryEntry> newer_;
    std::string label_, preset_title_;
    bool preset_ = false, detached_ = false, dirty_ = false, sampled_ = false, stale_ = false;
    bool editing_ = false, renaming_ = false, discard_armed_ = false, close_armed_ = false;
    std::vector<FieldRef> edited_, observed_;
    FieldRef selected_;
    bool lost_ = false;
    std::size_t hint_ = 0;
    double wheel_ = 0.0;
    message_draft::Path field_;
    component::TextBox line_;
    component::Clipboard clipboard_;
    std::uint64_t edits_ = 0;

    Pickup pickup_ = Pickup::idle;
    loom::Ticket pickup_ticket_;
    std::uint64_t pickup_ask_ = 0, pickup_gesture_ = 0;
    bool pickup_drag_ = false;
    loom::Bytes pickup_bytes_;
    std::string pickup_label_;
    Press press_;
    Watch watch_;
    /// A WATCH REQUEST STOPPED BEFORE WORKSHOP ANSWERED IT. Kept by the slot across retirement,
    /// at most one: its approval, if any, is ended on arrival (`hear(PaneObservationAnswered)`).
    loom::RoleRequest ending_;
    Sample sample_;
};

} // namespace zengine::info_pane
#endif
