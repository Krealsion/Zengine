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
// on every continuation (the one thing a pane MUST get right for the host to act), and the
// bookkeeping of "which press was that" -- a request carries the pane's own `subject`, the
// answer echoes it, and the pane keeps no menu state at all.
//
// WHAT THEY DO NOT DO: perform an operation. A chosen row is a fact about the maker's gesture;
// what it means is the pane's, judged against the pane's current subjects when the answer
// arrives (`chosen`). Nothing here reaches the host's authority.
//
//     void on(const PaneButton& b, loom::Mail& mail) {
//         if (!b.pressed || b.button != 3) return;             // a release: nothing to do
//         if (heading_row(b.row)) { pane_menu::pass_back(mail, kOffice, kPane); return; }
//         pane_menu::Offer(kPane, subject_at(b.row))
//             .at(b.row, b.column)
//             .row("mine.open", "Open")
//             .row("mine.forget", "Forget")
//             .send(mail, kOffice);                            // continues THIS press
//     }
//     void on(const PaneMenuAnswered& a, loom::Mail& mail) {
//         // `chosen_from` authenticates the presenter AND matches the row in one read, so a
//         // forged or foreign answer cannot spend the choice; the subject is still yours to judge.
//         if (pane_menu::chosen_from(mail, a, kPane, "mine.open")) open(a.subject, mail);
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

/// A MENU A PANE OFFERS, BUILT ROW BY ROW: about `subject` (the pane's own word, echoed back
/// unread), beside a place in the granted lattice. `send` continues the gesture `mail`
/// delivered -- its correlation is what makes the request eligible -- and returns the ticket.
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
    loom::Ticket send(loom::Mail& mail, std::string_view office,
                      std::string_view workshop = kWorkshopRole) const {
        return continuing(mail, office, mail.correlation(), workshop);
    }

    /// SEND CONTINUING A GESTURE BY ITS NUMBER -- for a pane that kept the number of a press it
    /// heard earlier in the same delivery chain, or that answers from a later delivery of its
    /// own. Zero continues nothing and is refused by the host.
    loom::Ticket continuing(loom::Mail& mail, std::string_view office, std::uint64_t correlation,
                            std::string_view workshop = kWorkshopRole) const {
        return mail.as_role(office).send_to_role(workshop, request_, correlation);
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

/// ASK THE HOST FOR THE KEYBOARD, continuing the menu answer this delivery brought -- for a pane
/// whose chosen row begins an edit (a capture, a typed spelling) and so needs the keys the menu
/// deliberately left where they were. The host grants them only while the choice is still the
/// maker's latest act, so a newer press or key defeats a late grab.
inline loom::Ticket take_keyboard(loom::Mail& mail, std::string_view office, std::string pane,
                                  std::string_view workshop = kWorkshopRole) {
    return mail.as_role(office).send_to_role(workshop, PaneKeyboardRequested{std::move(pane)},
                                             mail.correlation());
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

/// WAS THIS ROW CHOSEN, FOR THIS PANE? The payload half of the read a pane needs on an answer;
/// the subject it was about is `answer.subject`, for the pane to judge against what it holds now.
/// This checks the PAYLOAD only -- pair it with `from_workshop` (or use `chosen_from`, which does
/// both) so an answer forged by another office cannot spend a choice.
inline bool chosen(const PaneMenuAnswered& answer, std::string_view pane, std::string_view id) {
    return answer.chosen && answer.pane == pane && answer.id == id;
}

/// DOES THIS ANSWER COME FROM THE PRESENTER? An answer is the presenter's role speech (Workshop
/// by default) or Loom's answer to an ask this pane sent it; the office's holder at delivery is
/// the only one who could have given it. A pane that checks nothing else may check this.
inline bool from_workshop(const loom::Mail& mail, std::string_view workshop = kWorkshopRole) {
    return mail.authored_from_role(workshop);
}

/// WAS THIS ROW CHOSEN, FOR THIS PANE, BY THE PRESENTER? The safe one-call read: it authenticates
/// the sender (`from_workshop`) AND matches the payload (`chosen`), so a pane's low-ceremony
/// handler cannot act on a foreign or forged answer. The subject is `answer.subject`, still the
/// pane's to judge against what it holds now.
inline bool chosen_from(const loom::Mail& mail, const PaneMenuAnswered& answer,
                        std::string_view pane, std::string_view id,
                        std::string_view presenter = kWorkshopRole) {
    return from_workshop(mail, presenter) && chosen(answer, pane, id);
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
