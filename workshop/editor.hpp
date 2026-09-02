// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_EDITOR_HPP
#define ZENGINE_WORKSHOP_EDITOR_HPP

// THE SOURCE EDITOR'S OWN MACHINERY: a multiline buffer, the caret and selection in it,
// the source-byte law, and the tab geometry -- everything about editing a source document
// that is not presentation and not file custody.
// Workshop law: agents/workshop/editor.md

#include "component/text_box.hpp" // the word/character helpers and the owner-held Clipboard
#include "property.hpp"           // Written -- the one refusal-with-reason shape here
#include "input/vocabulary.hpp"   // scan/mod names for the editor's own key vocabulary

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

// ---- The editor's fixed policy numbers ---------------------------------------------------

/// THE TAB STOP IS FOUR COLUMNS, FIXED.
// WL-EDIT-08 -- agents/workshop/editor.md
inline constexpr std::int64_t kEditorTabStop = 4;

/// The largest file this editor will open. The document ceiling's own number: a source
/// file a maker edits by hand is far smaller, and a hostile file must not choose the cost.
inline constexpr std::uintmax_t kMaxSourceBytes = 1u << 22; // 4 MiB

/// How many undo steps the editor keeps, and how many bytes of snapshots it will hold.
// WL-EDIT-01 -- agents/workshop/editor.md
inline constexpr std::size_t kEditorUndoDepth = 100;
inline constexpr std::size_t kEditorUndoBudgetBytes = 8u << 20; // 8 MiB of snapshots

/// How many buffer lines one wheel notch scrolls. Three is the desktop convention;
/// fractional high-resolution notches accumulate in the session until they are worth a
/// line (`EditorState::wheel_accum`), so a precise wheel is not rounded to nothing.
inline constexpr std::int64_t kEditorWheelLines = 3;

// ---- The line-ending convention ----------------------------------------------------------

/// A DOCUMENT HAS ONE LINE-ENDING CONVENTION.
// WL-EDIT-07 -- agents/workshop/editor.md
namespace line_ending {
inline constexpr std::int64_t kLF = 0;
inline constexpr std::int64_t kCRLF = 1;
} // namespace line_ending

/// The bytes one line break costs under a convention -- the serializer's one table.
inline const char* line_break_of(std::int64_t convention) noexcept {
    return convention == line_ending::kCRLF ? "\r\n" : "\n";
}

// ---- The source-byte law -----------------------------------------------------------------

/// MAY THIS BYTE SIT INSIDE A LINE OF SOURCE THIS EDITOR HOLDS?
// WL-EDIT-07 -- agents/workshop/editor.md
inline constexpr bool source_byte_ok(unsigned char b) noexcept {
    return b == '\t' || (b >= 0x20u && b < 0x7Fu);
}

/// Is every byte of this insertion something a source line may hold? The door typed text
/// and completion-free inserts go through; a refusal is the caller's sentence.
inline bool source_text_ok(const std::string& text) noexcept {
    for (const char c : text) {
        if (!source_byte_ok(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

// ---- Tab geometry: byte positions <-> displayed columns ----------------------------------
// WL-EDIT-08 -- agents/workshop/editor.md

/// The displayed column immediately after `byte` bytes of this line.
inline std::int64_t visual_col_of(const std::string& line, std::size_t byte) noexcept {
    std::int64_t col = 0;
    const std::size_t end = byte < line.size() ? byte : line.size();
    for (std::size_t i = 0; i < end; ++i) {
        if (line[i] == '\t') {
            col += kEditorTabStop - (col % kEditorTabStop);
        } else {
            ++col;
        }
    }
    return col;
}

/// The whole line's displayed width.
inline std::int64_t visual_len(const std::string& line) noexcept {
    return visual_col_of(line, line.size());
}

/// THE BYTE A DISPLAYED COLUMN NAMES -- `visual_col_of` read backwards.
// WL-EDIT-08 -- agents/workshop/editor.md
inline std::size_t byte_of_visual_col(const std::string& line, std::int64_t col) noexcept {
    if (col <= 0) {
        return 0;
    }
    std::int64_t at = 0; // the displayed column byte `i` begins at
    for (std::size_t i = 0; i < line.size(); ++i) {
        const std::int64_t width =
            line[i] == '\t' ? kEditorTabStop - (at % kEditorTabStop) : 1;
        if (col < at + width) {
            return i; // inside this byte's span: the caret lands before it
        }
        at += width;
        if (col == at) {
            return i + 1; // exactly its right edge: the position after it
        }
    }
    return line.size();
}

/// THE SLICE OF A LINE A VIEWPORT SHOWS, tab-expanded: displayed columns
/// `[first_col, first_col + columns)`, as the exact bytes a region row carries.
// WL-EDIT-08 -- agents/workshop/editor.md
inline std::string expanded_slice(const std::string& line, std::int64_t first_col,
                                  std::int64_t columns) {
    if (columns <= 0) {
        return {};
    }
    std::string out;
    out.reserve(static_cast<std::size_t>(columns));
    const std::int64_t from = first_col > 0 ? first_col : 0;
    const std::int64_t to = from + columns;
    std::int64_t col = 0;
    for (const char c : line) {
        std::int64_t width = 1;
        if (c == '\t') {
            width = kEditorTabStop - (col % kEditorTabStop);
        }
        for (std::int64_t w = 0; w < width; ++w) {
            if (col >= to) {
                return out;
            }
            if (col >= from) {
                out.push_back(c == '\t' ? ' ' : c);
            }
            ++col;
        }
    }
    return out;
}

// ---- Reading and writing source bytes ----------------------------------------------------

/// What admitting a file's bytes produced: the lines and the convention, or the refusal in
/// words -- naming the first line that broke the law, because a maker looking at their own
/// file can fix that.
struct SourceIn {
    Written outcome;
    std::vector<std::string> lines;
    std::int64_t convention = line_ending::kLF;
};

/// SPLIT SOURCE BYTES INTO LINES, OR REFUSE THEM WHOLE.
// WL-EDIT-05, WL-EDIT-07 -- agents/workshop/editor.md
inline SourceIn source_in(const std::string& bytes) {
    SourceIn out;
    bool saw_crlf = false;
    bool saw_lf = false;
    std::vector<std::string> lines;
    std::string line;
    std::size_t at_line = 1;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(bytes[i]);
        if (b == '\r') {
            if (i + 1 < bytes.size() && bytes[i + 1] == '\n') {
                saw_crlf = true;
                lines.push_back(std::move(line));
                line.clear();
                ++at_line;
                ++i;
                continue;
            }
            out.outcome = Written::no("line " + std::to_string(at_line) +
                                      " ends in a bare carriage return, which this editor "
                                      "cannot represent -- the file is untouched");
            return out;
        }
        if (b == '\n') {
            saw_lf = true;
            lines.push_back(std::move(line));
            line.clear();
            ++at_line;
            continue;
        }
        if (!source_byte_ok(b)) {
            const char* what = b >= 0x80u ? "a byte outside plain ASCII"
                                          : "a control byte no canvas can draw";
            const char* hex = "0123456789abcdef";
            std::string code = "0x";
            code.push_back(hex[b >> 4]);
            code.push_back(hex[b & 0xFu]);
            out.outcome = Written::no(
                "line " + std::to_string(at_line) + " holds " + what + " (" + code +
                ") -- this editor cannot edit it truthfully, and the file is untouched");
            return out;
        }
        line.push_back(bytes[i]);
    }
    if (saw_crlf && saw_lf) {
        out.outcome = Written::no(
            "the file mixes CRLF and LF line endings -- one document has one convention "
            "here, and normalizing bytes a maker did not edit is refused; the file is "
            "untouched");
        return out;
    }
    lines.push_back(std::move(line));
    out.outcome = Written::ok();
    out.lines = std::move(lines);
    out.convention = saw_crlf ? line_ending::kCRLF : line_ending::kLF;
    return out;
}

/// THE LINES AS FILE BYTES -- `source_in` read backwards, byte-exact for everything it
/// admitted. A final empty line IS the trailing newline (the representation's identity),
/// so whether a file ends in one is preserved structurally rather than flagged.
inline std::string source_text(const std::vector<std::string>& lines,
                               std::int64_t convention) {
    const char* brk = line_break_of(convention);
    std::string out;
    std::size_t total = 0;
    for (const std::string& l : lines) {
        total += l.size() + 2;
    }
    out.reserve(total);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out += brk;
        }
        out += lines[i];
    }
    return out;
}

/// A CLIPBOARD'S BYTES AS SOURCE LINES, or the honest cannot.
// WL-EDIT-07 -- agents/workshop/editor.md
struct PasteableSource {
    bool representable = true;
    std::vector<std::string> lines;
};

inline PasteableSource pasteable_source(const std::string& text) {
    PasteableSource out;
    std::string line;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(text[i]);
        if (b >= 0x80u) {
            out.representable = false;
            out.lines.clear();
            return out;
        }
        if (b == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;
            }
            out.lines.push_back(std::move(line));
            line.clear();
            continue;
        }
        if (b == '\n') {
            out.lines.push_back(std::move(line));
            line.clear();
            continue;
        }
        if (source_byte_ok(b)) {
            line.push_back(text[i]);
        } else {
            line.push_back(' ');
        }
    }
    out.lines.push_back(std::move(line));
    return out;
}

// ---- The editor's own key vocabulary, declared beside the switch ------------------------
// WL-EDIT-02 -- agents/workshop/editor.md

inline constexpr component::EditingGesture kEditorVocabulary[] = {
    {input::scan::kC, input::mod::kCtrl, "copy"},
    {input::scan::kX, input::mod::kCtrl, "cut"},
    {input::scan::kV, input::mod::kCtrl, "paste"},
    {input::scan::kA, input::mod::kCtrl, "select all"},
    {input::scan::kZ, input::mod::kCtrl, "undo"},
    {input::scan::kZ, input::mod::kCtrl | input::mod::kShift, "redo"},
    {input::scan::kY, input::mod::kCtrl, "redo"},
    {input::scan::kLeft, input::mod::kNone, "left"},
    {input::scan::kRight, input::mod::kNone, "right"},
    {input::scan::kUp, input::mod::kNone, "up"},
    {input::scan::kDown, input::mod::kNone, "down"},
    {input::scan::kLeft, input::mod::kShift, "select left"},
    {input::scan::kRight, input::mod::kShift, "select right"},
    {input::scan::kUp, input::mod::kShift, "select up"},
    {input::scan::kDown, input::mod::kShift, "select down"},
    {input::scan::kHome, input::mod::kNone, "line start"},
    {input::scan::kEnd, input::mod::kNone, "line end"},
    {input::scan::kHome, input::mod::kShift, "select to line start"},
    {input::scan::kEnd, input::mod::kShift, "select to line end"},
    {input::scan::kHome, input::mod::kCtrl, "document start"},
    {input::scan::kEnd, input::mod::kCtrl, "document end"},
    {input::scan::kHome, input::mod::kCtrl | input::mod::kShift, "select to document start"},
    {input::scan::kEnd, input::mod::kCtrl | input::mod::kShift, "select to document end"},
    {input::scan::kLeft, input::mod::kCtrl, "word left"},
    {input::scan::kRight, input::mod::kCtrl, "word right"},
    {input::scan::kLeft, input::mod::kCtrl | input::mod::kShift, "select word left"},
    {input::scan::kRight, input::mod::kCtrl | input::mod::kShift, "select word right"},
    {input::scan::kBackspace, input::mod::kNone, "erase left"},
    {input::scan::kDelete, input::mod::kNone, "erase right"},
    {input::scan::kBackspace, input::mod::kCtrl, "erase word left"},
    {input::scan::kDelete, input::mod::kCtrl, "erase word right"},
    // -- the transparent spellings: same meanings, with a modifier the vocabulary
    //    deliberately ignores on these keys (the component's own rule) -----------------
    {input::scan::kBackspace, input::mod::kShift, "erase left"},
    {input::scan::kDelete, input::mod::kShift, "erase right"},
    {input::scan::kBackspace, input::mod::kCtrl | input::mod::kShift, "erase word left"},
    {input::scan::kDelete, input::mod::kCtrl | input::mod::kShift, "erase word right"},
};

inline constexpr std::size_t kEditorVocabularyCount =
    sizeof(kEditorVocabulary) / sizeof(kEditorVocabulary[0]);

// ---- The buffer --------------------------------------------------------------------------

/// A POSITION IN THE DOCUMENT: a line, and a byte of that line. Ordered reading-first so a
/// selection's two ends normalize by one comparison.
struct EditorPos {
    std::size_t row = 0;
    std::size_t byte = 0;

    friend constexpr bool operator==(const EditorPos& a, const EditorPos& b) noexcept {
        return a.row == b.row && a.byte == b.byte;
    }
    friend constexpr bool operator<(const EditorPos& a, const EditorPos& b) noexcept {
        return a.row != b.row ? a.row < b.row : a.byte < b.byte;
    }
};

/// THE MULTILINE BUFFER, ITS CARET, ITS SELECTION AND ITS HISTORY AS ONE STATE.
// WL-EDIT-01, WL-EDIT-02, WL-EDIT-11 -- agents/workshop/editor.md
class EditorBuffer {
public:
    EditorBuffer() : lines_(1) {}

    const std::vector<std::string>& lines() const noexcept { return lines_; }
    std::size_t line_count() const noexcept { return lines_.size(); }
    const std::string& line(std::size_t row) const noexcept {
        return lines_[row < lines_.size() ? row : lines_.size() - 1];
    }
    std::size_t caret_row() const noexcept { return caret_.row; }
    std::size_t caret_byte() const noexcept { return caret_.byte; }
    std::size_t anchor_row() const noexcept { return anchor_.row; }
    std::size_t anchor_byte() const noexcept { return anchor_.byte; }
    std::uint64_t revision() const noexcept { return revision_; }

    bool has_selection() const noexcept { return !(anchor_ == caret_); }
    EditorPos selection_begin() const noexcept { return caret_ < anchor_ ? caret_ : anchor_; }
    EditorPos selection_end() const noexcept { return caret_ < anchor_ ? anchor_ : caret_; }

    /// The selected bytes, lines joined by LF -- the in-process clipboard convention every
    /// text holder here mirrors; the document's own CRLF is a FILE fact and is spent at
    /// save, not inside a copy.
    std::string selected_text() const {
        if (!has_selection()) {
            return {};
        }
        const EditorPos from = selection_begin();
        const EditorPos to = selection_end();
        if (from.row == to.row) {
            return lines_[from.row].substr(from.byte, to.byte - from.byte);
        }
        std::string out = lines_[from.row].substr(from.byte);
        for (std::size_t r = from.row + 1; r < to.row; ++r) {
            out += '\n';
            out += lines_[r];
        }
        out += '\n';
        out += lines_[to.row].substr(0, to.byte);
        return out;
    }

    // ---- Whole-document doors -----------------------------------------------------------

    /// REPLACE THE WHOLE DOCUMENT -- how one is opened or replaced.
    // WL-EDIT-02, WL-EDIT-11 -- agents/workshop/editor.md
    void set_lines(std::vector<std::string> lines) {
        lines_ = std::move(lines);
        if (lines_.empty()) {
            lines_.emplace_back();
        }
        caret_ = EditorPos{};
        anchor_ = EditorPos{};
        preferred_ = -1;
        undo_.clear();
        redo_.clear();
        undo_bytes_ = 0;
        last_edit_ = EditKind::kNone;
        ++revision_;
    }

    /// PUT THE DOCUMENT BACK TO THESE LINES AS ONE ORDINARY STRUCTURAL EDIT.
    // WL-EDIT-03 -- agents/workshop/editor.md
    void revert_to(const std::vector<std::string>& lines) {
        remember(EditKind::kStructural);
        lines_ = lines;
        if (lines_.empty()) {
            lines_.emplace_back();
        }
        anchor_ = caret_;
        settle();
    }

    // ---- Insertion and structural editing ----------------------------------------------

    /// Insert admitted line bytes at the caret, replacing the selection. The TEXT is the
    /// caller's to have judged (`source_text_ok`) -- this is mechanics, and a refusal is
    /// policy with a sentence, which mechanics cannot speak.
    void type(const std::string& text) {
        if (text.empty()) {
            return;
        }
        if (has_selection()) {
            remember(EditKind::kStructural);
            erase_selection_bytes();
        } else {
            remember(EditKind::kTyping);
        }
        lines_[caret_.row].insert(caret_.byte, text);
        caret_.byte += text.size();
        anchor_ = caret_;
        settle();
    }

    /// Split the caret's line -- Return's meaning, said by the editor's policy layer.
    void newline() {
        remember(EditKind::kStructural);
        if (has_selection()) {
            erase_selection_bytes();
        }
        const std::string tail = lines_[caret_.row].substr(caret_.byte);
        lines_[caret_.row].resize(caret_.byte);
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(caret_.row) + 1, tail);
        caret_ = EditorPos{caret_.row + 1, 0};
        anchor_ = caret_;
        settle();
    }

    /// Erase one position leftward: the character before the caret, or -- at the start of
    /// a line -- the line break, joining with the line above. A selection is erased whole
    /// instead, every erase's precedence.
    void backspace() {
        if (has_selection()) {
            remember(EditKind::kStructural);
            erase_selection_bytes();
            settle();
            return;
        }
        if (caret_.byte > 0) {
            remember(EditKind::kEraseBack);
            const std::size_t from =
                component::character_before(lines_[caret_.row], caret_.byte);
            lines_[caret_.row].erase(from, caret_.byte - from);
            caret_.byte = from;
        } else if (caret_.row > 0) {
            remember(EditKind::kStructural); // a join is one gesture, one entry
            const std::size_t landing = lines_[caret_.row - 1].size();
            lines_[caret_.row - 1] += lines_[caret_.row];
            lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(caret_.row));
            caret_ = EditorPos{caret_.row - 1, landing};
        } else {
            return; // the document's start: nothing leftward exists
        }
        anchor_ = caret_;
        settle();
    }

    /// Erase one position rightward: the character at the caret, or -- at the end of a
    /// line -- the line break, joining with the line below. The caret does not move.
    void erase_forward() {
        if (has_selection()) {
            remember(EditKind::kStructural);
            erase_selection_bytes();
            settle();
            return;
        }
        if (caret_.byte < lines_[caret_.row].size()) {
            remember(EditKind::kEraseForward);
            const std::size_t to =
                component::character_after(lines_[caret_.row], caret_.byte);
            lines_[caret_.row].erase(caret_.byte, to - caret_.byte);
        } else if (caret_.row + 1 < lines_.size()) {
            remember(EditKind::kStructural);
            lines_[caret_.row] += lines_[caret_.row + 1];
            lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(caret_.row) + 1);
        } else {
            return; // the document's end
        }
        anchor_ = caret_;
        settle();
    }

    /// The word-grain erases: to the start of the word before, or the start of the word
    /// after. At a line's edge they mean the join, exactly as the character erases do --
    /// a boundary is not a place where erasing stops meaning anything.
    void erase_word_before() {
        if (has_selection() || caret_.byte == 0) {
            backspace(); // the selection rule and the join are both already backspace's
            return;
        }
        remember(EditKind::kStructural);
        const std::size_t from = component::word_before(lines_[caret_.row], caret_.byte);
        lines_[caret_.row].erase(from, caret_.byte - from);
        caret_.byte = from;
        anchor_ = caret_;
        settle();
    }

    void erase_word_after() {
        if (has_selection() || caret_.byte >= lines_[caret_.row].size()) {
            erase_forward();
            return;
        }
        remember(EditKind::kStructural);
        const std::size_t to = component::word_after(lines_[caret_.row], caret_.byte);
        lines_[caret_.row].erase(caret_.byte, to - caret_.byte);
        anchor_ = caret_;
        settle();
    }

    // ---- Movement. Plain movement COLLAPSES a selection; select_ variants EXTEND one. --
    // WL-EDIT-02 -- agents/workshop/editor.md

    void left() noexcept {
        if (has_selection()) {
            caret_ = selection_begin();
        } else {
            caret_ = position_left_of(caret_);
        }
        collapse_after_move();
    }
    void right() noexcept {
        if (has_selection()) {
            caret_ = selection_end();
        } else {
            caret_ = position_right_of(caret_);
        }
        collapse_after_move();
    }
    void home() noexcept {
        caret_.byte = 0;
        collapse_after_move();
    }
    void end() noexcept {
        caret_.byte = lines_[caret_.row].size();
        collapse_after_move();
    }
    void document_home() noexcept {
        caret_ = EditorPos{};
        collapse_after_move();
    }
    void document_end() noexcept {
        caret_ = EditorPos{lines_.size() - 1, lines_.back().size()};
        collapse_after_move();
    }
    void word_left() noexcept {
        caret_ = word_left_of(caret_);
        collapse_after_move();
    }
    void word_right() noexcept {
        caret_ = word_right_of(caret_);
        collapse_after_move();
    }
    void up() noexcept {
        vertical(-1, /*extend=*/false);
    }
    void down() noexcept {
        vertical(+1, /*extend=*/false);
    }

    void select_left() noexcept {
        caret_ = position_left_of(caret_);
        extend_after_move();
    }
    void select_right() noexcept {
        caret_ = position_right_of(caret_);
        extend_after_move();
    }
    void select_home() noexcept {
        caret_.byte = 0;
        extend_after_move();
    }
    void select_end() noexcept {
        caret_.byte = lines_[caret_.row].size();
        extend_after_move();
    }
    void select_document_home() noexcept {
        caret_ = EditorPos{};
        extend_after_move();
    }
    void select_document_end() noexcept {
        caret_ = EditorPos{lines_.size() - 1, lines_.back().size()};
        extend_after_move();
    }
    void select_word_left() noexcept {
        caret_ = word_left_of(caret_);
        extend_after_move();
    }
    void select_word_right() noexcept {
        caret_ = word_right_of(caret_);
        extend_after_move();
    }
    void select_up() noexcept { vertical(-1, /*extend=*/true); }
    void select_down() noexcept { vertical(+1, /*extend=*/true); }

    /// Select everything, caret at the end -- where the next keystroke belongs.
    void select_all() noexcept {
        anchor_ = EditorPos{};
        caret_ = EditorPos{lines_.size() - 1, lines_.back().size()};
        preferred_ = -1;
        last_edit_ = EditKind::kNone;
        ++revision_;
        settle();
    }

    /// PUT THE CARET WHERE A PRESS RESOLVED -- clamped into the document, collapsing any
    /// selection (a press is the gesture that STARTS one; the drag extends from here).
    void place(std::size_t row, std::size_t byte) noexcept {
        caret_.row = row < lines_.size() ? row : lines_.size() - 1;
        const std::size_t most = lines_[caret_.row].size();
        caret_.byte = byte < most ? byte : most;
        collapse_after_move();
    }

    /// EXTEND THE SELECTION TO WHERE A DRAG REACHED, in the row the motion resolved and
    /// the DISPLAYED column it swept to. A drag that has left the viewport still means
    /// something: the caller clamps the row into what is visible and steps it one row
    /// past the edge per motion (the component's leftward-step law, turned vertical), and
    /// a negative column steps one position leftward per motion for the same reason.
    void drag_to(std::size_t row, std::int64_t visual_col) noexcept {
        const std::size_t r = row < lines_.size() ? row : lines_.size() - 1;
        if (visual_col < 0) {
            caret_ = position_left_of(EditorPos{r, byte_of_visual_col(lines_[r], 0)});
        } else {
            caret_ = EditorPos{r, byte_of_visual_col(lines_[r], visual_col)};
        }
        preferred_ = -1;
        last_edit_ = EditKind::kNone;
        ++revision_;
        settle();
    }

    // ---- Clipboard (over the owner-held Clipboard, the component's custody split) ------

    /// Copy the selection. With nothing selected, nothing happens -- and the gesture is
    /// still the vocabulary's (`consume`).
    void copy(component::Clipboard& clip) const {
        if (!has_selection()) {
            return;
        }
        clip.text = selected_text();
        ++clip.writes;
    }

    /// Cut: copy, then erase. One gesture, one undo entry.
    void cut(component::Clipboard& clip) {
        if (!has_selection()) {
            return;
        }
        copy(clip);
        remember(EditKind::kStructural);
        erase_selection_bytes();
        settle();
    }

    /// PASTE ADMITTED SOURCE LINES at the caret, replacing the selection.
    // WL-EDIT-11 -- agents/workshop/editor.md
    void paste_lines(const std::vector<std::string>& add) {
        if (add.empty() || (add.size() == 1 && add.front().empty() && !has_selection())) {
            return; // nothing to insert and nothing to replace: nothing happened
        }
        remember(EditKind::kStructural);
        if (has_selection()) {
            erase_selection_bytes();
        }
        const std::string tail = lines_[caret_.row].substr(caret_.byte);
        lines_[caret_.row].resize(caret_.byte);
        lines_[caret_.row] += add.front();
        for (std::size_t i = 1; i < add.size(); ++i) {
            lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(caret_.row) +
                              static_cast<std::ptrdiff_t>(i),
                          add[i]);
        }
        caret_.row += add.size() - 1;
        caret_.byte = lines_[caret_.row].size();
        lines_[caret_.row] += tail;
        anchor_ = caret_;
        settle();
    }

    // ---- History ------------------------------------------------------------------------

    bool undo() {
        if (undo_.empty()) {
            return false;
        }
        push_redo(Memory{lines_, caret_, anchor_});
        lines_ = std::move(undo_.back().lines);
        caret_ = undo_.back().caret;
        anchor_ = undo_.back().anchor;
        undo_bytes_ -= undo_.back().bytes();
        undo_.pop_back();
        last_edit_ = EditKind::kNone;
        preferred_ = -1;
        settle();
        return true;
    }

    bool redo() {
        if (redo_.empty()) {
            return false;
        }
        push_undo(Memory{lines_, caret_, anchor_});
        lines_ = std::move(redo_.back().lines);
        caret_ = redo_.back().caret;
        anchor_ = redo_.back().anchor;
        redo_.pop_back();
        last_edit_ = EditKind::kNone;
        preferred_ = -1;
        settle();
        return true;
    }

    bool can_undo() const noexcept { return !undo_.empty(); }
    bool can_redo() const noexcept { return !redo_.empty(); }

    // ---- The editing-key vocabulary -----------------------------------------------------

    /// SPEND ONE KEY TRANSITION ON THIS BUFFER, OR SAY IT IS NOT MINE.
    // WL-EDIT-02 -- agents/workshop/editor.md
    bool consume(std::int64_t scancode, std::int64_t modifiers, component::Clipboard& clip) {
        namespace scan = input::scan;
        namespace mod = input::mod;
        if ((modifiers & (mod::kAlt | mod::kSuper)) != 0) {
            return false;
        }
        const bool shift = (modifiers & mod::kShift) != 0;
        if ((modifiers & mod::kCtrl) == 0) {
            switch (scancode) {
            case scan::kBackspace: backspace(); return true;
            case scan::kDelete: erase_forward(); return true;
            case scan::kLeft: shift ? select_left() : left(); return true;
            case scan::kRight: shift ? select_right() : right(); return true;
            case scan::kUp: shift ? select_up() : up(); return true;
            case scan::kDown: shift ? select_down() : down(); return true;
            case scan::kHome: shift ? select_home() : home(); return true;
            case scan::kEnd: shift ? select_end() : end(); return true;
            default: return false;
            }
        }
        switch (scancode) {
        case scan::kLeft: shift ? select_word_left() : word_left(); return true;
        case scan::kRight: shift ? select_word_right() : word_right(); return true;
        case scan::kHome: shift ? select_document_home() : document_home(); return true;
        case scan::kEnd: shift ? select_document_end() : document_end(); return true;
        case scan::kBackspace: erase_word_before(); return true;
        case scan::kDelete: erase_word_after(); return true;
        case scan::kA: if (shift) { return false; } select_all(); return true;
        case scan::kC: if (shift) { return false; } copy(clip); return true;
        case scan::kX: if (shift) { return false; } cut(clip); return true;
        case scan::kV:
            // A REQUEST, NOT A PASTE: the value a paste means is the clipboard's
            // CURRENT one, and only the owner can obtain it -- and here also judge it
            // against the source-byte law before any line moves.
            if (shift) { return false; }
            ++clip.paste_requests;
            return true;
        case scan::kZ:
            if (shift) {
                (void)redo();
            } else {
                (void)undo();
            }
            return true;
        case scan::kY: if (shift) { return false; } (void)redo(); return true;
        default: return false;
        }
    }

private:
    enum class EditKind : std::uint8_t {
        kNone,
        kTyping,
        kEraseBack,
        kEraseForward,
        kStructural,
    };

    /// One remembered state -- the whole document, its caret and its anchor. The
    /// viewport is deliberately not in it (presentation), and neither is the preferred
    /// column (a run of vertical movement is not an edit).
    struct Memory {
        std::vector<std::string> lines;
        EditorPos caret;
        EditorPos anchor;

        std::size_t bytes() const noexcept {
            std::size_t n = 0;
            for (const std::string& l : lines) {
                n += l.size();
            }
            return n;
        }
    };

    void collapse_after_move() noexcept {
        anchor_ = caret_;
        preferred_ = -1;
        last_edit_ = EditKind::kNone;
        ++revision_;
        settle();
    }

    void extend_after_move() noexcept {
        preferred_ = -1;
        last_edit_ = EditKind::kNone;
        ++revision_;
        settle();
    }

    /// One vertical step, re-aimed at the run's preferred DISPLAYED column. The first
    /// step of a run remembers the column the caret was at; every step lands as close to
    /// it as the landing line allows, and the memory survives short lines -- which is the
    /// whole reason it exists.
    void vertical(std::int64_t by, bool extend) noexcept {
        if (preferred_ < 0) {
            preferred_ = visual_col_of(lines_[caret_.row], caret_.byte);
        }
        if (by < 0) {
            if (caret_.row == 0) {
                caret_.byte = 0; // up from the first line: the document's start
            } else {
                caret_.row -= 1;
                caret_.byte = byte_of_visual_col(lines_[caret_.row], preferred_);
            }
        } else {
            if (caret_.row + 1 >= lines_.size()) {
                caret_.byte = lines_[caret_.row].size(); // down from the last: its end
            } else {
                caret_.row += 1;
                caret_.byte = byte_of_visual_col(lines_[caret_.row], preferred_);
            }
        }
        if (!extend) {
            anchor_ = caret_;
        }
        last_edit_ = EditKind::kNone;
        ++revision_;
        settle();
    }

    EditorPos position_left_of(const EditorPos& p) const noexcept {
        if (p.byte > 0) {
            return EditorPos{p.row, component::character_before(lines_[p.row], p.byte)};
        }
        if (p.row > 0) {
            return EditorPos{p.row - 1, lines_[p.row - 1].size()};
        }
        return p;
    }

    EditorPos position_right_of(const EditorPos& p) const noexcept {
        if (p.byte < lines_[p.row].size()) {
            return EditorPos{p.row, component::character_after(lines_[p.row], p.byte)};
        }
        if (p.row + 1 < lines_.size()) {
            return EditorPos{p.row + 1, 0};
        }
        return p;
    }

    /// The word steps, crossing line boundaries the way the character steps do: a word
    /// gesture at a line's edge crosses to the neighbouring line's edge, and the next
    /// press finds the word there. The in-line halves are the component's own helpers.
    EditorPos word_left_of(const EditorPos& p) const noexcept {
        if (p.byte == 0) {
            return position_left_of(p);
        }
        return EditorPos{p.row, component::word_before(lines_[p.row], p.byte)};
    }

    EditorPos word_right_of(const EditorPos& p) const noexcept {
        if (p.byte >= lines_[p.row].size()) {
            return position_right_of(p);
        }
        return EditorPos{p.row, component::word_after(lines_[p.row], p.byte)};
    }

    /// CALLED BEFORE EVERY MUTATION, with the pre-state still current -- the component's
    /// grouping rule, and its redo rule: an edit after an undo makes the undone future a
    /// road not taken.
    void remember(EditKind kind) {
        if (kind != last_edit_ || kind == EditKind::kStructural) {
            push_undo(Memory{lines_, caret_, anchor_});
        }
        redo_.clear();
        last_edit_ = kind;
        preferred_ = -1;
        ++revision_;
    }

    /// The two bounds, applied at the one growth door: depth first, then the byte
    /// budget, evicting oldest-first and always keeping the newest entry -- a bounded
    /// history's honest failure mode is forgetting the far past, never refusing the
    /// present.
    void push_undo(Memory m) {
        undo_bytes_ += m.bytes();
        undo_.push_back(std::move(m));
        while (undo_.size() > kEditorUndoDepth ||
               (undo_bytes_ > kEditorUndoBudgetBytes && undo_.size() > 1)) {
            undo_bytes_ -= undo_.front().bytes();
            undo_.erase(undo_.begin());
        }
    }

    void push_redo(Memory m) { redo_.push_back(std::move(m)); }

    /// Erase the selected span and collapse onto where it was. Callers `remember` first.
    void erase_selection_bytes() {
        const EditorPos from = selection_begin();
        const EditorPos to = selection_end();
        if (from.row == to.row) {
            lines_[from.row].erase(from.byte, to.byte - from.byte);
        } else {
            lines_[from.row] =
                lines_[from.row].substr(0, from.byte) + lines_[to.row].substr(to.byte);
            lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(from.row) + 1,
                         lines_.begin() + static_cast<std::ptrdiff_t>(to.row) + 1);
        }
        caret_ = from;
        anchor_ = from;
    }

    /// The invariant's keeper: at least one line, caret and anchor inside the document.
    /// Every operation ends with it, including the ones that cannot break it.
    void settle() noexcept {
        if (lines_.empty()) {
            lines_.emplace_back();
        }
        if (caret_.row >= lines_.size()) {
            caret_.row = lines_.size() - 1;
        }
        if (caret_.byte > lines_[caret_.row].size()) {
            caret_.byte = lines_[caret_.row].size();
        }
        if (anchor_.row >= lines_.size()) {
            anchor_.row = lines_.size() - 1;
        }
        if (anchor_.byte > lines_[anchor_.row].size()) {
            anchor_.byte = lines_[anchor_.row].size();
        }
    }

    std::vector<std::string> lines_;
    EditorPos caret_;
    EditorPos anchor_;
    std::int64_t preferred_ = -1; ///< the vertical run's displayed column; -1 = none
    std::vector<Memory> undo_;
    std::vector<Memory> redo_;
    std::size_t undo_bytes_ = 0;
    EditKind last_edit_ = EditKind::kNone;
    std::uint64_t revision_ = 0;
};

// ---- The document layer ------------------------------------------------------------------

/// THE SOURCE DOCUMENT WORKSHOP IS EDITING: its identity, its buffer, its saved copy, its
/// byte conventions, and its viewport.
// WL-EDIT-01, WL-EDIT-03, WL-EDIT-05, WL-EDIT-11 -- agents/workshop/editor.md
struct EditorState {
    /// The source identity: one NORMALIZED spelling, produced by the open door.
    // WL-EDIT-06 -- agents/workshop/editor.md
    std::string path;
    EditorBuffer buffer;
    std::vector<std::string> saved_lines{std::string()}; ///< the content as last loaded/saved
    std::int64_t convention = line_ending::kLF;
    std::uint64_t doc_epoch = 0;

    // The viewport: which part of the document the pane is showing. Presentation state
    // that lives beside the buffer because there is exactly one view (the component's own
    // `first_visible` argument, one dimension up); `first_col` is DISPLAYED columns.
    std::size_t first_row = 0;
    std::int64_t first_col = 0;
    double wheel_accum = 0.0; ///< fractional wheel notches not yet worth a line
    /// Whether the next reconcile should bring the caret into view. Set by every gesture
    /// that moves the caret or edits; deliberately NOT set by the wheel, whose whole
    /// meaning is to look elsewhere while the caret stays put.
    bool follow_caret = false;
    /// The body the viewport was last reconciled against, so a reconcile can tell a
    /// resize from a repaint: a changed room re-follows the caret (SC-form: the caret
    /// stays visible under resize), an unchanged one leaves a wheel-scrolled view alone.
    std::int64_t last_rows = 0;
    std::int64_t last_cols = 0;

    bool open_document() const noexcept { return !path.empty(); }
    bool dirty() const noexcept {
        return open_document() && buffer.lines() != saved_lines;
    }
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_EDITOR_HPP
