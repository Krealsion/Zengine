// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s external pane seam: offers, declared actions, settings and reveals.
// Workshop law: agents/workshop/text-box.md (+7 registers; agents/workshop.md routes)

#include "weave.hpp"

#include <algorithm>

namespace zengine::workshop {

// ---- THE EXTERNAL PANE SEAM: an office offers, Workshop seats, the pane draws

void WorkshopWeave::on(const PaneOffered& offer, loom::Mail& mail) {
    accept_pane_offer(offer, mail, 0, 0);
}

void WorkshopWeave::on(const v2::PaneOffered& offer, loom::Mail& mail) {
    accept_pane_offer(PaneOffered{offer.pane, offer.name, offer.summary}, mail,
                      offer.rows, offer.columns);
}

void WorkshopWeave::on(const v3::PaneOffered& offer, loom::Mail& mail) {
    accept_pane_offer(PaneOffered{offer.pane, offer.name, offer.summary}, mail, 0, 0,
                      PaneBody{offer.width, offer.height, offer.text_rows});
}

void WorkshopWeave::accept_pane_offer(const PaneOffered& offer, loom::Mail& mail,
                                      std::int64_t rows, std::int64_t columns, PaneBody body) {
    // Read as a view and kept as one: `admit_pane_offer` is the one place that judges these
    // bytes, so no owned copy precedes it.
    // The view borrows this delivery's stamp, valid for this handler only: never store it.
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        // Personal speech: no error for the weaver and no catalog change. `mail.sender()` is not
        // consulted: a WeaveId would make a reloaded provider a different pane.
        return;
    }
    const Admission admitted =
        admit_pane_offer(session_.panes.runtime, office, offer, rows, columns, body);
    if (!admitted.written.accepted) {
        // Workshop's sentence about its own law: `admit_pane_offer` names a `PaneRef` only after
        // both halves passed `check_pane_key`.
        say(admitted.written.refusal, true);
        repaint(mail);
        return;
    }
    if (admitted.refreshed) {
        // A re-offer is a correction: the descriptor was updated in place. Nothing closes and no
        // catalog position moves.
        if (ExternalPane* pane = session_.panes.external_pane(admitted.kind)) {
            // ITS SETTINGS ARE OWED AGAIN: a re-offer is how a reloaded image arrives, and a
            // reloaded image holds none it was handed, and can use none it was handed before.
            pane->settings_heard.reset();
            pane->settings_revision = 0;
            pane->settings_unused.clear();
        }
        // ...AND A MENU PRESENTED FOR THE PANE IT RE-OFFERED IS WITHDRAWN. A re-offer is how a
        // reloaded requester arrives, and what its predecessor asked about is no longer anything
        // the requester holds -- the presenter answers it unchosen, and the successor, which never
        // asked, settles nothing on it (`pane_menu::Asked`).
        if (session_.presented.open && session_.presented.office == office &&
            session_.presented.pane == offer.pane) {
            withdraw_menu("the pane was offered again", mail);
        }
        // ...AND A DECLARATION THAT NO LONGER COUNTS GOES: a reloaded image keeps the weave id
        // but not the incarnation it was counted by, so one that declares nothing takes nothing
        // its predecessor declared. The same image offering again keeps its declaration.
        for (RuntimePane& row : session_.panes.runtime.entries) {
            if (row.kind == admitted.kind && counted_settings(row) == nullptr) {
                row.settings_rows.clear();
                row.settings_rows_v2.clear();
                row.settings_from = {};
                row.settings_incarnation = 0;
            }
        }
    }
    // A party that has just arrived has heard nothing, so what is true is said again on the next
    // repaint: `say_conditions` is silent while the reading is unchanged (WL-ATTN-12), and an offer
    // is the one moment a new listener certainly exists.
    conditions_said_ = false;
    // ...and the terminal participant's record, for the same reason
    facet<TerminalDoorFacet>().say_again();
    inventory_published_ = false; // ...and the inventory, which a presenter reads (WL-DESK-04)
    keymap_published_ = false;    // ...and the effective keymap, for the same reason (WL-DESK-11)
    subject_published_ = false;   // ...and the inspector's subject (WL-INFO-14)
    // AND THE OFFER MAY RESOLVE AUTHORED INTENT THAT WAS WAITING FOR IT. This is the
    // one path -- the same `apply_setup` the doors and a restore go through -- so a
    // setup naming `third.party/hello` opens the moment that office offers it, without
    // the file having been touched and without a second way to open a pane existing.
    apply_setup(mail);
    repaint(mail);
}

/// A declaration from a pane built before ownership existed: `PaneActions` v1 is unchanged, and
/// its rows are widened here into the host's own type with an empty `supersedes` (it stands in
/// for nothing) and no causes, so everything below reads one population.
// WL-KEY-15 -- agents/workshop/keyboard.md
void WorkshopWeave::on(const PaneActions& actions, loom::Mail& mail) {
    v3::PaneActions widened{actions.pane, std::string(), {}, {}};
    widened.rows.reserve(actions.rows.size());
    for (const PaneActionRow& row : actions.rows) {
        widened.rows.push_back(action_row_of(row));
    }
    declare_pane_actions(widened, mail);
}

/// ...and from a pane built before causes existed: its rows cause nothing a reader can know.
// WL-KEY-15 -- agents/workshop/keyboard.md
void WorkshopWeave::on(const v2::PaneActions& actions, loom::Mail& mail) {
    v3::PaneActions widened{actions.pane, std::string(), {}, {}};
    widened.rows.reserve(actions.rows.size());
    for (const v2::PaneActionRow& row : actions.rows) {
        widened.rows.push_back(action_row_of(row));
    }
    declare_pane_actions(widened, mail);
}

// WL-CAUSE-01 -- agents/workshop/action-causes.md
void WorkshopWeave::on(const v3::PaneActions& actions, loom::Mail& mail) {
    declare_pane_actions(actions, mail);
}

// WL-KEY-15 -- agents/workshop/keyboard.md; WL-CAUSE-01 -- agents/workshop/action-causes.md
void WorkshopWeave::declare_pane_actions(const v3::PaneActions& declared, loom::Mail& mail) {
    const std::string& pane = declared.pane;
    const std::vector<v3::PaneActionRow>& rows = declared.rows;
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // personal speech: no rows, no notice, no catalog change (the offer's rule)
    }
    // THE SHAPE'S HALF FIRST: whose pane, and is it one this office offered. Then the
    // rows' half and the collision law, into a COPY of the effective keymap -- so a
    // refusal anywhere leaves the keymap in force, and the pane's retained declaration,
    // exactly what they were.
    const Admission admitted = admit_pane_actions(session_.panes.runtime, office, pane);
    if (!admitted.written.accepted) {
        // And the declarer is answered: the band tells the weaver, and the verdict, as the answer
        // to this declaration, tells the provider, which can act on it. Both say the same words.
        answer_declaration(std::string(office),
                           ActionsJudged{pane, false, 0, admitted.written.refusal}, mail);
        repaint(mail);
        return;
    }
    Keymap candidate = session_.keymap;
    Written joined = join_pane_rows(candidate, admitted.kind, rows);
    // ...AND WHAT EACH PRESS MAY CAUSE, AND THE MODES, judged beside the keys and refused with
    // them: a declaration is admitted whole or not at all.
    if (joined.accepted) {
        if (std::string wrong = pane_declaration_problem(declared); !wrong.empty()) {
            joined = Written::no(std::move(wrong));
        }
    }
    if (!joined.accepted) {
        const RuntimePane* row = session_.panes.runtime.of_kind(admitted.kind);
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
    // handle, because nothing holds a pointer into `entries` (panes.hpp). The declaration gets
    // its number here, and the verdict below is the only place the declarer learns it.
    const std::int64_t declaration = ++declarations_;
    for (RuntimePane& row : session_.panes.runtime.entries) {
        if (row.kind == admitted.kind) {
            row.actions = rows;
            row.mode = declared.mode;
            row.elsewhere = declared.elsewhere;
            row.declaration = declaration;
        }
    }
    session_.keymap = std::move(candidate);
    answer_declaration(std::string(office), ActionsJudged{pane, true, declaration, std::string()},
                       mail);
    // ...AND A ROW THE TERMINAL CANNOT PRODUCE STANDS, BOUND, AND IS SAID (WL-KEY-08).
    if (const std::string gaps = unsaid_gaps(admitted.kind); !gaps.empty()) {
        say(gaps, false);
    }
    repaint(mail); // the legend and the hotkey view read the map at every paint
}

// WL-KEY-08 -- agents/workshop/keyboard.md
std::string WorkshopWeave::unsaid_gaps(std::int64_t kind) {
    const PaneRows* held = session_.keymap.pane_rows(kind);
    const RuntimePane* row = session_.panes.runtime.of_kind(kind);
    if (held == nullptr || row == nullptr) {
        return std::string();
    }
    std::vector<std::string>& said = gaps_said_[kind];
    std::string fresh;
    for (const std::string& gap : held->gaps) {
        if (std::find(said.begin(), said.end(), gap) != said.end()) {
            continue;
        }
        said.push_back(gap);
        fresh += (fresh.empty() ? "" : "; ") + gap;
    }
    return fresh.empty() ? fresh : row->name + " @" + row->provider + ": " + fresh;
}

// WL-KEY-15 -- agents/workshop/keyboard.md
void WorkshopWeave::rejoin_pane_rows(std::string& refusals, std::string& gaps,
                                     loom::Mail& mail) {
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
    for (RuntimePane& row : session_.panes.runtime.entries) {
        if (row.actions.empty()) {
            continue;
        }
        const Written joined = join_pane_rows(session_.keymap, row.kind, row.actions);
        if (joined.accepted) {
            if (const std::string fresh = unsaid_gaps(row.kind); !fresh.empty()) {
                gaps += (gaps.empty() ? "" : "; ") + fresh;
            }
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
        row.mode.clear();
        row.elsewhere.clear();
        row.declaration = 0;
    }
    // A re-join that drops a pane's rows is a withdrawal the provider cannot see coming. It is not
    // an answer (that delivery is over), so it names the declaration by its verdict's number.
    for (const Dropped& d : told) {
        say_withdrawn(d.office, ActionsWithdrawn{d.pane, d.declaration, d.refusal}, mail);
    }
}

// WL-SETTING-03 -- agents/workshop/settings.md
void WorkshopWeave::on(const PaneSettingsDeclared& declared, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // personal speech declares nothing (the offer's rule)
    }
    const Admission admitted = admit_pane_settings(session_.panes.runtime, office, declared);
    if (!admitted.written.accepted) {
        // REFUSED ALOUD, AND THE SETTINGS IN FORCE STAND: only an accepted declaration replaces.
        say(admitted.written.refusal, true);
        repaint(mail);
        return;
    }
    std::vector<v2::PaneSettingRow> rows;
    rows.reserve(declared.rows.size());
    for (const PaneSettingRow& row : declared.rows) {
        rows.push_back(widen_setting_row(row));
    }
    keep_settings_rows(admitted.kind, std::move(rows), mail);
}

// WL-SETTING-03 -- agents/workshop/settings.md
void WorkshopWeave::on(const v2::PaneSettingsDeclared& declared, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return;
    }
    const Admission admitted = admit_pane_settings(session_.panes.runtime, office, declared);
    if (!admitted.written.accepted) {
        say(admitted.written.refusal, true);
        repaint(mail);
        return;
    }
    keep_settings_rows(admitted.kind, declared.rows, mail);
}

void WorkshopWeave::keep_settings_rows(std::int64_t kind, std::vector<v2::PaneSettingRow> rows,
                                       loom::Mail& mail) {
    // JUDGED WHOLE; ONLY NOW IS ANYTHING WRITTEN, by handle, as an action declaration is. The
    // sender is kept beside the settings, which count while it holds the office.
    for (RuntimePane& row : session_.panes.runtime.entries) {
        if (row.kind == kind) {
            row.settings_rows.clear();
            for (const v2::PaneSettingRow& one : rows) {
                row.settings_rows.push_back(narrow_setting_row(one));
            }
            row.settings_rows_v2 = rows;
            row.settings_from = mail.sender();
            row.settings_incarnation =
                host_->incarnation_of ? host_->incarnation_of(mail.sender()) : 0;
        }
    }
    repaint(mail);
}

// WL-SETTING-03 -- agents/workshop/settings.md
const std::vector<v2::PaneSettingRow>*
WorkshopWeave::counted_settings_v2(const RuntimePane& row) const {
    return counted_settings(row) != nullptr ? &row.settings_rows_v2 : nullptr;
}

// WL-SETTING-07 -- agents/workshop/settings.md
std::int64_t WorkshopWeave::settings_revision(const PaneRef& ref) {
    // A ROW THE LIVE LAYOUT NO LONGER HAS TAKES ITS RECORD WITH IT, so the list is bounded by the
    // desk's rows, and the row coming back is minted afresh.
    std::erase_if(settings_revisions_, [this](const SettingsRevision& r) {
        return pane_of(session_.setup.active, r.ref) == nullptr;
    });
    const SetupPane* seat = pane_of(session_.setup.active, ref);
    if (seat == nullptr) {
        return 0;
    }
    const RuntimePane* offered = session_.panes.runtime.find(ref.provider, ref.pane);
    const std::vector<v2::PaneSettingRow>* counted =
        offered != nullptr ? counted_settings_v2(*offered) : nullptr;
    SettingsRevision now{ref,
                         0,
                         session_.setup.put_live,
                         seat->settings,
                         counted != nullptr ? *counted : std::vector<v2::PaneSettingRow>{},
                         host_->role_holder ? host_->role_holder(ref.provider) : loom::WeaveId{}};
    for (SettingsRevision& kept : settings_revisions_) {
        if (!(kept.ref == ref)) {
            continue;
        }
        if (kept.desk != now.desk || kept.settings != now.settings || kept.rows != now.rows ||
            kept.holder != now.holder) {
            now.revision = ++settings_revision_;
            kept = std::move(now);
        }
        return kept.revision;
    }
    now.revision = ++settings_revision_;
    settings_revisions_.push_back(std::move(now));
    return settings_revisions_.back().revision;
}

// WL-SETTING-03 -- agents/workshop/settings.md
const std::vector<PaneSettingRow>* WorkshopWeave::counted_settings(const RuntimePane& row) const {
    if (!row.settings_from.valid() || !host_->role_holder ||
        host_->role_holder(row.provider) != row.settings_from) {
        return nullptr;
    }
    // ...AND AT THE INCARNATION THAT DECLARED: a reload in place keeps the id and moves this.
    if (host_->incarnation_of &&
        host_->incarnation_of(row.settings_from) != row.settings_incarnation) {
        return nullptr;
    }
    return &row.settings_rows;
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
    const RuntimePane* row = session_.panes.runtime.find(office, asked.pane);
    if (row == nullptr) {
        return;
    }
    // COPIED OUT BEFORE THE DESK MOVES: `apply_setup` may re-ask providers and the catalog
    // vector may grow under a pointer into it (panes.hpp's own warning).
    const std::int64_t kind = row->kind;
    const std::string name = row->name;
    const PaneRef ref{row->provider, row->pane};
    // This delivery is the commitment point, and it waits for no room: the stack is not rationed,
    // and a pane its column has no height left for begins the column again at its top
    // (`bounds_of`), in front as the newest row.
    const bool added = add_pane(session_.setup.active, ref);
    apply_setup(mail);
    if (!session_.panes.has(kind)) {
        // The belt under the seat: `apply_setup` seats every resolved row, so a pane it did not
        // seat is a defect. Answered as a refusal, with the row taken back: "seated" is the word
        // the asker acts on.
        if (added) {
            (void)remove_pane(session_.setup.active, ref);
            apply_setup(mail);
        }
        const std::string refusal = "defect: " + name + " was not seated";
        say(refusal, true);
        (void)mail.answer(PaneRevealAnswered{asked.pane, false, refusal});
        repaint(mail);
        return;
    }
    // AND IT SELECTS THE PANE IT JUST SEATED AND POINTS THE KEYS AT IT -- the keyboard
    // candidate's own argument, one question wider: a reveal that pointed the keys at a pane
    // still sitting behind another would put the first keystroke somewhere the weaver cannot
    // see. The two facts are written together everywhere they are written.
    session_.panes.selected = kind;
    session_.panes.keyboard = kind_takes_keyboard(kind) ? kind : kNoPaneKind;
    // SAID, so the sentence on the notice line is about what just happened and names who
    // asked for it -- the pane's own rows say what it is showing. Said and written BEFORE the
    // answer leaves, so what the asker hears is a fact about this desk and not a promise. A pane
    // whose own place stands off this screen keeps it, and the band says where it went.
    if (const std::string off = off_this_screen(kind, name); !off.empty()) {
        say(off, true);
    } else {
        say("showing " + name + " -- it asked to be shown, and it has the keys", false);
    }
    (void)mail.answer(PaneRevealAnswered{asked.pane, true, std::string()});
    repaint(mail);
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

std::string WorkshopWeave::hotkey(Act a) const {
    return hotkey_text(session_.keymap, session_.band_keys, a);
}

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
    // ASKED OF THE OFFICE HOLDING THE PLATFORM -- the window showing the desk, or the Skin -- and
    // settled only by that office's answer.
    const std::string office = medium_office();
    const loom::AskOpened opened = paste_asks_.open_to_role(
        office, zengine::graphics::ClipboardTextRequested::zen_name,
        zengine::graphics::ClipboardTextRequested::zen_version);
    if (!opened) {
        return; // the book is full: this paste is dropped, the outstanding ones stand
    }
    p.ask = opened.id;
    pending_pastes_.push_back(std::move(p));
    (void)mail.send_to_role(office, zengine::graphics::ClipboardTextRequested{},
                            opened.correlation);
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
