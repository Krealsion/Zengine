// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s inspection seam -- a pane an inspector names, the picture of its
// rows, and one write through the rows' owner -- compiled once into `zengine-workshop-logic` and
// linked by the host and every suite.
//
// ⭐ THE HOST'S PANE MANAGER WAS BESIDE THIS, in the file this one was cut from
// (`weave_pane_editor.cpp`): its subject, its two lists, its draft and its keys. Its subject and
// rows are what this seam hands Info; the rest retired with it.
// Workshop law: agents/workshop/info-body.md (agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

void WorkshopWeave::on(const PaneViewRequested& asked, loom::Mail& mail) {
    const auto* pane = session_.panels.runtime.find(asked.provider, asked.pane);
    const auto sc = screen_of(session_);
    const auto refuse = [&](const char* reason) { (void)mail.answer(loom::Refused{reason}); };
    if (!pane || !session_.panels.has(pane->kind) || session_.arrange.open ||
        session_.context.open || session_.presented.open) {
        refuse("pane view unavailable: closed, unknown or covered by an interaction"); return;
    }
    const auto* content = session_.panels.external_pane(pane->kind);
    if (!content || !content->heard || content->awaiting || content->canvas.heard ||
        content->picture != content->stamp.aimed) {
        refuse("pane view unavailable: no settled text picture"); return;
    }
    const auto bounds = bounds_of(session_.panels, session_.setup.active, pane->kind, sc);
    if (!bounds.open || bounds.rect.y < surface::subs_of_cells(kWorkspaceY) ||
        bounds.rect.y + bounds.rect.h > surface::subs_of_cells(sc.notice_y)) {
        refuse("pane view unavailable: pane extends outside the visible workspace"); return;
    }
    bool above = false;
    for (const auto kind : effective_pane_order(session_.setup.active, session_.panels)) {
        if (kind == pane->kind) { above = true; continue; }
        if (!above) continue;
        const auto other = bounds_of(session_.panels, session_.setup.active, kind, sc);
        if (other.open && other.rect.x < bounds.rect.x + bounds.rect.w &&
            other.rect.x + other.rect.w > bounds.rect.x &&
            other.rect.y < bounds.rect.y + bounds.rect.h &&
            other.rect.y + other.rect.h > bounds.rect.y) {
            refuse("pane view unavailable: another pane overlaps it"); return;
        }
    }
    const auto body = external_body_place(bounds.rect, sc,
        external_title_rows(session_.panels, pane->kind, session_.pane_titles));
    if (!body.present) { refuse("pane has no visible body"); return; }
    PaneView reply{asked.provider, asked.pane, content->stamp.aimed, {}};
    for (std::int64_t row = 0; row < body.rows && row < static_cast<std::int64_t>(content->shown.size()); ++row) {
        PaneViewRow out;
        out.row = row;
        const auto column = std::min<std::int64_t>(2, body.columns-1);
        if (sc.text_advance_px > 0 && sc.text_line_px > 0) {
            out.space = input::space::kPixels;
            if (body.fit.graphical()) {
                out.x = body.fit.view.x + body.fit.origin_x + column*body.fit.advance_px + body.fit.advance_px/2;
                out.y = body.fit.view.y + body.fit.origin_y + (row+body.header_rows)*body.fit.line_px + body.fit.line_px/2;
            } else {
                out.x = surface::px_of_cells(body.region_x+column) + surface::px_of_cells(1)/2;
                out.y = surface::px_of_cells(body.region_y+row+body.header_rows) + surface::px_of_cells(1)/2;
            }
        } else {
            out.space = input::space::kCells;
            out.x = body.region_x+column;
            out.y = body.region_y+row+body.header_rows+surface::kTuiCanvasTopRow;
        }
        const auto hit = external_press_at(session_.panels, session_.setup.active, sc, pane->kind,
            session_.pane_titles, out.space, out.x, out.y);
        if (!hit.named || hit.row != row) { refuse("pane has no addressable row center"); return; }
        out.text = detail::fit(content->shown[static_cast<std::size_t>(row)].text, body.columns);
        reply.rows.push_back(std::move(out));
    }
    (void)mail.answer(reply);
}

// ---- A PANE AS AN INSPECTOR'S SUBJECT (the Info pane's) -------------------------------------

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::refresh_inspected() {
    InspectedPane& in = session_.inspected;
    if (!in.addressed()) {
        return;
    }
    // THE THREE THINGS A NAME STANDS FOR: this pane (the door keeps it), this desk, and this
    // layout of rows. A value moving is none of them -- the rows read fresh -- and neither is a
    // provider arriving or leaving: its rows say so, and a write it made impossible is refused
    // by the owner in its own words.
    const std::int64_t region = inspected_region(session_, in.ref);
    if (in.name != 0 && in.desk == session_.setup.put_live && in.region == region) {
        return;
    }
    in.rows = pane_subject_rows(session_, in.ref);
    in.region = region;
    in.desk = session_.setup.put_live;
    in.name = ++in.minted;
}

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::on(const InspectPaneRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return; // an office asks; personal speech is answered by nobody
    }
    const PaneRef ref{asked.office, asked.pane};
    bool known = false;
    for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panels)) {
        if (row.ref == ref) {
            known = true;
            break;
        }
    }
    if (ref.provider.empty() || !known) {
        (void)mail.answer(PaneSubjectActed{
            false, ref_text(ref) +
                       " is in neither this build's vocabulary nor this desk -- nothing to inspect"});
        return;
    }
    InspectedPane& in = session_.inspected;
    if (!(in.ref == ref)) {
        in.ref = ref;
        in.name = 0; // another pane: named afresh below, so no draft typed for the last one lands
    }
    refresh_inspected();
    (void)mail.answer(PaneSubjectActed{true, std::string()});
    repaint(mail);
}

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::on(const PaneSubjectRequested&, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return;
    }
    refresh_inspected();
    (void)mail.answer(pane_subject_shown(session_));
}

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::publish_pane_subject(loom::Mail& mail) {
    // NOTHING IS SAID ABOUT A SUBJECT NOBODY NAMED. An inspector that arrives asks and is answered
    // "none" (`on(PaneSubjectRequested)`); until one names a pane, this host has no sentence here.
    if (!session_.inspected.addressed()) {
        return;
    }
    refresh_inspected();
    PaneSubjectShown said = pane_subject_shown(session_);
    if (subject_published_ && same_pane_subject(said, subject_said_)) {
        return; // no news is silence, and silence is what makes this seam terminate
    }
    subject_said_ = said;
    subject_published_ = true;
    (void)mail.as_role(kWorkshopProvider).publish(std::move(said));
}

// WL-INFO-15 -- agents/workshop/info-body.md
void WorkshopWeave::on(const PaneCommitRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return;
    }
    const auto answer = [&mail](bool accepted, std::string refusal) {
        (void)mail.answer(PaneSubjectActed{accepted, std::move(refusal)});
    };
    // THE NAME FIRST, BEFORE ANY ROW IS READ OR ANY SETTER REACHED -- and judged against the
    // facts as they are now, so a desk put live since the picture was drawn is caught here
    // even though no repaint has named it yet.
    refresh_inspected();
    InspectedPane& in = session_.inspected;
    if (in.name == 0 || asked.subject != in.name) {
        answer(false, kPaneCommitSubjectGone);
        return;
    }
    if (asked.row < 0 || static_cast<std::size_t>(asked.row) >= in.rows.size()) {
        answer(false, "that row is not in this pane's properties any more");
        return;
    }
    Row& row = in.rows[static_cast<std::size_t>(asked.row)];
    if (!row.editable()) {
        answer(false, row.label() + " is not authored -- it is what the screen makes of the "
                                    "authored value");
        return;
    }
    const Commit result = row.commit_text(asked.text);
    if (result != Commit::Accepted) {
        // THE OWNER'S OWN WORDS: an amount the face does not read, a pane that is not in this
        // desk or has no room, a definition that is not open -- each worded by its setter.
        answer(false, row.label() + ": " + row.refusal());
        return;
    }
    // SAID WITH WHAT WAS WRITTEN, TO WHICH PANE, read before the reseat below touches anything.
    const std::string written = "committed " + row.label() + " of " +
                                pane_subject_shown(session_).name + " = " + row.value();
    // A PLACEMENT WRITE OWES A RESEAT -- `editing_key`'s reason: an authored place leaves the
    // reactive stack, and `apply_setup` is the one path that reconciles the seating.
    apply_setup(mail);
    say(written, false);
    answer(true, std::string());
    repaint(mail);
}

} // namespace zengine::workshop
