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
#include "workshop/pane_menu.hpp"
#include "workshop/pane_parts.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_escape.hpp"
#include "inventory/codec.hpp"
#include "inventory/pane_client.hpp"

#include <zen/kernel/export.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/terminal/composer.hpp>
#include <zen/value.hpp>
#include <zen/weave.hpp>
#include <zen/weave/describe.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
using zengine::composer::Snapshot;
using zengine::introspection::kIntrospectionRole;
using zengine::introspection::LoadedSelected;
using zengine::workshop::PaneCatalogRequested;
using zengine::workshop::PaneContent;
using zengine::workshop::PaneKey;
using zengine::workshop::v2::PaneOffered;
using zengine::workshop::PaneRoom;
using zengine::workshop::PaneTextInput;
namespace stage = zengine::composer::stage;
namespace meaning = zengine::composer::meaning;

constexpr const char* kWorkshopRole = "zengine.workshop";

using zengine::composer::ComposerCommandContext;

struct ComposerState {
    std::int64_t offers = 0;
    std::int64_t rooms = 0;
    std::int64_t targets = 0;   ///< usable targets taken from a Loaded selection
    std::int64_t asked = 0;     ///< zen.DescribeAccepted requests submitted
    std::int64_t described = 0; ///< answers that became a decoded snapshot
    std::int64_t refused = 0;   ///< asks, rooms, presses and keys not authored by Workshop
    std::int64_t submitted = 0; ///< messages handed to the bus -- NOT messages delivered
    ZEN_EXPOSE();
    ZEN_SHAPE(ComposerState, 1, ZEN_FIELD(offers), ZEN_FIELD(rooms), ZEN_FIELD(targets),
              ZEN_FIELD(asked), ZEN_FIELD(described), ZEN_FIELD(refused), ZEN_FIELD(submitted));
};

class ComposerWeave final : public loom::Weave {
public:
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<ws::PaneResetRequested>(), loom::schema_of<loom::Activated>(),
                loom::schema_of<PaneCatalogRequested>(),
                loom::schema_of<PaneRoom>(),
                loom::schema_of<ws::PaneCanvasRoom>(), loom::schema_of<ws::PaneCanvasPointer>(),
                loom::schema_of<ws::PaneCanvasRejected>(),
                loom::schema_of<ws::PaneCanvasValueDrop>(), loom::schema_of<ws::PaneCanvasDrop>(),
                loom::schema_of<ws::PaneActionRequested>(), loom::schema_of<ws::PaneOperationAnswered>(),
                loom::schema_of<inv::InventoryEntry>(), loom::schema_of<loom::DispatchRefused>(),
                loom::schema_of<PaneKey>(),
                loom::schema_of<PaneTextInput>(),
                loom::schema_of<LoadedSelected>(),
                loom::schema_of<surface::ClipboardCopy>(),
                loom::schema_of<surface::ClipboardText>(),
                loom::accepted_shapes_schema(),
                loom::schema_of<loom::Refused>()};
    }

    /// What this weave says to Workshop. A submitted command is whatever shape the weaver
    /// filled, so it is not listed. Its rows are its own canvas picture; to a host granting no
    /// canvas it says them, and its caret, as prose.
    std::vector<std::shared_ptr<const loom::Schema>> emitted_schemas() const override {
        return {loom::schema_of<PaneOffered>(), loom::schema_of<ws::v4::PaneContent>(),
                loom::schema_of<ws::v5::PaneCanvasContent>(), loom::schema_of<ws::PaneCaret>(),
                loom::schema_of<ws::PaneActions>(), loom::schema_of<ws::PaneOperationRequested>(),
                loom::schema_of<ws::PaneEscapeUnspent>(), loom::schema_of<ws::PanePassRequested>()};
    }

    void handle(const loom::Message& in, loom::Bus& bus) override {
        loom::Mail mail(bus, in, loom::WeaveId{});
        const loom::Schema& shape = in.payload.schema();
        if (loom::same_identity(*loom::schema_of<ws::PaneResetRequested>(), shape)) {
            const auto request = loom::from_value<ws::PaneResetRequested>(in.payload);
            if (request.pane != kComposePane || busy() || awaiting_) {
                (void)mail.answer(loom::Refused{"Compose reset needs its pane and no pending operation"}); return;
            }
            ++draft_generation_;
            composing_ = zengine::composer::Composing{};
            shown_ = {};
            say(mail); (void)mail.answer(loom::Ack{}); return;
        }
        if (loom::same_identity(*loom::schema_of<ws::PaneCanvasPointer>(), shape)) {
            on_pointer(loom::from_value<ws::PaneCanvasPointer>(in.payload), mail); return;
        }
        if (loom::same_identity(*loom::schema_of<ws::PaneCanvasValueDrop>(), shape)) {
            const auto d = loom::from_value<ws::PaneCanvasValueDrop>(in.payload);
            on_drop(d.pane, d.grant, d.picture, d.x, d.y, d.data, false, mail); return;
        }
        if (loom::same_identity(*loom::schema_of<ws::PaneCanvasDrop>(), shape)) {
            const auto d = loom::from_value<ws::PaneCanvasDrop>(in.payload);
            on_drop(d.pane, d.grant, d.picture, d.x, d.y, d.data, true, mail); return;
        }
        if (loom::same_identity(*loom::schema_of<ws::PaneCanvasRoom>(), shape)) {
            on_canvas_room(loom::from_value<ws::PaneCanvasRoom>(in.payload), mail); return;
        }
        if (loom::same_identity(*loom::schema_of<ws::PaneCanvasRejected>(), shape)) {
            return; // a refused picture leaves the last good one showing; the next say draws again
        }
        if (loom::same_identity(*loom::schema_of<ws::PaneActionRequested>(), shape)) {
            const auto a = loom::from_value<ws::PaneActionRequested>(in.payload);
            if (!mail.authored_from_role(kWorkshopRole) || a.pane != kComposePane) return;
            if (a.id == "compose.enter") enter(mail);
            else if (a.id == "compose.submit") submit(mail);
            else if (a.id == "compose.store") store(mail);
            else if (a.id == "compose.preset") store(mail, true);
            else if (a.id == "compose.unset" && !busy()) {
                if (auto* field = field_under_cursor()) {
                    *field = {}; ++draft_generation_;
                    composing_.notice = "Field unset; its old value was discarded"; say(mail);
                }
            }
            return;
        }
        if (loom::same_identity(*loom::schema_of<ws::PaneOperationAnswered>(), shape)) {
            const auto answer = loom::from_value<ws::PaneOperationAnswered>(in.payload);
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
            say(mail); return;
        }
        if (loom::same_identity(*loom::schema_of<inv::InventoryEntry>(), shape)) {
            if (storage_.hear(loom::from_value<inv::InventoryEntry>(in.payload), mail)) storage_notice(mail);
            return;
        }
        if (loom::same_identity(*loom::schema_of<loom::DispatchRefused>(), shape)) {
            const auto refusal = loom::from_value<loom::DispatchRefused>(in.payload);
            if (storage_.hear(refusal, mail)) { storage_notice(mail); return; }
            if (!mail.dispatch_refused()) return;
            if (send_attempt_.valid() && refusal.refused_attempt().seq == send_attempt_.seq) {
                send_value_.reset(); send_attempt_ = {};
                complain("Submission was not delivered: " + refusal.reason); say(mail);
            } else if (discovery_.valid() && refusal.refused_attempt().seq == discovery_.seq) {
                awaiting_ = false; discovery_ = {};
                complain("Discovery was not delivered: " + refusal.reason); say(mail);
            }
            return;
        }
        if (loom::same_identity(*loom::schema_of<loom::Activated>(), shape)) {
            on_activated(loom::from_value<loom::Activated>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<PaneCatalogRequested>(), shape)) {
            on_catalog_requested(mail);
        } else if (loom::same_identity(*loom::schema_of<PaneRoom>(), shape)) {
            on_room(loom::from_value<PaneRoom>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<PaneKey>(), shape)) {
            on_key(loom::from_value<PaneKey>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<PaneTextInput>(), shape)) {
            on_text(loom::from_value<PaneTextInput>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<LoadedSelected>(), shape)) {
            on_selected(loom::from_value<LoadedSelected>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<surface::ClipboardCopy>(), shape)) {
            // A copy said anywhere in the process, mirrored so a weaver can copy in the Terminal
            // and paste into a field here -- the mirror's only feed, since the platform clipboard
            // is read at paste time through the Skin. The writes counter is untouched: it counts
            // this pane's copies, so the publish below never echoes another participant's copy.
            clip_.text = loom::from_value<surface::ClipboardCopy>(in.payload).text;
        } else if (loom::same_identity(*loom::schema_of<surface::ClipboardText>(), shape)) {
            on_clipboard_text(loom::from_value<surface::ClipboardText>(in.payload), mail);
        } else if (loom::same_identity(*loom::accepted_shapes_schema(), shape)) {
            on_described(in.payload, mail);
        } else if (loom::same_identity(*loom::schema_of<loom::Refused>(), shape)) {
            on_refused(loom::from_value<loom::Refused>(in.payload), mail);
        }
    }

    loom::Value snapshot() const override { return loom::to_value(state_); }

    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(0));
        v.set("revive_from_last_good", loom::Cell::boolean(false));
        return v;
    }

    void revive(const loom::Value& state) override {
        state_ = loom::from_value<ComposerState>(state);
    }

private:
    // ---- lifecycle and the pane protocol ------------------------------------

    void on_activated(const loom::Activated& a, loom::Mail& mail) {
        if (activation_.accept(mail, a)) {
            announce(mail);
        }
    }

    void on_catalog_requested(loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        announce(mail);
    }

    void on_room(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return; // a forged room grants nothing and produces no content
        }
        if (room.pane != kComposePane) {
            return; // a room for a pane this provider does not have
        }
        ++state_.rooms;
        prose_rows_ = room.rows;
        prose_columns_ = room.columns;
        if (on_canvas()) return; // the canvas room, granted first, already drew
        fit_room();
        say(mail);
    }

    /// THE PANE'S OWN CANVAS: while it holds a room there it draws its rows as its picture, the
    /// lattice's rows and columns its room, and says them as prose only to a host granting none.
    void on_canvas_room(const ws::PaneCanvasRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        if (room.pane != kComposePane) {
            return;
        }
        ++state_.rooms;
        canvas_ = room;
        fit_room();
        say(mail);
    }

    bool on_canvas() const { return canvas_.grant > 0 && canvas_.width > 0 && canvas_.height > 0; }

    /// The rows and columns the pane composes for: its canvas's lattice while it holds one.
    void fit_room() {
        const ws::CanvasRows lattice = ws::canvas_rows(canvas_);
        rows_ = on_canvas() ? lattice.rows : prose_rows_;
        columns_ = on_canvas() ? lattice.columns : prose_columns_;
    }

    /// A PRESS, THE WHEEL AND A RIGHT PRESS ON THE CANVAS: a press reads back to the row it fell
    /// on in the picture it was aimed at, and acts only while that picture was drawn under the
    /// meaning the rows have now; the wheel walks the cursor; a right press is handed back, so
    /// Workshop's own pane menu opens where it was made.
    void on_pointer(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        if (event.pane != kComposePane || event.grant != canvas_.grant || !on_canvas()) return;
        if (event.phase == ws::canvas_pointer::kWheel) {
            on_wheel(event.dy, mail);
            return;
        }
        if (event.phase != ws::canvas_pointer::kPress) return;
        if (event.button == 3) {
            (void)ws::pane_menu::pass_back(mail, kComposerRole, kComposePane);
            return;
        }
        const ws::RowCell at = ws::row_cell_at(ws::canvas_rows(canvas_), event.x, event.y);
        if (event.button != 1 || !at.shown || !pictures_.current(event.grant, event.picture)) return;
        on_pressed(at.row, mail);
    }

    void on_pressed(std::int64_t row, loom::Mail& mail) {
        if (busy()) return;
        const zengine::composer::RowMeaning what =
            zengine::composer::meaning_at_row(shown_, row);
        switch (what.what) {
        case meaning::kMessage:
            open_form(what.which, mail);
            return;
        case meaning::kField:
            // A press moves the cursor and nothing else -- no edit begun, no presence toggled,
            // no caret placed at the column: three answers a press could give, and nothing has
            // measured a preference between them.
            composing_.cursor = what.which;
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

    void on_key(const PaneKey& key, loom::Mail& mail) {
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
        // in the field that asked or nowhere (`on_clipboard_text`).
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
        switch (key.scancode) {
        case input::scan::kUp: move_cursor(-1); break;
        case input::scan::kDown: move_cursor(+1); break;
        case input::scan::kReturn: enter(mail); return;
        case input::scan::kEscape:
            // Escape means more in a form, which it leaves; at the catalog it goes back.
            if (composing_.stage == stage::kForm) {
                back_to_catalog(mail);
            } else if (ws::pane_escape::bare(key)) {
                ws::pane_escape::unspent(mail, kComposerRole, key.pane);
            }
            return;
        case input::scan::kTab: cycle_field(); break;
        default: return; // a key this pane has no word for changes nothing and says nothing
        }
        say(mail);
    }

    void on_wheel(double dy, loom::Mail& mail) {
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

    void on_text(const PaneTextInput& typed, loom::Mail& mail) {
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

    void on_selected(const LoadedSelected& sel, loom::Mail& mail) {
        if (busy()) return;
        if (!mail.authored_from_role(kIntrospectionRole)) {
            ++state_.refused;
            return;
        }
        composing_.library = sel.library;
        composing_.role = sel.role;
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

    void on_described(const loom::Value& answer, loom::Mail& mail) {
        if (!awaiting_ || !mail.answers_ask() || mail.correlation() != pending_) {
            return; // an answer to a question this weave is not waiting on
        }
        awaiting_ = false;
        ++state_.described;
        Snapshot fresh;
        fresh.deps = std::make_unique<loom::Registry>();
        try {
            loom::decode_accepted_referenced(answer, *fresh.deps);
            fresh.roots = loom::decode_accepted_roots(answer, *fresh.deps);
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
        say(mail);
    }

    void on_refused(const loom::Refused& refusal, loom::Mail& mail) {
        if (storage_.hear(refusal, mail)) { storage_notice(mail); return; }
        if (mail.answers_ask() && mail.correlation() == send_correlation_) {
            send_value_.reset(); complain("Receiver refused: " + refusal.reason); say(mail); return;
        }
        if (!awaiting_ || !mail.answers_ask() || mail.correlation() != pending_) {
            return;
        }
        awaiting_ = false;
        complain("the discovery request was refused");
        say(mail);
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

    void on_clipboard_text(const surface::ClipboardText& a, loom::Mail& mail) {
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
        if (busy() || grant != canvas_.grant || !pictures_.current(grant, picture)) {
            complain("Drop refused: form is busy or its picture changed"); say(mail); return;
        }
        try {
            auto item = inv::decode_pair({reinterpret_cast<const char*>(data.data()), data.size()}).item;
            if (reference && !loom::same_identity(item.schema(), *loom::schema_of<inv::InventoryReference>()))
                throw std::invalid_argument("Unsupported reference kind");
            const auto row = zengine::composer::meaning_at_row(
                shown_, ws::row_cell_at(ws::canvas_rows(canvas_), x, y).row);
            if (composing_.stage == stage::kForm && row.what == meaning::kField) {
                const auto i = static_cast<std::size_t>(row.which);
                if (composing_.draft.fields[i].present)
                    throw std::invalid_argument("Field already has a value; Tab excludes it before replacement");
                if (zengine::message_draft::is_field_value(item)) {
                    const auto field = zengine::message_draft::read_field(item);
                    zengine::message_draft::require_type(composing_.draft.field(i).type, field.type);
                    zengine::composer::put_field(composing_.draft, i, field.cell);
                } else zengine::composer::put_field(composing_.draft, i, loom::Cell::message(item));
                composing_.cursor = row.which;
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

    void announce(loom::Mail& mail) {
        ++state_.offers;
        (void)mail.as_role(kComposerRole)
            .send_to_role(kWorkshopRole,
                          PaneOffered{kComposePane, kComposePaneName, kComposePaneSummary, 14, 64});
    }

    void say(loom::Mail& mail) {
        if (zengine::composer::FieldDraft* d = field_under_cursor()) {
            if (zengine::composer::typeable(kind_under_cursor())) {
                d->value.keep_caret_visible(zengine::composer::value_capacity(
                    composing_.draft, static_cast<std::size_t>(composing_.cursor), columns_));
            }
        }
        shown_ = zengine::composer::project(composing_, rows_, columns_);
        const std::string meant = meaning_of();
        if (meant != meant_) {
            meant_ = meant;
            ++meaning_;
        }
        // A row that means nothing is not listed: a row entire lies over no other part.
        ws::PartNames<ws::PaneRowPart> named;
        for (std::size_t row = 0; row < shown_.rows.size(); ++row) {
            std::string name = zengine::composer::part_name(composing_, shown_.rows[row].meaning);
            if (!name.empty()) {
                (void)named.add(ws::PaneRowPart{std::move(name), static_cast<std::int64_t>(row), 0,
                                                columns_});
            }
        }
        if (on_canvas()) {
            ws::RowsCaret caret;
            caret.row = shown_.caret_row;
            caret.column = shown_.caret_col;
            (void)mail.as_role(kComposerRole).send_to_role(kWorkshopRole,
                ws::rows_picture(canvas_, pictures_.next(canvas_, meaning_),
                                 zengine::composer::rows_of(shown_), named.take(), caret));
        } else {
            ws::v4::PaneContent said;
            said.picture = meaning_;
            said.pane = kComposePane;
            said.rows = zengine::composer::rows_of(shown_);
            said.parts = named.take();
            (void)mail.as_role(kComposerRole).send_to_role(kWorkshopRole, said);
            // The caret beside the rows, said every time, so a caret that went goes.
            (void)mail.as_role(kComposerRole).send_to_role(kWorkshopRole,
                ws::PaneCaret{kComposePane, shown_.caret_row, shown_.caret_col});
        }
        (void)mail.as_role(kComposerRole).send_to_role(kWorkshopRole, ws::PaneActions{kComposePane, {
            {"compose.enter", "choose", input::scan::kReturn, input::mod::kNone},
            {"compose.submit", "submit", input::scan::kReturn, input::mod::kCtrl},
            {"compose.store", "store command", input::scan::kS, input::mod::kCtrl},
            {"compose.preset", "store preset (may be incomplete)", input::scan::kB, input::mod::kCtrl},
            {"compose.unset", "unset field and discard value", input::scan::kU, input::mod::kCtrl}}});
    }

    /// WHAT A PRESS OR A DROP ON EACH ROW WOULD MEAN, spelled: the target, the form, and every
    /// row's meaning and name. A picture drawn under the same spelling answers as this one does.
    std::string meaning_of() const {
        std::string out = composing_.role + '\n' + std::to_string(composing_.stage);
        if (composing_.draft.valid()) {
            out += '\n' + composing_.draft.schema->name() + " v" +
                   std::to_string(composing_.draft.schema->version());
        }
        for (const zengine::composer::RenderedRow& row : shown_.rows) {
            out += '\n' + std::to_string(row.meaning.what) + ':' + std::to_string(row.meaning.which) +
                   ':' + zengine::composer::part_name(composing_, row.meaning);
        }
        return out;
    }

    std::int64_t meaning_ = 0; ///< numbers what the rows mean; moves only when that changes
    std::string meant_;
    ws::PaneCanvasRoom canvas_;
    ws::CanvasPictures pictures_;
    std::int64_t prose_rows_ = 0, prose_columns_ = 0;
    std::uint64_t send_correlation_ = 0, storage_asks_ = 1000000;
    std::optional<loom::Value> send_value_;
    std::string send_role_;
    loom::Ticket send_attempt_, discovery_;
    inv::PaneClient storage_;
    ComposerState state_;
    zengine::ActivationCursor activation_;
    std::int64_t rows_ = 0;    ///< the room composed for: the canvas lattice's, else the prose room's
    std::int64_t columns_ = 0;
    std::uint64_t pending_ = 0; ///< the outstanding discovery question, if any
    bool awaiting_ = false;
    loom::AskBook clip_asks_{2};
    std::vector<PendingPaste> pending_pastes_;
    std::uint64_t draft_generation_ = 0; ///< bumped by open_form/back_to_catalog
    zengine::composer::Composing composing_;
    double wheel_ = 0.0;
    zengine::component::Clipboard clip_;
    zengine::composer::ComposerView shown_;
};

} // namespace

ZEN_EXPORT_WEAVE(ComposerWeave)
