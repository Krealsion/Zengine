// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s external pane seam: offers, declared actions, content, carets and reveals.
// Workshop law: agents/workshop/text-box.md (+7 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- THE EXTERNAL PANE SEAM: an office offers, Workshop grants, an office says

void WorkshopWeave::on(const PaneOffered& offer, loom::Mail& mail) {
    accept_pane_offer(offer, mail, 0, 0);
}

void WorkshopWeave::on(const v2::PaneOffered& offer, loom::Mail& mail) {
    accept_pane_offer(PaneOffered{offer.pane, offer.name, offer.summary}, mail,
                      offer.rows, offer.columns);
}

void WorkshopWeave::accept_pane_offer(const PaneOffered& offer, loom::Mail& mail,
                                      std::int64_t rows, std::int64_t columns) {
    // Read as a view and kept as one: `admit_pane_offer` is the one place that judges these
    // bytes, so no owned copy precedes it.
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        // Personal speech: no error for the maker and no catalog change. `mail.sender()` is not
        // consulted: a WeaveId would make a reloaded provider a different pane.
        return;
    }
    const Admission admitted = admit_pane_offer(session_.panels.runtime, office, offer, rows, columns);
    if (!admitted.written.accepted) {
        // Workshop's sentence about its own law: `admit_pane_offer` names a `PaneRef` only after
        // both halves passed `check_pane_key`.
        say(admitted.written.refusal, true);
        repaint(mail);
        return;
    }
    if (admitted.refreshed) {
        // A re-offer is a correction: the descriptor was updated in place, and the presentation's
        // copy is cleared so the repaint grants the current room again. Nothing closes and no
        // catalog position moves.
        if (ExternalPane* pane = session_.panels.external_pane(admitted.kind)) {
            pane->shown.clear();
            pane->heard = false;
            pane->awaiting = true;
            pane->granted = false;
            // ...AND ITS PICTURES START OVER: a re-offer is how a reloaded image arrives, and a
            // new image numbers from one again -- a press stamped with a number the old image
            // gave out must not match the new image's picture of the same number.
            pane->forget_pictures();
            pane->canvas = ExternalPane::Canvas{};
            // ...AND NOT THE REFUSAL, for the room grant's reason exactly: a provider
            // correcting its own summary has not sent content this host accepted, so what
            // last happened to this pane's content is still what happened to it.
        }
        // ...AND A MENU PRESENTED FOR THE PANE IT RE-OFFERED IS WITHDRAWN. A re-offer is how a
        // reloaded requester arrives, and what its predecessor asked about is no longer anything
        // the requester holds -- the presenter answers it unchosen, and the successor, which never
        // asked, settles nothing on it (`pane_menu::Asked`).
        if (session_.presented.open && session_.presented.office == office &&
            session_.presented.pane == offer.pane) {
            withdraw_menu("the pane was offered again", mail);
        }
    }
    // A party that has just arrived has heard nothing, so what is true is said again on the next
    // repaint: `say_conditions` is silent while the reading is unchanged (WL-ATTN-12), and an offer
    // is the one moment a new listener certainly exists.
    conditions_said_ = false;
    transcript_said_ = false; // ...and the terminal participant's record, for the same reason
    inventory_published_ = false; // ...and the inventory, which a presenter reads (WL-DESK-04)
    keymap_published_ = false;    // ...and the effective keymap, for the same reason (WL-DESK-11)
    subject_published_ = false;   // ...and the inspector's subject (WL-INFO-14)
    // AND THE OFFER MAY RESOLVE AUTHORED INTENT THAT WAS WAITING FOR IT. This is the
    // one path -- the same `apply_setup` the doors and a restore go through -- so a
    // setup naming `third.party/hello` opens the moment that office offers it, without
    // the file having been touched and without a second way to open a panel existing.
    apply_setup(mail);
    repaint(mail);
}

/// A declaration from a pane built before ownership existed: `PaneActions` v1 is unchanged, and
/// its rows are widened here into the host's own type with an empty `supersedes` (it stands in
/// for nothing), so everything below reads one population.
// WL-KEY-15 -- agents/workshop/keyboard.md
void WorkshopWeave::on(const PaneActions& actions, loom::Mail& mail) {
    std::vector<v2::PaneActionRow> widened;
    widened.reserve(actions.rows.size());
    for (const PaneActionRow& row : actions.rows) {
        widened.push_back(
            v2::PaneActionRow{row.id, row.label, row.scancode, row.modifiers, std::string()});
    }
    declare_pane_actions(actions.pane, widened, mail);
}

// WL-KEY-15 -- agents/workshop/keyboard.md
void WorkshopWeave::on(const v2::PaneActions& actions, loom::Mail& mail) {
    declare_pane_actions(actions.pane, actions.rows, mail);
}

// WL-KEY-15 -- agents/workshop/keyboard.md
void WorkshopWeave::declare_pane_actions(const std::string& pane,
                                         const std::vector<v2::PaneActionRow>& rows,
                                         loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // personal speech: no rows, no notice, no catalog change (the offer's rule)
    }
    // THE SHAPE'S HALF FIRST: whose pane, and is it one this office offered. Then the
    // rows' half and the collision law, into a COPY of the effective keymap -- so a
    // refusal anywhere leaves the keymap in force, and the pane's retained declaration,
    // exactly what they were.
    const Admission admitted = admit_pane_actions(session_.panels.runtime, office, pane);
    if (!admitted.written.accepted) {
        // And the declarer is answered: the band tells the maker, and the verdict, as the answer
        // to this declaration, tells the provider, which can act on it. Both say the same words.
        answer_declaration(std::string(office),
                           ActionsJudged{pane, false, 0, admitted.written.refusal}, mail);
        repaint(mail);
        return;
    }
    Keymap candidate = session_.keymap;
    const Written joined = join_pane_rows(candidate, admitted.kind, rows);
    if (!joined.accepted) {
        const RuntimePane* row = session_.panels.runtime.of_kind(admitted.kind);
        answer_declaration(std::string(office),
                           ActionsJudged{pane, false, 0,
                                         (row != nullptr ? row->name + " @" + row->provider + ": "
                                                         : std::string()) +
                                             joined.refusal},
                           mail);
        repaint(mail);
        return;
    }
    // BOTH HALVES PASSED; ONLY NOW IS ANYTHING WRITTEN. The row is looked up again by
    // handle, because nothing holds a pointer into `entries` (panel.hpp). The declaration gets
    // its number here, and the verdict below is the only place the declarer learns it.
    const std::int64_t declaration = ++declarations_;
    for (RuntimePane& row : session_.panels.runtime.entries) {
        if (row.kind == admitted.kind) {
            row.actions = rows;
            row.declaration = declaration;
        }
    }
    session_.keymap = std::move(candidate);
    answer_declaration(std::string(office), ActionsJudged{pane, true, declaration, std::string()},
                       mail);
    repaint(mail); // the legend and the hotkey view read the map at every paint
}

// WL-KEY-15 -- agents/workshop/keyboard.md
void WorkshopWeave::rejoin_pane_rows(std::string& refusals, loom::Mail& mail) {
    session_.keymap.panes.clear();
    // ⚠ THE PANE ROWS ARE COLLECTED BEFORE ANY REFUSAL IS SENT, and the copy is why. Telling a
    // provider inside this walk would let a re-declaration arriving in that delivery mutate
    // `entries` while the loop still holds its place in it.
    struct Dropped {
        std::string office;
        std::string pane;
        std::int64_t declaration = 0;
        std::string refusal;
    };
    std::vector<Dropped> told;
    for (RuntimePane& row : session_.panels.runtime.entries) {
        if (row.actions.empty()) {
            continue;
        }
        const Written joined = join_pane_rows(session_.keymap, row.kind, row.actions);
        if (joined.accepted) {
            continue;
        }
        drop_pane_rows(session_.keymap, row.kind);
        if (!refusals.empty()) {
            refusals += "; ";
        }
        const std::string said = row.name + " @" + row.provider + ": " + joined.refusal;
        refusals += said;
        told.push_back(Dropped{row.provider, row.pane, row.declaration, said});
        // ...AND THE DECLARATION IS NOT KEPT. It left the keymap, and a declaration kept here
        // would come back into force at the next re-join without its declarer being told.
        row.actions.clear();
        row.declaration = 0;
    }
    // A re-join that drops a pane's rows is a withdrawal the provider cannot see coming. It is not
    // an answer (that delivery is over), so it names the declaration by its verdict's number.
    for (const Dropped& d : told) {
        say_withdrawn(d.office, ActionsWithdrawn{d.pane, d.declaration, d.refusal}, mail);
    }
}

void WorkshopWeave::on(const PaneContent& content, loom::Mail& mail) {
    admit_content(mail.authored_role(), content.pane, content.rows, std::nullopt, std::nullopt,
                  mail);
}

// Content numbering its picture: v2's admission, and the number recorded for the press to echo.
void WorkshopWeave::on(const v3::PaneContent& content, loom::Mail& mail) {
    admit_content(mail.authored_role(), content.pane, content.rows,
                  content.generation > 0 ? std::optional<std::int64_t>(content.generation)
                                         : std::nullopt,
                  content.picture, mail);
}

// WL-DESK-14 -- agents/workshop/desktop-presenting.md
void WorkshopWeave::fence_pictures(loom::Mail& mail) {
    // WHICH NUMBERED PICTURES THIS CANVAS HANDED OUT FOR THE FIRST TIME -- a pane's, and a
    // presented menu's. A picture already in flight, or already the stamp, needs no second fence;
    // a repaint that moved no picture sends nothing, so the bus does not carry a fence per repaint.
    const std::int64_t number = fences_ + 1;
    bool handed_out = false;
    for (ExternalPane& pane : session_.panels.external) {
        handed_out = pane.stamp.hand_out(pane.picture, number) || handed_out;
    }
    if (session_.presented.open) {
        handed_out =
            session_.presented.stamp.hand_out(session_.presented.picture, number) || handed_out;
    }
    if (!handed_out) {
        return;
    }
    fences_ = number;
    // AUTHORED AS THE OFFICE AND ADDRESSED TO IT, so no other participant can make one: a fence
    // another weave could forge would let it decide which picture a maker's press names.
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(kWorkshopProvider, PictureFence{number, 1});
}

// WL-DESK-14 -- agents/workshop/desktop-presenting.md
void WorkshopWeave::on(const PictureFence& fence, loom::Mail& mail) {
    if (!mail.authored_from_role(kWorkshopProvider) || fence.number <= 0 ||
        fence.number > fences_) {
        return; // only this office's own fence, and only one it minted, moves a stamp
    }
    if (fence.hop == 1) {
        // THE FIRST TIME ROUND IT IS HANDLED RIGHT AFTER THE MEDIUM HANDLED THE CANVAS; a press
        // read before that is already queued ahead of the second hop, which is what keeps it
        // stamped with the picture the medium was still showing.
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(kWorkshopProvider, PictureFence{fence.number, 2});
        return;
    }
    if (fence.hop != 2) {
        return;
    }
    for (ExternalPane& pane : session_.panels.external) {
        pane.stamp.come_round(fence.number);
    }
    session_.presented.stamp.come_round(fence.number);
    // Nothing is repainted: what a press is stamped with is not something a maker sees.
}

// Content naming its generation (WL-OPEN-03).
void WorkshopWeave::on(const v2::PaneContent& content, loom::Mail& mail) {
    admit_content(mail.authored_role(), content.pane, content.rows, content.generation,
                  std::nullopt, mail);
}

void WorkshopWeave::admit_content(std::string_view office, const std::string& pane_key,
                                  const std::vector<surface::SurfaceTextRow>& rows,
                                  std::optional<std::int64_t> generation,
                                  std::optional<std::int64_t> picture, loom::Mail& mail) {
    if (office.empty()) {
        return; // personal speech: no cache, no notice, no catalog change
    }
    // Identity is asked of what was already admitted, with views: a pair matching no row returns
    // and retains nothing. No second admission law: admission refuses an offer that would shadow
    // a built-in, so no built-in can be a row here.
    const RuntimePane* row = session_.panels.runtime.find(office, pane_key);
    if (row == nullptr) {
        return; // an office speaking about a pane it never offered, or about a built-in
    }
    // THE HANDLE, TAKEN NOW. Nothing holds a pointer into `entries` (panel.hpp),
    // and the row is looked up again by handle at the moment a notice needs it.
    const std::int64_t kind = row->kind;
    ExternalPane* pane = session_.panels.external_pane(kind);
    if (pane == nullptr || !pane->granted) {
        // CONTENT FOR A CLOSED PANE, OR FOR ONE THAT HAS NOT BEEN GRANTED A ROOM YET.
        // Nothing is cached and nothing is opened: a provider cannot make a panel appear
        // by talking about it, which is what keeps discovery and presentation two doors.
        return;
    }
    if (pane->canvas.grant != 0) return; // prose is the fallback for a host without canvas
    // A projection of a generation this pane has moved past is dropped, not refused: it is late,
    // and the next composition of the current document is behind it.
    if (generation.has_value() && *generation < pane->content_generation) {
        return;
    }
    const PaneContent content{pane_key, rows};
    const Written judged = judge_content(content, *pane);
    if (!judged.accepted) {
        // The old rows go with the refusal: leaving them would present a previous answer as the
        // current one.
        pane->shown.clear();
        pane->heard = false;
        pane->awaiting = true;
        pane->refusal = kExternalRefused;
        // ...and why, kept beside the refusal it explains, so it is gone the moment the refusal
        // is; the attention projection reads it there (`attention_conditions`).
        pane->refusal_why = judged.refusal;
        // THE CARET GOES WITH THE ROWS. It was a position IN them, and the rows are gone.
        pane->clear_caret();
        repaint(mail);
        return;
    }
    pane->shown = content.rows; // only the validated rows, and only now
    pane->heard = true;
    pane->awaiting = false;
    pane->clear_refusal();
    if (generation.has_value()) {
        pane->content_generation = *generation;
    }
    // THE PICTURE'S NUMBER, RECORDED AND NEVER JUDGED: what every press this host sends the pane
    // from now on echoes, so the pane can refuse one aimed at an older picture (v3::PanePressed).
    if (picture.has_value()) {
        pane->picture = *picture;
    }
    // And the caret is re-judged against the rows that just arrived: content and caret are two
    // messages, so a caret admitted against the previous rows may name a place these lack. It is
    // dropped rather than drawn, and the pane's next `PaneCaret` puts it back.
    if (!judge_caret(PaneCaret{content.pane, pane->caret_row, pane->caret_col,
                               pane->sel_begin_row, pane->sel_begin_col, pane->sel_end_row,
                               pane->sel_end_col},
                     *pane)
             .accepted) {
        pane->clear_caret();
    }
    repaint(mail);
}

// WL-CARET-01, WL-CARET-03 -- agents/workshop/pane-caret.md
void WorkshopWeave::on(const PaneCaret& caret, loom::Mail& mail) {
    admit_caret(mail.authored_role(), caret, std::nullopt, mail);
}

// A caret naming its generation (WL-OPEN-03).
void WorkshopWeave::on(const v2::PaneCaret& caret, loom::Mail& mail) {
    admit_caret(mail.authored_role(),
                PaneCaret{caret.pane, caret.row, caret.column, caret.sel_begin_row,
                          caret.sel_begin_col, caret.sel_end_row, caret.sel_end_col},
                caret.generation, mail);
}

void WorkshopWeave::admit_caret(std::string_view office, const PaneCaret& caret,
                                std::optional<std::int64_t> generation, loom::Mail& mail) {
    if (office.empty()) {
        return; // personal speech, `on(PaneContent)`'s own first rule
    }
    const RuntimePane* row = session_.panels.runtime.find(office, caret.pane);
    if (row == nullptr) {
        return; // an office speaking about a pane it never offered, or about a built-in
    }
    ExternalPane* pane = session_.panels.external_pane(row->kind);
    if (pane == nullptr || !pane->granted) {
        return; // a caret for a closed pane opens nothing, exactly as content does not
    }
    if (generation.has_value() && *generation < pane->content_generation) {
        return; // a caret of a document this pane has moved past (see content)
    }
    // WHAT IT WAS, BEFORE ANYTHING IS WRITTEN -- so this handler can repaint on a caret
    // that MOVED with no content behind it (an arrow key) and stay silent on one that
    // repeats itself (a repaint the pane answered by re-saying everything it knows).
    const std::int64_t was_row = pane->caret_row;
    const std::int64_t was_col = pane->caret_col;
    const std::int64_t was_sel_row = pane->sel_begin_row;
    const std::int64_t was_sel_col = pane->sel_begin_col;
    const std::int64_t was_sel_end_row = pane->sel_end_row;
    const std::int64_t was_sel_end_col = pane->sel_end_col;
    const Written judged = judge_caret(caret, *pane);
    if (!judged.accepted) {
        // REFUSED WHOLE, AND THE PANE IS LEFT WITH NO CARET RATHER THAN ITS PREVIOUS ONE.
        // A stale caret is a position, and a position that is wrong is read as a fact --
        // the rows' own refusal rule, one shape over. It does NOT clear the rows: the
        // sentences a maker is reading were judged on their own and are still true.
        pane->clear_caret();
    } else if (caret.row == surface::kNoCaret && caret.sel_begin_row == surface::kNoSelection) {
        pane->clear_caret(); // the pane saying it has none: ordinary, and not a refusal
    } else {
        // A SELECTION MAY STAND WITH NO CARET (WL-CARET-03): a document whose caret has
        // scrolled out of the window still shows the range it belongs to, and the pane says
        // exactly that -- `kNoCaret` on the row, a range beside it. `judge_caret` has
        // already refused a range that names a row the content does not have, so this is
        // a copy and not a second policy.
        pane->caret_row = caret.row;
        pane->caret_col = caret.row == surface::kNoCaret ? 0 : caret.column;
        pane->sel_begin_row = caret.sel_begin_row;
        pane->sel_begin_col = caret.sel_begin_col;
        pane->sel_end_row = caret.sel_end_row;
        pane->sel_end_col = caret.sel_end_col;
    }
    if (pane->caret_row != was_row || pane->caret_col != was_col ||
        pane->sel_begin_row != was_sel_row || pane->sel_begin_col != was_sel_col ||
        pane->sel_end_row != was_sel_end_row || pane->sel_end_col != was_sel_end_col) {
        repaint(mail);
    }
}

// WL-EDIT-13 -- agents/workshop/editor.md; WL-FRONT-04 -- agents/workshop/planes.md
void WorkshopWeave::on(const PaneRevealRequested& asked, loom::Mail& mail) {
    // AN OFFICE, AND ONLY AN OFFICE -- the seam's rule for every sentence that changes the
    // desk. And the office must have offered the pane it names: a reveal of a pane this
    // session never admitted has no catalog row, no name for the refusal and no kind to
    // seat, and is dropped as the stray it is.
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return;
    }
    const RuntimePane* row = session_.panels.runtime.find(office, asked.pane);
    if (row == nullptr) {
        return;
    }
    // COPIED OUT BEFORE THE DESK MOVES: `apply_setup` may re-ask providers and the catalog
    // vector may grow under a pointer into it (panel.hpp's own warning).
    const std::int64_t kind = row->kind;
    const std::string name = row->name;
    const PaneRef ref{row->provider, row->pane};
    // This delivery is the commitment point: judged first through the launch door's trial seat,
    // on a copy of the setup, and written only if the seat is real.
    Setup candidate = session_.setup.active;
    const bool added = add_pane(candidate, ref);
    const Seating trial =
        seat_panes(candidate, session_.panels, stack_capacity(screen_of(session_)));
    for (const std::int64_t k : trial.waiting) {
        if (k == kind) {
            // The launch door's own words and outcome: nothing is authored behind a refusal. A
            // screen the maker shrank before this arrived is exactly this case.
            const std::string refusal = "no room for " + name +
                                        " on this screen -- make the window taller, then try "
                                        "again";
            say(refusal, true);
            (void)mail.answer(PaneRevealAnswered{asked.pane, false, refusal});
            repaint(mail);
            return;
        }
    }
    if (added) {
        session_.setup.active = std::move(candidate);
    }
    apply_setup(mail);
    if (!session_.panels.has(kind)) {
        // The belt under the trial: `apply_setup` seats through the same `seat_panes`, so a
        // mismatch is a defect. Answered as a refusal, with the row taken back: "seated" is the
        // word the asker acts on.
        if (added) {
            (void)remove_pane(session_.setup.active, ref);
            apply_setup(mail);
        }
        const std::string refusal = "no room for " + name + " on this screen";
        say(refusal, true);
        (void)mail.answer(PaneRevealAnswered{asked.pane, false, refusal});
        repaint(mail);
        return;
    }
    // AND IT SELECTS THE PANE IT JUST SEATED AND POINTS THE KEYS AT IT -- the keyboard
    // candidate's own argument, one question wider: a reveal that pointed the keys at a pane
    // still sitting behind another would put the first keystroke somewhere the maker cannot
    // see. The two facts are written together everywhere they are written.
    session_.panels.selected = kind;
    session_.panels.keyboard = kind_takes_keyboard(kind) ? kind : kNoPaneKind;
    // SAID, so the sentence on the notice line is about what just happened and names who
    // asked for it -- the pane's own rows say what it is showing. Said and written BEFORE the
    // answer leaves, so what the asker hears is a fact about this desk and not a promise.
    say("showing " + name + " -- it asked to be shown, and it has the keys", false);
    (void)mail.answer(PaneRevealAnswered{asked.pane, true, std::string()});
    repaint(mail);
}

// WL-CARET-03 -- agents/workshop/pane-caret.md
Written WorkshopWeave::judge_caret(const PaneCaret& caret, const ExternalPane& pane) {
    const std::int64_t rows = static_cast<std::int64_t>(pane.shown.size());
    const auto within = [&pane](std::int64_t row, std::int64_t column) {
        const std::int64_t width =
            static_cast<std::int64_t>(pane.shown[static_cast<std::size_t>(row)].text.size());
        // ONE PAST THE LAST BYTE IS LEGAL: that is where an insertion point sits at the end
        // of a line, and it is the position the terminal's own caret holds most of the time.
        return column >= 0 && column <= width;
    };
    // "I HAVE NONE" IS A SENTENCE, NOT A POSITION -- and it is judged apart from the range
    // beside it, because a caret that has scrolled out of the window and a selection that
    // is still in it are two facts that can be true at once.
    if (caret.row != surface::kNoCaret) {
        if (caret.row < 0 || caret.row >= rows) {
            return Written::no("put a caret on row " + std::to_string(caret.row) +
                               " of a pane showing " + std::to_string(rows) + " rows");
        }
        if (!within(caret.row, caret.column)) {
            return Written::no("put a caret at column " + std::to_string(caret.column) +
                               " of a row that has no such place");
        }
    }
    if (caret.sel_begin_row == surface::kNoSelection &&
        caret.sel_end_row == surface::kNoSelection) {
        return Written::ok();
    }
    // HALF A RANGE IS NOT A RANGE. Both ends or neither -- a selection with one end is a
    // shape nobody can draw, and admitting it would make the painter decide what it meant.
    if (caret.sel_begin_row == surface::kNoSelection ||
        caret.sel_end_row == surface::kNoSelection) {
        return Written::no("named one end of a selection and not the other");
    }
    // The begin names a shown row; the end may name one more: a reading-order range is
    // begin-inclusive and end-exclusive, so `(rows, 0)` is the one rowless position a range may
    // end at ("through the end of the last row"). Nothing past it is admitted.
    if (caret.sel_begin_row < 0 || caret.sel_begin_row >= rows || caret.sel_end_row < 0 ||
        caret.sel_end_row > rows ||
        (caret.sel_end_row == rows && caret.sel_end_col != 0)) {
        return Written::no("selected a row this pane is not showing");
    }
    if (!within(caret.sel_begin_row, caret.sel_begin_col) ||
        (caret.sel_end_row < rows && !within(caret.sel_end_row, caret.sel_end_col))) {
        return Written::no("selected a column a row of this pane does not have");
    }
    // READING ORDER, WHICH IS THE ONE THING `SurfaceTextRegion` ALREADY REQUIRES of a
    // range: a region whose end precedes its begin is refused there by being drawn as
    // nothing, and refusing it here says so in words instead.
    if (caret.sel_end_row < caret.sel_begin_row ||
        (caret.sel_end_row == caret.sel_begin_row && caret.sel_end_col < caret.sel_begin_col)) {
        return Written::no("selected backwards -- the end precedes the begin");
    }
    return Written::ok();
}

Written WorkshopWeave::judge_content(const PaneContent& content, const ExternalPane& pane) {
    if (static_cast<std::int64_t>(content.rows.size()) > pane.rows) {
        return Written::no("sent " + std::to_string(content.rows.size()) +
                           " rows into a pane granted " + std::to_string(pane.rows));
    }
    for (const surface::SurfaceTextRow& row : content.rows) {
        if (static_cast<std::int64_t>(row.text.size()) > pane.columns) {
            return Written::no("sent a row of " + std::to_string(row.text.size()) +
                               " bytes into a pane granted " +
                               std::to_string(pane.columns) + " columns");
        }
        for (const char c : row.text) {
            const unsigned char byte = static_cast<unsigned char>(c);
            if (byte < 0x20u || byte >= 0x7Fu) {
                return Written::no("sent a row carrying a byte a canvas cannot draw");
            }
        }
    }
    return Written::ok();
}

const Session& WorkshopWeave::session() const { return session_; }

// WL-KEY-12 -- agents/workshop/keyboard.md
bool WorkshopWeave::same_keystroke(const std::string& text, const std::string& owed) {
    if (text == owed) {
        return true;
    }
    if (text.size() != 1 || owed.size() != 1) {
        return false;
    }
    const auto lower = [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    };
    const char a = lower(text[0]);
    return a >= 'a' && a <= 'z' && a == lower(owed[0]);
}

std::string WorkshopWeave::hotkey(Act a) const { return hotkey_text(session_.keymap, a); }

// WL-TEXT-09 -- agents/workshop/text-box.md
component::TextBox* WorkshopWeave::naming_line() {
    if (session_.setup.naming.open) {
        return &session_.setup.naming.line;
    }
    return nullptr;
}

// WL-KEY-03 -- agents/workshop/keyboard.md; WL-TEXT-09 -- agents/workshop/text-box.md
WorkshopWeave::PasteOwner WorkshopWeave::paste_owner_now() {
    switch (keyboard_context(session_)) {
    case KeyContext::kNaming: return PasteOwner::kNaming;
    default: return PasteOwner::kNone;
    }
}

// WL-TEXT-09, WL-TEXT-10 -- agents/workshop/text-box.md
void WorkshopWeave::begin_clipboard_paste(loom::Mail& mail) {
    PendingPaste p;
    p.owner = paste_owner_now();
    switch (p.owner) {
    case PasteOwner::kNone: return; // no box of this weave's asked; nothing to do
    case PasteOwner::kNaming: {
        const component::TextBox* line = naming_line();
        if (line == nullptr) {
            return; // unreachable while the resolver holds; written anyway
        }
        p.epoch = line->draft_epoch();
        break;
    }
    }
    const loom::AskOpened opened = paste_asks_.open_to_role(
        zengine::surface::kSkinRole, zengine::surface::ClipboardTextRequested::zen_name,
        zengine::surface::ClipboardTextRequested::zen_version);
    if (!opened) {
        return; // the book is full: this paste is dropped, the outstanding ones stand
    }
    p.ask = opened.id;
    pending_pastes_.push_back(std::move(p));
    (void)mail.send_to_role(zengine::surface::kSkinRole,
                            zengine::surface::ClipboardTextRequested{}, opened.correlation);
}

WorkshopWeave::PendingPaste WorkshopWeave::take_pending_paste(std::uint64_t ask) {
    for (std::size_t i = 0; i < pending_pastes_.size(); ++i) {
        if (pending_pastes_[i].ask == ask) {
            PendingPaste p = std::move(pending_pastes_[i]);
            pending_pastes_.erase(pending_pastes_.begin() +
                                  static_cast<std::ptrdiff_t>(i));
            return p;
        }
    }
    return PendingPaste{};
}

} // namespace zengine::workshop
