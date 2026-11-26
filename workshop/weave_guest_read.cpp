// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// WHAT A GUEST MAY READ, PANE BY PANE: every guest is refused the words of the panes only the weaver's
// hand reads -- Connections, the Hotkeys pane -- and of an editor showing the guests file; a row that
// reads by pane reads only the panes Loom says its `capture` doors hold, asked at each read, and a
// pane outside them is said as its place, its catalog name and its holder, with no words, parts,
// fingerprint or change signal. Law: agents/workshop/guide.md.

#include "weave.hpp"

#include "attention/vocabulary.hpp"
#include "open_seam_vocabulary.hpp"

#include <zen/serialize.hpp>
#include <zen/switchboard/door.hpp>

namespace zengine::workshop {

// ---- A GUEST'S ACT, HELD FOR ITS ANSWERS --------------------------------------------------------

std::vector<AuthorityQuestion> pane_read_questions(const RuntimeCatalog& panes,
                                                   const scope::GuestRowFacts& row) {
    std::vector<AuthorityQuestion> out;
    if (!row.admitted || !row.reads_by_pane) return out;
    for (const RuntimePane& pane : panes.entries) {
        out.push_back(AuthorityQuestion{DeskReadRequested::zen_name, DeskReadRequested::zen_version,
                                        kWorkshopProvider, pane.provider + "/" + pane.pane});
    }
    return out;
}

// A GUEST'S ACT WAITS FOR ITS ROW AND ITS AUTHORITY, in its guest's order; past the bound it is
// refused in words, to the asker when it asked and on the desk's notice when it is a hand's.
// WL-GUEST-15 -- agents/workshop/guests.md
bool WorkshopWeave::guest_waits(loom::WeaveId guest, loom::Mail& mail, Came came,
                                GuestActsFacet::Again again, GuestActsFacet::Needs needs) {
    using Held = GuestActsFacet::Held;
    GuestActsFacet& acts = facet<GuestActsFacet>();
    if (!guest.valid() || acts.told(guest)) return false;
    const bool door = bus_now_.holder_of(kGuestsRole, mail).valid();
    const Held said = acts.hold(self_, guest, mail, came, door, std::move(again), std::move(needs));
    if (said == Held::kFull) {
        if (came == Came::kAsk) {
            (void)mail.answer(loom::Refused{GuestActsFacet::full_words()});
        } else {
            refuse_input("Refused: " + GuestActsFacet::full_words(), mail);
        }
    }
    return said != Held::kRuns;
}

// WL-GUIDE-02 -- agents/workshop/guide.md; WL-GUIDE-03 -- agents/workshop/guide.md
std::string WorkshopWeave::read_withheld(loom::Mail& mail, loom::WeaveId asker,
                                         const std::string& role, const std::string& pane) const {
    const scope::GuestRowFacts row = facet<GuestActsFacet>().row(asker);
    if (!row.admitted) return {};
    const scope::HostFact host = host_fact_now(mail);
    const std::string named = role + "/" + pane;
    // THE WEAVER'S OWN, ON EVERY HOST, whatever the row reads.
    if (const std::string only = scope::weaver_only_pane(role, pane); !only.empty()) {
        return "pane " + named + " is read only by the weaver's hand: " + only;
    }
    if (role == kEditorRole && !host.guests_file.empty()) {
        // THE EDITOR'S DOCUMENT, as its role's holder last claimed it.
        const loom::WeaveId holder = bus_now_.holder_of(kEditorRole, mail);
        const loom::SenseReading doc =
            holder.valid() ? mail.latest<EditorDocument>(holder) : loom::SenseReading{};
        if (doc && doc.by.author_incarnation_is_current) {
            const loom::Cell* path = doc.value ? doc.value->get("path") : nullptr;
            if (path != nullptr && scope::same_file(path->as_text(), host.guests_file)) {
                return "pane " + named + " shows the guests file, which only the weaver's hand reads";
            }
        }
    }
    if (!row.reads_by_pane) return {};
    // ATTENTION SAYS WHAT STANDS ACROSS EVERY PANE, so a row that reads by pane does not read it.
    if (role == attention_pane::kAttentionPaneRole) {
        return "pane " + named + " says what stands across every pane, and guest '" + row.name +
               "' reads only the panes its `capture` names";
    }
    const std::optional<loom::Door> door = loom::Door::parse(named);
    const AuthorityQuestion reads{DeskReadRequested::zen_name, DeskReadRequested::zen_version,
                                  kWorkshopProvider, named};
    if (door && facet<GuestActsFacet>().permits(asker, reads)) {
        return {};
    }
    return "pane " + named + " is outside guest '" + row.name + "''s `capture` doors";
}

// A PANE THE ASKER MAY NOT READ, SAID AS A STAMP: its place in the order, its role, pane and
// holder, and nothing that moves with what it shows.
PaneStamp WorkshopWeave::withheld_stamp(const PaneRef& ref, loom::Mail& mail) const {
    const PaneStamp of = stamp_of(ref.provider, ref.pane, mail);
    return PaneStamp{ref.provider, ref.pane, of.holder, 0, 0, 0};
}

// ...AND THE DESK, TO A ROW THAT READS BY PANE: every pane's place and name, and no band or desk
// number, which move with every pane's words.
void WorkshopWeave::withhold_desk(DeskView& desk) const {
    desk.words.clear();
    desk.desk = 0;
}

} // namespace zengine::workshop
