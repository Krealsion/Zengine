// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE OPENING MANAGER'S OWN WITNESSES (WL-OPEN, agents/workshop/opening.md): the record it
// retains and retires, the latest result and whose request it answers, the authority it holds,
// and the host's turn when a NATIVE owner cannot apply a publication. The Editor's office is held
// by a native stand-in (test instrumentation) that prepares and offers like the real image; the
// real Editor is test_workshop_panes_editor.cpp's. Proved here: manager bookkeeping, the
// capability's binding and the host's showing boundary, over the real desk, manager and pump.

#include "workshop_support.hpp"

#include "workshop/host_pump.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/opening.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
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

/// A NATIVE STAND-IN FOR THE EDITOR (test instrumentation). Its showing applies inside the host's
/// boundary (`contain_showing`), as Workshop's desk applies its own: armed, it throws there once
/// -- Failed, in its words, kept in the host's book -- or declines once; armed the third way the
/// hook throws once OUTSIDE any boundary, which Loom re-raises at the host's turn. Every arming
/// clears before it fires, so a swap's successor applies; the nudge handler can be armed to throw.
class NativeEditor
    : public loom::WeaveBase<NativeEditor, NativeEditorState,
                             loom::Accept<PaneCatalogRequested, PaneRoom, PrepareSourceRequested,
                                          ManagedOpenProgress, ManagedOpenSettled, SeatDo>,
                             loom::Emit<PaneOffered, SourcePrepared>,
                             loom::Claims<EditorDocument>> {
public:
    bool fail_next = false;        ///< armed: the next showing throws inside its boundary, once
    bool decline_next = false;     ///< armed: the next showing declines, once
    bool throw_raw_next = false;   ///< armed: the next showing throws outside any boundary, once
    bool throw_in_handler = false; ///< armed: the next nudge's handler throws, once
    ShowingFailures* book = nullptr; ///< the host's book, handed over as the host hands Workshop it
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

    /// THE SHOWING: counted, then applied inside the host's boundary -- or, once each, failed
    /// there before applying anything, declined, or thrown outside the boundary altogether.
    loom::Weave::PublishedClaim on_claim_published(const EditorDocument& published) {
        ++state_.published_seen;
        if (throw_raw_next) {
            throw_raw_next = false;
            throw std::runtime_error(
                "test instrumentation: the stand-in Editor threw outside its boundary applying " +
                published.path);
        }
        return contain_showing(book, self_, [this, &published] {
            if (fail_next) {
                fail_next = false;
                throw std::runtime_error(
                    "test instrumentation: the stand-in Editor could not apply " + published.path);
            }
            if (decline_next) {
                decline_next = false;
                return loom::Weave::PublishedClaim::Declined;
            }
            state_.path = published.path;
            state_.doc_epoch = published.doc_epoch;
            ++state_.applied;
            return loom::Weave::PublishedClaim::Applied;
        });
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
/// every one Preparing: what the manager meets as `Exhausted` at its next begin. Handed an
/// operation to `release`, its next delivery ends and releases that one instead -- capacity
/// returning by the other operator's own act, never by the manager's.
class SlotFiller : public loom::WeaveBase<SlotFiller, FillerState, loom::Accept<SeatDo>,
                                          loom::Emit<>> {
public:
    void on(const SeatDo&, loom::Mail& mail) {
        if (release != 0) {
            const std::uint64_t op = std::exchange(release, 0);
            released = mail.cancel_joint(authority, op).ok && mail.release_joint(authority, op).ok;
            return;
        }
        for (const loom::WeaveId id : owners) {
            begun.push_back(mail.begin_joint(authority, {loom::claim_key<SlotFact>(id)}));
        }
    }
    loom::JointAuthority authority;
    std::vector<loom::WeaveId> owners;
    std::vector<loom::JointBegin> begun;
    std::uint64_t release = 0;
    bool released = false;
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

    /// The other operator ends and releases its last live operation, and it is off the list.
    void release_last(PaneRig& r) {
        REQUIRE_FALSE(filler->begun.empty());
        filler->release = filler->begun.back().op;
        (void)r.bus.send(filler_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
        REQUIRE(filler->released);
        filler->begun.pop_back();
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
        editor->book = &r.host.showings;
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

    void mount_asker() { asker = mount_requester(kDoorAskerOffice); }

    /// A REQUESTER IN AN OFFICE OF ITS OWN, granted the one ask -- how a case tells who asked.
    DoorAsker* mount_requester(const char* office) {
        auto held = std::make_unique<DoorAsker>(std::string(office));
        DoorAsker* raw = held.get();
        loom::Grant grant;
        grant.allow_to_any(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version);
        const loom::WeaveId id =
            r.bus.register_weave(std::move(held), std::move(grant), std::string(office));
        raw->zen_set_self(id);
        raw->id = id;
        return raw;
    }

    loom::Ticket nudge(loom::WeaveId to) {
        return r.bus.send(to, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                            loom::WeaveId{}, 0));
    }
    /// THE HOST'S TURN, through the seam under test, telling from the host's own book.
    ServedTurn serve() {
        return serve_until_idle(r.bus, r.host.showings,
                                [this](const std::string& said) { told.push_back(said); });
    }
    /// Queue an open at the manager's door; nothing is drained.
    void enqueue_open(const std::string& path) { enqueue_open_as(*asker, path); }
    void enqueue_open_as(DoorAsker& who, const std::string& path) {
        who.next = [path](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kOpeningRole, OpenSourceRequested{path});
        };
        (void)nudge(who.id);
    }
    /// Ask, then serve turn by turn until the requester hears.
    SourceOpened open_and_serve(const std::string& path) { return open_and_serve_as(*asker, path); }
    SourceOpened open_and_serve_as(DoorAsker& who, const std::string& path) {
        const std::size_t before = who.opens.size();
        enqueue_open_as(who, path);
        for (int i = 0; i < 12 && who.opens.size() == before; ++i) {
            (void)serve();
        }
        REQUIRE_MESSAGE(who.opens.size() == before + 1, "the requester was never answered");
        return who.opens.back();
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
    std::vector<ServedTurn::Failed> failures;
    while (o.asker->opens.size() == before) {
        REQUIRE(++turns < 16);
        const ServedTurn turn = o.serve();
        failures.insert(failures.end(), turn.failed.begin(), turn.failed.end());
    }
    REQUIRE(failures.size() == 1);
    CHECK(failures[0].owner == o.editor_id);
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
// The latest terminal result: whose request it answers, apart from the live operation
// ============================================================================

TEST_CASE("the latest terminal result names the request it answers -- a refusal before any operation existed names its own path and requester and no operation, and the other operator's records and the next valid open are untouched") {
    OpeningRig o("open-latest-begin");
    o.open();
    REQUIRE(o.open_and_serve("/x/a.cpp").accepted);
    const std::int64_t first = static_cast<std::int64_t>(o.asker->id.value);
    const std::int64_t a_op = o.opening().last_op;
    REQUIRE(a_op != 0);
    CHECK(o.opening().last_path == "/x/a.cpp");
    CHECK(o.opening().last_requester == first);
    CHECK(o.opening().last_outcome == "committed");
    // A SECOND REQUESTER, in an office of its own, so the record has to say which one asked.
    DoorAsker* second = o.mount_requester("zengine.test.second-asker");
    const std::int64_t other = static_cast<std::int64_t>(second->id.value);
    REQUIRE(other != first);
    // EVERY SLOT THE BUS HAS, taken by an unrelated operator's live operations.
    Slots slots(o.r, loom::Switchboard::kMaxJointOperations);
    REQUIRE(o.r.bus.joint_records() == loom::Switchboard::kMaxJointOperations);
    const SourceOpened refused = o.open_and_serve_as(*second, "/x/c.cpp");
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal.find("too many opens") != std::string::npos);
    CHECK(refused.refusal.find("/x/c.cpp") != std::string::npos);
    {
        // THE RESULT AND ITS IDENTITY AGREE: C's refusal, the second requester, and no
        // operation -- `begin` allocated none, so none is named, and A's is not borrowed.
        const OpeningState& s = o.opening();
        CHECK(s.last_outcome == "refused");
        CHECK(s.last_refusal == refused.refusal);
        CHECK(s.last_path == "/x/c.cpp");
        CHECK(s.last_requester == other);
        CHECK(s.last_op == 0);
        CHECK(s.refused == 1);
        CHECK(s.committed == 1);
        // ...AND NOTHING IS LIVE, so the live fields name nothing.
        CHECK(s.op == 0);
        CHECK(s.path.empty());
        CHECK(s.stage == "idle");
        CHECK(s.awaiting.empty());
        CHECK(s.attempt == 0);
        CHECK(s.requester == 0);
    }
    for (const loom::JointBegin& b : slots.filler->begun) {
        CHECK(o.r.bus.joint_status(b.op).state == loom::JointState::Preparing);
    }
    // CAPACITY RETURNS by the other operator's own release, and the same requester's same
    // request takes -- the record then names that operation, that path and that requester.
    slots.release_last(o.r);
    REQUIRE(o.r.bus.joint_records() == loom::Switchboard::kMaxJointOperations - 1);
    const SourceOpened again = o.open_and_serve_as(*second, "/x/c.cpp");
    CHECK_MESSAGE(again.accepted, again.refusal);
    CHECK(o.editor->state().path == "/x/c.cpp");
    {
        const OpeningState& s = o.opening();
        CHECK(s.last_outcome == "committed");
        CHECK(s.last_refusal.empty());
        CHECK(s.last_path == "/x/c.cpp");
        CHECK(s.last_requester == other);
        CHECK(s.last_op != 0);
        CHECK(s.last_op != a_op);
        CHECK(s.committed == 2);
        CHECK(s.op == 0);
        CHECK(s.path.empty());
        CHECK(s.requester == 0);
    }
    for (const loom::JointBegin& b : slots.filler->begun) {
        CHECK(o.r.bus.joint_status(b.op).state == loom::JointState::Preparing);
    }
}

TEST_CASE("a request refused while a published commitment is still applying becomes the latest terminal result, and the live operation keeps its own identity until it settles") {
    OpeningRig o("open-latest-applying");
    o.open();
    REQUIRE(o.open_and_serve("/x/a.cpp").accepted);
    const loom::WeaveId first = o.asker->id;
    DoorAsker* second = o.mount_requester("zengine.test.second-asker");
    const loom::WeaveId other = second->id;
    const loom::WeaveId manager = o.r.opening_id;
    loom::Switchboard& bus = o.r.bus;
    // THE TURN ENDS WHERE THE MANAGER HAS HEARD THE PREPARATION: the desk's admission and the
    // commitment come next, and the second requester's ask is queued behind them, so it is
    // delivered after the commitment and before either owner has been shown it.
    bool stopped = false;
    const loom::ObserverId stop = bus.add_observer([&bus, &stopped, manager](const loom::BusEvent& ev) {
        if (!stopped && ev.kind == loom::EventKind::Delivered && ev.target == manager &&
            ev.schema_name == SourcePrepared::zen_name) {
            stopped = true;
            bus.stop();
        }
    });
    o.enqueue_open("/x/b.cpp");
    for (int i = 0; i < 4 && !stopped; ++i) {
        (void)o.serve();
    }
    bus.remove_observer(stop);
    REQUIRE(stopped);
    REQUIRE(o.opening().stage == "admit");
    // WHAT THE MANAGER'S RECORD SAYS THE MOMENT IT HAS ANSWERED THE SECOND REQUESTER.
    bool heard = false;
    OpeningState at;
    const loom::ObserverId look =
        bus.add_observer([&heard, &at, &o, manager, other](const loom::BusEvent& ev) {
            if (!heard && ev.kind == loom::EventKind::Delivered && ev.target == manager &&
                ev.sender == other && ev.schema_name == OpenSourceRequested::zen_name) {
                heard = true;
                at = o.opening();
            }
        });
    o.enqueue_open_as(*second, "/x/d.cpp");
    for (int i = 0; i < 12 && (second->opens.empty() || o.opening().op != 0); ++i) {
        (void)o.serve();
    }
    bus.remove_observer(look);
    REQUIRE(heard);
    REQUIRE(second->opens.size() == 1);
    const SourceOpened said = second->opens.back();
    CHECK_FALSE(said.accepted);
    CHECK(said.refusal.find("still applying /x/b.cpp") != std::string::npos);
    CHECK(said.refusal.find("/x/d.cpp was not opened") != std::string::npos);
    // THE LIVE OPERATION IS B'S, WHOLE: its operation, path, stage and requester.
    CHECK(at.op != 0);
    CHECK(at.path == "/x/b.cpp");
    CHECK(at.stage == "apply");
    CHECK(at.attempt == 0);
    CHECK(at.requester == static_cast<std::int64_t>(first.value));
    // ...AND THE LATEST TERMINAL RESULT IS D'S REFUSAL, NAMING NO OPERATION.
    CHECK(at.last_outcome == "refused");
    CHECK(at.last_refusal == said.refusal);
    CHECK(at.last_path == "/x/d.cpp");
    CHECK(at.last_requester == static_cast<std::int64_t>(other.value));
    CHECK(at.last_op == 0);
    CHECK(at.refused == 1);
    // B SETTLES AS ITSELF: its own outcome, operation, path and requester take the result.
    REQUIRE(o.asker->opens.size() == 2);
    CHECK_MESSAGE(o.asker->opens.back().accepted, o.asker->opens.back().refusal);
    const OpeningState& s = o.opening();
    CHECK(s.last_outcome == "committed");
    CHECK(s.last_path == "/x/b.cpp");
    CHECK(s.last_requester == static_cast<std::int64_t>(first.value));
    CHECK(s.last_op == at.op);
    CHECK(s.committed == 2);
    CHECK(s.refused == 1);
    CHECK(s.op == 0);
    CHECK(s.path.empty());
    CHECK(s.requester == 0);
    CHECK(o.editor->state().path == "/x/b.cpp");
}

// ============================================================================
// The pump seam: a native showing failure told in its owner's words, everything else propagates
// ============================================================================

namespace {
/// The sentence the host's turn tells for the stand-in, held, in the stand-in's own words.
std::string held_sentence(const OpeningRig& o, const std::string& words) {
    return "a published claim could not be applied by " + std::string(kEditorRole) + " (weave " +
           std::to_string(o.editor_id.value) +
           ") -- it is held until it is reloaded or removed; its own words: " + words;
}
} // namespace

TEST_CASE("OPEN-W3: a native owner's failed showing is told in the words its own boundary captured, and every exception that reaches the host's turn propagates as it came") {
    SUBCASE("a native owner's application throws inside its boundary: Failed and held, told once in its own words, served on, and the open settles in words while the real desk keeps what it applied") {
        OpeningRig o("open-pump-held");
        o.open();
        o.editor->fail_next = true;
        const std::size_t before = o.asker->opens.size();
        o.enqueue_open("/x/b.cpp");
        int turns = 0;
        std::vector<ServedTurn::Failed> failures;
        while (o.asker->opens.size() == before) {
            REQUIRE(++turns < 16);
            ServedTurn turn;
            CHECK_NOTHROW(turn = o.serve()); // nothing the boundary captured leaves as an exception
            failures.insert(failures.end(), turn.failed.begin(), turn.failed.end());
        }
        REQUIRE(failures.size() == 1);
        CHECK(failures[0].owner == o.editor_id);
        CHECK(failures[0].office == kEditorRole);
        CHECK(failures[0].held);
        const std::string expected =
            held_sentence(o, "test instrumentation: the stand-in Editor could not apply /x/b.cpp");
        CHECK(failures[0].diagnostic == expected);
        REQUIRE(o.told.size() == 1);
        CHECK(o.told[0] == expected);
        CHECK(o.r.host.showings.empty());
        CHECK(o.r.bus.has_failed_application(o.editor_id));
        CHECK(o.editor->state().published_seen == 1);
        CHECK(o.editor->state().applied == 0);
        // THE MANAGER SETTLES FROM THE RECORD, AND THE REQUESTER AND THE DESK ARE TOLD WHICH
        // OWNER COULD NOT APPLY IT.
        CHECK_FALSE(o.asker->opens.back().accepted);
        CHECK(o.asker->opens.back().refusal.find(kEditorRole) != std::string::npos);
        CHECK(o.opening().last_outcome == "committed, application failed");
        CHECK(o.r.session().notice.find("could not apply") != std::string::npos);
        // THE REAL DESK, THE OTHER NATIVE OWNER, APPLIED ITS HALF THROUGH THE SAME BOUNDARY,
        // which wrote nothing for it: the pane is seated and has the keys.
        CHECK(o.r.session().panels.has(o.kind));
        CHECK(o.r.session().panels.keyboard == o.kind);
        CHECK_FALSE(o.r.bus.has_failed_application(o.r.workshop_id));
        // THE BUS SERVES ON, and nothing more is told.
        const SourceOpened later = o.open_and_serve("/x/c.cpp");
        CHECK_FALSE(later.accepted); // the Editor's office is held: its preparation never runs
        CHECK(o.told.size() == 1);
    }
    SUBCASE("a native owner that declines inside its boundary gave an answer, not a failure: nothing is told or held, and the open settles as not applied") {
        OpeningRig o("open-pump-declined");
        o.open();
        o.editor->decline_next = true;
        const SourceOpened said = o.open_and_serve("/x/b.cpp");
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("did not apply") != std::string::npos);
        CHECK(o.opening().last_outcome == "committed, not applied");
        CHECK(o.editor->state().published_seen == 1);
        CHECK(o.editor->state().applied == 0);
        CHECK_FALSE(o.r.bus.has_failed_application(o.editor_id));
        CHECK(o.told.empty());
        CHECK(o.r.host.showings.empty());
        // A FUNCTIONING OWNER: the next open takes.
        const SourceOpened next = o.open_and_serve("/x/c.cpp");
        CHECK_MESSAGE(next.accepted, next.refusal);
        CHECK(o.told.empty());
    }
    SUBCASE("an ordinary handler that throws propagates out of the turn, told to nobody, and the bus serves the next turn") {
        OpeningRig o("open-pump-handler");
        o.open();
        o.editor->throw_in_handler = true;
        (void)o.nudge(o.editor_id);
        CHECK_THROWS_AS((void)o.serve(), std::runtime_error);
        CHECK(o.told.empty());
        CHECK_FALSE(o.r.bus.has_failed_application(o.editor_id));
        const SourceOpened next = o.open_and_serve("/x/a.cpp");
        CHECK_MESSAGE(next.accepted, next.refusal);
        CHECK(o.told.empty());
    }
    SUBCASE("an owner held since an earlier turn explains nothing: a later unrelated handler exception still propagates, even behind a refused delivery to the held owner") {
        OpeningRig o("open-pump-unrelated");
        o.open();
        o.editor->fail_next = true;
        const std::size_t before = o.asker->opens.size();
        o.enqueue_open("/x/b.cpp");
        int turns = 0;
        while (o.asker->opens.size() == before) {
            REQUIRE(++turns < 16);
            (void)o.serve();
        }
        REQUIRE(o.r.bus.has_failed_application(o.editor_id));
        REQUIRE(o.told.size() == 1);
        // ONE TURN: a delivery to the held owner, refused `ApplicationFailed` with no exception,
        // then a handler of another weave that throws.
        const loom::Ticket to_held = o.nudge(o.editor_id);
        o.asker->next = [](DoorAsker&, loom::Mail&) {
            throw std::runtime_error("test instrumentation: an unrelated handler failed");
        };
        (void)o.nudge(o.asker->id);
        CHECK_THROWS_AS((void)o.serve(), std::runtime_error);
        CHECK(o.r.bus.outcome(to_held).refusal.reason == loom::RefusalReason::ApplicationFailed);
        CHECK(o.told.size() == 1);                           // nothing new was told
        CHECK(o.r.bus.has_failed_application(o.editor_id)); // still held, still its own repair
    }
    SUBCASE("an owner held since an earlier turn explains nothing: a host observer that throws on a later delivery propagates, and no second diagnostic names the held owner") {
        OpeningRig o("open-pump-observer-later");
        o.open();
        o.editor->fail_next = true;
        const std::size_t before = o.asker->opens.size();
        o.enqueue_open("/x/b.cpp");
        int turns = 0;
        while (o.asker->opens.size() == before) {
            REQUIRE(++turns < 16);
            (void)o.serve();
        }
        REQUIRE(o.r.bus.has_failed_application(o.editor_id));
        REQUIRE(o.told.size() == 1);
        CHECK(o.told[0] ==
              held_sentence(o, "test instrumentation: the stand-in Editor could not apply /x/b.cpp"));
        // A HOST OBSERVER THAT THROWS ON THE SEPARATE ASKER'S ORDINARY DELIVERY.
        bool ran = false;
        const loom::WeaveId asker_id = o.asker->id;
        const loom::ObserverId observer =
            o.r.bus.add_observer([&ran, asker_id](const loom::BusEvent& ev) {
                if (ev.kind == loom::EventKind::Delivered && ev.target == asker_id) {
                    ran = true;
                    throw std::runtime_error(
                        "test instrumentation: an unrelated host observer failed");
                }
            });
        // ONE TURN: a delivery to the held owner, refused `ApplicationFailed` with no new
        // showing, then an ordinary delivery the observer throws on.
        const loom::Ticket to_held = o.nudge(o.editor_id);
        (void)o.nudge(asker_id);
        std::string what;
        try {
            (void)o.serve();
        } catch (const std::runtime_error& e) {
            what = e.what();
        }
        o.r.bus.remove_observer(observer);
        CHECK(ran);
        CHECK(what == "test instrumentation: an unrelated host observer failed");
        CHECK(o.r.bus.outcome(to_held).refusal.reason == loom::RefusalReason::ApplicationFailed);
        CHECK(o.editor->state().published_seen == 1); // no second showing was attempted
        REQUIRE(o.told.size() == 1);                   // nothing new was told
        CHECK(o.r.bus.has_failed_application(o.editor_id));
        CHECK(o.r.host.showings.empty());
    }
    SUBCASE("a host observer that throws on the showing's own refusal notification propagates, and the owner's failure is told in the owner's words and never the observer's") {
        OpeningRig o("open-pump-observer-on-refusal");
        o.open();
        o.editor->fail_next = true;
        bool ran = false;
        loom::ObserverId observer = 0;
        const loom::WeaveId editor_id = o.editor_id;
        loom::Switchboard& bus = o.r.bus;
        const std::size_t before = o.asker->opens.size();
        // REGISTERED FROM INSIDE A DELIVERY OF THE SAME TURN -- the asker's own, as it asks --
        // so it hears every event after the ask, the refusal of the delivery that met the
        // failed showing among them.
        o.asker->next = [&bus, &observer, &ran, editor_id](DoorAsker& a, loom::Mail& mail) {
            observer = bus.add_observer([&ran, editor_id](const loom::BusEvent& ev) {
                if (!ran && ev.kind == loom::EventKind::Refused && ev.target == editor_id &&
                    ev.refusal.reason == loom::RefusalReason::ApplicationFailed) {
                    ran = true;
                    throw std::runtime_error(
                        "test instrumentation: an observer threw on the showing's refusal");
                }
            });
            a.ask(mail, kOpeningRole, OpenSourceRequested{"/x/b.cpp"});
        };
        (void)o.nudge(o.asker->id);
        std::string what;
        try {
            (void)o.serve();
        } catch (const std::runtime_error& e) {
            what = e.what();
        }
        bus.remove_observer(observer);
        CHECK(ran);
        CHECK(what == "test instrumentation: an observer threw on the showing's refusal");
        // THE PARTICIPANT'S FAILURE IS RECORDED: shown once, applied nothing, held.
        CHECK(o.editor->state().published_seen == 1);
        CHECK(o.editor->state().applied == 0);
        CHECK(o.r.bus.has_failed_application(o.editor_id));
        // THE DIAGNOSTIC'S OWN WORDS: the owner's, told once before the observer's exception
        // left the turn -- not the observer's, and not a second sentence about it.
        REQUIRE(o.told.size() == 1);
        CHECK(o.told[0] ==
              held_sentence(o, "test instrumentation: the stand-in Editor could not apply /x/b.cpp"));
        CHECK(o.told[0].find("observer") == std::string::npos);
        CHECK(o.r.host.showings.empty());
        // THE BUS SERVES ON: the open settles in words, and nothing more is told.
        int turns = 0;
        while (o.asker->opens.size() == before) {
            REQUIRE(++turns < 16);
            (void)o.serve();
        }
        CHECK_FALSE(o.asker->opens.back().accepted);
        CHECK(o.asker->opens.back().refusal.find("could not apply") != std::string::npos);
        CHECK(o.told.size() == 1);
    }
    SUBCASE("a native owner that throws outside any boundary is recorded by Loom and re-raised, and the exception propagates described by nobody") {
        OpeningRig o("open-pump-raw");
        o.open();
        o.editor->throw_raw_next = true;
        o.enqueue_open("/x/b.cpp");
        std::string what;
        for (int turns = 0; turns < 8 && what.empty(); ++turns) {
            try {
                (void)o.serve();
            } catch (const std::runtime_error& e) {
                what = e.what();
            }
        }
        CHECK(what ==
              "test instrumentation: the stand-in Editor threw outside its boundary applying /x/b.cpp");
        CHECK(o.r.bus.has_failed_application(o.editor_id)); // Loom recorded it all the same
        CHECK(o.told.empty());
        CHECK(o.r.host.showings.empty());
    }
}
