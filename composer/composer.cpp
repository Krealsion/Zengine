// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A schema-directed message form. Workshop supplies attributed gestures; copied values
// remain data. Submission checks that gesture's actor against the exact destination shape.

#include "command_context.hpp"
#include "draft.hpp"
#include "view.hpp"
#include "vocabulary.hpp"
#include "workshop/setup_control.hpp"

#include "activation/activation.hpp"
#include "input/vocabulary.hpp"
#include "introspection/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_facets.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_escape.hpp"
#include "inventory/codec.hpp"
#include "inventory/pane_client.hpp"
#include "manual/vocabulary.hpp"
#include "message-draft/library.hpp"
#include "manuals/zengine.composer.hpp" // this pane's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/terminal/composer.hpp>
#include <zen/value.hpp>
#include <zen/weave.hpp>
#include <zen/weave/describe.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/// A target's answer to `zen.DescribeAccepted`, heard as the value the gate admitted.
ZENGINE_VALUE_DOOR(::zengine::RawValueDoor<&::loom::accepted_shapes_schema>)

namespace {

namespace ws = zengine::workshop;
namespace inv = zengine::inventory;
namespace surface = zengine::surface;
namespace input = zengine::input;
using zengine::composer::kComposePane;
using zengine::composer::kComposePaneName;
using zengine::composer::kComposePaneSummary;
using zengine::composer::kComposerRole;
using zengine::composer::MessageDraft;
using zengine::composer::RowMeaning;
using zengine::composer::Snapshot;
using zengine::introspection::kIntrospectionRole;
using zengine::introspection::LoadedSelected;
using zengine::workshop::PaneKey;
using zengine::workshop::v2::PaneOffered;
using zengine::workshop::PaneTextInput;
namespace stage = zengine::composer::stage;
namespace meaning = zengine::composer::meaning;

constexpr const char* kWorkshopRole = "zengine.workshop";

using zengine::composer::ComposerCommandContext;
using AcceptedShapes = zengine::RawValueDoor<&loom::accepted_shapes_schema>;

/// ONE FIELD OF THE OPEN FORM, at its place in the shape: whether it is present, and a typed
/// field's text as the weaver left it, valid or not.
struct ComposerFieldState {
    bool present = false;
    std::string text;
    ZEN_SHAPE(ComposerFieldState, 1, ZEN_FIELD(present), ZEN_FIELD(text));
};

/// The counters, and what a reload in place keeps: the target (the library pressed and the office
/// addressed) and the form open on it -- its message by name and version, each field's presence
/// and text, the values only a drop gives (a message, bytes, a list) as a draft library, and the
/// cursor -- and what the reload ended before it was answered, a submission or a store. Never the
/// target's accepted shapes, which are asked again, nor a paste in flight.
struct ComposerState {
    std::int64_t offers = 0;
    std::int64_t rooms = 0;
    std::int64_t targets = 0;   ///< usable targets taken from a Loaded selection
    std::int64_t asked = 0;     ///< zen.DescribeAccepted requests submitted
    std::int64_t described = 0; ///< answers that became a decoded snapshot
    std::int64_t refused = 0;   ///< keys, text and selections not authored by their owner
    std::int64_t submitted = 0; ///< messages handed to the bus -- NOT messages delivered
    std::string library;
    std::string role;
    std::string form_shape; ///< empty while no form is open
    std::int64_t form_version = 0;
    std::vector<ComposerFieldState> form_fields;
    loom::Bytes form_values;
    std::int64_t form_cursor = 0;
    std::string dropped;
    ZEN_EXPOSE();
    ZEN_SHAPE(ComposerState, 2, ZEN_FIELD(offers), ZEN_FIELD(rooms), ZEN_FIELD(targets),
              ZEN_FIELD(asked), ZEN_FIELD(described), ZEN_FIELD(refused), ZEN_FIELD(submitted),
              ZEN_FIELD(library), ZEN_FIELD(role), ZEN_FIELD(form_shape), ZEN_FIELD(form_version),
              ZEN_FIELD(form_fields), ZEN_FIELD(form_values), ZEN_FIELD(form_cursor),
              ZEN_FIELD(dropped));
};

/// The pane's offer, room, rows, presses and manual: each row's meaning is `RowMeaning`, which
/// names the target and form it was drawn under, so a press on a picture drawn for another
/// target or form, whose rows stood where these stand, opens nothing.
using Rows = ws::PaneRowsFacet<kComposePane, kComposerRole,
                               zengine::manual::pages::zengine_composer::kManual, RowMeaning>;
using Manual = zengine::manual::ManualFacet<zengine::manual::pages::zengine_composer::kManual>;
using Actions = ws::PaneActionFacet<kComposePane, kComposerRole>;

/// What this weave says to Workshop beside its rows and actions. A submitted command is whatever
/// shape the weaver filled, so it is not listed.
class ComposerWeave final
    : public zengine::With<Rows, Manual, Actions>::Weave<
          ComposerWeave, ComposerState,
          loom::Accept<ws::PaneResetRequested, ws::PaneCanvasValueDrop, ws::PaneCanvasDrop,
                       ws::PaneOperationAnswered, inv::InventoryEntry, loom::DispatchRefused,
                       PaneKey, PaneTextInput, LoadedSelected, surface::ClipboardCopy,
                       surface::ClipboardText, AcceptedShapes, loom::Refused>,
          loom::Emit<ws::PaneOperationRequested, ws::PaneEscapeUnspent>> {
public:
    // ---- the pane protocol: the facets call these ---------------------------

    PaneOffered offering() const {
        return PaneOffered{kComposePane, kComposePaneName, kComposePaneSummary, 14, 64};
    }

    void offered(loom::Mail&) { ++state_.offers; }

    /// The first activation of this image offers the pane and restores what a reload carried.
    void on_activated(loom::Mail& mail) {
        room().announce(*this, mail);
        if (!std::exchange(restored_, true)) {
            restore(mail);
        }
    }

    /// THE ROOM WORKSHOP GRANTED -- the canvas room, or the prose room where it grants none: the
    /// pane draws its rows there.
    void on_room(loom::Mail& mail) {
        ++state_.rooms;
        say(mail);
    }

    /// A PRESS ON A ROW of the picture it was aimed at, read back to what that row meant: a
    /// message opens its form, a field takes the cursor, and a control does its act.
    void press(const ws::RowPress& at, loom::Mail& mail) {
        if (busy()) return;
        const RowMeaning* what = room().map().at(at.row, at.column);
        if (what == nullptr) return;
        switch (what->what) {
        case meaning::kMessage:
            open_form(what->which, mail);
            return;
        case meaning::kField:
            // A press moves the cursor and nothing else -- no edit begun, no presence toggled,
            // no caret placed at the column: three answers a press could give, and nothing has
            // measured a preference between them.
            composing_.cursor = what->which;
            say(mail);
            return;
        case meaning::kSubmit:
            submit(mail);
            return;
        case meaning::kBack:
            back_to_catalog(mail);
            return;
        default:
            return;
        }
    }

    /// The wheel walks the cursor, a row a notch.
    void wheel(double dy, loom::Mail& mail) {
        wheel_ += dy;
        const std::int64_t rows = static_cast<std::int64_t>(wheel_);
        if (rows == 0) {
            return;
        }
        wheel_ -= static_cast<double>(rows);
        const std::int64_t was = composing_.cursor;
        move_cursor(-rows);
        if (composing_.cursor == was) {
            return;
        }
        say(mail);
    }

    // ---- what this pane hears ------------------------------------------------

    void on(const ws::PaneResetRequested& request, loom::Mail& mail) {
        if (request.pane != kComposePane || busy() || awaiting_) {
            (void)mail.answer(loom::Refused{"Compose reset needs its pane and no pending operation"}); return;
        }
        ++draft_generation_;
        composing_ = zengine::composer::Composing{};
        say(mail); (void)mail.answer(loom::Ack{});
    }

    void on(const ws::PaneCanvasValueDrop& d, loom::Mail& mail) {
        on_drop(d.pane, d.grant, d.picture, d.x, d.y, d.data, false, mail);
    }

    void on(const ws::PaneCanvasDrop& d, loom::Mail& mail) {
        on_drop(d.pane, d.grant, d.picture, d.x, d.y, d.data, true, mail);
    }

    /// One of the rows `say` declares, asked for by id from Workshop (`PaneActionFacet`).
    void on_action(const std::string& id, loom::Mail& mail) {
        if (id == "compose.enter") enter(mail);
        else if (id == "compose.submit") submit(mail);
        else if (id == "compose.store") store(mail);
        else if (id == "compose.preset") store(mail, true);
        else if (id == "compose.back") back_to_catalog(mail);
        else if (id == "compose.up" && !busy()) { move_cursor(-1); say(mail); }
        else if (id == "compose.down" && !busy()) { move_cursor(+1); say(mail); }
        else if (id == "compose.include" && !busy()) { cycle_field(); say(mail); }
        else if (id == "compose.unset" && !busy()) {
            if (auto* field = field_under_cursor()) {
                *field = {}; ++draft_generation_;
                composing_.notice = "Field unset; its old value was discarded"; say(mail);
            }
        }
    }

    void on(const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (storage_.hear(answer, mail)) { storage_notice(mail); return; }
        if (!send_value_ || !mail.answers_ask() || mail.correlation() != send_correlation_) return;
        if (!answer.allowed) { send_value_.reset(); complain(answer.reason); say(mail); return; }
        ++state_.submitted;
        send_attempt_ = mail.bus().office_send_to_role(kComposerRole, send_role_,
            loom::Message(std::move(*send_value_), {}, {}, send_correlation_));
        send_value_.reset();
        composing_.notice = send_attempt_.valid() ? "SUBMITTED -- queued; outcome belongs to the receiver"
                                                 : "Submission could not be queued";
        composing_.notice_role = send_attempt_.valid() ? surface::role::kAccent : surface::role::kAlert;
        say(mail);
    }

    void on(const inv::InventoryEntry& entry, loom::Mail& mail) {
        if (storage_.hear(entry, mail)) storage_notice(mail);
    }

    void on(const loom::DispatchRefused& refusal, loom::Mail& mail) {
        if (storage_.hear(refusal, mail)) { storage_notice(mail); return; }
        if (!mail.dispatch_refused()) return;
        if (send_attempt_.valid() && refusal.refused_attempt().seq == send_attempt_.seq) {
            send_value_.reset(); send_attempt_ = {};
            complain("Submission was not delivered: " + refusal.reason); say(mail);
        } else if (discovery_.valid() && refusal.refused_attempt().seq == discovery_.seq) {
            awaiting_ = false; discovery_ = {}; restoring_ = false;
            complain("Discovery was not delivered: " + refusal.reason); say(mail);
        }
    }

    void on(const PaneKey& key, loom::Mail& mail) {
        if (busy()) return;
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        if (key.pane != kComposePane) {
            return;
        }
        // The field's own vocabulary first (`TextBox::consume`). A copy the field took is said
        // to the process once, from the writes comparison; a paste it requested is asked of the
        // Skin (the component bumps `paste_requests` instead of pasting), and the answer lands
        // in the field that asked or nowhere (`on(ClipboardText)`).
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (edit_field(key.scancode, key.modifiers)) {
            if (clip_.writes != copied_before) {
                mail.publish(surface::ClipboardCopy{clip_.text});
            }
            if (clip_.paste_requests != pastes_before) {
                begin_clipboard_paste(mail);
            }
            say(mail);
            return;
        }
        // Every other word this pane has is an action row (`say`), asked by id; a key that is
        // none of them changes nothing, and a bare Escape is handed back to put the pane down.
        if (ws::pane_escape::bare(key)) {
            ws::pane_escape::unspent(mail, kComposerRole, key.pane);
        }
    }

    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (busy()) return;
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        if (typed.pane != kComposePane || typed.text.empty()) {
            return;
        }
        zengine::composer::FieldDraft* d = field_under_cursor();
        if (d == nullptr || !zengine::composer::typeable(kind_under_cursor())) {
            return;
        }
        d->present = true;
        d->value.type(typed.text);
        say(mail);
    }

    // ---- the one fact this tool listens to ----------------------------------

    void on(const LoadedSelected& sel, loom::Mail& mail) {
        if (busy()) return;
        if (!mail.authored_from_role(kIntrospectionRole)) {
            ++state_.refused;
            return;
        }
        composing_.library = sel.library;
        composing_.role = sel.role;
        restoring_ = false; // the weaver chose another target: the carried form goes with the old one
        composing_.cursor = 0;
        composing_.notice.clear();
        composing_.notice_role = surface::role::kMuted;
        // EVERY PREVIOUS TARGET'S VOCABULARY AND DRAFT GO NOW, before anything is
        // asked. A snapshot belongs to the target it was read from, and a draft
        // belongs to a shape out of that snapshot; carrying either across would put
        // one target's shapes in front of a weaver aiming at another.
        composing_.snapshot = Snapshot{};
        composing_.draft = MessageDraft{};
        if (composing_.role.empty()) {
            // AN OBSERVED ABSENCE. `LoadedWeave`'s own rule, one layer further on:
            // the kernel binds a role at load only when one was named, so an empty
            // role is the kernel saying this library holds none. There is nothing to
            // address, and nothing here manufactures an address.
            composing_.stage = stage::kNoRole;
            say(mail);
            return;
        }
        ++state_.targets;
        composing_.stage = stage::kAsking;
        ask(mail);
        say(mail);
    }

    /// A copy said anywhere in the process, mirrored so a weaver can copy in the Terminal and
    /// paste into a field here -- the mirror's only feed, since the platform clipboard is read at
    /// paste time through the Skin. The writes counter is untouched: it counts this pane's
    /// copies, so a key's publish never echoes another participant's copy.
    void on(const surface::ClipboardCopy& copied, loom::Mail&) { clip_.text = copied.text; }

    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        const std::optional<loom::PendingAsk> settled =
            clip_asks_.settle(mail.correlation(), mail.sender());
        if (!settled) {
            return;
        }
        PendingPaste p{};
        bool found = false;
        for (std::size_t i = 0; i < pending_pastes_.size(); ++i) {
            if (pending_pastes_[i].ask == settled->id) {
                p = pending_pastes_[i];
                pending_pastes_.erase(pending_pastes_.begin() + static_cast<std::ptrdiff_t>(i));
                found = true;
                break;
            }
        }
        if (!found || p.generation != draft_generation_ || composing_.stage != stage::kForm ||
            !composing_.draft.valid() || p.field < 0 ||
            p.field >= static_cast<std::int64_t>(composing_.draft.size())) {
            return; // the form that asked is gone; the payload is discarded
        }
        zengine::composer::FieldDraft& d =
            composing_.draft.fields[static_cast<std::size_t>(p.field)];
        if (!d.present || d.value.draft_epoch() != p.epoch) {
            return; // the field left the state that asked
        }
        if (a.readable) {
            clip_.text = a.text; // the platform's current truth, asked for by this paste
        }
        d.value.paste(clip_);
        say(mail);
    }

    // ---- discovery: the target's answer -------------------------------------

    void on(const AcceptedShapes& described, loom::Mail& mail) {
        if (!awaiting_ || !mail.answers_ask() || mail.correlation() != pending_) {
            return; // an answer to a question this weave is not waiting on
        }
        awaiting_ = false;
        ++state_.described;
        Snapshot fresh;
        fresh.deps = std::make_unique<loom::Registry>();
        try {
            loom::decode_accepted_referenced(described.value, *fresh.deps);
            fresh.roots = loom::decode_accepted_roots(described.value, *fresh.deps);
        } catch (const std::exception& e) {
            composing_.snapshot = Snapshot{};
            composing_.draft = MessageDraft{};
            composing_.stage = stage::kCatalog;
            complain(std::string("could not read the answer -- ") + e.what());
            say(mail);
            return;
        }
        composing_.snapshot = std::move(fresh);
        composing_.draft = MessageDraft{};
        composing_.cursor = 0;
        composing_.stage = stage::kCatalog;
        composing_.notice = "up/down move, enter chooses";
        composing_.notice_role = surface::role::kMuted;
        if (std::exchange(restoring_, false)) reopen();
        say(mail);
    }

    void on(const loom::Refused& refusal, loom::Mail& mail) {
        if (storage_.hear(refusal, mail)) { storage_notice(mail); return; }
        if (mail.answers_ask() && mail.correlation() == send_correlation_) {
            send_value_.reset(); complain("Receiver refused: " + refusal.reason); say(mail); return;
        }
        if (!awaiting_ || !mail.answers_ask() || mail.correlation() != pending_) {
            return;
        }
        awaiting_ = false;
        restoring_ = false; // a form a reload carried has no shapes to open on
        complain("the discovery request was refused");
        say(mail);
    }

    /// The counters and the work a reload keeps (`ComposerState`), read from the pane as it
    /// stands; a form a reload carried, while its target has not yet answered, as it came.
    loom::Value snapshot() const override {
        ComposerState saved = state_;
        saved.dropped = send_value_ ? "submission" : storage_.busy() ? "store" : "";
        if (restoring_) {
            if (saved.dropped.empty()) saved.dropped = state_.dropped;
            return loom::to_value(saved);
        }
        saved.library = composing_.library;
        saved.role = composing_.role;
        saved.form_shape.clear();
        saved.form_version = 0;
        saved.form_fields.clear();
        saved.form_values.clear();
        saved.form_cursor = 0;
        if (composing_.stage == stage::kForm && composing_.draft.valid()) {
            const MessageDraft& draft = composing_.draft;
            saved.form_shape = draft.schema->name();
            saved.form_version = static_cast<std::int64_t>(draft.schema->version());
            saved.form_cursor = composing_.cursor;
            zengine::message_draft::Draft values(draft.schema);
            bool any = false;
            for (std::size_t i = 0; i < draft.size(); ++i) {
                const zengine::composer::FieldDraft& field = draft.fields[i];
                saved.form_fields.push_back({field.present, field.typed ? std::string() : field.value.text()});
                if (field.present && field.typed) {
                    try {
                        values.set({draft.field(i).name}, *field.typed);
                        any = true;
                    } catch (const std::exception&) {
                    }
                }
            }
            if (any) {
                try {
                    zengine::message_draft::Library library;
                    library.put("value", std::move(values));
                    const std::string bytes = zengine::message_draft::library_bytes(library);
                    saved.form_values.assign(bytes.begin(), bytes.end());
                } catch (const std::exception&) {
                }
            }
        }
        return loom::to_value(saved);
    }

private:
    Rows& room() { return this->template facet<Rows>(); }

    /// A RELOAD IN PLACE KEEPS THE TARGET AND ITS FORM: the target is asked again what it
    /// accepts, and the form opens again once it answers (`reopen`). What the old image was still
    /// waiting on is said as dropped.
    void restore(loom::Mail& mail) {
        if (state_.library.empty() && state_.role.empty()) {
            return;
        }
        composing_.library = state_.library;
        composing_.role = state_.role;
        carried_notice_ = state_.dropped.empty()
            ? std::string()
            : "The " + state_.dropped + " waiting when Compose was reloaded was dropped; " +
                  (state_.dropped == "store" ? "store it again" : "submit again");
        if (composing_.role.empty()) {
            composing_.stage = stage::kNoRole;
            say(mail);
            return;
        }
        composing_.stage = stage::kAsking;
        restoring_ = !state_.form_shape.empty();
        ask(mail);
        if (!carried_notice_.empty() && !restoring_) complain(carried_notice_);
        say(mail);
    }

    /// THE FORM A RELOAD CARRIED, opened again on the target's fresh answer: its message by name
    /// and version among the shapes the target accepts now, its fields as they were left. A
    /// message the target no longer accepts drops the form, and says so.
    void reopen() {
        const std::string named = state_.form_shape + " v" + std::to_string(state_.form_version);
        std::shared_ptr<const loom::Schema> root;
        for (const auto& accepted : composing_.snapshot.roots) {
            if (accepted->name() == state_.form_shape &&
                static_cast<std::int64_t>(accepted->version()) == state_.form_version) {
                root = accepted;
            }
        }
        if (root == nullptr) {
            complain("The form for " + named + " was dropped: @" + composing_.role +
                     " no longer accepts it");
            return;
        }
        try {
            MessageDraft draft = zengine::composer::begin_draft(root);
            if (!state_.form_values.empty()) {
                const zengine::message_draft::Library library = zengine::message_draft::read_library(
                    std::string_view(reinterpret_cast<const char*>(state_.form_values.data()),
                                     state_.form_values.size()));
                if (const auto* values = library.find("value")) {
                    for (std::size_t i = 0; i < draft.size(); ++i) {
                        if (const auto* cell = values->value().get(draft.field(i).name)) {
                            zengine::composer::put_field(draft, i, *cell);
                        }
                    }
                }
            }
            for (std::size_t i = 0; i < draft.size() && i < state_.form_fields.size(); ++i) {
                zengine::composer::FieldDraft& field = draft.fields[i];
                if (field.typed) continue;
                field.present = state_.form_fields[i].present;
                field.value.set(state_.form_fields[i].text, state_.form_fields[i].text.size());
            }
            composing_.draft = std::move(draft);
        } catch (const std::exception& e) {
            complain("The form for " + named + " could not be opened again: " + e.what());
            return;
        }
        ++draft_generation_;
        composing_.stage = stage::kForm;
        composing_.cursor = std::clamp<std::int64_t>(state_.form_cursor, 0,
                                                     zengine::composer::form_items(composing_.draft) - 1);
        composing_.notice = carried_notice_.empty() ? "The form was kept across a reload -- review, then Submit"
                                                    : carried_notice_;
        composing_.notice_role = carried_notice_.empty() ? surface::role::kAccent : surface::role::kAlert;
    }

    // ---- discovery ----------------------------------------------------------

    void ask(loom::Mail& mail) {
        // One correlation sequence for this whole weave, minted from the clipboard book, so the
        // discovery conversation never shares a number with an open paste ask
        // (`AskBook::mint_correlation`'s own warning).
        pending_ = clip_asks_.mint_correlation();
        awaiting_ = true;
        ++state_.asked;
        discovery_ = mail.as_role(kComposerRole)
            .send_to_role(composing_.role, loom::DescribeAccepted{}, pending_);
        if (!discovery_.valid()) { awaiting_ = false; complain("Discovery could not be queued"); }
    }

    // ---- what a weaver does with what arrived --------------------------------

    void move_cursor(std::int64_t by) {
        const std::int64_t population = cursor_population();
        if (population <= 0) {
            return;
        }
        composing_.cursor += by;
        if (composing_.cursor < 0) {
            composing_.cursor = 0;
        }
        if (composing_.cursor >= population) {
            composing_.cursor = population - 1;
        }
    }

    std::int64_t cursor_population() const {
        if (composing_.stage == stage::kCatalog) {
            return static_cast<std::int64_t>(composing_.snapshot.roots.size());
        }
        if (composing_.stage == stage::kForm && composing_.draft.valid()) {
            return zengine::composer::form_items(composing_.draft);
        }
        return 0;
    }

    void enter(loom::Mail& mail) {
        if (composing_.stage == stage::kCatalog) {
            open_form(composing_.cursor, mail);
            return;
        }
        if (composing_.stage != stage::kForm || !composing_.draft.valid()) {
            return;
        }
        if (composing_.cursor == zengine::composer::submit_index(composing_.draft)) {
            submit(mail);
            return;
        }
        if (composing_.cursor == zengine::composer::back_index(composing_.draft)) {
            back_to_catalog(mail);
            return;
        }
        // On a field row `enter` is not a gesture this pane has. It does not commit
        // (there is nothing to commit to), does not move to the next field (that is
        // what down is for) and does not submit (that is a control with its own row).
    }

    void open_form(std::int64_t which, loom::Mail& mail) {
        if (busy()) return;
        if (which < 0 || which >= static_cast<std::int64_t>(composing_.snapshot.roots.size())) {
            return;
        }
        composing_.draft =
            zengine::composer::begin_draft(composing_.snapshot.roots[static_cast<std::size_t>(which)]);
        ++draft_generation_; // a new form; a paste asked for by the old one has no home
        composing_.stage = stage::kForm;
        composing_.cursor = 0;
        composing_.notice = "up/down move, tab include, enter acts, esc back";
        composing_.notice_role = surface::role::kMuted;
        say(mail);
    }

    void back_to_catalog(loom::Mail& mail) {
        if (busy()) return;
        if (composing_.stage != stage::kForm) {
            return;
        }
        composing_.draft = MessageDraft{};
        ++draft_generation_; // the dropped form takes its in-flight paste with it
        composing_.stage = stage::kCatalog;
        composing_.cursor = 0;
        composing_.notice.clear();
        composing_.notice_role = surface::role::kMuted;
        say(mail);
    }

    zengine::composer::FieldDraft* field_under_cursor() {
        if (composing_.stage != stage::kForm || !composing_.draft.valid()) {
            return nullptr;
        }
        if (composing_.cursor < 0 ||
            composing_.cursor >= static_cast<std::int64_t>(composing_.draft.size())) {
            return nullptr;
        }
        return &composing_.draft.fields[static_cast<std::size_t>(composing_.cursor)];
    }

    loom::Kind kind_under_cursor() const {
        if (composing_.stage != stage::kForm || !composing_.draft.valid() ||
            composing_.cursor < 0 ||
            composing_.cursor >= static_cast<std::int64_t>(composing_.draft.size())) {
            return loom::Kind::Message; // a kind nothing here can author -- a safe nothing
        }
        return composing_.draft.field(static_cast<std::size_t>(composing_.cursor)).type.kind;
    }

    void cycle_field() {
        zengine::composer::FieldDraft* d = field_under_cursor();
        if (d == nullptr) {
            return;
        }
        const loom::Kind kind = kind_under_cursor();
        if (!d->typed && zengine::composer::composability(kind) != zengine::composer::Composability::kScalar) {
            return; // a field this pane cannot author has no presence to give it
        }
        zengine::composer::cycle(*d, kind);
    }

    bool edit_field(std::int64_t scancode, std::int64_t modifiers) {
        zengine::composer::FieldDraft* d = field_under_cursor();
        if (d == nullptr || !zengine::composer::typeable(kind_under_cursor()) || !d->present) {
            return false;
        }
        return d->value.consume(scancode, modifiers, clip_);
    }

    struct PendingPaste {
        std::uint64_t ask = 0;
        std::uint64_t generation = 0;
        std::int64_t field = 0;
        std::uint64_t epoch = 0;
    };

    void begin_clipboard_paste(loom::Mail& mail) {
        const zengine::composer::FieldDraft* d = field_under_cursor();
        if (d == nullptr) {
            return; // unreachable while edit_field's gate holds; written anyway
        }
        const loom::AskOpened opened = clip_asks_.open_to_role(
            surface::kSkinRole, surface::ClipboardTextRequested::zen_name,
            surface::ClipboardTextRequested::zen_version);
        if (!opened) {
            return;
        }
        PendingPaste p;
        p.ask = opened.id;
        p.generation = draft_generation_;
        p.field = composing_.cursor;
        p.epoch = d->value.draft_epoch();
        pending_pastes_.push_back(p);
        (void)mail.as_role(kComposerRole)
            .send_to_role(surface::kSkinRole, surface::ClipboardTextRequested{},
                          opened.correlation);
    }

    // ---- submission ---------------------------------------------------------

    void submit(loom::Mail& mail) {
        if (busy()) return;
        if (composing_.stage != stage::kForm || !composing_.draft.valid()) {
            return;
        }
        const loom::Composition made =
            zengine::composer::compose(composing_.snapshot, composing_.draft);
        if (made.status == loom::Composition::Status::Error) {
            complain(made.error);
            say(mail);
            return;
        }
        if (made.status == loom::Composition::Status::NeedsInput) {
            std::string open;
            for (const loom::FieldDesc& f : made.open_fields) {
                if (f.required) {
                    open += open.empty() ? "" : ", ";
                    open += f.name;
                }
            }
            complain(open.empty() ? "not ready to send" : "still needed: " + open);
            say(mail);
            return;
        }
        send_value_ = loom::assemble(made);
        send_role_ = composing_.role;
        send_correlation_ = clip_asks_.mint_correlation();
        send_attempt_ = mail.as_role(kComposerRole).send_to_role(kWorkshopRole,
            ws::PaneOperationRequested{kComposePane, send_role_, made.schema->name(),
                made.schema->version(), static_cast<std::int64_t>(mail.correlation())}, send_correlation_);
        if (!send_attempt_.valid()) { send_value_.reset(); complain("Permission request could not be queued"); }
        else composing_.notice = "Checking submission authority";
        say(mail);
    }

    // ---- saying it ----------------------------------------------------------

    bool busy() const { return send_value_.has_value() || storage_.busy(); }
    void storage_notice(loom::Mail& mail) {
        composing_.notice = storage_.result ? "Stored as a new inventory entry" : storage_.notice;
        composing_.notice_role = surface::role::kAccent;
        say(mail);
    }
    void store(loom::Mail& mail, bool preset = false) {
        if (busy() || !composing_.draft.valid()) return;
        const auto made = zengine::composer::compose(composing_.snapshot, composing_.draft);
        if (!preset && made.status != loom::Composition::Status::Ready) {
            complain("Complete the command before storing it"); say(mail); return;
        }
        try {
            const auto value = preset
                ? zengine::message_draft::store_draft(composing_.draft.schema->name() + " preset",
                                                     zengine::composer::partial(composing_.draft))
                : loom::assemble(made);
            const auto bytes = inv::encode_pair(value,
                {loom::to_value(ComposerCommandContext{composing_.role})});
            // Reserve the two PaneClient correlations from the same sequence as all other asks.
            storage_asks_ = clip_asks_.mint_correlation();
            (void)clip_asks_.mint_correlation();
            (void)clip_asks_.mint_correlation();
            storage_.begin(inv::InventoryAdd{loom::Bytes(bytes.begin(), bytes.end()),
                composing_.draft.schema->name() + (preset ? " preset" : "")}, kComposePane, kComposerRole, mail, storage_asks_);
            storage_notice(mail);
        } catch (const std::exception& e) { complain(e.what()); say(mail); }
    }
    /// A VALUE OR A REFERENCE PLACED ON THE CANVAS: its place reads back to the row it fell on,
    /// in the picture it was aimed at, and a field row takes it as that field's value while any
    /// other row takes it as the whole command.
    void on_drop(const std::string& pane, std::int64_t grant, std::int64_t picture, std::int64_t x,
                 std::int64_t y, const loom::Bytes& data, bool reference, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || pane != kComposePane) return;
        if (busy() || grant != room().canvas_room().grant || !room().pictures().current(grant, picture)) {
            complain("Drop refused: form is busy or its picture changed"); say(mail); return;
        }
        try {
            auto item = inv::decode_pair({reinterpret_cast<const char*>(data.data()), data.size()}).item;
            if (reference && !loom::same_identity(item.schema(), *loom::schema_of<inv::InventoryReference>()))
                throw std::invalid_argument("Unsupported reference kind");
            const RowMeaning* row = room().map().at_row(
                ws::row_cell_at(ws::canvas_rows(room().canvas_room()), x, y).row);
            if (composing_.stage == stage::kForm && row != nullptr && row->what == meaning::kField) {
                const auto i = static_cast<std::size_t>(row->which);
                if (composing_.draft.fields[i].present)
                    throw std::invalid_argument("Field already has a value; Tab excludes it before replacement");
                if (zengine::message_draft::is_field_value(item)) {
                    const auto field = zengine::message_draft::read_field(item);
                    zengine::message_draft::require_type(composing_.draft.field(i).type, field.type);
                    zengine::composer::put_field(composing_.draft, i, field.cell);
                } else zengine::composer::put_field(composing_.draft, i, loom::Cell::message(item));
                composing_.cursor = row->which;
            } else {
                if (zengine::composer::has_work(composing_.draft))
                    throw std::invalid_argument("Existing draft kept; go Back before dropping another command");
                const auto preset = zengine::message_draft::is_stored_draft(item)
                    ? std::optional(zengine::message_draft::read_draft(item)) : std::nullopt;
                const auto& shape = preset ? *preset->draft.schema() : item.schema();
                std::shared_ptr<const loom::Schema> expected;
                for (const auto& root : composing_.snapshot.roots)
                    if (loom::same_identity(*root, shape)) { expected = root; break; }
                if (!expected) throw std::invalid_argument("Target does not accept this message shape");
                composing_.draft = preset ? zengine::composer::from_draft(expected, preset->draft)
                                          : zengine::composer::from_message(expected, item);
                composing_.stage = stage::kForm; composing_.cursor = 0;
            }
            ++draft_generation_;
            composing_.notice = "Copied data into form -- review, then Submit";
            composing_.notice_role = surface::role::kAccent;
        } catch (const std::exception& e) { complain(std::string("Drop refused: ") + e.what()); }
        say(mail);
    }

    void complain(std::string what) {
        composing_.notice = std::move(what);
        composing_.notice_role = surface::role::kAlert;
    }

    /// The rows as `project` composes them for the room, each with what it means, published by the
    /// facet: its canvas picture while it holds a canvas room, else prose with the caret beside it.
    void say(loom::Mail& mail) {
        Rows& rows = room();
        if (zengine::composer::FieldDraft* d = field_under_cursor()) {
            if (zengine::composer::typeable(kind_under_cursor())) {
                d->value.keep_caret_visible(zengine::composer::value_capacity(
                    composing_.draft, static_cast<std::size_t>(composing_.cursor), rows.columns()));
            }
        }
        const zengine::composer::ComposerView shown =
            zengine::composer::project(composing_, rows.rows(), rows.columns());
        rows.begin();
        for (const zengine::composer::RenderedRow& row : shown.rows) {
            std::optional<RowMeaning> means;
            if (row.meaning.what != meaning::kNothing) means = row.meaning;
            (void)rows.push(row.row, std::move(means));
        }
        rows.caret().row = shown.caret_row;
        rows.caret().column = shown.caret_col;
        rows.publish(
            mail, [](const RowMeaning& m) { return m.part; }, [](const RowMeaning&) { return true; });
        const bool in_form = composing_.stage == stage::kForm;
        this->template facet<Actions>().declare(mail, {
            {"compose.enter", "choose", input::scan::kReturn, input::mod::kNone},
            {"compose.submit", "submit", input::scan::kReturn, input::mod::kCtrl},
            // Storing adds the command to the Inventory once Workshop allows it; a submit sends the
            // command composed, a shape no row can name before the weaver fills the form.
            {"compose.store", "store command", input::scan::kS, input::mod::kCtrl, {},
             {ws::cause_of<inv::InventoryAdd>()}},
            {"compose.preset", "store preset (may be incomplete)", input::scan::kB, input::mod::kCtrl, {},
             {ws::cause_of<inv::InventoryAdd>()}},
            {"compose.unset", "unset field and discard value", input::scan::kU, input::mod::kCtrl},
            // Escape leaves a form; at the catalog it is unbound, so a bare one puts the pane down.
            {"compose.back", "back to the catalog",
             in_form ? input::scan::kEscape : input::scan::kUnknown, input::mod::kNone},
            {"compose.up", "row up", input::scan::kUp, input::mod::kNone},
            {"compose.down", "row down", input::scan::kDown, input::mod::kNone},
            {"compose.include", "include or leave out the field",
             in_form ? input::scan::kTab : input::scan::kUnknown, input::mod::kNone}});
    }

    std::uint64_t send_correlation_ = 0, storage_asks_ = 1000000;
    std::optional<loom::Value> send_value_;
    std::string send_role_;
    loom::Ticket send_attempt_, discovery_;
    inv::PaneClient storage_;
    std::uint64_t pending_ = 0; ///< the outstanding discovery question, if any
    bool awaiting_ = false;
    loom::AskBook clip_asks_{2};
    std::vector<PendingPaste> pending_pastes_;
    std::uint64_t draft_generation_ = 0; ///< bumped by open_form/back_to_catalog
    /// The first activation of this image has restored what a reload carried; a form it carried
    /// waits on the target's answer; and what to say of a conversation the reload ended.
    bool restored_ = false;
    bool restoring_ = false;
    std::string carried_notice_;
    zengine::composer::Composing composing_;
    double wheel_ = 0.0;
    zengine::component::Clipboard clip_;
};

} // namespace

ZEN_EXPORT_WEAVE(ComposerWeave)
