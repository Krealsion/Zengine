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
    const Admission admitted = admit_pane_offer(session_.panels.runtime, office, offer);
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
            // ...AND NOT THE REFUSAL, for the room grant's reason exactly: a provider
            // correcting its own summary has not sent content this host accepted, so what
            // last happened to this pane's content is still what happened to it.
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
    document_said_ = false;   // ...and the same for the document's picture, for the same reason
    transcript_said_ = false; // ...and the terminal participant's record, for the same reason
    // AND THE OFFER MAY RESOLVE AUTHORED INTENT THAT WAS WAITING FOR IT. This is the
    // one path -- the same `apply_setup` the picker and a restore go through -- so a
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
        say(admitted.written.refusal, true);
        repaint(mail);
        return;
    }
    Keymap candidate = session_.keymap;
    const Written joined = join_pane_rows(candidate, admitted.kind, rows);
    if (!joined.accepted) {
        const RuntimePane* row = session_.panels.runtime.of_kind(admitted.kind);
        say((row != nullptr ? row->name + " @" + row->provider + ": " : std::string()) +
                joined.refusal,
            true);
        repaint(mail);
        return;
    }
    // BOTH HALVES PASSED; ONLY NOW IS ANYTHING WRITTEN. The row is looked up again by
    // handle, because nothing holds a pointer into `entries` (panel.hpp).
    for (RuntimePane& row : session_.panels.runtime.entries) {
        if (row.kind == admitted.kind) {
            row.actions = rows;
        }
    }
    session_.keymap = std::move(candidate);
    repaint(mail); // the legend and the hotkey view read the map at every paint
}

// WL-KEY-15 -- agents/workshop/keyboard.md
void WorkshopWeave::rejoin_pane_rows(std::string& refusals) {
    session_.keymap.panes.clear();
    for (const RuntimePane& row : session_.panels.runtime.entries) {
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
        refusals += row.name + " @" + row.provider + ": " + joined.refusal;
    }
}

void WorkshopWeave::on(const PaneContent& content, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
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
    const RuntimePane* row = session_.panels.runtime.find(office, content.pane);
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
    const std::string_view office = mail.authored_role();
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
    // desk makes the presentation true. Judged first, through the picker's own trial seat, on
    // a COPY of the setup; written only if the seat is real. MEASURED, twice, the other ways:
    // seating at an answer the asker then refused, and answering capacity the asker spent
    // later, after the seat was gone.
    Setup candidate = session_.setup.active;
    const bool added = add_pane(candidate, ref);
    const Seating trial =
        seat_panes(candidate, session_.panels, stack_capacity(screen_of(session_)));
    for (const std::int64_t k : trial.waiting) {
        if (k == kind) {
            // THE PICKER'S OWN WORDS, and the picker's own outcome: nothing is authored
            // behind a refusal, and the asker is told why in a sentence it can pass on. A
            // screen the maker shrank before this arrived is exactly this case -- the pane
            // is waiting for room, so there is no seat to commit to.
            const std::string refusal = "no room for " + name +
                                        " on this screen -- make the window taller, then p "
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

const WorkshopDoc& WorkshopWeave::document() const { return state_; }

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

Row* WorkshopWeave::pane_editor_editing_row() {
    for (Row& r : session_.pane_editor.rows) {
        if (r.editing()) {
            return &r;
        }
    }
    return nullptr;
}

// WL-PED-07 -- agents/workshop/pane-manager.md
// ⭐ AND THE INSPECTOR'S HALF OF THIS IS GONE. It scanned `session_.rows` for a row in draft,
// and nothing in this host opens one any more: the Info weave holds the draft, in its own line,
// and commits it through the document door -- which writes with `commit_text` and leaves no row
// editing. The Pane Manager's draft is the only one this host still has.
//
// ⚠ SO `^s` NO LONGER REFUSES OVER A HALF-TYPED PROPERTY, AND THAT IS A NAMED LOSS. The save
// door asked this question so that a file could not be written while a maker was looking at a
// value they had not committed; the host cannot see that draft now, and telling it about one
// would be a second host-to-pane sentence this migration does not have. The Pane Manager's
// draft is still guarded, by this same call.
Row* WorkshopWeave::editing_row() {
    if (pane_editor_has_keyboard(session_)) {
        if (Row* mine = pane_editor_editing_row()) {
            return mine;
        }
    }
    return nullptr;
}

// WL-MAKER-11 -- agents/workshop/maker-pane.md; WL-TEXT-09 -- agents/workshop/text-box.md
component::TextBox* WorkshopWeave::naming_line() {
    if (session_.setup.naming.open) {
        return &session_.setup.naming.line;
    }
    if (session_.pane_naming.open) {
        return &session_.pane_naming.line;
    }
    return nullptr;
}

// WL-KEY-03 -- agents/workshop/keyboard.md; WL-TEXT-09 -- agents/workshop/text-box.md
WorkshopWeave::PasteOwner WorkshopWeave::paste_owner_now() {
    switch (keyboard_context(session_)) {
    case KeyContext::kNaming:
    case KeyContext::kPaneNaming: return PasteOwner::kNaming;
    case KeyContext::kDraft: return PasteOwner::kDraft;
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
    case PasteOwner::kDraft: {
        Row* row = editing_row();
        if (row == nullptr) {
            return; // unreachable while the mirror holds; written anyway
        }
        p.epoch = row->editor().draft_epoch();
        p.object = session_.selected;
        p.label = row->label();
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

// WL-DOC-20 -- agents/workshop/document.md
void WorkshopWeave::on(const DocumentActRequested& asked, loom::Mail& mail) {
    // AN OFFICE MAY ASK; ANONYMOUS SPEECH MAY NOT -- the arrangement door's rule, and the
    // reason it names nobody: a tool added tomorrow asks with no edit here.
    if (mail.authored_role().empty()) {
        return;
    }
    const auto answer = [&mail](bool accepted, std::string refusal) {
        (void)mail.answer(DocumentActed{accepted, std::move(refusal)});
    };
    if (asked.act == kDocumentSelect) {
        // A SELECTION IS NOT A REFUSABLE ACT. An identity the document does not have is the
        // same answer as one it does: nothing is selected that was not already, and the
        // picture published on this repaint says what is true. `select` is total.
        select(asked.identity);
        answer(true, std::string());
        repaint(mail);
        return;
    }
    if (asked.act == kDocumentCreate) {
        const std::int64_t id = create(state_, session_);
        if (id == 0) {
            // The mint is spent. Unreachable by pressing `n`; reachable in one line of a
            // loaded file, which is why this act has an answer rather than an overflow.
            answer(false, "this document has no identity left to give -- nothing was created");
            return;
        }
        say("created #" + std::to_string(id) + " -- a new identity, not a new name", false);
        answer(true, std::string());
        repaint(mail);
        return;
    }
    if (asked.act == kDocumentDelete) {
        const std::int64_t was = session_.selected;
        const Written gone = delete_selected(state_, session_);
        if (!gone.accepted) {
            answer(false, gone.refusal);
            return;
        }
        say(deleted_notice(was), false);
        answer(true, std::string());
        repaint(mail);
        return;
    }
    if (asked.act == kDocumentCommit) {
        // ⚠ THE INDEX IS JUDGED AGAINST THE CURRENT DERIVATION, not against the one the pane
        // was shown. A row the rows no longer have is refused by name rather than applied to
        // whatever moved into its place, which is the one hazard an index across a seam has.
        if (asked.row < 0 ||
            static_cast<std::size_t>(asked.row) >= session_.rows.size()) {
            answer(false, "that row is not in this object's properties any more");
            return;
        }
        Row& row = session_.rows[static_cast<std::size_t>(asked.row)];
        if (!row.editable()) {
            answer(false, row.label() + " is not authored -- it is what the workspace makes "
                                        "of the authored value");
            return;
        }
        const Commit result = row.commit_text(asked.text);
        if (result != Commit::Accepted) {
            // Two different failures, and the row already words each one for its own kind:
            // an unparseable draft reads "not <what would have worked>", a refused value
            // carries the setter's own reason.
            answer(false, row.label() + ": " + row.refusal());
            return;
        }
        say("committed " + row.label() + " = " + row.value(), false);
        answer(true, std::string());
        repaint(mail);
        return;
    }
    answer(false, "`" + asked.act + "` is not something this document can be asked for");
}

// WL-TEXT-02 -- agents/workshop/text-box.md
void WorkshopWeave::editing_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    Row* row = editing_row();
    // A PANE EDITOR ROW'S COMMIT OWES A RESEAT. Its write closure spent the
    // setup door; what a place write also changes is the SEATING -- an authored place
    // leaves the reactive stack and every reactive pane below it moves up a slot --
    // and `apply_setup` is the one path that reconciles it, exactly as it is for the
    // arrangement's `arrange_place`. Asked once, here, for every accepted commit, so no
    // write closure has to know which of the four axes it was.
    const bool pane_row = row != nullptr && pane_editor_has_keyboard(session_) &&
                          pane_editor_editing_row() == row;
    // THE DRAFT'S OWN VOCABULARY FIRST. One call owns what four switches used
    // to spell separately — the six editing keys, and now selection, clipboard, word
    // movement and history behind them — and a `true` is bool: the gesture
    // reached the layer that owns what it means, whether or not anything changed. The
    // component's vocabulary outranks the application keymap INSIDE a text context,
    // deliberately (owner-first refusal): a maker who remaps a draft control onto an
    // editing chord has authored a binding the box will answer first, and the hotkey
    // view shows both rows. What is left below is exactly the policy: what a draft
    // MEANS when a maker commits or abandons it, which the component is deliberately
    // unable to know -- resolved through the keymap executed here as
    // ever.
    if (row->consume(k.scancode, k.modifiers, session_.clipboard)) {
        return;
    }
    switch (session_.keymap.action_for(KeyContext::kDraft, k.scancode, k.modifiers)) {
    case Act::kDraftCommit: {
        const Commit result = row->commit();
        if (result == Commit::Accepted) {
            if (pane_row) {
                apply_setup(mail);
            }
            say("committed " + row->label() + " = " + row->value(), false);
        } else {
            // Two different failures, and the row already words each one for
            // its own kind: an unparseable draft reads "not <what would have
            // worked>", a refused value carries the setter's own reason. The
            // first live run appended the expected form AGAIN here, which said
            // it twice and then ran off the end of the line -- the notice is
            // one line, so a line's worth is all it may spend.
            (void)result;
            say(row->label() + ": " + row->refusal(), true);
        }
        break;
    }
    case Act::kDraftCancel:
        row->cancel();
        say("edit cancelled -- nothing was written", false);
        break;
    default: break;
    }
}

// WL-PED-07 -- agents/workshop/pane-manager.md
void WorkshopWeave::refresh_inspector() {
    const Screen sc = screen_of(session_);
    // THE PANE EDITOR'S DRAFT FIRST, against ITS body's capacity -- the same
    // one measurer, one pane over; a closed Pane Editor is skipped, not a zero.
    const PanelBounds editor =
        bounds_of(session_.panels, session_.setup.active, panel::kPaneEditor, sc);
    if (editor.open) {
        if (Row* mine = pane_editor_editing_row()) {
            const PaneEditorBodyPlace body = pane_editor_body(session_, sc, editor.rect);
            if (body.present) {
                mine->keep_caret_visible(body.value_columns);
            }
        }
    }
    // ⭐ AND THE INFO PANEL'S DRAFT IS NOT KEPT HERE ANY MORE, because this host holds no such
    // draft. `Session::rows` is still the derived inspector and the document door still commits
    // through it, but the LINE a maker types into is the Info weave's own, windowed by the
    // component inside the room the pane was granted -- which is the pane's measurer, not this
    // one's.
}

// WL-PTR-02, WL-PTR-03 -- agents/workshop/pointer.md
bool WorkshopWeave::press_selects_word(std::int64_t modifiers, std::int64_t place,
                                       component::TextBox& box, std::size_t at) {
    if (modifiers != zengine::input::mod::kNone) {
        session_.click = ClickMemory{};
        return false;
    }
    const component::WordSpan word = box.word_at(at);
    const std::uint64_t epoch = box.draft_epoch();
    const std::int64_t now = interaction_now();
    if (doubles_a_click(session_.click, place, epoch, word, now)) {
        // AND THE ARMING IS SPENT. A third press in the same place is an ordinary press
        // again: there is no triple-click in this application, and an arming that
        // survived its own gesture would make every press after a double-click select
        // the word once more.
        session_.click = ClickMemory{};
        return box.select_word_at(at);
    }
    session_.click = click_landed(place, epoch, word, now);
    return false;
}

bool WorkshopWeave::press_selects_word(std::int64_t modifiers, Row& row, std::size_t at) {
    if (modifiers != zengine::input::mod::kNone) {
        session_.click = ClickMemory{};
        return false;
    }
    const component::WordSpan word = row.editor().word_at(at);
    const std::uint64_t epoch = row.editor().draft_epoch();
    const std::int64_t now = interaction_now();
    if (doubles_a_click(session_.click, text_drag_place::kPropertyDraft, epoch, word,
                        now)) {
        session_.click = ClickMemory{};
        return row.select_word_at(at);
    }
    session_.click = click_landed(text_drag_place::kPropertyDraft, epoch, word, now);
    return false;
}

// ⭐ `info_press`, `actions_press` AND `objects_press` LEFT WITH THE INFO PANEL. They were the
// panel's three inverses -- a press inside the live property draft, a press on a bracketed
// control, a press on an object name -- each resolved against a body this host composed. The
// pane composes it now, so a press inside its rectangle crosses as `PanePressed` carrying the
// pane's own prose row and column, and the pane answers it with the same three questions in
// the same order (`info-pane/pane.cpp`). What crossed back from the two that ACTED is an
// ordinary ask at the document door (`DocumentActRequested`), judged by the host that owns it.

} // namespace zengine::workshop
