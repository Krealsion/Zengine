// SPDX-License-Identifier: MPL-2.0
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

// WL-GUIDE-02 -- agents/workshop/guide.md
bool WorkshopWeave::reads_by_pane(loom::WeaveId asker) const {
    if (!host_->guest_row) return false;
    const scope::GuestRowFacts row = host_->guest_row(asker);
    return row.admitted && row.reads_by_pane;
}

// WL-GUIDE-02 -- agents/workshop/guide.md; WL-GUIDE-03 -- agents/workshop/guide.md
std::string WorkshopWeave::read_withheld(loom::Mail& mail, loom::WeaveId asker,
                                         const std::string& office, const std::string& pane) const {
    if (!host_->guest_row) return {};
    const scope::GuestRowFacts row = host_->guest_row(asker);
    if (!row.admitted) return {};
    const std::string named = office + "/" + pane;
    // THE WEAVER'S OWN, ON EVERY HOST, whatever the row reads.
    if (const std::string only = scope::weaver_only_pane(office, pane); !only.empty()) {
        return "pane " + named + " is read only by the weaver's hand: " + only;
    }
    if (office == kEditorRole && !host_->host_fact.guests_file.empty() && host_->role_holder) {
        // THE EDITOR'S DOCUMENT, as its office's holder last claimed it.
        const loom::WeaveId holder = host_->role_holder(kEditorRole);
        const loom::SenseReading doc =
            holder.valid() ? mail.latest<EditorDocument>(holder) : loom::SenseReading{};
        if (doc && doc.by.author_incarnation_is_current) {
            const loom::Cell* path = doc.value ? doc.value->get("path") : nullptr;
            if (path != nullptr && scope::same_file(path->as_text(), host_->host_fact.guests_file)) {
                return "pane " + named + " shows the guests file, which only the weaver's hand reads";
            }
        }
    }
    if (!row.reads_by_pane) return {};
    // ATTENTION SAYS WHAT STANDS ACROSS EVERY PANE, so a row that reads by pane does not read it.
    if (office == attention_pane::kAttentionPaneRole) {
        return "pane " + named + " says what stands across every pane, and guest '" + row.name +
               "' reads only the panes its `capture` names";
    }
    const std::optional<loom::Door> door = loom::Door::parse(named);
    const loom::GrantAuthority cap =
        host_->input_authority ? host_->input_authority(asker) : loom::GrantAuthority{};
    const loom::AuthorityView live = mail.describe_authority(cap);
    if (door && live.available &&
        live.permits_role(v2::DeskReadRequested::zen_name, v2::DeskReadRequested::zen_version,
                          kWorkshopProvider, *door)) {
        return {};
    }
    return "pane " + named + " is outside guest '" + row.name + "''s `capture` doors";
}

// A PANE THE ASKER MAY NOT READ, SAID AS A STAMP: its place in the order, its office, pane and
// holder, and nothing that moves with what it shows.
v2::PaneStamp WorkshopWeave::withheld_stamp(const PaneRef& ref) const {
    const PaneStamp of = stamp_of(ref.provider, ref.pane, 0, 0);
    return v2::PaneStamp{ref.provider, ref.pane, of.holder, 0, 0, 0};
}

// ...AND THE DESK, TO A ROW THAT READS BY PANE: every pane's place and name, and no band, status or
// desk number, which move with every pane's words.
void WorkshopWeave::withhold_desk(v3::DeskView& desk) const {
    desk.words.clear();
    desk.slots.clear();
    desk.desk = 0;
}

} // namespace zengine::workshop
