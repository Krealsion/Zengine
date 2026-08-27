// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Message Composer — a loadable weave that offers Workshop one pane in which a
// maker writes a real Loom message to a real target, from that target's own real
// accepted vocabulary, with nothing about the message hard-coded anywhere.
//
// ---- WHAT IT KNOWS, AND HOW IT COMES TO KNOW IT -----------------------------
//
//     the target     a role, learned from `LoadedSelected` -- somebody else's fact
//                    about a maker's gesture in somebody else's pane
//     the vocabulary the target's OWN accepted roots, learned by asking IT:
//                    zen.DescribeAccepted -> zen.AcceptedShapes -> decode
//     the form       generated from the decoded `loom::Schema` and from nothing
//                    else. There is no `if (name == "StartTimer")` in this file
//     the message    `compose_message` -> `assemble` -> `send_to_role`, all three
//                    Loom's, none of them re-implemented here
//
// ---- THE WHOLE OF WHAT IT CLAIMS AFTER A SEND -------------------------------
//
//     SUBMITTED      it composed, it assembled, and it handed the value to the bus
//
// and NOT delivered, not accepted, not understood, not acted on and not successful.
// Loom does not tell a sender its fate, so any stronger word would be this pane
// inventing one. A target that happens to say something later is not thereby
// answering this message: nothing correlates a submission to anything, and no
// spinner here is waiting for a reply that may not exist.
//
// ---- WHY IT IS A RAW `loom::Weave` AND NOT A `WeaveBase` --------------------
//
// The answer it exists to read, `zen.AcceptedShapes`, is not a ZEN_SHAPE: its
// fields are lists of `zen.SchemaDesc`, an existing SchemaBuilder shape rather than
// a C++ struct, exactly as `zen.Manifest`'s are. `Accept<...>` takes types, so
// there is no type to list -- and `WeaveBase::accepted_schemas()` is `final`, which
// makes that a hard wall rather than an inconvenience. MSG-1's own suite says so at
// the fixture that reads the answer: this is what a stranger written against the
// installed package does when it wants the Value rather than a struct.
//
// WHAT THAT COSTS IS STATED RATHER THAN HIDDEN: this weave advertises no zen.Poke*
// doors and no zen.DescribeAccepted door of its own, because the construction layer
// that answers those is the one it declined. It is not lying about them -- it
// simply does not offer them, which is the transparent half of the same trade
// `loom::Weave`'s own documentation describes. A maker cannot ask the Composer what
// the Composer accepts. That is a real asymmetry in a tool whose whole subject is
// asking that question, and it is worth knowing before somebody adds a sixth kind
// of state here expecting to poke at it.
//
// ---- WHAT IT CANNOT DO ------------------------------------------------------
//
// It writes no file, starts no process, opens no socket, holds no timer, reads no
// Sense, publishes no canvas, commands no lifecycle, and enumerates nothing. It
// never addresses a `WeaveId`: every send it makes is to a ROLE, so a target
// replaced between the discovery and the submission is addressed correctly at
// delivery rather than pinned at the moment a maker pointed at it.
//
// AND ITS OUTBOUND VOCABULARY IS OPEN AT EXACTLY ONE POINT, which is the honest
// thing about it: `zen.DescribeAccepted` is a fixed shape to one resolved role, and
// the SUBMISSION is whatever shape the maker chose out of the target's own
// accept-set. A host that wanted to bound that would write a grant naming the
// shapes and the roles this office may send to -- and today no host does, because
// `Kernel::load` binds `Grant{}.allow_any()` to every library it opens. So the
// narrowness above is a fact about what this weave DOES, not a containment claim
// about the loader, and this file makes none.

#include "draft.hpp"
#include "view.hpp"
#include "vocabulary.hpp"

#include "activation/activation.hpp"
#include "input/vocabulary.hpp"
#include "introspection/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"

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
using zengine::workshop::PaneOffered;
using zengine::workshop::PanePressed;
using zengine::workshop::PaneRoom;
using zengine::workshop::PaneTextInput;
namespace stage = zengine::composer::stage;
namespace meaning = zengine::composer::meaning;

/// The office Workshop holds, named as a STRING rather than reached through
/// `workshop/panel.hpp`. A provider is a stranger to Workshop's internals and must
/// be able to say who it is talking to the way a third party would.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// WHAT THIS PROVIDER HAS DONE, and it is all counters.
///
/// NO TARGET, NO SNAPSHOT AND NO DRAFT ARE IN HERE, and that is one decision rather
/// than three. All of it is transient UI state belonging to the projection
/// currently on screen: a revived incarnation has been granted no room, is showing
/// nothing, and has no pane a draft could be OF -- carrying one across would restore
/// a half-written message against a target nobody is looking at. It is therefore
/// not snapshotted, not revived, not persisted, and in no saved setup.
///
/// `submitted` IS A COUNT OF SUBMISSIONS AND NOT OF DELIVERIES, and the name is
/// chosen for the same reason the row a maker reads says `SUBMITTED`: this weave
/// cannot observe what became of anything it sent.
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
    /// THE ACCEPT-SET, WRITTEN OUT BECAUSE ONE OF ITS MEMBERS HAS NO C++ TYPE.
    ///
    /// `loom::accepted_shapes_schema()` is the answer to the one question this tool
    /// asks, and it is a SchemaBuilder shape. Everything else here is an ordinary
    /// ZEN_SHAPE and would have been spelled `Accept<...>` if the set could have
    /// been.
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<loom::Activated>(),
                loom::schema_of<PaneCatalogRequested>(),
                loom::schema_of<PaneRoom>(),
                loom::schema_of<PanePressed>(),
                loom::schema_of<PaneKey>(),
                loom::schema_of<PaneTextInput>(),
                loom::schema_of<LoadedSelected>(),
                loom::schema_of<surface::ClipboardCopy>(),
                loom::schema_of<surface::ClipboardText>(),
                loom::accepted_shapes_schema(),
                loom::schema_of<loom::Refused>()};
    }

    void handle(const loom::Message& in, loom::Bus& bus) override {
        loom::Mail mail(bus, in, loom::WeaveId{});
        const loom::Schema& shape = in.payload.schema();
        if (loom::same_identity(*loom::schema_of<loom::Activated>(), shape)) {
            on_activated(loom::from_value<loom::Activated>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<PaneCatalogRequested>(), shape)) {
            on_catalog_requested(mail);
        } else if (loom::same_identity(*loom::schema_of<PaneRoom>(), shape)) {
            on_room(loom::from_value<PaneRoom>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<PanePressed>(), shape)) {
            on_pressed(loom::from_value<PanePressed>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<PaneKey>(), shape)) {
            on_key(loom::from_value<PaneKey>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<PaneTextInput>(), shape)) {
            on_text(loom::from_value<PaneTextInput>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<LoadedSelected>(), shape)) {
            on_selected(loom::from_value<LoadedSelected>(in.payload), mail);
        } else if (loom::same_identity(*loom::schema_of<surface::ClipboardCopy>(), shape)) {
            // A copy said anywhere in the process, mirrored so a maker can copy in the
            // Terminal and paste into a field here. The writes counter is untouched: it
            // counts THIS pane's copies, which is what keeps the publish below from
            // echoing another participant's copy back at the bus (TEXT-0). Since QR-11
            // this is the mirror's ONLY feed — the platform's clipboard is read at paste
            // time, through the Skin, never watched.
            clip_.text = loom::from_value<surface::ClipboardCopy>(in.payload).text;
        } else if (loom::same_identity(*loom::schema_of<surface::ClipboardText>(), shape)) {
            on_clipboard_text(loom::from_value<surface::ClipboardText>(in.payload), mail);
        } else if (loom::same_identity(*loom::accepted_shapes_schema(), shape)) {
            on_described(in.payload, mail);
        } else if (loom::same_identity(*loom::schema_of<loom::Refused>(), shape)) {
            on_refused(mail);
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

    /// FIRST BREATH, AND ONLY IF LOOM SAYS SO. `ActivationCursor` owns both halves
    /// of that sentence: the lifecycle attestation must be Loom's, and the sequence
    /// must be one this incarnation has not already acted on. An ordinary
    /// `zen.Activated` sent by anybody granted the shape announces nothing here.
    void on_activated(const loom::Activated& a, loom::Mail& mail) {
        if (activation_.accept(mail, a)) {
            announce(mail);
        }
    }

    /// WORKSHOP ASKING WHO HAS PANES. Answered only when Workshop actually asked --
    /// `authored_from_role` and not `sender()`, because the ask is a PUBLICATION and
    /// what says it was Workshop is Loom's stamp on the authorship, which no payload
    /// can write and no sender can choose.
    void on_catalog_requested(loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        announce(mail);
    }

    /// WORKSHOP GRANTING THIS PANE ITS PROSE BUDGET.
    ///
    /// A ROOM GRANT IS NOT A BEAT ON WHICH ANYTHING IS OBSERVED, and that is the
    /// difference between this tool and the Loaded pane beside it. That one re-reads
    /// the kernel's map on every grant because its subject is a fact about the
    /// running system and a snapshot is only true when it is taken. This one's
    /// subject is a CONVERSATION with one target and a message a maker is part-way
    /// through writing -- so a resize re-projects and re-says, and re-asks nothing.
    /// Discovery happens when the maker names a target, and at no other time: no
    /// poll, no timer, no refresh on room, and nothing in this file has a clock.
    ///
    /// THE CARET'S WINDOW IS RECONCILED HERE, once, before the projection -- the
    /// same rule `refresh_terminal` and `refresh_inspector` keep one layer out, and
    /// for the same reason: the window a maker sees must be the window the value was
    /// last drawn with, and a resize is not an edit.
    void on_room(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return; // a forged room grants nothing and produces no content
        }
        if (room.pane != kComposePane) {
            return; // a room for a pane this provider does not have
        }
        ++state_.rooms;
        rows_ = room.rows;
        columns_ = room.columns;
        say(mail);
    }

    /// A MAKER PRESSED A ROW OF THIS PANE.
    ///
    /// THE PRESS IS READ AGAINST THE PROJECTION CURRENTLY ON SCREEN and nothing
    /// happens first -- no re-project, no re-decode, no re-ask. `meaning_at_row` is
    /// one lookup into the value the painter built, which is what makes a press
    /// select the thing the maker was looking at rather than the thing that would be
    /// there if the pane were redrawn now.
    ///
    /// A ROW THAT NAMES NOTHING DOES NOTHING, and it does not clear anything either:
    /// pressing a heading is not a deselection gesture and inventing one out of a
    /// miss would make an unsteady hand destroy a maker's work.
    void on_pressed(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        if (press.pane != kComposePane) {
            return;
        }
        const zengine::composer::RowMeaning what =
            zengine::composer::meaning_at_row(shown_, press.row);
        switch (what.what) {
        case meaning::kMessage:
            open_form(what.which, mail);
            return;
        case meaning::kField:
            // A PRESS MOVES THE CURSOR AND NOTHING ELSE. It does not begin an edit,
            // toggle presence, or place the caret at the column pressed -- HD-6
            // refused the first of those for a property row and the reasoning
            // carries: three answers a press could give, and nothing has measured a
            // preference between them.
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

    /// A KEY WHILE THIS PANE HELD THE KEYBOARD (MSG-0's new seam).
    ///
    /// WHAT EACH KEY MEANS IS DECIDED HERE AND NOWHERE ELSE. Workshop forwarded a
    /// scancode and a modifier mask and knows none of the words below: that `tab`
    /// changes whether a field is SENT, that `enter` on one row chooses a shape and
    /// on another submits a message, that `esc` is back rather than cancel. Same law
    /// as the press -- Workshop knows the interaction, the provider knows the
    /// meaning.
    void on_key(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        if (key.pane != kComposePane) {
            return;
        }
        // THE FIELD'S OWN VOCABULARY FIRST (TEXT-0) — the fourth of the four switches the
        // component call collapsed, and the one this weave was about to make a fifth of.
        // A copy the field took is then said to the process once, from the same
        // writes-comparison Workshop makes around its own chain — and a PASTE the field
        // requested is asked for the same way (QR-11): the component bumps
        // `paste_requests` instead of pasting, this weave asks the Skin for the platform
        // clipboard's current text, and the answer lands in the field that asked or
        // nowhere (`on_clipboard_text`).
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
        case input::scan::kEscape: back_to_catalog(mail); return;
        case input::scan::kTab: cycle_field(); break;
        default: return; // a key this pane has no word for changes nothing and says nothing
        }
        say(mail);
    }

    /// THE CHARACTERS THE PLATFORM MADE OF A KEYSTROKE, typed into the field under
    /// the cursor.
    ///
    /// TYPING INTO AN ABSENT FIELD MAKES IT PRESENT. A maker who has begun writing a
    /// value has authored the field, and asking them to say so twice would be
    /// ceremony; the reverse gesture (`tab`) is what takes it back out of the
    /// message, and it keeps the bytes.
    ///
    /// A BOOL TAKES NO TEXT. Its value space is two words this pane writes for
    /// itself, so a character typed at one is a character with nowhere to go.
    void on_text(const PaneTextInput& typed, loom::Mail& mail) {
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

    /// A MAKER SELECTED A LOADED WEAVE, IN SOMEBODY ELSE'S PANE (SEL-0's fact, and
    /// this is its first listener in the shipped product).
    ///
    /// ---- THIS IS A LOCAL V0 POLICY AND NOT THE COMPOSITION MODEL --------------
    ///
    /// Every Message Composer in this build follows every Loaded pane, always,
    /// because this file says so. That is a hard-wired workflow edge: it is not
    /// authored by a maker, cannot be turned off, cannot be pointed at a second
    /// Loaded pane rather than a first, and would be wrong the moment somebody wants
    /// two Composers aimed at two different targets. It is here because it is the
    /// smallest thing that makes the product real, and it is written down as a
    /// LIMITATION rather than as a design. What should eventually replace it is
    /// maker-authored logic -- `LoadedSelected(x) -> Composer.target = x.role` --
    /// and NOT a binding engine extracted from this one edge.
    ///
    /// ---- THE OFFICE IS VERIFIED -------------------------------------------------
    ///
    /// `authored_from_role(zengine.introspection)`, for the pane protocol's reason
    /// exactly: personal speech carries no verifiable author, and a fact about a
    /// maker's gesture is worth exactly as much as the office it came from. Any
    /// weave granted this shape could otherwise retarget a maker's Composer.
    ///
    /// ---- THE SAME SELECTION TWICE ASKS TWICE ------------------------------------
    ///
    /// `LoadedSelected` is an OCCURRENCE and not a transition -- SEL-0 published it
    /// that way deliberately -- so pressing the same Loaded row again is a maker
    /// asking again, and it produces a second `zen.DescribeAccepted`. Suppressing it
    /// because `new_role == old_role` would throw away the only honest refresh
    /// gesture in the product and leave nothing but a poll in its place.
    ///
    /// ---- AND IT DECIDES NOTHING ABOUT ANY OTHER PANE ---------------------------
    ///
    /// It does not open this pane, close it, hide another, move one, change the
    /// setup, or reach the tool whose fact it just heard. If the Composer is not
    /// open, a selection updates a target nobody is looking at, and that is the
    /// whole of it.
    void on_selected(const LoadedSelected& sel, loom::Mail& mail) {
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
        // one target's shapes in front of a maker aiming at another.
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

    /// ASK THE TARGET WHAT IT ACCEPTS.
    ///
    /// BY ROLE, never by `WeaveId`: the office is the address that survives its
    /// holder being replaced, and this weave never learns an id for it anyway.
    ///
    /// ONE QUESTION OUTSTANDING AT A TIME, and the correlation is what makes a stale
    /// answer harmless. A second selection replaces the correlation rather than
    /// queueing a second question, so an answer to the target before it arrives
    /// carrying a number this weave is no longer waiting on and is dropped.
    ///
    /// THAT IS THE WHOLE OF THE BOUND, AND IT IS STATED RATHER THAN OVERSOLD. The
    /// answer is sent PERSONALLY by the construction layer (`answer_substrate`:
    /// `bus.send(to, Message(answer, self_, self_, in.correlation))`), so there is
    /// no authored office on it to verify; and `send_to_role` never told this weave
    /// which incarnation the question resolved to, so there is no expected sender
    /// either. What can be checked is that the correlation is one this incarnation
    /// minted and has not retired. The population that could forge one is every
    /// dynamic weave in this process, each of which already holds `allow_any()` from
    /// the loader -- that is the process tier's problem and not a claim this seam
    /// makes.
    void ask(loom::Mail& mail) {
        // ONE CORRELATION SEQUENCE FOR THIS WHOLE WEAVE (QR-11): minted from the clipboard
        // book so the discovery conversation can never share a number with an open paste
        // ask — two counters beside each other is how an answer to one conversation
        // settles the other (`AskBook::mint_correlation`'s own warning).
        pending_ = clip_asks_.mint_correlation();
        awaiting_ = true;
        ++state_.asked;
        (void)mail.as_role(kComposerRole)
            .send_to_role(composing_.role, loom::DescribeAccepted{}, pending_);
    }

    /// THE TARGET'S ANSWER, AND THE MOMENT THIS PANE LEARNS A VOCABULARY.
    ///
    /// A FRESH REGISTRY PER SNAPSHOT, and the reasoning is in `Snapshot`'s own
    /// comment: `register_schema` takes a claim nobody ever releases, so one
    /// long-lived Registry would accumulate every vocabulary a maker ever looked at
    /// and would become the schema catalog this Loom deliberately does not have.
    ///
    /// THE CLOSURE IS REGISTERED BEFORE THE ROOTS ARE DECODED, in that order,
    /// because that is the order the encoder guarantees: `referenced` is in
    /// post-order, so entry N+1's type tokens resolve against what entries 0..N have
    /// already put there, and a root that nests anything resolves against all of
    /// them.
    ///
    /// AND IT CAN FAIL. `decode_schema` throws on a mis-ordered closure or a type
    /// nested past the codec's depth cap; a hand-built answer is a thing an
    /// `allow_any` process can produce. The failure becomes this pane's own
    /// sentence rather than a half-built vocabulary -- the snapshot is left empty
    /// and the maker is told the answer could not be read.
    void on_described(const loom::Value& answer, loom::Mail& mail) {
        if (!awaiting_ || mail.correlation() != pending_) {
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

    /// THE TARGET, OR THE BUS, DECLINING. It RETIRES THE QUESTION, which is the only
    /// state it owns -- leaving `awaiting_` standing would make a later answer
    /// bearing this same correlation, a number this incarnation will not mint twice,
    /// look like the answer to a question that was already closed.
    ///
    /// AND IT IS NOT MADE INTO A CLAIM ABOUT THE TARGET. A refusal here may be the
    /// gate, an office nobody holds, or a weave that does not accept the shape, and
    /// this pane cannot tell them apart -- so it says what it observed and stops.
    void on_refused(loom::Mail& mail) {
        if (!awaiting_ || mail.correlation() != pending_) {
            return;
        }
        awaiting_ = false;
        complain("the discovery request was refused");
        say(mail);
    }

    // ---- what a maker does with what arrived --------------------------------

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

    /// `enter` -- and it means whatever the row under the cursor means.
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

    /// CHOOSING A ROOT OPENS A FORM ON IT -- generated from the runtime `Schema` and
    /// from nothing else.
    ///
    /// A NEW CHOICE REPLACES THE DRAFT WHOLE. `begin_draft` starts every field
    /// absent and every value empty, so nothing survives from the previous shape:
    /// two shapes with a field of the same name are two different messages, and
    /// carrying a value across would put a maker's answer to one question under
    /// another one.
    void open_form(std::int64_t which, loom::Mail& mail) {
        if (which < 0 || which >= static_cast<std::int64_t>(composing_.snapshot.roots.size())) {
            return;
        }
        composing_.draft =
            zengine::composer::begin_draft(composing_.snapshot.roots[static_cast<std::size_t>(which)]);
        ++draft_generation_; // a new form; a paste asked for by the old one has no home (QR-11)
        composing_.stage = stage::kForm;
        composing_.cursor = 0;
        composing_.notice = "up/down move, tab include, enter acts, esc back";
        composing_.notice_role = surface::role::kMuted;
        say(mail);
    }

    /// Back to the catalog, which DROPS the draft.
    ///
    /// `esc` IS BACK AND NOT CANCEL, which is this application's own word for it:
    /// every immediate-commit gesture here is reversible only by performing the
    /// inverse, and there is no undo. Going back to the catalog and choosing the
    /// same shape again gives a fresh form, because that is what choosing a shape
    /// does.
    void back_to_catalog(loom::Mail& mail) {
        if (composing_.stage != stage::kForm) {
            return;
        }
        composing_.draft = MessageDraft{};
        ++draft_generation_; // the dropped form takes its in-flight paste with it (QR-11)
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
        if (zengine::composer::composability(kind) != zengine::composer::Composability::kScalar) {
            return; // a field this pane cannot author has no presence to give it
        }
        zengine::composer::cycle(*d, kind);
    }

    /// The editing keys, spent on the `TextBox` under the cursor — HD-5's component,
    /// fourth consumer, now through the vocabulary the component owns (TEXT-0): the six
    /// gestures this used to spell, and selection, clipboard, word movement and history
    /// behind them, one call. QR-2's bool: false means "not the field's", so a chord this
    /// pane binds (or ignores) still reaches its own switch.
    ///
    /// THE HONEST LIMIT, stated where the capability lives: the pane seam carries ROWS and
    /// no spans (`PaneContent`'s own discipline — a provider supplies no geometry), so a
    /// selection in a field here moves the caret character to its active end and cannot be
    /// shown as a highlight until the seam can carry one. The mechanics are uniform anyway
    /// — typing replaces what Shift+arrows swept, and Ctrl+Z takes it back — because a
    /// vocabulary that shrank per consumer would be four vocabularies again.
    bool edit_field(std::int64_t scancode, std::int64_t modifiers) {
        zengine::composer::FieldDraft* d = field_under_cursor();
        if (d == nullptr || !zengine::composer::typeable(kind_under_cursor()) || !d->present) {
            return false;
        }
        return d->value.consume(scancode, modifiers, clip_);
    }

    /// ONE PASTE STILL IN FLIGHT, and the field it belongs to (QR-11). The generation
    /// says WHICH form (open_form and back_to_catalog bump it — "a new choice replaces
    /// the draft whole", so text asked for by one form must not land in the next, even a
    /// re-opened form of the same shape); the index says which field; the epoch says the
    /// field's box has not been reset under it (`TextBox::draft_epoch`).
    struct PendingPaste {
        std::uint64_t ask = 0;
        std::uint64_t generation = 0;
        std::int64_t field = 0;
        std::uint64_t epoch = 0;
    };

    /// ASK THE SKIN WHAT THE PLATFORM CLIPBOARD HOLDS, because the field under the
    /// cursor consumed a paste request (QR-11). The same conversation Workshop has —
    /// the Medium owns the platform clipboard in both directions — held in this asker's
    /// own book: refused at capacity with the outstanding pastes untouched, and the
    /// dropped paste's truthful outcome is that nothing is inserted.
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

    /// THE SKIN'S ANSWER TO A PASTE THIS PANE REQUESTED — the one road foreign clipboard
    /// text has into this provider, walked only under a maker's paste (QR-11; Workshop's
    /// `on(ClipboardText)` states the shared law). `answers_ask()` plus the book's own
    /// settlement, then the field that asked must still be standing: same form
    /// (generation), same field, same draft in its box (epoch), still present — the same
    /// gate `edit_field` spends. Anything else discards the payload whole; a paste is
    /// never redirected to whichever field holds the cursor later.
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

    /// COMPOSE, ASSEMBLE, SEND -- and then say `SUBMITTED` and nothing more.
    ///
    /// ---- EVERY REFUSAL BELOW IS LOOM'S, IN LOOM'S WORDS ------------------------
    ///
    /// `compose_message` answers `Ready`, `NeedsInput` or `Error`, and the three are
    /// three different sentences for a maker: `Error` means this can never be what
    /// you meant (a value that does not fit its field's declared kind), `NeedsInput`
    /// means a required field is still open. This pane repeats them and invents
    /// none: it holds the vocabulary for "not a valid Int" only because the ladder
    /// does, and a second copy of that judgement here would eventually disagree with
    /// the one that actually decides.
    ///
    /// ---- WHAT IT DOES NOT AND CANNOT CHECK ------------------------------------
    ///
    /// Whether the id exists, whether the target is in a state where this operation
    /// makes sense, whether an earlier message had to succeed first, whether the
    /// value is in a range nobody declared, whether this office is permitted to say
    /// it, and whether the delivery will happen at all. None of those is knowable
    /// from a schema, and this pane says nothing about any of them.
    ///
    /// ---- SO THE WORD IS `SUBMITTED` -------------------------------------------
    ///
    /// It is the strongest true word available. `Sent` would imply the bus took it,
    /// `Delivered` that it arrived, `Accepted` that the target's gate passed it, and
    /// `Timer created` that something happened. The Ticket a send returns is not
    /// checked for a reason: an office send answers whether the AUTHORSHIP was
    /// permitted, which is one of five things that must go right and would be the
    /// most misleading of them to report as success.
    void submit(loom::Mail& mail) {
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
        ++state_.submitted;
        // AS THE OFFICE, and to a ROLE. The office is the only verifiable thing a
        // target can learn about where this came from -- a maker has no identity in
        // this Loom -- and the role resolves at DELIVERY, so a target replaced since
        // the maker pointed at it still receives the message they addressed to that
        // office. No `WeaveId` is pinned anywhere in this tool for exactly that
        // reason.
        (void)mail.bus().office_send_to_role(
            kComposerRole, composing_.role,
            loom::Message(loom::assemble(made), loom::WeaveId{}, loom::WeaveId{}, 0));
        composing_.notice = "SUBMITTED -- a sender is not told its fate";
        composing_.notice_role = surface::role::kAccent;
        say(mail);
    }

    // ---- saying it ----------------------------------------------------------

    void complain(std::string what) {
        composing_.notice = std::move(what);
        composing_.notice_role = surface::role::kAlert;
    }

    /// One offer, authored as this office and addressed to the Workshop office.
    void announce(loom::Mail& mail) {
        ++state_.offers;
        (void)mail.as_role(kComposerRole)
            .send_to_role(kWorkshopRole,
                          PaneOffered{kComposePane, kComposePaneName, kComposePaneSummary});
    }

    /// SAY WHAT THIS PANE NOW SHOWS -- the one place content leaves this weave, and
    /// the one place the projection is built.
    ///
    /// THE CARET'S WINDOW IS RECONCILED FIRST, against the SAME capacity the
    /// projector is about to cut the value with (`value_capacity`, called by both).
    /// HD-4 paid for learning that a second copy of a window's capacity is right
    /// until the first value long enough to scroll.
    ///
    /// DELIBERATELY AS THIS OFFICE. `mail.send_to_role(...)` would be PERSONAL
    /// speech from a weave that happens to hold the office, and Workshop drops it --
    /// holding is never speaking-for.
    void say(loom::Mail& mail) {
        if (zengine::composer::FieldDraft* d = field_under_cursor()) {
            if (zengine::composer::typeable(kind_under_cursor())) {
                d->value.keep_caret_visible(zengine::composer::value_capacity(
                    composing_.draft, static_cast<std::size_t>(composing_.cursor), columns_));
            }
        }
        shown_ = zengine::composer::project(composing_, rows_, columns_);
        PaneContent said;
        said.pane = kComposePane;
        said.rows = zengine::composer::rows_of(shown_);
        (void)mail.as_role(kComposerRole).send_to_role(kWorkshopRole, said);
    }

    ComposerState state_;
    zengine::ActivationCursor activation_;
    /// THE LAST ROOM GRANTED, and it is NOT state. A snapshot that carried it would
    /// revive an incarnation believing it holds a grant Workshop's own cache says it
    /// does not; the grant is re-sent whenever a valid offer refreshes the pane.
    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    std::uint64_t pending_ = 0; ///< the outstanding discovery question, if any
    bool awaiting_ = false;
    /// THE ASKER'S OWN BOOK OF PASTES STILL IN FLIGHT (QR-11), and the fields each one
    /// belongs to. Per incarnation, like every other transient here — and the book's
    /// counter is this weave's ONE correlation sequence: the discovery ask above mints
    /// from it too (`mint_correlation`), so no two of this weave's conversations can
    /// share a number.
    loom::AskBook clip_asks_{2};
    std::vector<PendingPaste> pending_pastes_;
    std::uint64_t draft_generation_ = 0; ///< bumped by open_form/back_to_catalog (QR-11)
    /// THE TARGET, THE VOCABULARY AND THE DRAFT -- transient, local, and in no state
    /// shape. See `ComposerState`.
    zengine::composer::Composing composing_;
    /// THE CLIPBOARD THIS PANE'S FIELDS OPERATE ON (TEXT-0) — transient for the draft's own
    /// reason: what a maker copied is part of what they are doing, and a revived
    /// incarnation holding a dead pane has no business resurrecting it. A MIRROR of the
    /// freshest copy said IN this process (its own copies, other participants'
    /// `ClipboardCopy`) — since QR-11 nothing watches the platform's clipboard; a paste
    /// reads it through the Skin at the moment it is requested — so copy-in-the-Terminal,
    /// paste-here works wherever the process's clipboard story does.
    zengine::component::Clipboard clip_;
    /// WHAT THIS PANE IS CURRENTLY SHOWING, and the map from its rows back to the
    /// items they name. It is the PRESENTATION and not an inventory: it holds only
    /// what reached a row, is bounded by the granted room rather than by the
    /// population, and is replaced whole by every projection. Nothing consults it to
    /// decide what is true.
    zengine::composer::ComposerView shown_;
};

} // namespace

ZEN_EXPORT_WEAVE(ComposerWeave)
