// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE OPENING MANAGER'S OWN WITNESSES (WL-OPEN, agents/workshop/opening.md): the record it
// retains and retires, the authority it holds, and the host's turn of the bus when a NATIVE
// owner cannot apply a publication. The Editor's office is held here by a native stand-in --
// test instrumentation, labeled as such -- that offers the pane, claims a document identity,
// prepares and offers like the real image, and, armed, fails its showing once. Nothing here
// is a document: the real loaded Editor's own cases stand in test_workshop_panes_editor.cpp,
// and the loaded owner that fails through the real ABI is EDIT-W78 there. What these prove is
// manager bookkeeping, the capability's binding and the host's attribution -- with the real
// Workshop as the desk, the real manager as the operator and the real pump seam as the loop.

#include "workshop_support.hpp"

#include "workshop/host_pump.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/opening.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct NativeEditorState {
    std::int64_t published_seen = 0; ///< showings attempted (the failure included)
    std::int64_t applied = 0;        ///< showings completed
    std::int64_t prepared = 0;       ///< preparations answered
    std::string path;                ///< the document this stand-in stands behind
    std::int64_t doc_epoch = 0;
    ZEN_SHAPE(NativeEditorState, 1, ZEN_FIELD(published_seen), ZEN_FIELD(applied),
              ZEN_FIELD(prepared), ZEN_FIELD(path), ZEN_FIELD(doc_epoch));
};

/// A NATIVE STAND-IN FOR THE EDITOR (test instrumentation). Its showing hook throws once when
/// armed -- the native failure Loom records first and re-raises to the host afterwards -- and
/// the arming is cleared before the throw, so the successor of a swap applies. Its ordinary
/// nudge handler can be armed to throw instead: the control an unrelated exception needs.
class NativeEditor
    : public loom::WeaveBase<NativeEditor, NativeEditorState,
                             loom::Accept<PaneCatalogRequested, PaneRoom, PrepareSourceRequested,
                                          ManagedOpenProgress, ManagedOpenSettled, SeatDo>,
                             loom::Emit<PaneOffered, SourcePrepared>,
                             loom::Claims<EditorDocument>> {
public:
    bool fail_next = false;        ///< armed: the next showing throws, once
    bool throw_in_handler = false; ///< armed: the next nudge's handler throws, once
    std::vector<ManagedOpenSettled> settled;

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopProvider)) {
            return;
        }
        (void)mail.as_role(kEditorRole)
            .send_to_role(kWorkshopProvider,
                          PaneOffered{"editor", "Editor", "a native stand-in for the Editor"});
    }
    void on(const PaneRoom&, loom::Mail&) {}
    /// THE NUDGE: claim the current document identity from inside a delivery, so an operation
    /// can bind it -- or, as the control, fail abnormally the way any handler might.
    void on(const SeatDo&, loom::Mail& mail) {
        if (throw_in_handler) {
            throw_in_handler = false;
            throw std::runtime_error("test instrumentation: the stand-in's handler failed abnormally");
        }
        EditorDocument now;
        now.path = state_.path;
        now.doc_epoch = state_.doc_epoch;
        (void)mail.claim(now);
    }
    /// PREPARE AND OFFER, as the real Editor does: an identity for the operation, one row for
    /// the trial room, and the answer. No file is read; a stand-in has no document.
    void on(const PrepareSourceRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kOpeningRole)) {
            return;
        }
        ++state_.prepared;
        EditorDocument offered;
        offered.path = asked.path;
        offered.doc_epoch = state_.doc_epoch + 1;
        offered.opened_by = asked.op;
        const loom::JointResult r = mail.offer(static_cast<std::uint64_t>(asked.op), offered);
        SourcePrepared said;
        said.op = asked.op;
        said.ok = r.ok;
        if (!r.ok) {
            said.refusal = "the stand-in's offer was refused (" +
                           std::string(loom::name_of(r.why)) + ")";
        }
        said.generation = offered.doc_epoch;
        surface::SurfaceTextRow row;
        row.text = "stand-in: " + asked.path;
        if (asked.columns > 0 && static_cast<std::int64_t>(row.text.size()) > asked.columns) {
            row.text.resize(static_cast<std::size_t>(asked.columns));
        }
        said.rows.push_back(row);
        said.caret_row = 0;
        said.caret_col = 0;
        (void)mail.answer(said);
    }
    void on(const ManagedOpenProgress&, loom::Mail&) {}
    void on(const ManagedOpenSettled& said, loom::Mail&) { settled.push_back(said); }

    /// THE SHOWING: counted, then applied -- or, once, failed before applying anything.
    void on_claim_published(const EditorDocument& published) {
        ++state_.published_seen;
        if (fail_next) {
            fail_next = false;
            throw std::runtime_error("test instrumentation: the stand-in Editor could not apply " +
                                     published.path);
        }
        state_.path = published.path;
        state_.doc_epoch = published.doc_epoch;
        ++state_.applied;
    }

    const NativeEditorState& state() const { return state_; }
};

// ---- Unrelated operators, to fill the bus's slots ---------------------------------------

struct SlotFact {
    std::string v;
    ZEN_SHAPE(SlotFact, 1, ZEN_FIELD(v));
};
struct SlotState {
    std::string v = "a";
    ZEN_SHAPE(SlotState, 1, ZEN_FIELD(v));
};
class SlotOwner : public loom::WeaveBase<SlotOwner, SlotState, loom::Accept<SeatDo>, loom::Emit<>,
                                         loom::Claims<SlotFact>> {
public:
    void on(const SeatDo&, loom::Mail& mail) { claimed = mail.claim(SlotFact{state_.v}); }
    loom::SenseClaimResult claimed{};
};
struct FillerState {
    std::int64_t n = 0;
    ZEN_SHAPE(FillerState, 1, ZEN_FIELD(n));
};
/// AN OPERATOR OF ITS OWN that begins one live operation per owner it is handed, and keeps
/// every one Preparing: what the manager meets as `Exhausted` at its next begin.
class SlotFiller : public loom::WeaveBase<SlotFiller, FillerState, loom::Accept<SeatDo>,
                                          loom::Emit<>> {
public:
    void on(const SeatDo&, loom::Mail& mail) {
        for (const loom::WeaveId id : owners) {
            begun.push_back(mail.begin_joint(authority, {loom::claim_key<SlotFact>(id)}));
        }
    }
    loom::JointAuthority authority;
    std::vector<loom::WeaveId> owners;
    std::vector<loom::JointBegin> begun;
};

/// `count` unrelated live operations on a rig's bus, each over its own owner's claim.
struct Slots {
    std::vector<loom::WeaveId> owners;
    SlotFiller* filler = nullptr;
    loom::WeaveId filler_id{};

    Slots(PaneRig& r, std::size_t count) {
        std::vector<std::string> roles;
        for (std::size_t i = 0; i < count; ++i) {
            const std::string role = "test.slot." + std::to_string(i + 1);
            auto owner = std::make_unique<SlotOwner>();
            SlotOwner* raw = owner.get();
            const loom::WeaveId id =
                r.bus.register_weave(std::move(owner), loom::Grant{}, std::string(role));
            raw->zen_set_self(id);
            (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                               loom::WeaveId{}, 0));
            r.bus.drain_until_idle();
            REQUIRE(raw->claimed.accepted);
            owners.push_back(id);
            roles.push_back(role);
        }
        auto f = std::make_unique<SlotFiller>();
        filler = f.get();
        filler_id = r.bus.register_weave(std::move(f), loom::Grant{}, std::string("test.filler"));
        filler->zen_set_self(filler_id);
        filler->authority = r.bus.mint_joint_authority(filler_id, roles);
        filler->owners = owners;
        (void)r.bus.send(filler_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
        REQUIRE(filler->begun.size() == count);
        for (const loom::JointBegin& b : filler->begun) {
            REQUIRE_MESSAGE(b.ok, loom::name_of(b.why));
        }
    }
};

// ---- The rig ------------------------------------------------------------------------------

/// THE REAL DESK, THE REAL MANAGER, THE REAL PUMP SEAM, and a native stand-in in the Editor's
/// office. Every turn of the bus goes through `serve_until_idle`, the seam under test.
struct OpeningRig {
    TempDir dir;
    std::filesystem::path root;
    PaneRig r;
    NativeEditor* editor = nullptr;
    loom::WeaveId editor_id{};
    DoorAsker* asker = nullptr;
    std::int64_t kind = 0;
    std::vector<std::string> told; ///< what the pump seam told this host, in order

    explicit OpeningRig(const char* tag) : dir(tag) {
        root = dir.path();
        r.host.project_dir = root.generic_string();
    }

    void open() {
        r.host.managed_pane = PaneRef{kEditorRole, "editor"};
        r.mount_workshop();
        r.mount_opening();
        auto seat = std::make_unique<NativeEditor>();
        editor = seat.get();
        loom::Grant say;
        say.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
        say.allow_to_any(SourcePrepared::zen_name, SourcePrepared::zen_version);
        editor_id = r.bus.register_weave(std::move(seat), std::move(say), std::string(kEditorRole));
        editor->zen_set_self(editor_id);
        r.ready();
        r.extent(160, 48);
        const RuntimePane* row = r.session().panels.runtime.find(kEditorRole, "editor");
        REQUIRE_MESSAGE(row != nullptr, "the stand-in offered no `editor` pane");
        kind = row->kind;
        // THE STAND-IN CLAIMS ITS (EMPTY) DOCUMENT IDENTITY, so an operation can bind it.
        nudge(editor_id);
        (void)serve();
        REQUIRE(r.bus.observe(editor_id, EditorDocument::zen_name, EditorDocument::zen_version)
                    .value.has_value());
        mount_asker();
    }

    void mount_asker() {
        auto held = std::make_unique<DoorAsker>(std::string(kDoorAskerOffice));
        asker = held.get();
        loom::Grant grant;
        grant.allow_to_any(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version);
        const loom::WeaveId id =
            r.bus.register_weave(std::move(held), std::move(grant), std::string(kDoorAskerOffice));
        asker->zen_set_self(id);
        asker->id = id;
    }

    void nudge(loom::WeaveId to) {
        (void)r.bus.send(to, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                           loom::WeaveId{}, 0));
    }
    /// THE HOST'S TURN, through the seam under test.
    ServedTurn serve() {
        return serve_until_idle(r.bus, [this](const std::string& said) { told.push_back(said); });
    }
    /// Queue an open at the manager's door; nothing is drained.
    void enqueue_open(const std::string& path) {
        asker->next = [path](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kOpeningRole, OpenSourceRequested{path});
        };
        nudge(asker->id);
    }
    /// Ask, then serve turn by turn until the requester hears.
    SourceOpened open_and_serve(const std::string& path) {
        const std::size_t before = asker->opens.size();
        enqueue_open(path);
        for (int i = 0; i < 12 && asker->opens.size() == before; ++i) {
            (void)serve();
        }
        REQUIRE_MESSAGE(asker->opens.size() == before + 1, "the requester was never answered");
        return asker->opens.back();
    }
    const OpeningState& opening() { return r.opening->state(); }
};

} // namespace

// ============================================================================
// Records: retained for a repair's late word, retired by any newer terminal outcome
// ============================================================================

TEST_CASE("OPEN-W1: a newer immediate refusal retires the older retained repair record -- the repair's late word cannot overwrite it, the claimant's hold is untouched, and the freed slot lets the next open take") {
    OpeningRig o("open-retention");
    o.open();
    // AN OPEN WHOSE OWNER CANNOT APPLY IT: published, failed at the stand-in's showing, and
    // the record RETAINED by the manager for the late word about the owner's repair.
    o.editor->fail_next = true;
    const std::size_t before = o.asker->opens.size();
    o.enqueue_open("/x/b.cpp");
    int turns = 0;
    ServedTurn turn = o.serve();
    while (turn.outcome != ServedTurn::Outcome::kHeldParticipant) {
        REQUIRE(++turns < 8);
        turn = o.serve();
    }
    CHECK(turn.held == o.editor_id);
    while (o.asker->opens.size() == before) {
        REQUIRE(++turns < 16);
        (void)o.serve();
    }
    const SourceOpened failed = o.asker->opens.back();
    CHECK_FALSE(failed.accepted);
    CHECK(failed.refusal.find("could not apply") != std::string::npos);
    const std::int64_t op = o.opening().last_op;
    REQUIRE(op != 0);
    CHECK(o.opening().last_outcome == "committed, application failed");
    CHECK(o.opening().retained == op);
    CHECK(o.r.bus.has_failed_application(o.editor_id));
    CHECK(o.r.bus.joint_records() == 1);
    // THE REST OF THE BUS'S SLOTS, taken by an unrelated operator's live operations.
    Slots slots(o.r, loom::Switchboard::kMaxJointOperations - 1);
    CHECK(o.r.bus.joint_records() == loom::Switchboard::kMaxJointOperations);
    // A NEWER OPEN MEETS THE BOUND, in words -- an immediate terminal outcome that takes the
    // manager's public result...
    const SourceOpened exhausted = o.open_and_serve("/x/c.cpp");
    CHECK_FALSE(exhausted.accepted);
    CHECK(exhausted.refusal.find("too many opens") != std::string::npos);
    CHECK(exhausted.refusal.find("c.cpp") != std::string::npos);
    CHECK(o.opening().last_outcome == "refused");
    CHECK(o.opening().last_refusal == exhausted.refusal);
    CHECK(o.opening().stage == "idle");
    // ...AND RETIRES THE OLDER RETAINED RECORD UNDER THE SAME POLICY AS A SETTLEMENT: the
    // record is released, its slot is free, and the claimant's hold is exactly as it was --
    // releasing a record repairs nothing.
    CHECK(o.opening().retained == 0);
    CHECK(o.r.bus.joint_status(static_cast<std::uint64_t>(op)).state == loom::JointState::Missing);
    CHECK(o.r.bus.joint_records() == loom::Switchboard::kMaxJointOperations - 1);
    CHECK(o.r.bus.has_failed_application(o.editor_id));
    // THE REPAIR OF THE OLD OWNER, natively: the successor is shown the value at its first
    // delivery and applies it. Its late word consults a record this manager no longer holds,
    // so the newer refusal stands as the manager's result, unmixed with the old path.
    std::string bytes;
    CHECK_NOTHROW(bytes = o.r.bus.snapshot_bytes(o.editor_id,
                                                 loom::Switchboard::SnapshotAccess::Reload));
    REQUIRE(o.r.bus.swap_state(o.editor_id, bytes).revived);
    o.nudge(o.editor_id);
    (void)o.serve();
    CHECK_FALSE(o.r.bus.has_failed_application(o.editor_id));
    CHECK(o.editor->state().applied == 1);
    CHECK(o.editor->state().path == "/x/b.cpp");
    CHECK(o.opening().last_outcome == "refused");
    CHECK(o.opening().last_refusal == exhausted.refusal);
    CHECK(o.opening().retained == 0);
    CHECK(o.opening().unapplied == 1);
    CHECK(o.opening().committed == 0);
    // SUBSEQUENT VALID WORK: with one slot free, the same request takes.
    const SourceOpened again = o.open_and_serve("/x/c.cpp");
    CHECK_MESSAGE(again.accepted, again.refusal);
    CHECK(o.opening().last_outcome == "committed");
    CHECK(o.opening().committed == 1);
    CHECK(o.editor->state().path == "/x/c.cpp");
    CHECK(o.r.bus.joint_records() == loom::Switchboard::kMaxJointOperations - 1); // released
    // ...AND THE OTHER OPERATOR'S RECORDS ARE EXACTLY AS THEY WERE.
    for (const loom::JointBegin& b : slots.filler->begun) {
        CHECK(o.r.bus.joint_status(b.op).state == loom::JointState::Preparing);
    }
}

// ============================================================================
// Authority: the exact life and incarnation, and the host's reauthorization
// ============================================================================

TEST_CASE("OPEN-W2: the manager's authority names its exact incarnation -- a swapped manager's retained capability is refused in words, and the host authorizes the successor by minting again") {
    OpeningRig o("open-reauthorize");
    o.open();
    REQUIRE(o.open_and_serve("/x/a.cpp").accepted);
    CHECK(o.editor->state().path == "/x/a.cpp");
    // THE MANAGER'S CODE IS SWAPPED IN PLACE: same id, same object, new incarnation -- and the
    // capability it kept in its own member names the incarnation that is gone.
    std::string bytes;
    CHECK_NOTHROW(bytes = o.r.bus.snapshot_bytes(o.r.opening_id,
                                                 loom::Switchboard::SnapshotAccess::Reload));
    REQUIRE(o.r.bus.swap_state(o.r.opening_id, bytes).revived);
    const SourceOpened stale = o.open_and_serve("/x/b.cpp");
    CHECK_FALSE(stale.accepted);
    CHECK(stale.refusal.find("NotOperator") != std::string::npos);
    CHECK(o.opening().last_outcome == "refused");
    CHECK(o.opening().stage == "idle");
    CHECK(o.r.bus.joint_pending() == 0);
    CHECK(o.editor->state().path == "/x/a.cpp");
    // THE HOST MINTS AGAIN, FOR THE LIFE AND INCARNATION THAT EXISTS, and the open takes.
    o.r.opening->set_authority(o.r.bus.mint_joint_authority(
        o.r.opening_id, {std::string(kEditorRole), std::string(kWorkshopProvider)}));
    const SourceOpened fresh = o.open_and_serve("/x/b.cpp");
    CHECK_MESSAGE(fresh.accepted, fresh.refusal);
    CHECK(o.editor->state().path == "/x/b.cpp");
    CHECK(o.opening().committed == 2);
}

// ============================================================================
// The pump seam: a native showing failure attributed from the record, everything else propagates
// ============================================================================

TEST_CASE("OPEN-W3: the host's turn attributes a native showing failure from Loom's record and serves on, and an exception the record does not explain propagates") {
    SUBCASE("a native owner's showing throws: named from the record, told once, served on, and the open settles in words") {
        OpeningRig o("open-pump-held");
        o.open();
        o.editor->fail_next = true;
        const std::size_t before = o.asker->opens.size();
        o.enqueue_open("/x/b.cpp");
        int turns = 0;
        ServedTurn turn = o.serve();
        while (turn.outcome != ServedTurn::Outcome::kHeldParticipant) {
            REQUIRE(++turns < 8);
            turn = o.serve();
        }
        CHECK(turn.held == o.editor_id);
        CHECK(turn.office == kEditorRole);
        CHECK(turn.diagnostic.find("could not be applied by " + std::string(kEditorRole)) !=
              std::string::npos);
        CHECK(turn.diagnostic.find("stand-in Editor could not apply") != std::string::npos);
        REQUIRE(o.told.size() == 1);
        CHECK(o.told[0] == turn.diagnostic);
        CHECK(o.r.bus.has_failed_application(o.editor_id));
        // THE NEXT TURN DELIVERS: the manager settles from the record, and the requester and
        // the desk are told which owner is held.
        while (o.asker->opens.size() == before) {
            REQUIRE(++turns < 16);
            const ServedTurn next = o.serve();
            CHECK(next.outcome == ServedTurn::Outcome::kIdle);
        }
        CHECK_FALSE(o.asker->opens.back().accepted);
        CHECK(o.asker->opens.back().refusal.find(kEditorRole) != std::string::npos);
        CHECK(o.r.session().notice.find("could not apply") != std::string::npos);
        CHECK(o.told.size() == 1);
    }
    SUBCASE("an ordinary handler that throws propagates out of the turn, told to nobody, and the bus serves the next turn") {
        OpeningRig o("open-pump-handler");
        o.open();
        o.editor->throw_in_handler = true;
        o.nudge(o.editor_id);
        CHECK_THROWS_AS((void)o.serve(), std::runtime_error);
        CHECK(o.told.empty());
        CHECK_FALSE(o.r.bus.has_failed_application(o.editor_id));
        const SourceOpened next = o.open_and_serve("/x/a.cpp");
        CHECK_MESSAGE(next.accepted, next.refusal);
        CHECK(o.told.empty());
    }
    SUBCASE("an owner held since an earlier turn explains nothing: a later unrelated exception still propagates, even behind a refused delivery to the held owner") {
        OpeningRig o("open-pump-unrelated");
        o.open();
        o.editor->fail_next = true;
        o.enqueue_open("/x/b.cpp");
        int turns = 0;
        ServedTurn turn = o.serve();
        while (turn.outcome != ServedTurn::Outcome::kHeldParticipant) {
            REQUIRE(++turns < 8);
            turn = o.serve();
        }
        (void)o.serve();
        REQUIRE(o.r.bus.has_failed_application(o.editor_id));
        REQUIRE(o.told.size() == 1);
        // ONE TURN: a delivery to the held owner, refused `ApplicationFailed` with no exception,
        // then a handler of another weave that throws.
        o.nudge(o.editor_id);
        o.asker->next = [](DoorAsker&, loom::Mail&) {
            throw std::runtime_error("test instrumentation: an unrelated handler failed");
        };
        o.nudge(o.asker->id);
        CHECK_THROWS_AS((void)o.serve(), std::runtime_error);
        CHECK(o.told.size() == 1);                           // nothing new was attributed
        CHECK(o.r.bus.has_failed_application(o.editor_id)); // still held, still its own repair
    }
}
