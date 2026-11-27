// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_GUEST_ACTS_HPP
#define ZENGINE_WORKSHOP_GUEST_ACTS_HPP

// A guest's acts, held for their answers. Workshop judges a guest's act -- a press, a key, a
// carry, a desk read, a guide mark -- by the row the guest door admitted the guest with and by
// what Loom says the guest may do now. Neither is Workshop's to keep, so each act asks both: the
// guest door (`GuestRowOfRequested`) and Loom's authority reader (`zen.AuthorityAsked`), and the
// act waits, held with every later act of the same guest in the order they came, until both have
// answered. Then it runs again as it came -- its sender, its provenance, its answer right -- with
// those facts in scope for that run alone. The weaver's own acts never wait here.
// Workshop law: agents/workshop/guests.md

#include "actor_scope.hpp"
#include "guest_seam_vocabulary.hpp"
#include "pane/menu.hpp"

#include <zen/host/authority_reader.hpp>
#include <zen/host/participants.hpp>
#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The role this host mounts Loom's authority reader in (`loom::host::AuthorityReader`), which
/// holds no role of its own: the door that says what one participant may do now.
inline constexpr const char* kAuthorityRole = "zengine.authority";

/// How many acts Workshop holds at once, every guest's together, while their answers come; an
/// act past it is refused in words.
inline constexpr std::size_t kMaxHeldGuestActs = 32;

/// One question for the authority reader about the act's guest: may it send `shape` at `version`
/// to `role`, at `door` when one is named.
struct AuthorityQuestion {
    std::string shape;
    std::int64_t version = 0;
    std::string role;
    std::string door;
    bool operator==(const AuthorityQuestion&) const = default;
};

/// What one held act was told, in scope while it runs again and gone when it ends: its guest's
/// row as the guest door said it, and the reader's word on each of its questions.
struct GuestFacts {
    loom::WeaveId guest;
    scope::GuestRowFacts row;
    std::vector<std::pair<AuthorityQuestion, bool>> permits;
    scope::HostFact host; ///< whose host this is, as the guest door claims it as the act runs
};

/// WHOSE HOST THIS IS NOW, read from the guest door's claim (`GuestHost`) in `mail`'s delivery: a
/// weaver's when no door claims it or the reader may not observe it.
// WL-GUEST-03 -- agents/workshop/guests.md
inline scope::HostFact host_fact_now(loom::Mail& mail) {
    const loom::SenseReading read = mail.latest_from_role<GuestHost>(kGuestsRole);
    if (!read || !read.value.has_value()) return {};
    const GuestHost said = loom::from_value<GuestHost>(*read.value);
    return scope::HostFact{said.development, said.guests_file};
}

/// The bus an act runs again on: Loom's own, but for the answer right, which is the one the act
/// held when it came. An answer the run gives spends it; a run that defers its answer is handed
/// it; any other answer has no delivery to answer and is refused, as a root's is.
class ReplayBus final : public loom::Bus {
public:
    ReplayBus(loom::Bus& bus, loom::DeferredAnswer held) : bus_(bus), held_(std::move(held)) {}
    ReplayBus(const ReplayBus&) = delete;
    ReplayBus& operator=(const ReplayBus&) = delete;
    /// An answer right the run neither spent nor took is let go, so the slot Loom keeps for it is
    /// free again.
    ~ReplayBus() override {
        if (held_.valid()) bus_.release_deferred(held_);
    }

    loom::Ticket answer(loom::Message msg) override {
        if (!held_.valid()) return loom::Ticket{};
        const loom::Ticket sent = bus_.spend_deferred(held_, std::move(msg));
        held_ = loom::DeferredAnswer{};
        return sent;
    }
    loom::DeferredAnswer make_deferred_answer() override { return std::exchange(held_, {}); }

    loom::Ticket send(loom::WeaveId target, loom::Message msg) override {
        return bus_.send(target, std::move(msg));
    }
    std::size_t publish(loom::Message msg) override { return bus_.publish(std::move(msg)); }
    loom::Ticket send_to_role(std::string_view role, loom::Message msg) override {
        return bus_.send_to_role(role, std::move(msg));
    }
    loom::Ticket ask(loom::WeaveId target, loom::Message msg) override {
        return bus_.ask(target, std::move(msg));
    }
    loom::Ticket ask_role(std::string_view role, loom::Message msg) override {
        return bus_.ask_role(role, std::move(msg));
    }
    loom::Ticket role_ask(std::string_view as_role, loom::WeaveId target,
                          loom::Message msg) override {
        return bus_.role_ask(as_role, target, std::move(msg));
    }
    loom::Ticket role_ask_role(std::string_view as_role, std::string_view to_role,
                               loom::Message msg) override {
        return bus_.role_ask_role(as_role, to_role, std::move(msg));
    }
    loom::Ticket spend_deferred(const loom::DeferredAnswer& answer, loom::Message msg) override {
        return bus_.spend_deferred(answer, std::move(msg));
    }
    void release_deferred(const loom::DeferredAnswer& answer) override {
        bus_.release_deferred(answer);
    }
    loom::Ticket announce_lifecycle(const loom::LifecycleAuthority& authority,
                                    loom::WeaveId target, loom::Message msg,
                                    std::int64_t sequence) override {
        return bus_.announce_lifecycle(authority, target, std::move(msg), sequence);
    }
    loom::Ticket role_send(std::string_view as_role, loom::WeaveId target,
                           loom::Message msg) override {
        return bus_.role_send(as_role, target, std::move(msg));
    }
    loom::Ticket role_send_to_role(std::string_view as_role, std::string_view to_role,
                                   loom::Message msg) override {
        return bus_.role_send_to_role(as_role, to_role, std::move(msg));
    }
    loom::RolePublication role_publish(std::string_view as_role, loom::Message msg) override {
        return bus_.role_publish(as_role, std::move(msg));
    }
    loom::RoleRelease release_role(std::string_view role) override {
        return bus_.release_role(role);
    }
    loom::SenseClaimResult claim(loom::Value value) override {
        return bus_.claim(std::move(value));
    }
    loom::SenseClaimResult role_claim(std::string_view as_role, loom::Value value) override {
        return bus_.role_claim(as_role, std::move(value));
    }
    loom::SenseReading observe(loom::WeaveId author,
                               std::shared_ptr<const loom::Schema> shape) override {
        return bus_.observe(author, std::move(shape));
    }
    loom::SenseReading observe_role(std::string_view role,
                                    std::shared_ptr<const loom::Schema> shape) override {
        return bus_.observe_role(role, std::move(shape));
    }
    loom::CallResult call(std::string_view role, loom::Value argument,
                          std::shared_ptr<const loom::Schema> output) override {
        return bus_.call(role, std::move(argument), std::move(output));
    }
    loom::GrantChange delegate_authority(const loom::GrantAuthority& authority,
                                         loom::LiveAuthority requested) override {
        return bus_.delegate_authority(authority, std::move(requested));
    }
    loom::AuthorityView describe_authority(const loom::GrantAuthority& authority) override {
        return bus_.describe_authority(authority);
    }
    loom::JointResult offer_claim(std::uint64_t op, loom::Value value) override {
        return bus_.offer_claim(op, std::move(value));
    }
    loom::JointBegin begin_joint(const loom::JointAuthority& authority,
                                 std::vector<loom::ClaimKey> keys) override {
        return bus_.begin_joint(authority, std::move(keys));
    }
    loom::JointResult commit_joint(const loom::JointAuthority& authority,
                                   std::uint64_t op) override {
        return bus_.commit_joint(authority, op);
    }
    loom::JointResult cancel_joint(const loom::JointAuthority& authority,
                                   std::uint64_t op) override {
        return bus_.cancel_joint(authority, op);
    }
    loom::JointStatus joint_status(const loom::JointAuthority& authority,
                                   std::uint64_t op) override {
        return bus_.joint_status(authority, op);
    }
    loom::JointResult release_joint(const loom::JointAuthority& authority,
                                    std::uint64_t op) override {
        return bus_.release_joint(authority, op);
    }

private:
    loom::Bus& bus_;
    loom::DeferredAnswer held_;
};

/// How a held act came: an ask, whose answer right it holds; speech that asks for no answer; or
/// an act that goes on from one already taken -- a read waiting on the window, a key behind a
/// pointer moment -- which runs again on the Mail of the delivery its answers arrive in.
enum class Came : std::uint8_t { kAsk, kSaid, kOn };

/// The facet: where each act enters (`hold`) and the facts of the act running again (`row`,
/// `permits`).
class GuestActs {
public:
    using Accept = loom::Accept<GuestRowOf, loom::host::AuthorityAnswered>;
    using Emit = loom::Emit<GuestRowOfRequested, loom::host::AuthorityAsked>;

    /// The act, run again: as the delivery it was, on the Mail it is handed.
    using Again = std::function<void(loom::Mail&)>;
    /// What the act asks the authority reader, once its guest's row is known.
    using Needs = std::function<std::vector<AuthorityQuestion>(const scope::GuestRowFacts&)>;

    /// What `hold` did: the act is the caller's to run (its guest's facts are in scope, or it is
    /// nobody's), it ran already as no guest's, it waits, or it is refused for want of room and the
    /// caller says so.
    enum class Held : std::uint8_t { kRuns, kRan, kWaits, kFull };

    /// Is `guest`'s act running again now, with its facts?
    bool told(loom::WeaveId guest) const { return now_ != nullptr && now_->guest == guest; }

    /// THE ROW `who` WAS ADMITTED WITH, as the guest door said it for the act running now. A
    /// participant no door was asked about for this act is judged a guest with no powers: never
    /// taken for the weaver.
    scope::GuestRowFacts row(loom::WeaveId who) const {
        if (!who.valid()) return {};
        if (told(who)) return now_->row;
        scope::GuestRowFacts unasked;
        unasked.admitted = true;
        return unasked;
    }

    /// WHOSE HOST THIS IS, as the guest door claimed it for `who`'s act running now; a weaver's for
    /// any other, the host that narrows a guest most.
    scope::HostFact host_fact(loom::WeaveId who) const {
        return told(who) ? now_->host : scope::HostFact{};
    }

    /// Whether the authority reader said `who` may do `asked`, for the act running now; no when
    /// it was not asked.
    bool permits(loom::WeaveId who, const AuthorityQuestion& asked) const {
        if (!told(who)) return false;
        for (const auto& [question, yes] : now_->permits) {
            if (question == asked) return yes;
        }
        return false;
    }

    /// `guest`'s act held last was a press the window landed on a pane of `role`.
    void aim(loom::WeaveId guest, std::string role) {
        const auto line = acts_.find(guest.value);
        if (line != acts_.end() && !line->second.empty()) line->second.back().aimed = std::move(role);
    }

    /// A ROLE'S SPEECH BEHIND A GUEST'S HELD PRESS: the window gave the pane a guest's press at
    /// once, and what the pane says next -- citing the press's number, or anything its role says --
    /// waits behind Workshop's word of that press, so the act it follows is taken first. It runs
    /// again in its turn as it came, its own judgments asked then. An answer, Loom's word and a
    /// delivery running again are never held here; past the bound it is not held at all.
    template <class Self, class S>
    bool holds(Self& self, const S& msg, loom::Mail& mail) {
        if (now_ != nullptr || ordering_.valid() || acts_.empty() || held_ >= kMaxHeldGuestActs) {
            return false;
        }
        if (mail.answers_ask() || mail.ask_ended() || mail.dispatch_refused()) return false;
        const std::string_view role = mail.authored_role();
        if (role.empty()) return false;
        const loom::WeaveId guest = behind(mail.correlation(), role);
        if (!guest.valid()) return false;
        Act act;
        act.self = self_id_;
        act.envelope = envelope_of(mail);
        act.incarnation = incarnation_of(mail.sender(), mail);
        act.answer = mail.defer_answer();
        act.ordered = true;
        act.again = [&self, msg](loom::Mail& m) {
            static_cast<typename Self::With&>(self).on(S(msg), m);
        };
        acts_[guest.value].push_back(std::move(act));
        ++held_;
        return true;
    }

    /// The words an act past the bound is refused in.
    static std::string full_words() {
        return "Workshop already holds " + std::to_string(kMaxHeldGuestActs) +
               " guests' acts waiting on the guest door and the authority reader; act again "
               "once they are answered";
    }

    /// An act of `guest`, held unless its guest's facts are in scope: one that came as a delivery
    /// with its sender and provenance, to run again as the weave `self`, and an ask with its
    /// answer right. Where no guest door listens (`door` false) nobody is a guest, and an act that
    /// asks the reader nothing runs now, judged so, unless an act of its participant is held.
    Held hold(loom::WeaveId self, loom::WeaveId guest, loom::Mail& mail, Came came, bool door,
              Again again, Needs needs = {}) {
        if (!guest.valid() || told(guest)) return Held::kRuns;
        self_id_ = self;
        const auto line_of = acts_.find(guest.value);
        if (!door && (line_of == acts_.end() || line_of->second.empty()) &&
            (!needs || needs(scope::GuestRowFacts{}).empty())) {
            GuestFacts facts{guest, {}, {}, {}};
            const Told told(now_, &facts);
            again(mail);
            return Held::kRan;
        }
        if (held_ >= kMaxHeldGuestActs) return Held::kFull;
        Act act;
        act.self = self;
        if (came != Came::kOn) {
            act.envelope = envelope_of(mail);
            act.incarnation = incarnation_of(mail.sender(), mail);
        }
        // ITS ANSWER RIGHT, when it came as an ask: one sent as plain speech has none to hold.
        if (came == Came::kAsk) act.answer = mail.defer_answer();
        act.again = std::move(again);
        act.needs = std::move(needs);
        std::deque<Act>& line = acts_[guest.value];
        ++held_;
        // AN ACT TAKEN IN ITS TURN BEHIND A PRESS asks first, ahead of the acts that came after it.
        if (ordering_ == guest) {
            line.push_front(std::move(act));
            ask_row(guest, mail);
            return Held::kWaits;
        }
        line.push_back(std::move(act));
        if (line.size() == 1) ask_row(guest, mail);
        return Held::kWaits;
    }

    /// THE ROW THAT ADMITTED A HELD ACT'S GUEST, from the guest door.
    template <class Self>
    void on(Self&, const GuestRowOf& said, loom::Mail& mail) {
        if (!mail.answers_ask()) return; // the shape alone is anybody's speech
        Act* act = asked(mail.correlation());
        if (act == nullptr || act->row_question != mail.correlation()) return;
        row_told(guest_of(mail.correlation()), scope::row_said(said), mail);
    }

    /// THE READER'S WORD ON ONE OF A HELD ACT'S QUESTIONS.
    template <class Self>
    void on(Self&, const loom::host::AuthorityAnswered& said, loom::Mail& mail) {
        if (!mail.answers_ask()) return;
        permit_told(mail.correlation(), said.present && said.permits, mail);
    }

    /// LOOM'S WORD THAT ONE OF THESE ASKS ENDED UNANSWERED, or its answerer's refusal: no guest
    /// door said a row, so the guest is no guest; no reader said a yes, so the act may not. True
    /// when the correlation was one of these asks.
    bool ended(std::uint64_t correlation, loom::Mail& mail) {
        Act* act = asked(correlation);
        if (act == nullptr) return false;
        if (act->row_question == correlation) {
            row_told(guest_of(correlation), scope::GuestRowFacts{}, mail);
        } else {
            permit_told(correlation, false, mail);
        }
        return true;
    }

private:
    /// The facts in scope for one run, and those of the run around it again after.
    struct Told {
        const GuestFacts*& now;
        const GuestFacts* outer;
        Told(const GuestFacts*& slot, const GuestFacts* facts) : now(slot), outer(slot) {
            now = facts;
        }
        ~Told() { now = outer; }
        Told(const Told&) = delete;
        Told& operator=(const Told&) = delete;
    };

    struct Act {
        loom::WeaveId self;                     ///< the weave it runs again as
        std::optional<loom::Message> envelope; ///< the delivery it came as, when it was one
        std::int64_t incarnation = 0;           ///< its sender's, when it came
        bool ordered = false; ///< it only keeps its turn: it asks nothing and is told nothing
        std::string aimed;    ///< the role of the pane a held press was landed on
        loom::DeferredAnswer answer;
        Again again;
        Needs needs;
        std::uint64_t row_question = 0;
        std::uint64_t first_question = 0; ///< the authority questions' numbers start here
        scope::GuestRowFacts row;
        std::vector<AuthorityQuestion> questions;
        std::vector<std::optional<bool>> said;
    };

    /// The asks' numbers stand apart from every other number Workshop asks under.
    static constexpr std::uint64_t kFirstQuestion = std::uint64_t{1} << 62;

    /// The delivery `mail` carries, as its handler read it: who sent it, where an answer goes,
    /// under which number, authored as which role, addressed to which, in which turn.
    /// Its payload is never read: the act runs again from its own closure, which holds what came.
    static loom::Message envelope_of(const loom::Mail& mail) {
        loom::Message env{loom::to_value(loom::Ack{}), mail.sender(), mail.reply_to(),
                          mail.correlation()};
        env.provenance = loom::Provenance::attested(loom::Provenance::Kind::None, 0)
                             .with_authored_role(std::string(mail.authored_role()));
        env.addressed_role = std::string(mail.addressed_role());
        if (const auto ms = mail.turn_ms(); ms) {
            env.turn = loom::TurnFacts{*ms, mail.turn_date_ms().value_or(0)};
        }
        return env;
    }

    /// Ask the guest door which row admitted `guest`, for its first held act.
    void ask_row(loom::WeaveId guest, loom::Mail& mail) {
        Act& act = acts_[guest.value].front();
        if (act.ordered) {
            run_first(guest, mail);
            return;
        }
        act.row_question = number(guest);
        const loom::Ticket sent = mail.as_role(pane::pane_menu::kWorkshopRole)
                                      .ask_role(kGuestsRole,
                                                GuestRowOfRequested{static_cast<std::int64_t>(
                                                    guest.value)},
                                                act.row_question);
        if (!sent.valid()) {
            // NO DOOR TO ASK: nobody admitted this participant, so it is no guest.
            row_told(guest, scope::GuestRowFacts{}, mail);
        }
    }

    /// The row came: the act's questions for the reader, asked together, or the act runs.
    void row_told(loom::WeaveId guest, scope::GuestRowFacts row, loom::Mail& mail) {
        Act& act = acts_[guest.value].front();
        questions_.erase(act.row_question);
        act.row_question = 0;
        act.row = std::move(row);
        act.questions = act.needs ? act.needs(act.row) : std::vector<AuthorityQuestion>{};
        act.said.assign(act.questions.size(), std::nullopt);
        if (act.questions.empty()) {
            run_first(guest, mail);
            return;
        }
        act.first_question = 0;
        for (std::size_t i = 0; i < act.questions.size(); ++i) {
            const std::uint64_t n = number(guest);
            if (i == 0) act.first_question = n;
            const AuthorityQuestion& q = act.questions[i];
            const loom::Ticket sent = mail.as_role(pane::pane_menu::kWorkshopRole)
                                          .ask_role(kAuthorityRole,
                                                    loom::host::AuthorityAsked{
                                                        static_cast<std::int64_t>(guest.value),
                                                        q.shape, q.version, q.role, q.door},
                                                    n);
            if (!sent.valid()) {
                questions_.erase(n);
                act.said[i] = false; // no reader to ask: nothing says the guest may
            }
        }
        if (all_said(act)) run_first(guest, mail);
    }

    void permit_told(std::uint64_t correlation, bool yes, loom::Mail& mail) {
        Act* act = asked(correlation);
        if (act == nullptr || act->first_question == 0 || correlation < act->first_question) return;
        const std::size_t i = static_cast<std::size_t>(correlation - act->first_question);
        if (i >= act->said.size() || act->said[i].has_value()) return;
        const loom::WeaveId guest = guest_of(correlation);
        questions_.erase(correlation);
        act->said[i] = yes;
        if (all_said(*act)) run_first(guest, mail);
    }

    /// The guest's first act, run again with its facts; then the next act's row is asked.
    void run_first(loom::WeaveId guest, loom::Mail& mail) {
        auto line = acts_.find(guest.value);
        Act act = std::move(line->second.front());
        line->second.pop_front();
        --held_;
        GuestFacts facts{guest, std::move(act.row), {}, {}};
        if (facts.row.admitted) facts.host = host_fact_now(mail);
        for (std::size_t i = 0; i < act.questions.size(); ++i) {
            facts.permits.emplace_back(act.questions[i], act.said[i].value_or(false));
        }
        // A SENDER REPLACED SINCE IT ASKED is not the one the act came from: the act lands on
        // nobody, and its answer right goes with it.
        const bool replaced = act.envelope && act.envelope->sender.valid() &&
                              incarnation_of(act.envelope->sender, mail) != act.incarnation;
        if (replaced) {
            if (act.answer.valid()) mail.bus().release_deferred(act.answer);
        } else {
            const Told told(now_, act.ordered ? nullptr : &facts);
            const loom::WeaveId outer = std::exchange(ordering_, act.ordered ? guest : loom::WeaveId{});
            if (act.envelope) {
                ReplayBus bus(mail.bus(), std::move(act.answer));
                loom::Mail again(bus, *act.envelope, act.self);
                act.again(again);
            } else {
                act.again(mail);
            }
            ordering_ = outer;
        }
        line = acts_.find(guest.value);
        if (line == acts_.end()) return;
        if (line->second.empty()) {
            acts_.erase(line);
            return;
        }
        if (line->second.front().row_question == 0 && line->second.front().said.empty()) {
            ask_row(guest, mail);
        }
    }

    /// Whose held press a role's speech follows: an act held under `correlation`, or a press the
    /// window landed on a pane of `role`. Nobody when none is.
    loom::WeaveId behind(std::uint64_t correlation, std::string_view role) const {
        for (const auto& [guest, line] : acts_) {
            for (const Act& act : line) {
                if ((correlation != 0 && act.envelope && act.envelope->correlation == correlation) ||
                    (!act.aimed.empty() && act.aimed == role)) {
                    return loom::WeaveId{guest};
                }
            }
        }
        return {};
    }

    /// The incarnation `who` is at now, as Loom's participants claim says; 0 for one not on it.
    static std::int64_t incarnation_of(loom::WeaveId who, loom::Mail& mail) {
        const std::optional<loom::host::Participants> now = loom::host::read_participants(mail);
        const loom::host::Participant* row = now ? now->find(who) : nullptr;
        return row != nullptr ? row->incarnation : 0;
    }

    static bool all_said(const Act& act) {
        for (const std::optional<bool>& s : act.said) {
            if (!s.has_value()) return false;
        }
        return true;
    }

    std::uint64_t number(loom::WeaveId guest) {
        const std::uint64_t n = kFirstQuestion + ++questions_asked_;
        questions_[n] = guest.value;
        return n;
    }
    loom::WeaveId guest_of(std::uint64_t correlation) const {
        const auto it = questions_.find(correlation);
        return loom::WeaveId{it != questions_.end() ? it->second : 0};
    }
    /// The held act an ask under `correlation` was for: the first of its guest's.
    Act* asked(std::uint64_t correlation) {
        const auto q = questions_.find(correlation);
        if (q == questions_.end()) return nullptr;
        const auto line = acts_.find(q->second);
        if (line == acts_.end() || line->second.empty()) return nullptr;
        return &line->second.front();
    }

    /// Each guest's held acts, in the order they came.
    std::map<std::uint64_t, std::deque<Act>> acts_;
    std::size_t held_ = 0;
    /// Every ask in flight: its number, and whose act it is for.
    std::map<std::uint64_t, std::uint64_t> questions_;
    std::uint64_t questions_asked_ = 0;
    const GuestFacts* now_ = nullptr;
    /// The guest whose act runs again now only to keep its turn: an act of that guest it holds
    /// asks first.
    loom::WeaveId ordering_;
    /// The weave the facet is part of, as its hold was told.
    loom::WeaveId self_id_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_GUEST_ACTS_HPP
