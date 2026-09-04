// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- the source editor and the filesystem browser --
// compiled once into `zengine-workshop-logic` and linked by the host and every suite; the
// declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/files.md (+5 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- THE SOURCE EDITOR: choose source, edit, save, and never lose a byte ----------

// WL-EDIT-02, WL-EDIT-09 -- agents/workshop/editor.md
void WorkshopWeave::editor_key(const zengine::input::KeyPressed& k) {
    EditorState& e = session_.editor;
    if (e.buffer.consume(k.scancode, k.modifiers, session_.clipboard)) {
        e.follow_caret = true;
        return;
    }
    switch (session_.keymap.action_for(KeyContext::kEditor, k.scancode, k.modifiers)) {
    case Act::kEditorSave: save_source(); break;
    case Act::kEditorNewline:
        e.buffer.newline();
        e.follow_caret = true;
        break;
    case Act::kEditorTab:
        // A TAB BYTE, PRESERVED AS ONE -- the byte policy's insertion half. It
        // arrives as a key rather than as text because no backend delivers a
        // control byte as entered text (input's own law).
        e.buffer.type("\t");
        e.follow_caret = true;
        break;
    case Act::kEditorDiscard: discard_source_edits(); break;
    default: break;
    }
}

// WL-EDIT-07 -- agents/workshop/editor.md
void WorkshopWeave::editor_text(const std::string& text) {
    if (text.empty()) {
        return;
    }
    if (!source_text_ok(text)) {
        say("that text holds bytes outside plain ASCII, which this editor cannot "
            "carry truthfully -- nothing was inserted",
            true);
        return;
    }
    session_.editor.buffer.type(text);
    session_.editor.follow_caret = true;
}

// WL-EDIT-08, WL-EDIT-12 -- agents/workshop/editor.md
void WorkshopWeave::editor_press(const zengine::input::PointerButton& b) {
    EditorState& e = session_.editor;
    if (!e.open_document()) {
        return; // consumed: a press into the empty editor is only a focus statement
    }
    const EditorPressAt at =
        editor_press_at(session_, screen_of(session_), b.space, b.x, b.y);
    if (!at.named) {
        return; // the header, or the strip below the last prose row: consumed, still
    }
    const std::size_t row = e.first_row + static_cast<std::size_t>(at.row);
    const std::size_t target =
        row < e.buffer.line_count() ? row : e.buffer.line_count() - 1;
    e.buffer.place(target,
                   byte_of_visual_col(e.buffer.line(target), e.first_col + at.column));
    session_.text_drag.active = true;
    session_.text_drag.place = text_drag_place::kEditorBody;
    e.follow_caret = true;
}

void WorkshopWeave::refresh_editor() { reconcile_editor_view(session_); }

// ---- The filesystem browser ----------------------------------------------

// WL-FILES-02 -- agents/workshop/files.md
std::string WorkshopWeave::files_dir() const { return session_.panels.files.current_dir; }

// WL-FILES-02 -- agents/workshop/files.md
void WorkshopWeave::ensure_marks() {
    if (session_.marks.settled) {
        return;
    }
    session_.marks.settled = true;
    session_.marks.origin = admit_location(host_->project_dir);
    load_marks(); // has its own once-guard: startup may have run it already
    if (session_.panels.files.current_dir.empty()) {
        session_.panels.files.current_dir = session_.marks.origin;
    }
}

// WL-FILES-08 -- agents/workshop/files.md
void WorkshopWeave::load_marks() {
    if (marks_loaded_) {
        return;
    }
    marks_loaded_ = true;
    if (host_->marks_path.empty() || !std::filesystem::exists(host_->marks_path)) {
        return;
    }
    const marks_persist::LoadedMarks loaded = marks_persist::load_file(host_->marks_path);
    if (!loaded.outcome.accepted) {
        marks_refused_ = true;
        session_.conditions.establish(Condition{
            kMarksWallKey, "marks refused -- this run remembers no places",
            loaded.outcome.refusal + " (the file is left exactly as it is; fix or "
                                     "delete it)",
            surface::role::kAlert, std::string()});
        return;
    }
    session_.marks.maker = loaded.maker;
    if (!loaded.skipped.empty()) {
        // A SECOND STANDING CONDITION, AND A QUIETER ONE. The file was read and most of
        // it is in force; what is standing is that part of it is not, and that the next
        // mark this maker makes will write the list WITHOUT those rows. Saying it once
        // as an event would be saying it exactly where nobody could act on it.
        session_.conditions.establish(
            Condition{kMarksSkippedKey, "some marks could not be used",
                      loaded.skipped + " (fix the file before marking anything else, or "
                                       "those rows are dropped by the next save)",
                      surface::role::kAccent, std::string()});
    }
}

// WL-FILES-08 -- agents/workshop/files.md
void WorkshopWeave::save_marks() {
    if (host_->marks_path.empty() || marks_refused_) {
        return;
    }
    const Written done =
        marks_persist::save_file(host_->marks_path, session_.marks.maker);
    if (!done.accepted) {
        say("could not write your marks: " + done.refusal, true);
    }
}

// WL-FILES-12 -- agents/workshop/files.md
void WorkshopWeave::files_refresh() {
    ensure_marks();
    FilesPane& pane = session_.panels.files;
    const std::string dir = files_dir();
    if (dir.empty()) {
        pane.listing = Listing{};
        pane.listing.refusal =
            "this run began nowhere -- Workshop could not tell where it was launched "
            "from, so there is no origin to browse from";
        pane.cursor = 0;
        return;
    }
    pane.listing = enumerate_directory(dir);
    pane.cursor = 0;
    pane.wheel_accum = 0.0;
}

// WL-FILES-12 -- agents/workshop/files.md
void WorkshopWeave::files_build_settled() {
    if (!session_.panels.has(panel::kProjectFiles)) {
        return;
    }
    const FileRow* row = row_at(session_.panels.files.listing, session_.panels.files.cursor);
    const std::string was = row != nullptr ? row->name : std::string();
    files_refresh();
    if (!was.empty()) {
        files_point_at(was);
    }
}

void WorkshopWeave::files_point_at(const std::string& name) {
    const FilesPane& pane = session_.panels.files;
    for (std::size_t i = 0; i < pane.listing.rows.size(); ++i) {
        if (pane.listing.rows[i].name == name) {
            session_.panels.files.cursor = i;
            return;
        }
    }
}

void WorkshopWeave::files_move(std::int64_t by) {
    FilesPane& pane = session_.panels.files;
    const std::size_t total = pane.listing.rows.size();
    if (total == 0) {
        pane.cursor = 0;
        return;
    }
    std::int64_t at = static_cast<std::int64_t>(pane.cursor < total ? pane.cursor : 0) + by;
    if (at < 0) {
        at = 0;
    }
    if (at >= static_cast<std::int64_t>(total)) {
        at = static_cast<std::int64_t>(total) - 1;
    }
    pane.cursor = static_cast<std::size_t>(at);
}

// WL-FILES-03 -- agents/workshop/files.md
void WorkshopWeave::files_parent() {
    ensure_marks();
    FilesPane& pane = session_.panels.files;
    if (pane.current_dir.empty()) {
        say("there is nowhere to go up from -- this run began nowhere", true);
        return;
    }
    const std::string up = parent_location(pane.current_dir);
    if (up.empty()) {
        say(pane.current_dir + " is the top of this filesystem -- there is nothing "
                               "above it to go to",
            false);
        return;
    }
    const std::filesystem::path was(pane.current_dir);
    const AdmittedName leaf = admit_filename(was.filename());
    pane.current_dir = up;
    files_refresh();
    if (leaf.exact) {
        files_point_at(leaf.name);
    }
    files_say_where();
}

std::string WorkshopWeave::files_where() const {
    const std::string& dir = session_.panels.files.current_dir;
    return dir.empty() ? std::string("nowhere") : dir;
}

// WL-FILES-07 -- agents/workshop/files.md
void WorkshopWeave::files_say_where() {
    const std::string where = files_where();
    const std::string why =
        provenance_words(session_.marks.provenance(session_.panels.files.current_dir));
    say(why.empty() ? "in " + where : "in " + where + " (" + why + ")", false);
}

// WL-FILES-04, WL-FILES-10, WL-FILES-11 -- agents/workshop/files.md
// WL-FOCUS-04 -- agents/workshop/focus.md
void WorkshopWeave::files_open(loom::Mail& mail) {
    ensure_marks();
    FilesPane& pane = session_.panels.files;
    const FileRow* row = row_at(pane.listing, pane.cursor);
    if (row == nullptr) {
        say("no row is selected -- nothing was opened", true);
        return;
    }
    if (!row->openable) {
        say("`" + shown_name(row->name) +
                "` has bytes this Workshop cannot carry in a path -- it is shown so you "
                "know it is there, and cannot be opened from here",
            true);
        return;
    }
    const std::string dir = files_dir();
    if (dir.empty()) {
        say("this run began nowhere -- there is no location to act in", true);
        return;
    }
    if (row->directory) {
        // THE SAME NORMALIZATION EVERY OTHER SPELLING IN THIS APPLICATION GOES
        // THROUGH (`persist::resolved_against`), so entering a directory at a
        // filesystem root cannot produce the doubled separator that would name a
        // different root on POSIX.
        const std::string into =
            admit_location(persist::resolved_against(dir, row->name));
        if (into.empty()) {
            say("`" + shown_name(row->name) +
                    "` cannot be reached from here in a path this Workshop can carry",
                true);
            return;
        }
        pane.current_dir = into;
        files_refresh();
        files_say_where();
        return;
    }
    open_source(persist::resolved_against(dir, row->name), mail);
}

// WL-FILES-05 -- agents/workshop/files.md
void WorkshopWeave::files_mark() {
    ensure_marks();
    const std::string where = session_.panels.files.current_dir;
    if (where.empty()) {
        say("there is nowhere to mark -- this run began nowhere", true);
        return;
    }
    const bool removed = session_.marks.forget(where);
    if (!removed) {
        session_.marks.remember(where);
    }
    save_marks();
    say((removed ? "no longer marked: " : "marked: ") + where, false);
}

// WL-FILES-06 -- agents/workshop/files.md
void WorkshopWeave::files_jump_mark(std::int64_t by) {
    ensure_marks();
    const std::vector<MarkedPlace> stops =
        session_.marks.destinations(host_filesystem_roots());
    if (stops.empty()) {
        say("there is nowhere to jump to -- no origin, no marks, and this system "
            "reports no filesystem roots",
            true);
        return;
    }
    const std::int64_t total = static_cast<std::int64_t>(stops.size());
    std::int64_t at = by > 0 ? -1 : 0;
    for (std::int64_t i = 0; i < total; ++i) {
        if (stops[static_cast<std::size_t>(i)].path == session_.panels.files.current_dir) {
            at = i;
            break;
        }
    }
    const std::int64_t to = ((at + by) % total + total) % total;
    const MarkedPlace& went = stops[static_cast<std::size_t>(to)];
    session_.panels.files.current_dir = went.path;
    files_refresh();
    const std::string why = provenance_words(went.from);
    say("at " + went.path + (why.empty() ? std::string() : " (" + why + ")"), false);
}

// WL-FILES-11 -- agents/workshop/files.md; WL-PROJ-05 -- agents/workshop/project.md
void WorkshopWeave::files_use_recipes(loom::Mail& mail) {
    ensure_marks();
    FilesPane& pane = session_.panels.files;
    const FileRow* row = row_at(pane.listing, pane.cursor);
    if (row == nullptr) {
        say("no row is selected -- the recipes in force are unchanged", true);
        return;
    }
    if (!row->openable) {
        say("`" + shown_name(row->name) +
                "` has bytes this Workshop cannot carry in a path -- the recipes in "
                "force are unchanged",
            true);
        return;
    }
    if (row->directory) {
        say("`" + shown_name(row->name) +
                "` is a directory -- a recipe catalog is one authored file",
            true);
        return;
    }
    if (!host_->use_recipes) {
        say("this host cannot change recipe catalogs -- the recipes in force are "
            "unchanged",
            true);
        return;
    }
    const std::string dir = files_dir();
    if (dir.empty()) {
        say("this run began nowhere -- the recipes in force are unchanged", true);
        return;
    }
    const HostContext::RecipeSwap done =
        host_->use_recipes(persist::resolved_against(dir, row->name));
    if (!done.accepted) {
        // BOTH HALVES, IN ONE SENTENCE, AND IN THE ORDER THAT SURVIVES THE CUT. What
        // went wrong and what is still running are both owed here -- a refusal that
        // named only the first would leave a maker guessing whether they had just lost
        // the catalog they were using. The notice row is `detail::fit`-cut at the
        // band's width, so the two SHORT fixed statements go first and the two long
        // variable ones -- the owner's own sentence, then the path -- take the tail in
        // that order. MEASURED (the live witness): with the reason first, the reassuring
        // half was exactly the half that elided.
        say("not a recipe catalog -- the recipes in force are unchanged: " +
                done.refusal + "; still using " +
                (done.path.empty() ? std::string("no catalog") : done.path),
            true);
        return;
    }
    // THE SESSION LEARNS THAT ITS CATALOG HAS MOVED, AND TO WHAT. It is a projection
    // for the screen and never a second owner: no recipe is copied here, and every
    // consumer goes on reading the one owner exactly as before.
    //
    // ⚠ IT IS THE OWNER'S OWN ABSOLUTE PATH, AND IT IS READ BACK RATHER THAN
    // RECOMPOSED. It used to be a spelling relative to the Files root, which was
    // unambiguous only while that root WAS the project -- and a browser that can stand
    // anywhere makes a based spelling with no stated base a wrong-looking name for the
    // right file. `done.path` is what the catalog owner is holding after the attempt,
    // so the projection cannot name a different file from the one in force, and the
    // panel's column fits it with the tail intact (`detail::fit_path`) rather than
    // losing the half that says which catalog this is.
    session_.recipes_moved_to = done.path;
    // ...AND THE BUILDER IS ASKED TO SAY WHAT IT IS, through the same message an
    // opening panel has always sent. The tool reads the owner's views, so it already
    // holds the new catalog; what it has not done is SAY so, and this is the existing
    // sentence for that. No observer, no subscription, no second recipe event.
    (void)mail.send_to_role(zengine::builder::kBuilderRole,
                            zengine::builder::StatusRequested{});
    say("build recipes: " + done.path + " (" + std::to_string(done.recipes) +
            (done.recipes == 1 ? " recipe)" : " recipes)"),
        false);
    repaint(mail);
}

void WorkshopWeave::files_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    switch (session_.keymap.action_for(KeyContext::kFiles, k.scancode, k.modifiers)) {
    case Act::kFilesUp: files_move(-1); break;
    case Act::kFilesDown: files_move(1); break;
    case Act::kFilesOpen: files_open(mail); break;
    case Act::kFilesParent: files_parent(); break;
    case Act::kFilesRefresh:
        files_refresh();
        say("listed " + files_where() + " again", false);
        break;
    case Act::kFilesUseRecipes: files_use_recipes(mail); break;
    case Act::kFilesMark: files_mark(); break;
    case Act::kFilesNextMark: files_jump_mark(1); break;
    case Act::kFilesPreviousMark: files_jump_mark(-1); break;
    default: break; // an unbound key in this pane means nothing, and says nothing
    }
}

// WL-EDIT-10 -- agents/workshop/editor.md
void WorkshopWeave::files_wheel(const zengine::input::PointerWheel& w, const Screen& sc,
                                loom::Mail& mail) {
    if (!over_files_body(session_, sc, w.space, w.x, w.y)) {
        return;
    }
    FilesPane& pane = session_.panels.files;
    if (!pane.listing.known || pane.listing.rows.empty()) {
        return;
    }
    const std::int64_t rows = spend_wheel(pane.wheel_accum, w.dy, kFilesWheelRows);
    if (rows == 0) {
        return;
    }
    const std::size_t was = pane.cursor;
    files_move(-rows);
    if (pane.cursor == was) {
        return; // already at the edge: nothing moved, nothing repaints
    }
    repaint(mail);
}

// WL-PRESS-05 -- agents/workshop/press-chain.md; WL-TAB-09 -- agents/workshop/tab-run.md
bool WorkshopWeave::layouts_press(const zengine::input::PointerButton& b, loom::Mail& mail) {
    const LayoutTabPress tab =
        band_tab_at(session_, screen_of(session_), b.space, b.x, b.y);
    if (!tab.hit) {
        return false;
    }
    if (tab.create) {
        // THE `+` IS THE POINTER'S SPELLING OF `layout.new` AND NOTHING MORE:
        // the same door, the same ceiling, the same refusal in the same words. It arms
        // no double-click and begins no drag -- it is not a tab.
        session_.tab_click = TabClickMemory{};
        new_layout(mail);
        return true;
    }
    // A SECOND PRESS ON THE SAME TAB RENAMES IT, and the first one has already
    // made that tab live -- which is why the editor's subject and the switch cannot
    // disagree. `press_selects_word`'s discipline exactly: the completing press SPENDS
    // the arming, so there is no triple-click, and a first press is an ordinary switch
    // with an arming left beside it.
    const std::int64_t now = interaction_now();
    if (doubles_a_tab_click(session_.tab_click, tab.at, now)) {
        session_.tab_click = TabClickMemory{};
        open_layout_rename(tab.at);
        return true;
    }
    session_.tab_click = TabClickMemory{true, tab.at, now};
    // AND THE PRESS TAKES HOLD OF THE TAB. A press that becomes a drag
    // reorders; a press that does not is exactly the switch it always was, because a
    // drag that never moved lands the layout back where it started. The record holds no
    // position: the switch below has just made this tab the live one, so what is being
    // carried is always `setup.active_at`.
    session_.tab_drag.active = true;
    switch_layout(tab.at, mail);
    return true;
}

// WL-FOCUS-04 -- agents/workshop/focus.md
void WorkshopWeave::files_press(const zengine::input::PointerButton& b, bool had_keyboard,
                                loom::Mail& mail) {
    FilesPane& pane = session_.panels.files;
    if (!pane.listing.known) {
        return; // consumed: a press into a browser with nothing listed is a focus statement
    }
    const Screen sc = screen_of(session_);
    const FilesPressAt at = files_press_at(session_, sc, b.space, b.x, b.y);
    if (!at.named) {
        return; // the header, or the strip below the last row: consumed, still
    }
    const ExternalBodyPlace body = files_body(session_, sc);
    std::size_t which = 0;
    if (!files_row_of_body_row(pane, body.rows, at.row, which)) {
        return; // a marker row or blank space names no entry, and invents none
    }
    if (had_keyboard && which == pane.cursor) {
        files_open(mail);
        return;
    }
    pane.cursor = which;
}

} // namespace zengine::workshop
