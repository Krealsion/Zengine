// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- the source editor, and the Layouts pane's own
// press -- compiled once into `zengine-workshop-logic` and linked by the host and every
// suite; the declarations, the constants and the constexpr functions stay in the header.
//
// THE FILESYSTEM BROWSER USED TO BE HERE and is a weave now (`Zengine/files/`): what it
// spent of this host -- the project root, its marks file, the recipe catalog and the one
// Editor door -- it asks for across the seam instead.
// Workshop law: agents/workshop/editor.md (+4 registers; agents/workshop.md routes)

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

} // namespace zengine::workshop
