// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- the pane editor and the pane creator -- compiled once
// into `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/pane-manager.md (+3 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- THE PANE EDITOR: a pane as a subject ------------------------------------------------

// WL-PED-03 -- agents/workshop/pane-manager.md
void WorkshopWeave::repair_pane_editor_subject() {
    PaneEditor& ed = session_.pane_editor;
    if (!ed.addressed() || pane_editor_subject_row(session_).has_value()) {
        return;
    }
    const std::string was = ref_text(ed.subject);
    ed.subject = PaneRef{};
    ed.rows.clear();
    ed.row_cursor = 0;
    ed.on_rows = false;
    say("the Pane Manager's subject " + was +
            " is in neither this build's vocabulary nor this layout -- subject cleared",
        true);
}

// WL-PED-02 -- agents/workshop/pane-manager.md
void WorkshopWeave::choose_subject(const PaneRef& ref) {
    PaneEditor& ed = session_.pane_editor;
    ed.subject = ref;
    ed.rows = pane_editor_rows(session_);
    ed.row_cursor = first_editable(ed.rows);
    const std::optional<CatalogRow> row = pane_editor_subject_row(session_);
    std::string name = ref_text(ref);
    std::string state;
    if (row) {
        name = row->kind == kNoPaneKind ? ref_text(ref) : row->name;
        state = pane_state_word(pane_state_of(session_.panels, session_.setup.active,
                                              screen_of(session_), *row));
    }
    say("Pane Manager: " + name + (state.empty() ? "" : " (" + state + ")") + " -- " +
            hotkey(Act::kPaneEditorSwitch) + " reaches its rows",
        false);
}

void WorkshopWeave::pane_editor_move(std::int64_t by) {
    pane_editor_move_in(session_.pane_editor.on_rows, by);
}

// WL-PED-08 -- agents/workshop/pane-manager.md
void WorkshopWeave::pane_editor_move_in(bool rows, std::int64_t by) {
    PaneEditor& ed = session_.pane_editor;
    if (!rows) {
        const std::size_t total = picker_population().size();
        if (total == 0) {
            return;
        }
        if (ed.cursor >= total) {
            ed.cursor = total - 1;
        }
        if (by < 0 && ed.cursor > 0) {
            --ed.cursor;
        } else if (by > 0 && ed.cursor + 1 < total) {
            ++ed.cursor;
        }
        return;
    }
    const std::size_t total = ed.rows.size();
    if (total == 0) {
        return;
    }
    std::size_t at = ed.row_cursor < total ? ed.row_cursor : total - 1;
    while (true) {
        if (by < 0) {
            if (at == 0) {
                return;
            }
            --at;
        } else {
            if (at + 1 >= total) {
                return;
            }
            ++at;
        }
        if (!ed.rows[at].section()) {
            ed.row_cursor = at;
            return;
        }
    }
}

// WL-EDIT-10 -- agents/workshop/editor.md; WL-PED-08 -- agents/workshop/pane-manager.md
void WorkshopWeave::pane_editor_wheel(const zengine::input::PointerWheel& w, loom::Mail& mail) {
    repair_pane_editor_subject();
    const PaneEditorAt where = pane_editor_at(session_, w.space, w.x, w.y);
    if (!where.present) {
        return;
    }
    PaneEditor& ed = session_.pane_editor;
    const std::int64_t rows = spend_wheel(ed.wheel_accum, w.dy, kListWheelRows);
    if (rows == 0) {
        return;
    }
    const bool on_fields =
        where.at.row >= static_cast<std::int64_t>(where.body.panes_rows);
    const std::size_t was = on_fields ? ed.row_cursor : ed.cursor;
    const std::int64_t steps = rows < 0 ? -rows : rows;
    for (std::int64_t n = 0; n < steps; ++n) {
        pane_editor_move_in(on_fields, rows > 0 ? -1 : +1);
    }
    if ((on_fields ? ed.row_cursor : ed.cursor) == was) {
        return; // already at the edge: nothing moved, nothing repaints
    }
    repaint(mail);
}

void WorkshopWeave::pane_editor_switch() {
    PaneEditor& ed = session_.pane_editor;
    if (!ed.on_rows && ed.rows.empty()) {
        say("the Pane Manager has no subject -- " + hotkey(Act::kPaneEditorChoose) +
                " on a pane in its list chooses one",
            true);
        return;
    }
    ed.on_rows = !ed.on_rows;
}

void WorkshopWeave::pane_editor_choose() {
    PaneEditor& ed = session_.pane_editor;
    if (!ed.on_rows) {
        const std::vector<CatalogRow> rows = picker_population();
        if (ed.cursor >= rows.size()) {
            return; // the belt: a population that shrank under the cursor
        }
        if (pane_editor_draft_live(session_)) {
            say(finish_draft_first(), true); // a new subject would drop the draft
            return;
        }
        choose_subject(rows[ed.cursor].ref);
        return;
    }
    if (ed.row_cursor >= ed.rows.size()) {
        return;
    }
    Row& row = ed.rows[ed.row_cursor];
    if (!row.editable()) {
        say(row.label() + " is not authored -- it is what the screen makes of the "
                          "authored value",
            true);
        return;
    }
    row.begin();
    say("editing " + row.label() + " -- " + hotkey(Act::kDraftCommit) + " commits, " +
            hotkey(Act::kDraftCancel) + " cancels, `-` resets it",
        false);
}

void WorkshopWeave::pane_editor_toggle(loom::Mail& mail) {
    const std::optional<CatalogRow> row = pane_editor_subject_row(session_);
    if (!row) {
        say("the Pane Manager has no subject -- " + hotkey(Act::kPaneEditorChoose) +
                " on a pane in its list chooses one",
            true);
        return;
    }
    if (pane_editor_draft_live(session_)) {
        say(finish_draft_first(), true);
        return;
    }
    toggle_participation(*row, hotkey(Act::kPaneEditorOpen), mail);
}

void WorkshopWeave::pane_editor_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    repair_pane_editor_subject();
    const PaneRef subject = session_.pane_editor.subject;
    const auto order = [&](Act a) {
        if (subject.provider.empty()) {
            say("the Pane Manager has no subject -- " + hotkey(Act::kPaneEditorChoose) +
                    " on a pane in its list chooses one",
                true);
            return;
        }
        spend_pane_action(a, subject, mail);
    };
    switch (session_.keymap.action_for(KeyContext::kPaneEditor, k.scancode, k.modifiers)) {
    case Act::kPaneEditorUp: pane_editor_move(-1); break;
    case Act::kPaneEditorDown: pane_editor_move(+1); break;
    case Act::kPaneEditorSwitch: pane_editor_switch(); break;
    case Act::kPaneEditorChoose: pane_editor_choose(); break;
    case Act::kPaneEditorOpen: pane_editor_toggle(mail); break;
    case Act::kPaneEditorFront: order(Act::kManageFront); break;
    case Act::kPaneEditorBack: order(Act::kManageBack); break;
    case Act::kPaneEditorRaise: order(Act::kManageRaise); break;
    case Act::kPaneEditorLower: order(Act::kManageLower); break;
    // THE PANE CREATOR'S THREE: make a pane, keep it, put it back.
    case Act::kPaneCreatorNew: open_pane_naming(); break;
    case Act::kPaneCreatorSave: save_maker_pane(); break;
    case Act::kPaneCreatorDiscard: discard_maker_pane_edits(mail); break;
    default: break; // an unbound key in this pane means nothing, and says nothing
    }
}

// ---- THE PANE CREATOR: a pane made of authored data ---------------------------------------

// WL-PED-04 -- agents/workshop/pane-manager.md
void WorkshopWeave::rebuild_subject_rows() {
    PaneEditor& ed = session_.pane_editor;
    if (!ed.addressed()) {
        return;
    }
    ed.rows = pane_editor_rows(session_);
    if (ed.row_cursor >= ed.rows.size()) {
        ed.row_cursor = first_editable(ed.rows);
    }
}

std::string WorkshopWeave::maker_pane_dirty_sentence(const char* consequence) const {
    const MakerPane& m = session_.panels.maker;
    const std::string name = m.definition.open() ? m.definition.name : m.saved.name;
    return "pane " + name + " has unsaved changes -- " + hotkey(Act::kPaneCreatorSave) +
           " in the Pane Manager saves it, " + hotkey(Act::kPaneCreatorDiscard) +
           " discards them; " + consequence;
}

// WL-MAKER-08 -- agents/workshop/maker-pane.md
void WorkshopWeave::open_maker_pane(const std::string& requested, loom::Mail& mail) {
    const std::string path = persist::resolved_against(host_->project_dir, requested);
    MakerPane& m = session_.panels.maker;
    if (m.dirty()) {
        say(maker_pane_dirty_sentence("nothing was opened"), true);
        return;
    }
    const pane_definition_persist::LoadedDefinition loaded =
        pane_definition_persist::load_file(path);
    if (!loaded.outcome.accepted) {
        if (path == host_pane_path()) {
            pane_refused_ = true;
            session_.conditions.establish(Condition{
                kPaneWallKey, "pane definition refused -- this run will not write over it",
                loaded.outcome.refusal +
                    " (the file is left exactly as it is; fix it, or start Workshop with "
                    "--pane <another path>)",
                surface::role::kAlert, std::string()});
        }
        say(path + ": " + loaded.outcome.refusal, true);
        return;
    }
    m.path = path;
    m.definition = loaded.definition;
    m.saved = loaded.definition;
    rebuild_subject_rows();
    // AND THE DESK RE-SEATS, because a reference that was unresolved a moment ago may
    // resolve now -- the same reconcile a provider's late offer earns.
    apply_setup(mail);
    say("opened pane " + m.definition.name + " from " + path, false);
}

// WL-MAKER-08, WL-MAKER-11 -- agents/workshop/maker-pane.md
bool WorkshopWeave::new_maker_pane(const std::string& name, loom::Mail& mail) {
    MakerPane& m = session_.panels.maker;
    if (m.dirty()) {
        say(maker_pane_dirty_sentence("nothing was made"), true);
        return false;
    }
    const Written legal = check_maker_pane_name(name);
    if (!legal.accepted) {
        say(legal.refusal + " -- " + hotkey(Act::kPaneNamingCommit) + " tries again, " +
                hotkey(Act::kPaneNamingCancel) + " cancels",
            true);
        return false;
    }
    const PaneRef ref = maker_pane_ref(name);
    m.definition = new_definition(name);
    m.saved = PaneDefinition{};
    m.path = host_pane_path();
    (void)add_pane(session_.setup.active, ref);
    const Seating trial = seat_panes(session_.setup.active, session_.panels,
                                     stack_capacity(screen_of(session_)));
    bool waiting = false;
    for (const std::int64_t k : trial.waiting) {
        if (is_maker_kind(k)) {
            waiting = true;
        }
    }
    apply_setup(mail);
    // THE CREATOR'S SUBJECT IS THE NEW PANE'S REGION: the manager's subject through its
    // one writer, then the keys onto the region's own rows, on the text first.
    choose_subject(ref);
    PaneEditor& ed = session_.pane_editor;
    ed.on_rows = true;
    for (std::size_t i = 0; i < ed.rows.size(); ++i) {
        if (ed.rows[i].label() == "Text") {
            ed.row_cursor = i;
            break;
        }
    }
    std::string said = "Pane Creator: " + name + " is on this layout";
    if (waiting) {
        said += " (waiting for room -- make the window taller)";
    }
    said += " -- its text region's rows are under INTERIOR; " +
            hotkey(Act::kPaneCreatorSave) + " saves it";
    if (m.path.empty()) {
        said += " (no pane file this run)";
    }
    say(said, false);
    return true;
}

void WorkshopWeave::open_pane_naming() {
    if (session_.panels.maker.dirty()) {
        say(maker_pane_dirty_sentence("no new pane was started"), true);
        return;
    }
    session_.pane_naming.open = true;
    session_.pane_naming.line.set(std::string(), 0);
    say("new pane -- type its name; " + hotkey(Act::kPaneNamingCommit) + " makes it, " +
            hotkey(Act::kPaneNamingCancel) + " cancels",
        false);
}

void WorkshopWeave::pane_naming_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    PaneNaming& naming = session_.pane_naming;
    if (naming.line.consume(k.scancode, k.modifiers, session_.clipboard)) {
        return;
    }
    switch (session_.keymap.action_for(KeyContext::kPaneNaming, k.scancode, k.modifiers)) {
    case Act::kPaneNamingCommit:
        if (new_maker_pane(naming.line.text(), mail)) {
            close_pane_naming();
        }
        break;
    case Act::kPaneNamingCancel:
        close_pane_naming();
        say("no pane was made", false);
        break;
    default: break;
    }
}

void WorkshopWeave::close_pane_naming() { session_.pane_naming = PaneNaming{}; }

// WL-MAKER-08 -- agents/workshop/maker-pane.md
void WorkshopWeave::save_maker_pane() {
    MakerPane& m = session_.panels.maker;
    if (!m.open()) {
        say("no pane is open -- " + hotkey(Act::kPaneCreatorNew) +
                " in the Pane Manager makes one",
            true);
        return;
    }
    if (m.path.empty()) {
        say(kNoPaneFile, true);
        return;
    }
    if (pane_refused_ && m.path == host_pane_path()) {
        say("the pane file " + m.path +
                " was refused at startup and will not be written over -- fix it, or start "
                "Workshop with --pane <another path>",
            true);
        return;
    }
    if (const Row* draft = pane_editor_editing_row()) {
        say(draft->label() + " is still being edited -- " + hotkey(Act::kDraftCommit) +
                " commits, " + hotkey(Act::kDraftCancel) + " cancels; nothing was saved",
            true);
        return;
    }
    const Written written = pane_definition_persist::save_file(m.path, m.definition);
    if (!written.accepted) {
        say(written.refusal, true);
        return;
    }
    m.saved = m.definition;
    say("saved pane " + m.definition.name + " to " + m.path, false);
}

// WL-MAKER-08 -- agents/workshop/maker-pane.md
void WorkshopWeave::discard_maker_pane_edits(loom::Mail& mail) {
    MakerPane& m = session_.panels.maker;
    if (!m.open() && !m.saved.open()) {
        say("no pane is open -- nothing to discard", true);
        return;
    }
    if (!m.dirty()) {
        say("pane " + m.definition.name + " matches " + m.path + " -- nothing to discard",
            false);
        return;
    }
    const std::string was = m.definition.name;
    m.definition = m.saved;
    rebuild_subject_rows();
    apply_setup(mail);
    if (m.open()) {
        say("discarded unsaved edits -- pane " + m.definition.name + " is back to what " +
                m.path + " holds",
            false);
    } else {
        say("discarded pane " + was +
                " -- it was never saved, so no pane is open; its row on this layout is "
                "kept and reads unresolved",
            false);
    }
}

void WorkshopWeave::refresh_pane_name() {
    if (!session_.pane_naming.open) {
        return;
    }
    const Screen sc = screen_of(session_);
    const PanelBounds where =
        bounds_of(session_.panels, session_.setup.active, panel::kPaneEditor, sc);
    if (!where.open) {
        return;
    }
    const PaneEditorBodyPlace body = pane_editor_body(session_, sc, where.rect);
    session_.pane_naming.line.keep_caret_visible(pane_name_columns(body.fit.columns));
}

// WL-PED-02 -- agents/workshop/pane-manager.md
void WorkshopWeave::pane_editor_press(const zengine::input::PointerButton& b, std::int64_t modifiers) {
    repair_pane_editor_subject();
    PaneEditor& ed = session_.pane_editor;
    const PaneEditorAt where = pane_editor_at(session_, b.space, b.x, b.y);
    if (!where.present) {
        return; // the heading, or the padding: consumed, still
    }
    for (std::size_t i = 0; i < ed.rows.size(); ++i) {
        Row& row = ed.rows[i];
        if (!row.editing()) {
            continue;
        }
        if (where.at.column < 0 || where.at.column > where.body.fit.columns ||
            field_at_prose_row(where.body, where.at.row) != i) {
            break; // on the pane, but not on the draft's row
        }
        const std::size_t target =
            row.editor().position_at_column(property_value_column(where.at.column));
        if (!press_selects_word(modifiers, row, target)) {
            row.place(target);
        }
        session_.text_drag.active = true;
        session_.text_drag.place = text_drag_place::kPaneEditorDraft;
        return;
    }
    const std::size_t pane = editor_pane_at_prose_row(where.body, where.at.row);
    if (pane != kNoObject) {
        const std::vector<CatalogRow> rows = picker_population();
        if (pane >= rows.size()) {
            return;
        }
        if (pane_editor_draft_live(session_)) {
            say(finish_draft_first(), true);
            return;
        }
        ed.on_rows = false;
        ed.cursor = pane;
        choose_subject(rows[pane].ref);
        return;
    }
    const std::size_t field = field_at_prose_row(where.body, where.at.row);
    if (field != kNoProperty && field < ed.rows.size() && !ed.rows[field].section()) {
        if (pane_editor_draft_live(session_)) {
            say(finish_draft_first(), true);
            return;
        }
        ed.on_rows = true;
        ed.row_cursor = field;
    }
}

// WL-EDIT-05 -- agents/workshop/editor.md
void WorkshopWeave::edit_source(loom::Mail& mail) {
    if (!session_.panels.has(panel::kBuilder)) {
        return; // an unbound key with no Builder panel open, exactly as `b` is
    }
    const BuilderPane& pane = session_.panels.builder;
    if (!pane.heard) {
        say("the Builder has not said what it builds yet -- nothing was opened", true);
        return;
    }
    if (pane.known.recipes.empty()) {
        say("this project has no build recipes -- nothing was opened", true);
        return;
    }
    const std::size_t at =
        pane.chosen < pane.known.recipes.size() ? pane.chosen : std::size_t{0};
    const std::string chosen = pane.known.recipes[at].recipe;
    if (!host_->recipe_source) {
        say("this host resolves no recipe sources -- nothing was opened", true);
        return;
    }
    const HostContext::RecipeSource named = host_->recipe_source(chosen);
    if (!named.known) {
        say("the Builder's catalog and this project's recipes disagree about `" +
                chosen + "` -- nothing was opened",
            true);
        return;
    }
    if (named.source.empty()) {
        // THE RECIPE OWNER'S OWN VOCABULARY: the kind word is the recipe file's, so
        // the refusal reads in the terms the maker authored.
        say("`" + chosen + "` is a " + named.kind +
                " recipe -- it names no single source file to edit",
            true);
        return;
    }
    open_source(named.source, mail);
}

// WL-EDIT-03, WL-EDIT-05, WL-EDIT-06, WL-EDIT-11, WL-EDIT-13 -- agents/workshop/editor.md
// WL-FRONT-04 -- agents/workshop/planes.md
void WorkshopWeave::open_source(const std::string& requested, loom::Mail& mail) {
    const std::string path = persist::resolved_against(host_->project_dir, requested);
    EditorState& e = session_.editor;
    if (e.open_document() && e.path == path) {
        // RE-REQUESTING THE OPEN SOURCE REVEALS IT AND DESTROYS NOTHING: the buffer,
        // its caret, its selection, its history and its viewport all stand; what
        // moves is presence (a removed pane comes back) and the keyboard.
        if (!ensure_editor_pane(mail)) {
            return;
        }
        session_.panels.selected = panel::kEditor;
        session_.panels.keyboard = panel::kEditor;
        e.follow_caret = true;
        say("editing " + e.path + (e.dirty() ? " -- UNSAVED edits stand" : ""), false);
        return;
    }
    if (e.dirty()) {
        // THE UNSAVED-LOSS FLOOR: a different source must not silently replace a
        // dirty buffer. The two ways out are the editor's own save and its one
        // deliberate discard door, both named with their effective gestures.
        say(e.path + " has unsaved changes -- " + hotkey(Act::kEditorSave) +
                " in the editor saves them, " + hotkey(Act::kEditorDiscard) +
                " discards them; nothing was opened",
            true);
        return;
    }
    // READ AND JUDGE BEFORE ANYTHING MOVES: a refused file costs the maker the
    // notice and nothing else -- the pane, the setup, the current document (if any)
    // and the file itself are all exactly as they were.
    const persist::FileText read =
        persist::read_file(path, kMaxSourceBytes, "a source file");
    if (!read.outcome.accepted) {
        say(read.outcome.refusal, true);
        return;
    }
    SourceIn admitted = source_in(read.text);
    if (!admitted.outcome.accepted) {
        say(path + ": " + admitted.outcome.refusal, true);
        return;
    }
    if (!ensure_editor_pane(mail)) {
        return;
    }
    e.path = path;
    e.saved_lines = admitted.lines;
    e.buffer.set_lines(std::move(admitted.lines));
    e.convention = admitted.convention;
    ++e.doc_epoch;
    e.first_row = 0;
    e.first_col = 0;
    e.wheel_accum = 0.0;
    e.follow_caret = true;
    // AND IT SELECTS THE PANE IT JUST FILLED. The keyboard candidate's own
    // argument, one question wider: an open that pointed the keys at a pane still
    // sitting behind another would put the first keystroke somewhere the maker
    // cannot see. The two facts are written together everywhere they are written.
    session_.panels.selected = panel::kEditor;
    session_.panels.keyboard = panel::kEditor;
    say("editing " + e.path, false);
}

bool WorkshopWeave::ensure_editor_pane(loom::Mail& mail) {
    const PaneRef ref = pane_ref_of(panel::kEditor);
    Setup candidate = session_.setup.active;
    const bool added = add_pane(candidate, ref);
    const Seating trial = seat_panes(candidate, session_.panels,
                                     stack_capacity(screen_of(session_)));
    for (const std::int64_t k : trial.waiting) {
        if (k == panel::kEditor) {
            say("no room for the Editor on this screen -- make the window taller, "
                "then try again",
                true);
            return false;
        }
    }
    if (added) {
        session_.setup.active = std::move(candidate);
    }
    apply_setup(mail);
    return true;
}

// WL-EDIT-01 -- agents/workshop/editor.md
void WorkshopWeave::save_source() {
    EditorState& e = session_.editor;
    if (!e.open_document()) {
        say("no source is open -- nothing was saved", true);
        return;
    }
    const Written written =
        persist::write_file(e.path, source_text(e.buffer.lines(), e.convention));
    if (!written.accepted) {
        say(written.refusal, true);
        return;
    }
    e.saved_lines = e.buffer.lines();
    say("saved " + e.path, false);
}

// WL-EDIT-03 -- agents/workshop/editor.md
void WorkshopWeave::discard_source_edits() {
    EditorState& e = session_.editor;
    if (!e.open_document()) {
        say("no source is open -- nothing to discard", true);
        return;
    }
    if (!e.dirty()) {
        say("the source matches its last saved state -- nothing to discard", false);
        return;
    }
    e.buffer.revert_to(e.saved_lines);
    e.follow_caret = true;
    say("discarded unsaved edits -- " + e.path +
            " is back to its last saved state; undo takes them back",
        false);
}

} // namespace zengine::workshop
