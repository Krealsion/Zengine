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
            pane->clear_refusal();
            pane->heard = false;
            pane->awaiting = true;
            pane->granted = false;
        }
    }
    // AND THE OFFER MAY RESOLVE AUTHORED INTENT THAT WAS WAITING FOR IT. This is the
    // one path -- the same `apply_setup` the picker and a restore go through -- so a
    // setup naming `third.party/hello` opens the moment that office offers it, without
    // the file having been touched and without a second way to open a panel existing.
    apply_setup(mail);
    repaint(mail);
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
        repaint(mail);
        return;
    }
    pane->shown = content.rows; // only the validated rows, and only now
    pane->heard = true;
    pane->awaiting = false;
    pane->clear_refusal();
    repaint(mail);
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
Row* WorkshopWeave::editing_row() {
    if (pane_editor_has_keyboard(session_)) {
        if (Row* mine = pane_editor_editing_row()) {
            return mine;
        }
    }
    for (Row& r : session_.rows) {
        if (r.editing()) {
            return &r;
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
    case KeyContext::kTerminal: return PasteOwner::kTerminal;
    case KeyContext::kNaming:
    case KeyContext::kPaneNaming: return PasteOwner::kNaming;
    case KeyContext::kDraft: return PasteOwner::kDraft;
    case KeyContext::kEditor: return PasteOwner::kEditor;
    default: return PasteOwner::kNone;
    }
}

// WL-TEXT-09, WL-TEXT-10 -- agents/workshop/text-box.md
void WorkshopWeave::begin_clipboard_paste(loom::Mail& mail) {
    PendingPaste p;
    p.owner = paste_owner_now();
    switch (p.owner) {
    case PasteOwner::kNone: return; // no box of this weave's asked; nothing to do
    case PasteOwner::kTerminal: p.epoch = session_.terminal.input.draft_epoch(); break;
    case PasteOwner::kNaming: {
        const component::TextBox* line = naming_line();
        if (line == nullptr) {
            return; // unreachable while the resolver holds; written anyway
        }
        p.epoch = line->draft_epoch();
        break;
    }
    case PasteOwner::kEditor:
        p.editor_doc = session_.editor.doc_epoch;
        p.editor_revision = session_.editor.buffer.revision();
        break;
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

// WL-INFO-01, WL-INFO-05, WL-INFO-06 -- agents/workshop/info-body.md
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
    const PanelBounds info = bounds_of(session_.panels, session_.setup.active, panel::kInfo, sc);
    if (!info.open) {
        return;
    }
    for (std::size_t i = 0; i < session_.rows.size(); ++i) {
        if (!session_.rows[i].editing()) {
            continue;
        }
        const InfoBodyPlace body = info_body_place(info.rect, sc, state_, session_);
        if (body.present) {
            session_.rows[i].keep_caret_visible(body.value_columns);
        }
        return;
    }
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

// WL-PRESS-01, WL-PRESS-04 -- agents/workshop/press-chain.md
// WL-PTR-02 -- agents/workshop/pointer.md
// WL-INFO-01 -- agents/workshop/info-body.md
bool WorkshopWeave::info_press(const InfoBodyAt& where, std::int64_t modifiers) {
    if (!where.present) {
        return false;
    }
    for (std::size_t i = 0; i < session_.rows.size(); ++i) {
        Row& row = session_.rows[i];
        if (!row.editing()) {
            continue;
        }
        if (!property_row_hit(where.body, i, where.at.column, where.at.row)) {
            return false; // on the panel, but not on the draft's row: not consumed
        }
        // THROUGH THE WINDOW THE ROW WAS DRAWN WITH. A visible column names
        // `first_visible + offset` of the WHOLE draft, never the offset alone -- the one
        // subtraction a horizontal window adds to a hit test, and the one that is right
        // to leave out for exactly as long as no value is long enough to scroll. The
        // component holds the offset the last repaint resolved, which is the one the
        // maker is looking at.
        //
        // AND THE ROW'S OWN PROSE OFFSET COMES OFF FIRST. A body row carries the
        // mark and the property's name before the value, exactly as the pane's row
        // carries `> ` before the command, so a pressed column is a column of the ROW and
        // the value's column is that minus what the name spent. `property_value_column`
        // is the one subtraction and it is the inverse of the one
        // `property_caret_column` added.
        const std::size_t target =
            row.editor().position_at_column(property_value_column(where.at.column));
        //...AND A SECOND PRESS IN THE SAME WORD SELECTS IT. The first press is
        // still an ordinary press and still places the caret; only the second one means
        // something else, and it means it in the component's own word vocabulary.
        if (!press_selects_word(modifiers, row, target)) {
            row.place(target);
        }
        //...AND THE PRESS OPENS A SELECTION DRAG. The press placed the caret,
        // which is the anchor; every motion until release extends from it. The record
        // holds WHICH line and nothing else — the geometry is re-resolved per motion by
        // the same functions this press just spent, `PaneGesture`'s no-live-position law.
        session_.text_drag.active = true;
        session_.text_drag.place = text_drag_place::kPropertyDraft;
        return true; // consumed: the press was on the draft's own row
    }
    return false; // no draft is live, so this panel has no editor to press
}

// WL-CTRL-03, WL-CTRL-05 -- agents/workshop/info-controls.md
// WL-PRESS-01 -- agents/workshop/press-chain.md
bool WorkshopWeave::actions_press(const InfoBodyAt& where) {
    if (!where.present) {
        return false;
    }
    const std::size_t which = action_press_at(where.body, where.at.column, where.at.row);
    if (which == kNoAction) {
        return false; // a list row, the heading, a spare row, or off the body entirely
    }
    if (action_availability(which, state_, session_) == Availability::kDraftLive) {
        say(finish_draft_first(), true);
        return true; // consumed, and refused in this application's own words
    }
    if (which == kActionCreate) {
        create_object();
    } else {
        delete_object();
    }
    return true; // consumed, whatever the document then made of it
}

// WL-INFO-01, WL-INFO-09 -- agents/workshop/info-body.md
// WL-PRESS-01, WL-PRESS-02 -- agents/workshop/press-chain.md
bool WorkshopWeave::objects_press(const InfoBodyAt& where) {
    if (!where.present) {
        return false;
    }
    const std::size_t which = object_press_at(where.body, where.at.column, where.at.row);
    if (which == kNoObject || which >= state_.elements.size()) {
        return false; // a marker, the heading, a property row, a spare row, or off the body
    }
    if (draft_live(session_)) {
        say(finish_draft_first(), true);
        return true; // consumed: nothing moved, and the reason is on the notice line
    }
    const std::int64_t id = state_.elements[which].id;
    if (id == session_.selected) {
        // NOT CONSUMED, DELIBERATELY. The press is on this list and this list has nothing
        // to do with it; letting it through is how a maker gets the panel's answer rather
        // than silence.
        return false;
    }
    select(id);
    say("selected #" + std::to_string(id), false);
    return true; // consumed
}

} // namespace zengine::workshop
