// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_MENU_HPP
#define ZENGINE_WORKSHOP_PANE_MENU_HPP

// THE SMALL HELPERS A PANE USES TO SPEND THE SECOND BUTTON AND ASK FOR A MENU -- installed
// beside the protocol (`workshop/pane_vocabulary.hpp`, the `zengine::pane` target), so a pane
// built against the package alone has them, and optional: everything here is a few lines over
// the raw shapes, and a pane may write those lines itself or leave the helpers out.
//
// WHAT THEY REMOVE: the declaration of a menu row by row, the echo of the gesture's correlation
// on every continuation (the one thing a pane MUST get right for the host to act), and the four
// checks an answer needs before a pane may act on it -- in one read (`Asked::take`).
//
// WHAT THEY DO NOT DO: perform an operation. A chosen row is a fact about the maker's gesture;
// what it means is the pane's, judged against the pane's current subjects when the answer
// arrives. Nothing here reaches the host's authority.
//
//     pane_menu::Asked asked_;   // THIS image's one outstanding menu -- never reload-kept state
//
//     void on(const PaneButton& b, loom::Mail& mail) {
//         if (!b.pressed || b.button != 3) return;             // a release: nothing to do
//         if (heading_row(b.row)) { pane_menu::pass_back(mail, kOffice, kPane); return; }
//         asked_ = pane_menu::Offer(kPane, subject_at(b.row))
//                      .at(b.row, b.column)
//                      .row("mine.open", "Open")
//                      .row("mine.forget", "Forget")
//                      .send(mail, kOffice);                   // continues THIS press
//     }
//     void on(const PaneMenuAnswered& a, loom::Mail& mail) {
//         const std::string id = asked_.take(mail, a);         // provenance, lifetime, once, subject
//         if (id == "mine.open") open(a.subject, mail);        // ...and the subject is still yours
//         else if (id == "mine.forget") forget(a.subject, mail);  // to judge against what you hold
//     }

#include "workshop/pane_vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace zengine::workshop::pane_menu {

/// THE OFFICE WORKSHOP HOLDS, spelled here so a pane that includes only the installed protocol
/// need not spell it. A stranger's pane addresses Workshop by this name and no other.
inline constexpr const char* kWorkshopRole = "zengine.workshop";

/// THE OFFICE THAT PRESENTS A MENU AND ANSWERS IT (`workshop::kPresenterRole`).
inline constexpr const char* kPresenterRole = ::zengine::workshop::kPresenterRole;

/// ONE MENU THIS IMAGE ASKED FOR, UNTIL IT IS ANSWERED: the request's number, and the pane and
/// subject it was about. What makes an answer safe to act on is checked here in one read --
///
///   provenance  a CHOICE counts only from the presenter's office; a refusal may also be
///               Workshop's, which answers an ask it never presented. Anybody else's word,
///               however it is shaped, settles nothing.
///   lifetime    the answer settles THIS image's outstanding ask, matched by its number; an
///               answer to an ask this image never sent -- a predecessor's, a forged one -- is
///               nothing, and a newer ask replaces an older one whose answer then matches nothing.
///   once        the first authenticated answer settles the ask; a duplicate finds none pending.
///   subject     the answer must be about the pane and subject asked.
///
/// Whether that subject STILL applies is the pane's own question, asked after, against what it
/// holds when the answer arrives.
///
/// KEEP IT IN THE IMAGE, NEVER IN RELOAD-KEPT STATE. A successor that inherited a predecessor's
/// record would accept the predecessor's menu as its own; an image-local record means a reloaded
/// pane CANCELS every menu its predecessor had open -- the policy the shipped panes choose. A pane
/// that wants its menus to survive a reload would carry the record deliberately, and say so.
class Asked {
public:
    /// Is there an ask this image is still waiting to hear answered?
    bool pending() const noexcept { return correlation_ != 0; }
    std::uint64_t correlation() const noexcept { return correlation_; }
    const std::string& pane() const noexcept { return pane_; }
    const std::string& subject() const noexcept { return subject_; }

    /// SETTLE `answer` IF IT ANSWERS THIS ASK, and return the id of the row chosen -- empty when
    /// nothing was chosen, or when the answer does not answer this ask (then nothing is settled).
    std::string take(const loom::Mail& mail, const PaneMenuAnswered& answer,
                     std::string_view presenter = kPresenterRole,
                     std::string_view workshop = kWorkshopRole) {
        if (!pending() || mail.correlation() != correlation_ || answer.pane != pane_ ||
            answer.subject != subject_) {
            return std::string(); // not this image's ask, or not about what it asked
        }
        const bool presented = mail.authored_from_role(presenter);
        if (!presented && !(mail.authored_from_role(workshop) && !answer.chosen)) {
            return std::string(); // nobody who may answer said this; the ask stays pending
        }
        clear(); // settled: a duplicate finds nothing pending
        return answer.chosen ? answer.id : std::string();
    }

    /// GIVE UP ON THE ASK: its answer, when it comes, matches nothing.
    void clear() noexcept {
        correlation_ = 0;
        pane_.clear();
        subject_.clear();
    }

private:
    friend class Offer;
    std::uint64_t correlation_ = 0;
    std::string pane_;
    std::string subject_;
};

/// A MENU A PANE OFFERS, BUILT ROW BY ROW: about `subject` (the pane's own word, echoed back
/// unread), beside a place in the granted lattice. `send` continues the gesture `mail`
/// delivered -- its correlation is what makes the request eligible -- and returns the record
/// of the ask (`Asked`), pending exactly when a request was queued under a nonzero number.
class Offer {
public:
    Offer(std::string pane, std::string subject) {
        request_.pane = std::move(pane);
        request_.subject = std::move(subject);
    }

    Offer& at(std::int64_t row, std::int64_t column) {
        request_.row = row;
        request_.column = column;
        return *this;
    }

    Offer& row(std::string id, std::string label) {
        request_.rows.push_back(PaneMenuRow{std::move(id), std::move(label)});
        return *this;
    }

    bool empty() const noexcept { return request_.rows.empty(); }
    const PaneMenuRequested& request() const noexcept { return request_; }

    /// SEND AS `office`, CONTINUING THE GESTURE THIS DELIVERY BROUGHT (a `PaneButton` press or a
    /// `PaneActionRequested`): the request goes out under `mail.correlation()`.
    Asked send(loom::Mail& mail, std::string_view office,
               std::string_view workshop = kWorkshopRole) const {
        return continuing(mail, office, mail.correlation(), workshop);
    }

    /// SEND CONTINUING A GESTURE BY ITS NUMBER -- for a pane that kept the number of a press it
    /// heard earlier in the same delivery chain, or that answers from a later delivery of its
    /// own. Zero continues nothing: the host refuses it, and the record is not pending.
    Asked continuing(loom::Mail& mail, std::string_view office, std::uint64_t correlation,
                     std::string_view workshop = kWorkshopRole) const {
        Asked asked;
        const loom::Ticket sent = mail.as_role(office).send_to_role(workshop, request_, correlation);
        if (sent.valid() && correlation != 0) {
            asked.correlation_ = correlation;
            asked.pane_ = request_.pane;
            asked.subject_ = request_.subject;
        }
        return asked;
    }

private:
    PaneMenuRequested request_;
};

/// HAND THE PRESS THIS DELIVERY BROUGHT BACK TO THE HOST: "not mine -- open your own surface for
/// my pane". Echoes the press's correlation; the host opens its pane menu once, at the press's
/// place, while the press is still the maker's latest act.
inline loom::Ticket pass_back(loom::Mail& mail, std::string_view office, std::string pane,
                              std::string_view workshop = kWorkshopRole) {
    return mail.as_role(office).send_to_role(workshop, PanePassRequested{std::move(pane)},
                                             mail.correlation());
}

/// ASK THE HOST FOR THE KEYBOARD, CONTINUING A MENU ANSWER BY ITS NUMBER -- for a pane whose
/// chosen row begins an edit that opens a DOOR first, so the line the maker will type into
/// exists only once an office has answered, one or more deliveries later. The pane keeps the
/// choice's number across that round trip and spends it here; the host judges it exactly as it
/// judges the same-delivery form -- once, and only while that choice is still the maker's
/// latest act -- so a round trip the maker interrupted grabs nothing. Zero continues nothing.
inline loom::Ticket take_keyboard_continuing(loom::Mail& mail, std::string_view office,
                                             std::string pane, std::uint64_t correlation,
                                             std::string_view workshop = kWorkshopRole) {
    return mail.as_role(office).send_to_role(workshop, PaneKeyboardRequested{std::move(pane)},
                                             correlation);
}

/// ASK THE HOST FOR THE KEYBOARD, continuing the menu answer this delivery brought -- for a pane
/// whose chosen row begins an edit (a capture, a typed spelling) and so needs the keys the menu
/// deliberately left where they were. The host grants them only while the choice is still the
/// maker's latest act, so a newer press or key defeats a late grab.
inline loom::Ticket take_keyboard(loom::Mail& mail, std::string_view office, std::string pane,
                                  std::string_view workshop = kWorkshopRole) {
    return take_keyboard_continuing(mail, office, std::move(pane), mail.correlation(), workshop);
}

/// ASK THE HOST FOR ITS OWN PANE MENU ON `target` (a `PaneRef`'s two halves), continuing the
/// menu answer this delivery brought -- the deliberate management route for a pane that is
/// covered, closed, or consumes every right press.
inline loom::Ticket manage(loom::Mail& mail, std::string_view office, std::string pane,
                           std::string target_office, std::string target,
                           std::string_view workshop = kWorkshopRole) {
    return mail.as_role(office).send_to_role(
        workshop, PaneManageRequested{std::move(pane), std::move(target_office), std::move(target)},
        mail.correlation());
}

/// A HELD SECONDARY BUTTON, FOR A PANE THAT ACTS WHILE IT IS DOWN: the whole bookkeeping is one
/// bool and whether the last end was a `lost` release. `take` returns true when the state moved.
struct HeldButton {
    std::int64_t button = 0;
    bool held = false;
    bool lost = false;

    bool take(const PaneButton& b) noexcept {
        if (b.pressed) {
            const bool moved = !held || button != b.button;
            button = b.button;
            held = true;
            lost = false;
            return moved;
        }
        if (!held || b.button != button) {
            return false; // a release of a button this pane does not hold: nothing
        }
        held = false;
        lost = b.lost;
        return true;
    }
};

} // namespace zengine::workshop::pane_menu

#endif // ZENGINE_WORKSHOP_PANE_MENU_HPP
