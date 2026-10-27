// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "weave.hpp"
#include "first-run/setup.hpp"
#include "inventory/codec.hpp"
#include <algorithm>
#include <limits>

namespace zengine::workshop {
// ONE HAND PER ACTOR: the weaver's for platform input and input no actor was attributed to, and
// one per guest participant, the oldest forgotten past the bound.
WorkshopWeave::Hand& WorkshopWeave::hand_of(const InputActor& actor) {
    if (!actor.known || actor.local) return weaver_hand_;
    for (Hand& h : guest_hands_)
        if (h.actor.participant == actor.participant) return h;
    if (guest_hands_.size() >= kMaxGuestHands) {
        const auto oldest = std::min_element(guest_hands_.begin(), guest_hands_.end(),
            [](const Hand& a, const Hand& b) { return a.latest < b.latest; });
        guest_hands_.erase(oldest);
    }
    Hand fresh;
    fresh.actor = actor;
    guest_hands_.push_back(fresh);
    return guest_hands_.back();
}

const WorkshopWeave::Hand* WorkshopWeave::find_hand(const InputActor& actor) const {
    if (!actor.known || actor.local) return &weaver_hand_;
    for (const Hand& h : guest_hands_)
        if (h.actor.participant == actor.participant) return &h;
    return nullptr;
}

std::uint64_t WorkshopWeave::latest_of(const InputActor& actor) const {
    const Hand* h = find_hand(actor);
    return h != nullptr ? h->latest : 0;
}

void WorkshopWeave::keep_act(Hand& h, const GestureSent& sent) {
    if (sent.answering == 0) return;
    if (h.kept.size() >= kKeptActs) h.kept.erase(h.kept.begin());
    h.kept.push_back(sent);
}

void WorkshopWeave::drop_kept(std::uint64_t correlation) {
    each_hand([correlation](Hand& h) {
        h.kept.erase(std::remove_if(h.kept.begin(), h.kept.end(),
                                    [correlation](const GestureSent& k) { return k.answering == correlation; }),
                     h.kept.end());
    });
}

void WorkshopWeave::count_gesture() {
    ++gestures_;
    Hand& h = hand();
    if (!shift_press_) {
        h.shift_tap = {};
    }
    h.actor = input_actor_;
    h.latest = gestures_;
    if (recent_acts_.size() >= kMaxRecentActs) recent_acts_.erase(recent_acts_.begin());
    recent_acts_.emplace_back(gestures_, input_actor_);
}

WorkshopWeave::Hand* WorkshopWeave::hand_of_act(std::uint64_t act) {
    for (const auto& [number, actor] : recent_acts_)
        if (number == act) return const_cast<Hand*>(find_hand(actor));
    return nullptr;
}

bool WorkshopWeave::duplicate_input(const loom::Mail& mail) const {
    return !applying_attributed_ && attributed_producer_.valid() &&
           mail.sender() == attributed_producer_;
}

// AN INJECTED BATCH IS TAKEN IN ITS ORDER: while a window shows the desk it lands a participant's
// pointer moments and Workshop takes its keys, so a key behind a pointer moment the window may not
// have landed yet waits until the window answers an ask sent after that moment (WL-GUEST-13).
void WorkshopWeave::on(const input::AttributedInput& event, loom::Mail& mail) {
    if (!mail.authored_from_role(input::kInputRole) ||
        (event.local ? event.actor != 0 : event.actor <= 0)) return;
    attributed_producer_ = mail.sender();
    const std::string& kind = event.event.kind;
    const bool pointer = kind == "PointerButton" || kind == "PointerMoved" || kind == "PointerWheel";
    if (event.local || !window_.shown()) {
        apply_attributed(event, mail);
        return;
    }
    if (pointer) {
        apply_attributed(event, mail); // ends a carry it ends, and leaves the moment to the window
        if (batch_held_.empty() || !batch_held_.back().pointer) {
            batch_held_.push_back(BatchInput{++held_seq_, true, {}});
        } else {
            batch_held_.back().seq = ++held_seq_;
        }
        return;
    }
    const bool behind = std::any_of(batch_held_.begin(), batch_held_.end(), [&](const BatchInput& h) {
        return !h.pointer || h.seq > fence_covers_;
    });
    if (!behind) {
        batch_held_.clear();
        apply_attributed(event, mail);
        return;
    }
    if (batch_held_.size() >= kMaxBatchHeld) {
        say("Input refused: too many keys wait behind the pointer; send them again", true);
        return;
    }
    batch_held_.push_back(BatchInput{++held_seq_, false, event});
    release_held_input(mail);
}

// THE KEYS NO UNLANDED POINTER MOMENT STANDS BEFORE, taken in order; the first such moment asks the
// window once more, or, with no window to ask, waits for nothing.
void WorkshopWeave::release_held_input(loom::Mail& mail) {
    while (!batch_held_.empty()) {
        if (batch_held_.front().pointer && batch_held_.front().seq > fence_covers_) {
            if (key_fence_ != 0) return;
            if (send_key_fence(mail)) return;
            fence_covers_ = held_seq_;
            continue;
        }
        const BatchInput next = std::move(batch_held_.front());
        batch_held_.erase(batch_held_.begin());
        if (!next.pointer) apply_attributed(next.event, mail);
    }
}

// AN ASK THE WINDOW ANSWERS ONLY ONCE IT HAS LANDED EVERY MOMENT BEFORE IT: what Workshop's overlay
// shows, since the version it last said.
bool WorkshopWeave::send_key_fence(loom::Mail& mail) {
    if (!window_.shown()) return false;
    const std::string office = std::string(graphics::kWindowOfficePrefix) + window_name_;
    const loom::AskOpened opened = area_asks_.open_to_role(
        office, graphics::AreaShowAsked::zen_name, graphics::AreaShowAsked::zen_version);
    if (!opened) return false;
    const loom::Ticket sent =
        mail.as_role(kWorkshopProvider)
            .send_to_role(office, graphics::AreaShowAsked{kWorkshopOverlayArea, fence_since_},
                          opened.correlation);
    if (!sent.valid()) {
        (void)area_asks_.forget(opened.id);
        return false;
    }
    key_fence_ = opened.id;
    fence_sent_covers_ = held_seq_;
    return true;
}

// ...ANSWERED: every pointer moment before it is landed, and the keys behind them go.
bool WorkshopWeave::take_key_fence(std::uint64_t ask, const graphics::AreaShown* shown,
                                   loom::Mail& mail) {
    if (key_fence_ == 0 || ask != key_fence_) return false;
    key_fence_ = 0;
    fence_covers_ = fence_sent_covers_;
    fence_since_ = shown != nullptr ? shown->version : 0;
    release_held_input(mail);
    return true;
}

// A REFERENCE CARRIED BY AN INPUT ACTOR THAT HAS LEFT IS PUT DOWN at the next moment of input
// Workshop hears, the desk standing behind the panes again: nobody is left carrying it.
bool WorkshopWeave::release_departed_carry(loom::Mail& mail) {
    if (carried_.data.empty() || carried_.actor.local) return false;
    const auto authority = host_->input_authority
        ? host_->input_authority(carried_.actor.participant) : loom::GrantAuthority{};
    if (mail.describe_authority(authority).available) return false;
    carried_ = {};
    say("Carried reference released: its input actor has left", false);
    repaint(mail);
    return true;
}

void WorkshopWeave::apply_attributed(const input::AttributedInput& event, loom::Mail& mail) {
    (void)release_departed_carry(mail);
    struct RestoreInput {
        InputActor& actor;
        bool& applying;
        InputActor before;
        bool was_applying;
        ~RestoreInput() { actor = before; applying = was_applying; }
    } restore{input_actor_, applying_attributed_, input_actor_, applying_attributed_};
    input_actor_ = {true, event.local, loom::WeaveId{static_cast<std::uint64_t>(event.actor)}};
    applying_attributed_ = true;
    const auto& e = event.event;
    // A DRAG CARRY ENDS AT ITS CARRIER'S NEXT GESTURE, OR THE WEAVER'S: another guest's never.
    // The wheel is none: it turns the pane under the hand, and the carry stands.
    if (!quitting_ && carried_.drag &&
        (input_actor_.local || (input_actor_.local == carried_.actor.local &&
                                input_actor_.participant == carried_.actor.participant)) &&
        (e.kind == "KeyPressed" || e.kind == "TextEntered" ||
         (e.kind == "PointerButton" && e.pressed))) {
        carried_ = {}; value_drag_ = {};
        say("Value drag cancelled by a new gesture", false);
    }
    // A PARTICIPANT'S POINTER IS THE WINDOW'S TO LAND: it takes the input office's moments as the
    // hand they name, and says the hand on every moment it gives. Workshop takes its keys.
    if (e.kind == "KeyPressed") on(input::KeyPressed{e.scancode, e.name, e.modifiers}, mail);
    else if (e.kind == "KeyReleased") on(input::KeyReleased{e.scancode, e.name, e.modifiers}, mail);
    else if (e.kind == "TextEntered") on(input::TextEntered{e.text}, mail);
}

void WorkshopWeave::on(const PaneShortcutInvoked& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(kDesktopRole) || !mail.correlation()) return;
    Hand* by = nullptr;
    each_hand([&](Hand& h) {
        if (h.app_asked.answering == mail.correlation() && h.app_asked.gesture == h.latest) by = &h;
    });
    if (by == nullptr) return;
    by->app_asked = {};
    const auto* pane = session_.panes.runtime.find(asked.office, asked.pane);
    const auto* rows = pane ? session_.keymap.pane_rows(pane->kind) : nullptr;
    bool declared = false;
    if (rows) for (const auto& row : rows->rows) if (row.id == asked.action) declared = true;
    if (!pane || !declared || asked.holder <= 0 || !host_->role_holder ||
        host_->role_holder(asked.office).value != static_cast<std::uint64_t>(asked.holder)) {
        say("Shortcut unavailable: its pane, action or provider has changed", true); repaint(mail); return;
    }
    if (const std::string why = refused_toward(by->actor, pane->kind); !why.empty()) {
        refuse_input(why, mail);
        return;
    }
    const auto correlation = ++gesture_asks_;
    const auto sent = mail.as_role(kWorkshopProvider).send_to_role(asked.office,
        PaneActionRequested{asked.pane, asked.action}, correlation);
    if (sent.valid()) keep_act(*by, by->shortcut_sent = {pane->kind, by->latest, correlation});
    else { say("Shortcut invocation could not be queued", true); repaint(mail); }
}

// ONE CURRENT GESTURE APPROVES ONE OPERATION FOR THE PANE IT WAS DELIVERED TO, ONCE: the requester
// holds the office that offered the pane, the pane is on the desk (or a shortcut sent this action),
// the correlation is that pane's unspent press, action, menu choice or shortcut still its hand's
// latest, a guest's live Loom authority permits the send it names, and its admitted row holds every
// action class the act is. The gesture is spent, and `approved` says whose it was.
// WL-GUEST-03, WL-GUEST-05 -- agents/workshop/guests.md
std::string WorkshopWeave::approve_gesture(const std::string& pane_key, std::int64_t gesture,
                                           const std::string& role, const std::string& shape,
                                           std::int64_t version, loom::Mail& mail,
                                           const std::vector<std::string>& classes,
                                           const std::string& subject, InputActor* approved) {
    const auto* pane = session_.panes.runtime.find(mail.authored_role(), pane_key);
    const auto corr = gesture > 0 ? static_cast<std::uint64_t>(gesture) : 0;
    bool shortcut = false;
    if (pane && corr)
        each_hand([&](Hand& h) {
            shortcut = shortcut || (h.shortcut_sent.kind == pane->kind &&
                                    h.shortcut_sent.answering == corr && h.shortcut_sent.gesture == h.latest);
        });
    if (mail.authored_role().empty() || !pane || (!session_.panes.has(pane->kind) && !shortcut) ||
        !host_->role_holder || host_->role_holder(mail.authored_role()) != mail.sender()) {
        return "the requesting pane is no longer on this desk";
    }
    const bool names_send = !role.empty() || !shape.empty() || version != 0;
    // THE RECORD THIS CORRELATION NAMES, in whichever hand sent it, current while nothing of that
    // hand's own came since: another hand's act neither spends nor stales it.
    Hand* by = nullptr;
    each_hand([&](Hand& h) {
        if (!corr) return;
        for (auto* sent : {&h.action_sent, &h.press_sent, &h.shortcut_sent}) {
            if (sent->answering == corr && sent->kind == pane->kind && sent->gesture == h.latest) {
                by = &h;
                *sent = GestureSent{};
            }
        }
        for (auto& sent : h.secondary_cont) {
            if (sent.live && !sent.spent && !sent.interrupted && sent.correlation == corr &&
                sent.kind == pane->kind && sent.gesture_at_press == h.latest) {
                by = &h;
                sent.spent = true;
            }
        }
        if (!h.choice_answered.spent && h.choice_answered.correlation == corr &&
            h.choice_answered.kind == pane->kind && h.choice_answered.gesture == h.latest) {
            by = &h;
            h.choice_answered.spent = true;
        }
    });
    // A CLASSED ASK NAMING NO SEND spends its act's record while it is unspent, whatever its hand
    // did since: no send rides on it, so nothing is laundered through a stale gesture, and its
    // classes are judged for the hand whose act it was.
    if (by == nullptr && corr && !names_send && !classes.empty()) {
        each_hand([&](Hand& h) {
            if (by != nullptr) return;
            for (const GestureSent& k : h.kept) {
                if (k.answering != corr || k.kind != pane->kind) continue;
                by = &h;
                for (auto* sent : {&h.action_sent, &h.press_sent, &h.shortcut_sent})
                    if (sent->answering == corr) *sent = GestureSent{};
                if (h.choice_answered.correlation == corr) h.choice_answered.spent = true;
                return;
            }
        });
    }
    if (by != nullptr) drop_kept(corr);
    // A SEND ON AN ACTOR'S BEHALF NEEDS THAT ACTOR NAMED; an act the pane does itself, judged by its
    // classes alone, takes unattributed input as the weaver's hand -- no guest's moment arrives
    // unattributed, since no guest may say a raw input moment.
    if (by == nullptr || (!by->actor.known && (names_send || classes.empty()))) {
        return "this operation needs a current attributed input gesture";
    }
    const InputActor actor = by->actor;
    if ((names_send || classes.empty()) &&
        (role.empty() || shape.empty() || version <= 0 ||
         version > std::numeric_limits<std::uint32_t>::max())) {
        return "the operation must name its destination and versioned shape";
    }
    if (names_send && !actor.local) {
        const auto authority = host_->input_authority
            ? host_->input_authority(actor.participant) : loom::GrantAuthority{};
        const auto live = mail.describe_authority(authority);
        if (!live.available || !live.permits_role(shape, static_cast<std::uint32_t>(version), role)) {
            return "the input actor has no authority for this operation";
        }
    }
    // A SEND ON A GUEST'S BEHALF IS THE CLASSES ITS SHAPE IS: the owner hears it from the pane
    // that relays it, never from the guest, so it is judged here, for the hand that asked.
    if (names_send) {
        if (std::string refused = scope::judge_send(guest_of(actor), host_->host_fact, shape); !refused.empty())
            return refused;
    }
    if (std::string refused = judge_classes(actor, classes, subject); !refused.empty()) return refused;
    if (approved != nullptr) *approved = actor;
    return {};
}

// The classes an act is, judged for one actor: the weaver's own hand and every participant no guest
// door admitted are not narrowed; a guest's are judged against its admitted row and this host.
// WL-GUEST-03 -- agents/workshop/guests.md
std::string WorkshopWeave::judge_classes(const InputActor& actor, const std::vector<std::string>& classes,
                                         const std::string& subject) const {
    if (classes.empty()) return {};
    return scope::judge_all(guest_of(actor), host_->host_fact, classes, subject);
}

scope::GuestRowFacts WorkshopWeave::guest_of(const InputActor& actor) const {
    if (!actor.known || actor.local || !host_->guest_row) return {};
    return host_->guest_row(actor.participant);
}

// ON A WEAVER'S HOST, THE PLACES ONLY THE WEAVER'S HAND REACHES: the editor office, whichever
// implementation holds it, the Terminal, the Hotkeys pane, whose every act edits the keymap file,
// and the quick setup, which asks the editor switch and writes Workshop's settings file. A guest's
// keys, text and presses bound there are refused in words at the dispatch.
// WL-GUEST-06 -- agents/workshop/guests.md
std::string WorkshopWeave::refused_toward(const InputActor& actor, std::int64_t kind) const {
    const RuntimePane* row = session_.panes.runtime.of_kind(kind);
    if (row == nullptr) return {};
    const scope::GuestRowFacts guest = guest_of(actor);
    if (!guest.admitted || !weaver_only_pane(*row)) return {};
    if (row->provider == kEditorRole) return scope::refuse_toward(guest, host_->host_fact, "the editor");
    if (row->provider == kTerminalRole) return scope::refuse_toward(guest, host_->host_fact, "the Terminal");
    if (row->provider == first_run::kFirstRunRole) {
        return scope::refuse_toward(guest, host_->host_fact,
                                    "the quick setup, which switches the editor and writes the settings file,");
    }
    return scope::refuse_toward(guest, host_->host_fact, "the Hotkeys pane, which edits the keymap file,");
}

// ...AND SO PLACED IN A WINDOW while a guest door listens on a weaver's host: the window, which
// cannot tell a guest's hand from another participant's, then lands no injected hand there.
// WL-GUEST-06 -- agents/workshop/guests.md
bool WorkshopWeave::weaver_only_pane(const RuntimePane& row) const {
    if (host_->host_fact.development || !host_->guest_row) return false;
    return row.provider == kEditorRole || row.provider == kTerminalRole ||
           row.provider == first_run::kFirstRunRole ||
           (row.provider == kDesktopRole && row.pane == kHotkeysPane);
}

void WorkshopWeave::refuse_input(const std::string& why, loom::Mail& mail) {
    say(why, true);
    repaint(mail);
}

std::string WorkshopWeave::authorize_pane_operation(const PaneOperationRequested& asked, loom::Mail& mail) {
    InputActor actor;
    auto reason = approve_gesture(asked.pane, asked.gesture, asked.role, asked.shape, asked.version, mail,
                                  {}, {}, &actor);
    if (reason.empty())
        approved_operation_ = {mail.sender(), asked.pane,
                               asked.gesture > 0 ? static_cast<std::uint64_t>(asked.gesture) : 0,
                               latest_of(actor), actor};
    return reason;
}
void WorkshopWeave::on(const PaneOperationRequested& asked, loom::Mail& mail) {
    const auto reason = authorize_pane_operation(asked, mail);
    (void)mail.answer(PaneOperationAnswered{reason.empty(), reason});
}
// A TRANSCRIPT VALUE, captured for the current actor's gesture: the exact participant and
// observation, authorized now and carried as content, never a replay of the line or its authority.
// WL-TERM-17 -- agents/workshop/terminal.md
void WorkshopWeave::on(const TerminalValueRequested& asked, loom::Mail& mail) {
    const auto reason = authorize_pane_operation({asked.pane, kWorkshopProvider,
        TerminalValueRequested::zen_name, TerminalValueRequested::zen_version, asked.gesture}, mail);
    const auto refuse = [&](std::string why) {
        approved_operation_ = {};
        (void)mail.answer(TerminalValueAnswered{false, std::move(why), {}, {}});
    };
    if (!reason.empty()) { refuse(reason); return; }
    const auto* terminal = host_->terminal;
    if (!terminal || asked.participant <= 0 || terminal->id().value !=
            static_cast<std::uint64_t>(asked.participant) || asked.observation <= 0) {
        refuse("The source terminal is no longer available"); return;
    }
    const auto& record = terminal->transcript();
    const auto observation = static_cast<std::uint64_t>(asked.observation);
    const auto value = record.retained_value(observation);
    if (!value) { refuse("This transcript value is no longer retained, or is only local text"); return; }
    const auto entries = record.entries();
    const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& e) { return e.seq == observation; });
    if (entry == entries.end()) { refuse("This transcript entry is no longer retained"); return; }
    TerminalCaptureFacts facts;
    facts.participant = asked.participant; facts.observation = asked.observation;
    facts.kind = loom::name_of(entry->kind);
    if (entry->kind == loom::TranscriptKind::Submitted) {
        facts.addressing = loom::name_of(entry->addressing);
        if (entry->addressing == loom::Addressing::Weave) facts.target = std::to_string(entry->target.value);
        if (entry->addressing == loom::Addressing::Role) facts.role = entry->role;
    } else {
        facts.sender = std::to_string(entry->sender.value);
        facts.role = entry->authored_role;
        facts.authenticated_answer = entry->answers_ask;
    }
    try {
        const auto bytes = inventory::encode_pair(*value, {loom::to_value(facts)});
        if (bytes.size() > 65536) { refuse("This value exceeds the 64 KiB carry limit"); return; }
        (void)mail.answer(TerminalValueAnswered{true, {}, value->schema().name(),
                                               loom::Bytes(bytes.begin(), bytes.end())});
    } catch (const std::exception& e) { refuse(std::string("This value could not be captured: ") + e.what()); }
}
// WL-GUEST-03 -- agents/workshop/guests.md
// A classed act's approval is the pane's to carry to its later beats; this host keeps nothing of it,
// so it never stands in for the one carry `approved_operation_` waits on.
void WorkshopWeave::on(const v2::PaneOperationRequested& asked, loom::Mail& mail) {
    const auto reason = approve_gesture(asked.pane, asked.gesture, asked.role, asked.shape, asked.version,
                                        mail, asked.classes, asked.subject);
    (void)mail.answer(PaneOperationAnswered{reason.empty(), reason});
}

// AN OBSERVATION LEASE: the same one-gesture approval, kept for repeated reads of one shape at one
// role about one subject. One lease per pane (a new request replaces it), a bounded book, and no
// authority of its own: every continuation is judged again below.
void WorkshopWeave::on(const PaneObservationRequested& asked, loom::Mail& mail) {
    InputActor actor;
    auto reason = approve_gesture(asked.pane, asked.gesture, asked.role, asked.shape, asked.version, mail,
                                  {}, {}, &actor);
    if (reason.empty() && (asked.subject.empty() || asked.subject.size() > 256))
        reason = "the observation must name its subject";
    const std::string office(mail.authored_role());
    if (reason.empty()) {
        // A LEASE WHOSE HOLDER NO LONGER HOLDS ITS OFFICE can never be continued: forget it
        // before counting, so a departed provider's approvals cannot fill the book.
        std::erase_if(leases_, [&](const ObservationLease& l) {
            return (l.office == office && l.pane == asked.pane) ||
                   !host_->role_holder || host_->role_holder(l.office) != l.holder;
        });
        if (leases_.size() >= kMaxObservationLeases)
            reason = "too many observations are active; pause one first";
    }
    if (!reason.empty()) { (void)mail.answer(PaneObservationAnswered{false, reason, 0}); return; }
    leases_.push_back({++next_lease_, mail.sender(), office, asked.pane, asked.role, asked.shape,
                       asked.subject, asked.version, actor});
    (void)mail.answer(PaneObservationAnswered{true, {}, leases_.back().id});
}

// ONE MORE OBSERVATION UNDER A LEASE: its holder still holds the office, the pane is still on the
// desk, the subject is the one approved, and a guest actor is still present with the authority.
// Any lapse refuses AND forgets the lease; a request from anyone but its holder forgets nothing.
void WorkshopWeave::on(const PaneObservationContinued& asked, loom::Mail& mail) {
    const std::string office(mail.authored_role());
    const auto it = std::find_if(leases_.begin(), leases_.end(), [&](const ObservationLease& l) {
        return l.id == asked.lease && l.office == office && l.pane == asked.pane && l.holder == mail.sender();
    });
    if (office.empty() || it == leases_.end()) {
        (void)mail.answer(PaneObservationAnswered{false, "this observation is not current; start it again with a gesture", 0});
        return;
    }
    std::string reason;
    const auto* pane = session_.panes.runtime.find(office, asked.pane);
    if (!host_->role_holder || host_->role_holder(office) != mail.sender()) {
        reason = "the observing pane's office changed hands";
    } else if (asked.subject != it->subject) {
        reason = "the observation's subject changed; start it again";
    } else if (!pane || !session_.panes.has(pane->kind)) {
        reason = "the observing pane is no longer on the desk";
    } else if (!it->actor.local) {
        const auto authority = host_->input_authority
            ? host_->input_authority(it->actor.participant) : loom::GrantAuthority{};
        const auto live = mail.describe_authority(authority);
        if (!live.available || !live.permits_role(it->shape, static_cast<std::uint32_t>(it->version), it->role))
            reason = "the actor who started this observation has left or lost its authority";
    }
    if (!reason.empty()) {
        leases_.erase(it);
        (void)mail.answer(PaneObservationAnswered{false, reason, 0});
        return;
    }
    (void)mail.answer(PaneObservationAnswered{true, {}, it->id});
}

// ONLY THE HOLDER ENDS ITS OWN LEASE -- one by id, or with lease 0 every one it holds on that pane,
// which is what an arriving image says: a reload keeps the WeaveId, so this book cannot tell the
// predecessor's leases from the successor's. Ending only removes approvals; it grants nothing.
void WorkshopWeave::on(const PaneObservationEnded& asked, loom::Mail& mail) {
    const std::string office(mail.authored_role());
    std::erase_if(leases_, [&](const ObservationLease& l) {
        return (asked.lease == 0 || l.id == asked.lease) && l.office == office && l.pane == asked.pane &&
               l.holder == mail.sender();
    });
}

void WorkshopWeave::on(const PaneCarryRequested& asked, loom::Mail& mail) {
    accept_carry(asked, false, false, mail);
}
void WorkshopWeave::on(const PaneValueCarryRequested& asked, loom::Mail& mail) {
    accept_carry({asked.pane, asked.label, asked.data}, true, asked.drag, mail);
}
void WorkshopWeave::on(const v2::PaneValueCarryRequested& asked, loom::Mail& mail) {
    if (asked.token.empty() || asked.token.size() > 128) {
        (void)mail.answer(PaneCarryAnswered{false, "the transfer token needs 1..128 bytes"}); return;
    }
    accept_carry({asked.pane, asked.label, asked.data}, true, asked.drag, mail, asked.token);
}
void WorkshopWeave::accept_carry(const PaneCarryRequested& asked, bool value, bool drag, loom::Mail& mail,
                                 std::string token) {
    const auto* pane = session_.panes.runtime.find(mail.authored_role(), asked.pane);
    if (!pane || !session_.panes.has(pane->kind) || !approved_operation_.actor.known ||
        approved_operation_.pane_owner != mail.sender() ||
        approved_operation_.pane != asked.pane ||
        approved_operation_.correlation != mail.correlation() ||
        approved_operation_.gesture != latest_of(approved_operation_.actor)) {
        (void)mail.answer(PaneCarryAnswered{false, "the approved acquisition is no longer current"});
        return;
    }
    const InputActor carrier = approved_operation_.actor;
    const std::uint64_t approved = approved_operation_.gesture;
    approved_operation_ = {};
    // ON A WEAVER'S HOST WHAT A GUEST WOULD CARRY RESTS IN NO PANE, as its typed text rests in
    // none: dropped on a pane it would stand as a draft the weaver's own Submit, commit or save
    // sends as the weaver's. So the carry does not begin, and the one carry the desk holds stays
    // the weaver's to take.
    if (const std::string why = scope::refuse_carry(guest_of(carrier), host_->host_fact);
        !why.empty()) {
        say(why, true);
        (void)mail.answer(PaneCarryAnswered{false, why});
        repaint(mail);
        return;
    }
    // A DRAG IS THE CARRIER'S OWN APPROVED PRESS, still its latest: another hand's press, made
    // since, holds no carry of this one's.
    if (drag && (value_drag_.gesture == 0 || value_drag_.gesture != approved ||
                 value_drag_.actor.local != carrier.local ||
                 value_drag_.actor.participant != carrier.participant)) {
        (void)mail.answer(PaneCarryAnswered{false, "this value drag no longer has its primary press"});
        return;
    }
    if (asked.data.empty() || asked.data.size() > kMaxCarryBytes || asked.label.size() > kMaxCarryLabelBytes) {
        (void)mail.answer(PaneCarryAnswered{false, "the carried reference exceeds its limits"});
        return;
    }
    if (!carried_.data.empty()) {
        (void)mail.answer(PaneCarryAnswered{false, "another item is already being carried"}); return;
    }
    carried_ = {asked.data, asked.label, carrier, value, drag,
                std::string(mail.authored_role()), asked.pane, std::move(token)};
    if (!drag) say("Carrying " + asked.label + " -- click a receiving pane; Escape cancels", false);
    // A DRAG IS NOW A CARRY: Workshop's area comes in front of the pane's at the repaint, so the
    // window ends the press's hold there, and its later motion and its release are the carry's.
    (void)mail.answer(PaneCarryAnswered{true, {}});
    if (drag && value_drag_.released) finish_value_drag(mail);
    repaint(mail);
}

// WL-CANVAS-04 -- agents/workshop/canvas.md
WorkshopWeave::AreaPoint WorkshopWeave::pane_area_at(std::int64_t x, std::int64_t y) const {
    AreaPoint out;
    std::int64_t order = 0;
    for (const std::int64_t kind : placed_kinds_) {
        const RuntimePane* row = session_.panes.runtime.of_kind(kind);
        const auto placed =
            row == nullptr ? areas_placed_.end() : areas_placed_.find(pane_window_area(session_.panes, *row));
        if (placed == areas_placed_.end()) continue;
        const graphics::AreaPlaced& a = placed->second;
        const bool holds = x >= a.x && y >= a.y && x < graphics::add_cells(a.x, a.w) &&
                           y < graphics::add_cells(a.y, a.h);
        if (holds && (out.kind == kNoPaneKind || a.order > order)) {
            out = AreaPoint{kind, a.area, graphics::sub_px(x, a.x), graphics::sub_px(y, a.y)};
            order = a.order;
        }
    }
    return out;
}

// A VALUE OR A REFERENCE PLACED ON A PANE: the place in its area's own pixels, for the pane to read
// against what it drew there; a pane without the door for what is carried is told nothing and the
// item stays held, as on any place that does not accept it.
// WL-GUEST-06 -- agents/workshop/guests.md; WL-CANVAS-04 -- agents/workshop/canvas.md
bool WorkshopWeave::drop_carry(const AreaPoint& at, loom::Mail& mail) {
    if (carried_.data.empty()) return false;
    if (!input_actor_.known || input_actor_.local != carried_.actor.local ||
        input_actor_.participant != carried_.actor.participant) {
        say("Only the actor carrying this reference may place it", true);
        return true;
    }
    // A GUEST'S DROP, by click or by a drag's release, hands its bytes to the pane it lands on, and
    // on a weaver's host the places only the weaver's hand reaches take none.
    if (const std::string why = refused_toward(input_actor_, at.kind); !why.empty()) {
        say(why, true);
        return true;
    }
    const RuntimePane* pane = session_.panes.runtime.of_kind(at.kind);
    const bool value = carried_.value;
    if (pane == nullptr || !host_->holder_accepts ||
        !host_->holder_accepts(pane->provider, value ? *loom::schema_of<PaneCanvasValueDrop>()
                                                     : *loom::schema_of<PaneCanvasDrop>())) {
        say("This place does not accept the carried item; Escape cancels", true);
        return true;
    }
    const auto correlation = ++gesture_asks_;
    // WHAT THE WINDOW DREW UNDER THE HAND, where the moment being answered is its word about this
    // pane's area: the pane judges the drop by it.
    const bool word = window_under_.under == at.area;
    const std::string under = word ? window_under_.under : std::string();
    const std::string part = word ? window_under_.part : std::string();
    const std::int64_t drawing = word ? window_under_.drawing : 0;
    const auto sent = value
        ? mail.as_role(kWorkshopProvider).send_to_role(pane->provider,
              PaneCanvasValueDrop{pane->pane, at.area, at.x, at.y, carried_.data,
                                  carried_.source_office, carried_.source_pane, carried_.token,
                                  under, part, drawing},
              correlation)
        : mail.as_role(kWorkshopProvider).send_to_role(pane->provider,
              PaneCanvasDrop{pane->pane, at.area, at.x, at.y, carried_.data, under, part, drawing},
              correlation);
    if (!sent.valid()) {
        say("Item placement could not be queued; it is still held", true);
        return true;
    }
    // WHAT WAS CARRIED, FROM WHERE AND TO WHERE: its before and its after alone (WL-NOTE-06).
    const RuntimePane* from =
        session_.panes.runtime.find(carried_.source_office, carried_.source_pane);
    const std::string moved = carried_.label + ": " +
                              (from != nullptr ? from->name : carried_.source_office) + " -> " +
                              pane->name;
    carried_ = {};
    leave_carry_hover(mail);
    const auto kind = at.kind;
    keep_act(hand(), hand().press_sent = GestureSent{kind, hand().latest, correlation});
    session_.panes.selected = kind;
    session_.panes.keyboard = kind;
    note_routed(kind);
    say(moved, false);
    return true;
}

// WL-CANVAS-06 -- agents/workshop/canvas.md
void WorkshopWeave::carry_hover(std::int64_t x, std::int64_t y, loom::Mail& mail) {
    const AreaPoint at = carried_.data.empty() ? AreaPoint{} : pane_area_at(x, y);
    const RuntimePane* row =
        at.kind == kNoPaneKind ? nullptr : session_.panes.runtime.of_kind(at.kind);
    if (row == nullptr || !host_->holder_accepts ||
        !host_->holder_accepts(row->provider, *loom::schema_of<PaneCanvasHover>())) {
        leave_carry_hover(mail);
        return;
    }
    if (carry_hover_.kind != at.kind || carry_hover_.area != at.area) leave_carry_hover(mail);
    if (carry_hover_.kind == at.kind && carry_hover_.x == at.x && carry_hover_.y == at.y) return;
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(row->provider, PaneCanvasHover{row->pane, at.area, at.x, at.y, true});
    carry_hover_ = CarryHover{at.kind, at.area, at.x, at.y};
}

void WorkshopWeave::leave_carry_hover(loom::Mail& mail) {
    if (carry_hover_.kind == kNoPaneKind) return;
    if (const RuntimePane* row = session_.panes.runtime.of_kind(carry_hover_.kind)) {
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, PaneCanvasHover{row->pane, carry_hover_.area,
                                                         carry_hover_.x, carry_hover_.y, false});
    }
    carry_hover_ = CarryHover{};
}

void WorkshopWeave::begin_value_drag(const input::PointerButton& b) {
    value_drag_ = {};
    drag_pointer_ = canvas_point_of(b.space, b.x, b.y);
    value_drag_.gesture = gestures_;
    value_drag_.actor = input_actor_;
    value_drag_.x = b.x; value_drag_.y = b.y; value_drag_.space = b.space;
}
bool WorkshopWeave::move_value_drag(const input::PointerMoved& motion, loom::Mail& mail) {
    auto& drag = value_drag_;
    if (!drag.gesture || drag.gesture != latest_of(drag.actor) || drag.released || !input_actor_.known ||
        input_actor_.local != drag.actor.local || input_actor_.participant != drag.actor.participant)
        return false;
    if (motion.space != drag.space) return false;
    drag_pointer_ = canvas_point_of(motion.space, motion.x, motion.y);
    const std::uint64_t threshold = motion.space == input::space::kPixels ? 4 : 1;
    const auto distance = [](std::int64_t a, std::int64_t b) -> std::uint64_t {
        return a >= b ? std::uint64_t(a) - std::uint64_t(b) : std::uint64_t(b) - std::uint64_t(a);
    };
    if (distance(motion.x, drag.x) >= threshold || distance(motion.y, drag.y) >= threshold)
        drag.moved = true;
    if (!carried_.drag) return false;
    // A DRAG UNDER WAY SAYS NOTHING: its drop says where it went (WL-NOTE-06).
    if (drag.moved) repaint(mail);
    return true;
}
bool WorkshopWeave::release_value_drag(const input::PointerButton& button, loom::Mail& mail) {
    auto& drag = value_drag_;
    if (button.button != 1 || !drag.gesture || drag.gesture != latest_of(drag.actor) || drag.released ||
        !input_actor_.known || input_actor_.local != drag.actor.local ||
        input_actor_.participant != drag.actor.participant) return false;
    // The release is retained even when it precedes the asynchronous acquisition answer, and one
    // let go away from its press moved, whoever heard the motion between.
    drag.released = true;
    if (button.space == drag.space) {
        const std::uint64_t threshold = button.space == input::space::kPixels ? 4 : 1;
        const auto distance = [](std::int64_t a, std::int64_t b) -> std::uint64_t {
            return a >= b ? std::uint64_t(a) - std::uint64_t(b) : std::uint64_t(b) - std::uint64_t(a);
        };
        if (distance(button.x, drag.x) >= threshold || distance(button.y, drag.y) >= threshold)
            drag.moved = true;
    }
    // ...AND THE PANE'S AREA IT WAS LET GO OVER, and where in it, with the pane's holder then.
    if (button.space == drag.space && !session_.arrange.open && !session_.context.open && !session_.presented.open) {
        const auto point = canvas_point_of(button.space, button.x, button.y);
        const AreaPoint over = point.understood ? pane_area_at(point.px.x, point.px.y) : AreaPoint{};
        const auto* pane = over.kind == kNoPaneKind ? nullptr : session_.panes.runtime.of_kind(over.kind);
        if (pane && host_->role_holder) {
            drag.at = over;
            drag.receiver = host_->role_holder(pane->provider);
            drag.word = window_under_;
        }
    }
    if (!carried_.drag) return false;
    finish_value_drag(mail); return true;
}
void WorkshopWeave::finish_value_drag(loom::Mail& mail) {
    const auto drag = value_drag_;
    value_drag_ = {};
    if (!carried_.drag) return;
    if (!drag.moved) { carried_ = {}; leave_carry_hover(mail); return; }
    const auto* pane = session_.panes.runtime.of_kind(drag.at.kind);
    if (drag.gesture != latest_of(drag.actor) || !pane || !drag.receiver.valid() || !host_->role_holder ||
        host_->role_holder(pane->provider) != drag.receiver) {
        carried_ = {}; leave_carry_hover(mail);
        say("Value drag cancelled: no current receiver at the release", true); return;
    }
    const auto previous = input_actor_;
    input_actor_ = drag.actor;
    const WindowUnder answering = window_under_;
    window_under_ = drag.word; // what the window drew under the hand at the release
    (void)drop_carry(drag.at, mail);
    window_under_ = answering;
    input_actor_ = previous;
    if (!carried_.data.empty()) {
        carried_ = {};
        leave_carry_hover(mail);
        say("Value was not placed: " + session_.notice, true);
    }
}
} // namespace zengine::workshop
