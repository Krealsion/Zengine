// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- the external pane seam -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
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
    // READ AS A VIEW AND KEPT AS ONE. The stamp belongs to the delivery
    // being handled and outlives every line below it; nothing here stores it, so
    // no view survives this handler. Making an owned string of it HERE would put
    // the copy before the law -- `admit_pane_offer` is the one place that decides
    // whether these bytes are a provider key at all, and it now gets them
    // unowned.
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        // PERSONAL SPEECH. Not an error to report to a maker -- an unauthenticated
        // message is not a fact about their arrangement -- and emphatically not a
        // catalog change. `mail.sender()` is deliberately not consulted: it is a
        // WeaveId, so a reloaded provider would be a different pane, and it is not the
        // durable route a saved setup names.
        return;
    }
    const Admission admitted = admit_pane_offer(session_.panels.runtime, office, offer, rows, columns);
    if (!admitted.written.accepted) {
        // THE REFUSAL IS WORKSHOP'S SENTENCE ABOUT ITS OWN LAW and interpolates no
        // field that failed one: `admit_pane_offer` names a `PaneRef` only after both
        // halves have passed `check_pane_key`, so no unvalidated byte reaches the
        // notice line a maker is reading.
        say(admitted.written.refusal, true);
        repaint(mail);
        return;
    }
    if (admitted.refreshed) {
        // A RE-OFFER IS A CORRECTION, AND AN OPEN PANE MUST NOT KEEP ANSWERING WITH THE
        // OLD ONE. The descriptor was updated in place; what this clears is the
        // PRESENTATION's copy, so the pane returns to waiting and the repaint below
        // grants the current room again. Nothing is closed and no catalog position
        // moves -- a provider correcting its own summary is not a reason for a maker's
        // panel to vanish.
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
    // ⚠ AND A PARTY THAT HAS JUST ARRIVED HAS HEARD NOTHING, so what is currently true is
    // said again on the next repaint. `say_conditions` is silent when the reading has not
    // changed (WL-ATTN-12) -- which is what stops the seam looping and would otherwise mean
    // that a pane loaded after this host last spoke never hears a word, and shows a
    // permanent "waiting" over a screen where everything is known. An OFFER is the one
    // moment a new listener certainly exists, and it is cheap: it costs one publication of
    // a reading this host derives anyway.
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

/// ⚠ A DECLARATION FROM A PANE BUILT BEFORE OWNERSHIP EXISTED (VD-27). `PaneActions` v1 is
/// exactly the shape it always was, so a provider compiled against the old header registers,
/// declares and dispatches with this host unchanged. Its rows are widened here -- one field,
/// empty, meaning what v1 always meant: this pane stands in for nothing -- so everything
/// below reads one population. Nothing reinterprets old bytes: the v1 shape decoded as a v1
/// shape, and this is a copy into the host's own type.
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
        // ⭐ AND THE DECLARER IS ANSWERED (BL-WORK-04). The band names the pane and the reason
        // for the MAKER; the verdict names them for the PROVIDER, which is the party that can do
        // something about it -- as the answer to this declaration, so it carries the number the
        // declaration was sent under. Both say the same words.
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
    // ⭐ A RE-JOIN THAT DROPS A PANE'S ROWS IS A WITHDRAWAL (BL-WORK-04), and this one is the
    // rejection a provider is LEAST able to see coming: its declaration was accepted, and a keymap
    // file read afterwards took the gesture away. It is not an answer -- the delivery it judged is
    // over -- so it names the declaration by the number its verdict gave it. Nothing about being
    // told mandates a recovery: the pane may re-declare elsewhere, or simply know.
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
    // IDENTITY IS ASKED OF WHAT WAS ALREADY ADMITTED, WITH VIEWS. The pair
    // is compared against rows this session accepted under `check_pane_key`, so
    // the question is answered without owning either half and without building a
    // `PaneRef` out of an office no law here has judged. THIS IS NOT A SECOND
    // ADMISSION LAW: a pair that matches no row returns nothing and retains
    // nothing, which is the same answer the built-in-first `resolve_pane` gave --
    // admission refuses a runtime offer that would shadow a built-in, so no
    // built-in reference can be a row here to find.
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
    // A PROJECTION OF A GENERATION THIS PANE HAS ALREADY MOVED PAST IS NOT
    // ADMITTED. It was composed for a document that has since been replaced under a
    // commitment; painting it here would show the old document over the new one's admitted
    // rows. Dropped, not refused: it is not wrong, it is late, and the pane's next
    // composition of the current document is on its way behind it.
    if (generation.has_value() && *generation < pane->content_generation) {
        return;
    }
    const PaneContent content{pane_key, rows};
    const Written judged = judge_content(content, *pane);
    if (!judged.accepted) {
        // THE OLD ROWS GO WITH THE REFUSAL. Leaving them would present a previous
        // answer as the current one at the exact moment this pane knows it is not --
        // the `awaiting` distinction the Builder panel established, one provider out.
        pane->shown.clear();
        pane->heard = false;
        pane->awaiting = true;
        pane->refusal = kExternalRefused;
        // AND WHY, KEPT WHERE THE REFUSAL IS. This is Workshop's own sentence
        // about the content and carries none of the refused message; what changed in
        // what changed is where it LIVES. It used to be said on the notice row, which has no
        // lifetime: the pane cleared its refusal on the next valid content and the row
        // kept the refusal sentence for the rest of the process -- resolving a
        // condition could not un-say it. Held beside the refusal it explains, it is
        // gone the moment the refusal is, and the attention projection reads it where
        // it lives rather than being told about it (`attention_conditions`).
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
    // ⚠ AND THE CARET IS RE-JUDGED AGAINST THE ROWS THAT JUST ARRIVED. A pane sends its
    // content and its caret as two messages, in that order, so between them there is one
    // instant where a caret admitted against the PREVIOUS rows is held against these. If
    // the new answer is shorter, that caret is now at a place with no text under it -- so
    // it is dropped here rather than drawn there, and the pane's own next `PaneCaret`
    // (which arrives on this same drain) puts it back where it belongs. A pane that moved
    // its caret and its rows together therefore never sees this arm; a pane that shrank its
    // rows and said nothing about its caret has none until it does.
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
    // ⚠ THIS DELIVERY IS THE COMMITMENT POINT, AND IT IS ONE DELIVERY ON PURPOSE. The asker
    // has frozen its own eligibility until it hears back (pane_vocabulary.hpp says how), so
    // the one instant at which its fact and this desk's fact both hold is the instant this
    // desk makes the presentation true. Judged first, through the launch door's own trial seat, on
    // a COPY of the setup; written only if the seat is real. MEASURED, twice, the other ways:
    // seating at an answer the asker then refused, and answering capacity the asker spent
    // later, after the seat was gone.
    Setup candidate = session_.setup.active;
    const bool added = add_pane(candidate, ref);
    const Seating trial =
        seat_panes(candidate, session_.panels, stack_capacity(screen_of(session_)));
    for (const std::int64_t k : trial.waiting) {
        if (k == kind) {
            // THE LAUNCH DOOR'S OWN WORDS, and its own outcome: nothing is authored behind a
            // refusal, and the asker is told why in a sentence it can pass on. A screen the
            // maker shrank before this arrived is exactly this case -- the pane is waiting for
            // room, so there is no seat to commit to. (It ended "then p again", the retired
            // picker's key, until the picker retired.)
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
        // THE BELT UNDER THE TRIAL. `apply_setup` seats through the same `seat_panes` over
        // the same capacity, so a trial that said yes and a seat that did not happen is a
        // defect in one of them. Answered as a refusal rather than as a seat, with the row
        // this delivery authored taken back, because "seated" is the one word the asker
        // acts on and it must never be said of a pane the screen does not show.
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
    // THE BEGIN NAMES A ROW THAT IS SHOWN. THE END MAY NAME ONE MORE: a reading-order range
    // is begin-inclusive and end-exclusive, so `(rows, 0)` -- the start of the row after the
    // last -- is the one position with no row that a range may honestly end at, and it is
    // what "selected through the end of the last row" spells. Any other end names a row
    // that is shown, at a column that row has. This is the whole of the relaxation: a
    // multiline selection running past the window is representable, and nothing past
    // that one exclusive-end position is admitted.
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

// ⭐ `pane_editor_editing_row` AND `editing_row` WERE HERE: the property draft under the keys,
// the host's Pane Manager's being the last this host held. A property is drafted in an
// inspector's own image now and crosses as finished text (`Row::commit_text`).

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

// ⭐ `editing_key` AND `refresh_inspector` WERE HERE -- a property draft's two controls, its
// commit owing a reseat, and its horizontal window -- and left with the host's Pane Manager.

// ⭐ `press_selects_word` WAS HERE -- the one word-selecting press every editable line this host
// held spent, arming on a first press and spending the arming on the completing one -- and it
// left with its last caller, the host Pane Manager's draft. The layout name line takes keys and
// text and no press, so nothing on this side selects a word under a pointer now.


// ⭐ `info_press`, `actions_press` AND `objects_press` LEFT WITH THE INFO PANEL. They were the
// panel's three inverses -- a press inside the live property draft, a press on a bracketed
// control, a press on an object name -- each resolved against a body this host composed. The
// pane composes it now, so a press inside its rectangle crosses as `PanePressed` carrying the
// pane's own prose row and column, and the pane answers it with the same three questions in
// the same order (`info-pane/pane.cpp`). What crossed back from the two that ACTED is an
// ordinary ask at the document door (`DocumentActRequested`), judged by the host that owns it.

} // namespace zengine::workshop
