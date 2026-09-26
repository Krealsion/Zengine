// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Files tool: a loadable weave that offers Workshop one pane -- a browser over the machine,
// the marks a maker keeps, and the two authored files a maker writes from it. What it needs of
// the host (where this run began, where its marks live, whether a file is a recipe catalog, one
// path opened in the Editor) it asks for through the doors `workshop/pane_seam_vocabulary.hpp`
// spells (WL-FILES-17); what crosses is values. It owns its listing, its marks and its two
// modes, and spends the pure half (`files.hpp`, `marks.hpp`, `marks_persist.hpp`).
// Files law: agents/workshop/files.md

#include "files/vocabulary.hpp"

#include "files/files.hpp"
#include "files/filesystem_roots.hpp"
#include "workshop/open_seam_vocabulary.hpp" // the opening office the open is asked of
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "files/marks_persist.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/path_admission.hpp"
#include "workshop/persist.hpp"

#include "workshop/pane_menu.hpp"

#include "activation/activation.hpp"
#include "builder/vocabulary.hpp"
#include "component/control_strip.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace component = zengine::component;
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace files = zengine::files;
namespace pane_menu = zengine::workshop::pane_menu;

using ws::AdmittedName;
using ws::FileRow;
using ws::Listing;
using ws::LocationMarks;
using ws::MarkedPlace;
using ws::OpenSourceRequested;
using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneButton;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneKey;
using ws::PaneManageRequested;
using ws::PaneMenuAnswered;
using ws::PaneMenuRequested;
using ws::v2::PaneOffered;
using ws::PanePassRequested;
using ws::PanePressed;
using ws::PaneRoom;
using ws::PaneTextInput;
using ws::PaneWheel;
using ws::ProjectRoot;
using ws::ProjectRootRequested;
using ws::RecipeAuthorRequested;
using ws::RecipeOutcome;
using ws::RecipeUseRequested;
using ws::SourceOpened;

/// The office Workshop holds, named as a string rather than through `workshop/panel.hpp`: a
/// provider is a stranger to Workshop's internals.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// How many rows the wheel is worth per notch in a cursor-windowed list.
constexpr std::int64_t kFilesWheelRows = 3;

// ---- Small pure helpers the recipe chooser needs ------------------------------------------

std::string trimmed(const std::string& text) {
    std::size_t b = 0;
    std::size_t e = text.size();
    while (b < e && text[b] == ' ') {
        ++b;
    }
    while (e > b && text[e - 1] == ' ') {
        --e;
    }
    return text.substr(b, e - b);
}

std::vector<std::string> split_list(const std::string& text) {
    std::vector<std::string> out;
    std::string item;
    for (const char c : text) {
        if (c == ',') {
            const std::string t = trimmed(item);
            if (!t.empty()) {
                out.push_back(t);
            }
            item.clear();
        } else {
            item.push_back(c);
        }
    }
    const std::string t = trimmed(item);
    if (!t.empty()) {
        out.push_back(t);
    }
    return out;
}

bool source_name(const std::string& name) {
    for (const char* suffix : {".cpp", ".cc", ".cxx"}) {
        const std::string s(suffix);
        if (name.size() > s.size() && name.compare(name.size() - s.size(), s.size(), s) == 0) {
            return true;
        }
    }
    return false;
}

std::string stem_of(const std::string& name) {
    const std::size_t dot = name.rfind('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

/// The shared pane text helpers (`workshop/pane_text.hpp`); `judge_content` is why the fit must
/// be applied at all.
using zengine::workshop::pane_text::ascii_spelling;
using zengine::workshop::pane_text::fit;
using zengine::workshop::pane_text::fitted_label;


/// One directory row's text.
std::string row_text(const FileRow& row) {
    std::string out = ws::shown_name(row.name);
    if (row.directory) {
        out += "/";
        if (row.linked) {
            out += "  (link)";
        }
    }
    if (!row.openable) {
        out += "  (name this Workshop cannot open)";
    }
    return out;
}

/// A build candidate the chooser holds: a place, and which of the two recipe kinds it may be
/// tried as. Local: it is the pane's mode state and crosses no wire.
struct BuildCandidate {
    std::string name;
    bool tree = false;
    bool multi_config = false;
};

/// Whether a configured CMake build tree needs `cmake --build --config` to say which
/// configuration it means: a multi-config generator writes `CMAKE_CONFIGURATION_TYPES` into its
/// cache, a single-config one never does. Read, never invoked; this package configures nothing.
bool cache_is_multi_config(const std::filesystem::path& cache_file) {
    std::ifstream in(cache_file);
    if (!in) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("CMAKE_CONFIGURATION_TYPES:", 0) == 0) {
            const std::size_t eq = line.find('=');
            return eq != std::string::npos && eq + 1 < line.size();
        }
    }
    return false;
}

/// How a menu's subject carries two facts across the seam: `PaneMenuAnswered` echoes one string,
/// and an answer must re-establish both the place the menu was opened in and its row. A unit
/// separator is a byte no admitted path or filename carries (`admit_filename` refuses everything
/// under 0x20), so the join is unambiguous; the desktop spells the same join, as a stranger.
constexpr char kSubjectSep = '\x1f';

std::string join_subject(const std::string& place, const std::string& row) {
    return place + kSubjectSep + row;
}

std::vector<std::string> split_subject(const std::string& subject) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (true) {
        const std::size_t sep = subject.find(kSubjectSep, at);
        out.push_back(subject.substr(at, sep == std::string::npos ? std::string::npos : sep - at));
        if (sep == std::string::npos) {
            return out;
        }
        at = sep + 1;
    }
}

/// The sentence for a press that named a picture this pane has since replaced: a press is aimed
/// at what a maker could see, so rows that moved between the aim and the delivery are said,
/// never spent on whatever slid into that place.
constexpr const char* kMovedSentence = "the rows moved -- press again";

/// HOW MANY ROWS OF ITS OWN THE CONTROL STRIP MAY SPEND. Three is what the widest strip needs
/// at the narrowest room a maker works in and still leaves the listing its own rows; past that
/// the rest of the controls are the pane menu's, which is what `[menu]` is first for.
constexpr std::int64_t kMaxControlRows = 3;

/// The floor the active field's value never gives up, however long its label: in a thirty-column
/// body `package prefix (comma-separated)> ` alone fills the room, and typing was accepted and
/// never shown. Twelve columns is enough to read what was typed, not a promise nothing scrolls.
constexpr std::int64_t kMinFieldValueColumns = 12;

// ---- What a published row, or a run of columns inside one, MEANS (component::RowMap) -------

namespace files_row {
/// Kinds a row can carry. `kNone` is a row that names nothing a press may spend -- the header,
/// a marker, a sentence, the field a mode is only showing.
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kEntry = 1;     ///< a listing entry; `index` into `listing_.rows`
inline constexpr std::int64_t kCandidate = 2; ///< a chooser candidate; `index` into them
inline constexpr std::int64_t kField = 3;     ///< one authoring field; `index` is its step
inline constexpr std::int64_t kLine = 4;      ///< the authoring line itself (the caret's row)
inline constexpr std::int64_t kControl = 5;   ///< a labelled control; `id` is its operation
} // namespace files_row

/// One meaning, and the subject it was painted about. The subject is part of the meaning, which
/// keeps the picture number honest for a control: `[use as recipes]` is about whatever the cursor
/// is on, and moving the cursor moves no row, so without the subject a queued press would spend
/// the control on a file nobody aimed at. The handler checks the subject again on arrival.
struct FilesMeaning {
    std::int64_t kind = files_row::kNone;
    std::size_t index = 0;
    std::string id;      ///< a control's operation id; empty for the other kinds
    std::string subject; ///< what the row or control was painted ABOUT

    bool operator==(const FilesMeaning& o) const {
        return kind == o.kind && index == o.index && id == o.id && subject == o.subject;
    }
};

// =============================================================================
// The weave
// =============================================================================

class FilesWeave
    : public loom::WeaveBase<
          FilesWeave, files::FilesState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed,
                       ws::v2::PanePressed, ws::v3::PanePressed, PaneKey, PaneTextInput,
                       PaneWheel, PaneButton, PaneMenuAnswered, PaneActionRequested, ProjectRoot,
                       RecipeOutcome, SourceOpened, loom::DispatchRefused,
                       zengine::builder::BuildStatus, surface::ClipboardCopy,
                       surface::ClipboardText>,
          loom::Emit<PaneOffered, PaneActions, ws::v3::PaneContent, ProjectRootRequested,
                     RecipeUseRequested, RecipeAuthorRequested, OpenSourceRequested,
                     zengine::builder::StatusRequested, PaneMenuRequested, PanePassRequested,
                     PaneManageRequested, ws::PaneKeyboardRequested, surface::ClipboardCopy,
                     surface::ClipboardTextRequested>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        announce(mail);
    }

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        announce(mail);
    }

    /// Workshop grants the pane its room: the one beat on which this tool draws. The listing is
    /// re-enumerated here (WL-FILES-12); with no origin yet, the grant is when the browser first
    /// asks the host where it began. A grant is not a maker's act, so it keeps the cursor: a room
    /// moves for resizes, dragged edges and a title row returned right behind a selecting press.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != files::kProjectFilesPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        if (!settled_) {
            ask_project_root(mail);
            say(mail); // (waiting) until the answer arrives; the header still names the pane
            return;
        }
        relist();
        say(mail);
    }

    /// THE HOST'S ANSWER ABOUT WHERE THIS RUN BEGAN AND WHERE ITS MARKS LIVE (the seam's
    /// read-only door). Settled once; the origin becomes a mark, `current_dir` seeds to it
    /// when this run has nowhere else, and the durable marks are read.
    void on(const ProjectRoot& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !root_.awaiting || mail.correlation() != root_.pending) {
            return;
        }
        root_.awaiting = false;
        settled_ = true;
        marks_path_ = said.marks_path;
        marks_.origin = ws::admit_location(said.project_dir);
        load_marks();
        if (state_.current_dir.empty()) {
            state_.current_dir = marks_.origin;
        }
        refresh();
        say(mail);
    }

    /// A PRESS FROM A HOST THAT SAYS NOTHING ABOUT WHERE THE KEYS WERE -- one that predates the
    /// second version, or could not answer which version this pane accepts. Not knowing is
    /// not permission: the press selects, and Return is how such a maker opens the row.
    void on(const PanePressed& press, loom::Mail& mail) {
        pressed(press.pane, press.row, press.column, /*keys_went_here=*/false,
                /*fenced=*/false, mail);
    }

    /// ...AND ONE THAT DOES: Workshop read, before the press moved the keyboard, whether an
    /// ordinary key was reaching this pane. Still no picture, so still unfenced.
    void on(const ws::v2::PanePressed& press, loom::Mail& mail) {
        pressed(press.pane, press.row, press.column, press.keys_went_here, /*fenced=*/false, mail);
    }

    /// ...and one that also names the picture it was aimed at: the version this host sends, and
    /// the only one that can be judged. A press about rows since replaced is refused in words;
    /// the two above serve a host that cannot say, and not knowing is not permission.
    void on(const ws::v3::PanePressed& press, loom::Mail& mail) {
        pressed(press.pane, press.row, press.column, press.keys_went_here,
                /*fenced=*/!map_.current(press.picture), mail);
    }

    // WL-FOCUS-04 -- agents/workshop/focus.md
    void pressed(const std::string& pane, std::int64_t row, std::int64_t column,
                 bool keys_went_here, bool fenced, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || pane != files::kProjectFilesPane) {
            return;
        }
        if (fenced) {
            notice_ = kMovedSentence;
            say(mail);
            return;
        }
        // THE PRESS IS READ AGAINST THE PICTURE IT WAS AIMED AT, before anything below changes the
        // next one: spending the notice moves every row up, and a new cursor moves the window.
        const FilesMeaning* m = map_.at(row, column);
        if (m == nullptr || m->kind == files_row::kNone) {
            return; // the notice, the header, a marker row, or blank space names nothing
        }
        // THE MAKER HAS ACTED, SO THE LAST ACT'S ANSWER IS SPENT -- in the rows Workshop holds,
        // too (`on(PaneActionRequested)` says why an open needs the saying).
        const bool spent = !notice_.empty();
        const std::uint64_t published = published_;
        notice_.clear();
        if (m->kind == files_row::kControl) {
            perform(m->id, mail);
        } else if (m->kind == files_row::kEntry && !chooser_.open && !authoring_.open) {
            // A PRESS ON THE ALREADY-SELECTED ROW ACTIVATES IT, AND ONLY WHERE THE KEYS ALREADY
            // WERE: the press that brings the keys to this pane is a maker pointing at it, not an
            // act in it.
            if (keys_went_here && m->index == static_cast<std::size_t>(state_.cursor)) {
                open(mail);
            } else {
                state_.cursor = static_cast<std::int64_t>(m->index);
                say(mail);
            }
        } else if (m->kind == files_row::kCandidate && chooser_.open) {
            // THE CHOOSER'S OWN SECOND PRESS: the first names the candidate, the second authors
            // it -- the browser's rule, one mode over, so no press means two things at once.
            // And only where the keys already were (WL-FOCUS-04): the press that brings them
            // here is a maker pointing at the pane, not a maker choosing a candidate in it.
            if (keys_went_here && m->index == chooser_.cursor) {
                chooser_choose(mail);
            } else {
                chooser_.cursor = m->index;
                say(mail);
            }
        } else if (m->kind == files_row::kField && authoring_.open) {
            edit_field(m->index, mail);
        } else if (m->kind == files_row::kLine && authoring_.open) {
            // THE CARET GOES WHERE THE HAND IS. The prompt is drawn in front of the text, so the
            // column a press names is measured from the first column the text occupies -- the
            // prompt AS DRAWN (`active_prompt`), which a narrow room may have shortened from
            // `authoring_.prompt`'s full label.
            const std::int64_t prompt = static_cast<std::int64_t>(active_prompt().size());
            authoring_.line.place(authoring_.line.position_at_column(column - prompt));
            say(mail);
        }
        if (spent && published_ == published) {
            say(mail);
        }
    }

    /// THE SECOND BUTTON. A right press on a row this pane owns OFFERS that row's menu, beside
    /// the press, continuing it; a right press on anything else -- the header, a marker, blank
    /// space -- is handed back to the host, whose own pane menu answers (WL-CTX-08). A middle
    /// press and every release mean nothing here and are consumed.
    void on(const PaneButton& b, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || b.pane != files::kProjectFilesPane) {
            return;
        }
        if (!b.pressed || b.button != 3) {
            return;
        }
        if (!map_.current(b.picture)) {
            notice_ = kMovedSentence;
            say(mail);
            return;
        }
        const FilesMeaning* m = map_.at(b.row, b.column);
        if (m == nullptr || m->kind == files_row::kNone) {
            (void)pane_menu::pass_back(mail, files::kFilesRole, files::kProjectFilesPane);
            return;
        }
        notice_.clear();
        offer_for(*m, b.row, b.column, mail.correlation(), mail);
        say(mail);
    }

    /// WHAT A MENU CAME TO -- if it answers one of THIS image's own asks. `Asked::take` is the
    /// whole of what makes an answer safe to act on: a choice counts only from the presenter's
    /// office, under the number of an ask this image sent and has not heard answered, about the
    /// pane and subject it asked about, once. A reloaded browser asked nothing, so its
    /// predecessor's menus act on nothing. What the row MEANS is judged here, against what this
    /// pane holds now -- the listing may have been walked again while the menu was open.
    void on(const PaneMenuAnswered& a, loom::Mail& mail) {
        chosen_id_ = asked_menu_.take(mail, a);
        if (chosen_id_.empty()) {
            return;
        }
        chose(a.subject, mail);
    }

    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != files::kProjectFilesPane) {
            return;
        }
        // ONLY THE AUTHORING LINE READS RAW KEYS. Everything else this pane does arrives as
        // a resolved id (`on(PaneActionRequested)`); a component's editing gestures are the
        // component's, not the pane's commands.
        if (!authoring_.open) {
            return;
        }
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!authoring_.line.consume(key.scancode, key.modifiers, clip_)) {
            return; // a key that means nothing to the line is no act: the notice stands, unsaid
        }
        notice_.clear();
        if (clip_.writes != copied_before) {
            mail.publish(surface::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_paste(mail);
        }
        say(mail);
    }

    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || typed.pane != files::kProjectFilesPane) {
            return;
        }
        if (!authoring_.open || typed.text.empty() || !admissible(typed.text)) {
            return;
        }
        notice_.clear();
        authoring_.line.type(typed.text);
        say(mail);
    }

    void on(const PaneWheel& wheel, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || wheel.pane != files::kProjectFilesPane) {
            return;
        }
        if (authoring_.open) {
            return; // a line has one row: there is nothing for a notch to walk
        }
        const std::size_t population =
            chooser_.open ? chooser_.candidates.size()
                          : (listing_.known ? listing_.rows.size() : std::size_t{0});
        if (population == 0) {
            return;
        }
        wheel_accum_ += wheel.dy * static_cast<double>(kFilesWheelRows);
        const std::int64_t notches = static_cast<std::int64_t>(wheel_accum_);
        wheel_accum_ -= static_cast<double>(notches);
        if (notches == 0) {
            return;
        }
        if (chooser_.open) {
            const std::size_t was = chooser_.cursor;
            const std::int64_t to = static_cast<std::int64_t>(chooser_.cursor) - notches;
            chooser_.cursor = to < 0 ? 0
                                     : (static_cast<std::size_t>(to) >= population
                                            ? population - 1
                                            : static_cast<std::size_t>(to));
            if (chooser_.cursor != was) {
                notice_.clear();
                say(mail);
            }
            return;
        }
        const std::int64_t was = state_.cursor;
        move(-notches);
        if (state_.cursor != was) {
            notice_.clear();
            say(mail);
        }
    }

    /// ONE OF THE PANE'S DECLARED ACTIONS, ASKED FOR BY NAME (WL-KEY-15). Workshop resolved
    /// the keystroke against the effective keymap -- the maker's override where one is
    /// authored, this office's declared default otherwise -- so what arrives is the id.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != files::kProjectFilesPane) {
            return;
        }
        // A MODE OWNS THE PANE'S ACTIONS FIRST, AND AN ID IT DOES NOT ANSWER TO IS NO ACT. The
        // declaration and the keystroke race across two messages -- Escape twice in one poll
        // resolves to two cancels against the authoring line's rows, and the second arrives when
        // the browser declares none -- so a stale id, or one nobody declared, spends nothing: no
        // act, and the notice stands (`agents/panes.md`).
        if (!answers(asked.id)) {
            return;
        }
        // THE MAKER HAS ACTED, SO THE LAST ACT'S ANSWER IS SPENT -- and spent means gone from the
        // rows Workshop holds, which are the rows a maker reads. Most acts say their own picture;
        // one whose answer is still on its way (an open at the opening office, a catalog at the
        // recipes office) or that meant nothing says none, and the spent notice would stand
        // painted beside the act that spent it. So when a notice stood and the act published
        // nothing, the rows are said here, once, without it.
        const bool spent = !notice_.empty();
        const std::uint64_t published = published_;
        notice_.clear();
        act(asked, mail);
        if (spent && published_ == published) {
            say(mail);
        }
    }

    /// What one action does, by id: with the notice already spent, and only for an id `answers`
    /// admitted in the pane's current mode. Each mode's operations have ids of their own (Return
    /// is `files.choose`, `files.commit-field` or `files.open`), so an id resolved in one mode and
    /// delivered in the next is refused, never the next mode's operation. A key and a control
    /// reach one operation through the same `perform`, against the subject `subject_of` names,
    /// so a remapped key and the button beside it cannot mean two things.
    void act(const PaneActionRequested& asked, loom::Mail& mail) {
        if (asked.id == files::kActionUp) {
            step(-1, mail);
            return;
        }
        if (asked.id == files::kActionDown) {
            step(1, mail);
            return;
        }
        perform(asked.id, mail);
    }

    /// WHAT AN ID ACTS ON, RIGHT NOW -- the one place that says so, read when a control is
    /// painted, when a menu is offered, and again when either is spent. An id with no subject
    /// of its own answers empty and is never subject-checked.
    std::string subject_of(const std::string& id) const {
        if (id == files::kActionOpen || id == files::kActionUseRecipes) {
            const FileRow* row = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
            return row == nullptr ? std::string() : row->name;
        }
        if (id == files::kActionParent || id == files::kActionRefresh ||
            id == files::kActionMark || id == files::kActionPickBuildable) {
            return state_.current_dir;
        }
        if (id == files::kActionChoose) {
            return chooser_.cursor < chooser_.candidates.size()
                       ? chooser_.candidates[chooser_.cursor].name
                       : std::string();
        }
        if (id == files::kActionCommitField || id == files::kActionWriteRecipe ||
            id == files::kActionNextField) {
            return authoring_.chosen.name;
        }
        return std::string();
    }

    /// STEP THE CURSOR OF WHATEVER LIST IS IN FORCE. One id per direction, in every mode that
    /// declares it, because it means one thing in each: the cursor of the list shown.
    void step(std::int64_t by, loom::Mail& mail) {
        if (chooser_.open) {
            if (by < 0 && chooser_.cursor > 0) {
                --chooser_.cursor;
            } else if (by > 0 && chooser_.cursor + 1 < chooser_.candidates.size()) {
                ++chooser_.cursor;
            }
            say(mail);
            return;
        }
        if (authoring_.open) {
            const std::int64_t to = static_cast<std::int64_t>(authoring_.step) + by;
            if (to >= 0 && to < static_cast<std::int64_t>(field_count())) {
                edit_field(static_cast<std::size_t>(to), mail);
            } else {
                say(mail);
            }
            return;
        }
        move(by);
        say(mail);
    }

    /// One operation, asked for by a key, a control or a menu row, about the subject it was named
    /// on: refused, never retargeted, when that is no longer what is here. A menu is checked and a
    /// control is not: a control acts on what the pane shows as selected, which only the maker's
    /// own act moves, while a menu stands open across other acts and answers later. Naming the
    /// selection inside a control's meaning would move the picture on every selection, and the
    /// picture fence would then refuse a double-click's second press.
    void perform_on(const std::string& id, const std::string& subject, loom::Mail& mail) {
        if (!subject.empty() && subject_of(id) != subject && subject_needed(id)) {
            notice_ = "`" + ws::shown_name(subject) + "` is not what is here now -- aim again";
            say(mail);
            return;
        }
        perform(id, mail);
    }

    /// DOES THIS OPERATION ACT ON THE ROW A MENU WAS OPENED ON? The place-scoped ones -- look
    /// again, up a directory, mark here, pick something buildable, the mark jumps -- act on
    /// where the browser is standing, which the caller established separately; only the two
    /// that act on an ENTRY need the row to be the row the menu named.
    static bool subject_needed(const std::string& id) {
        return id == files::kActionOpen || id == files::kActionUseRecipes ||
               id == files::kActionChoose;
    }

    void perform(const std::string& id, loom::Mail& mail) {
        if (id == files::kActionOpen) {
            open(mail);
        } else if (id == files::kActionParent) {
            parent(mail);
        } else if (id == files::kActionRefresh) {
            refresh();
            notice_ = "listed " + where() + " again";
            say(mail);
        } else if (id == files::kActionUseRecipes) {
            use_recipes(mail);
        } else if (id == files::kActionMark) {
            mark(mail);
        } else if (id == files::kActionNextMark) {
            jump_mark(1);
            say(mail);
        } else if (id == files::kActionPreviousMark) {
            jump_mark(-1);
            say(mail);
        } else if (id == files::kActionPickBuildable) {
            pick_buildable(mail);
        } else if (id == files::kActionChoose) {
            chooser_choose(mail);
        } else if (id == files::kActionCommitField) {
            authoring_commit(mail);
        } else if (id == files::kActionNextField) {
            next_field(mail);
        } else if (id == files::kActionWriteRecipe) {
            write_recipe(mail);
        } else if (id == files::kActionCancel) {
            cancel_mode(mail);
        } else if (id == files::kActionMenu) {
            offer_here(mail, mail.correlation());
            say(mail);
        }
    }

    /// OUT OF WHATEVER MODE IS OPEN, WHOLE -- and nothing was written by leaving it.
    void cancel_mode(loom::Mail& mail) {
        if (chooser_.open) {
            chooser_ = Chooser{};
            declare(mail);
            notice_ = "no recipe was authored";
        } else if (authoring_.open) {
            authoring_ = Authoring{};
            declare(mail);
            notice_ = "no recipe was written";
        }
        say(mail);
    }

    // ---- The pane's own menu (WL-CTX-09) ------------------------------------------------

    /// THE ROWS A MENU OFFERS FOR WHAT WAS POINTED AT, and the ask recorded in this image.
    ///
    /// EVERY ROW IS AN OPERATION THIS PANE ALREADY HAS, spelled with the subject it will act
    /// on, so the menu teaches the same list the controls and the keymap name. The subject
    /// crossing the seam is the pane's own word, echoed back unread, and judged against what
    /// this pane holds when the answer arrives.
    void offer_for(const FilesMeaning& m, std::int64_t row, std::int64_t column,
                   std::uint64_t correlation, loom::Mail& mail) {
        if (m.kind == files_row::kEntry) {
            if (m.index != static_cast<std::size_t>(state_.cursor)) {
                state_.cursor = static_cast<std::int64_t>(m.index); // a menu is about a row
            }
            offer_entry(row, column, correlation, mail);
            return;
        }
        if (m.kind == files_row::kCandidate) {
            chooser_.cursor = m.index;
            offer_candidate(row, column, correlation, mail);
            return;
        }
        if (m.kind == files_row::kField || m.kind == files_row::kLine) {
            offer_field(m.index, row, column, correlation, mail);
            return;
        }
        // A CONTROL: the menu for the mode it belongs to, opened where the hand is.
        offer_at(row, column, correlation, mail);
    }

    /// THE MENU KEY'S ENTRANCE: the same rows, about what is selected, beside the selected row.
    void offer_here(loom::Mail& mail, std::uint64_t correlation) {
        std::int64_t row = 0;
        if (chooser_.open) {
            const std::int64_t at = map_.row_of(
                FilesMeaning{files_row::kCandidate, chooser_.cursor, {},
                             chooser_.cursor < chooser_.candidates.size()
                                 ? chooser_.candidates[chooser_.cursor].name
                                 : std::string()});
            row = at < 0 ? 0 : at;
        } else if (authoring_.open) {
            const Field& field =
                field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, authoring_.step);
            const std::int64_t at =
                map_.row_of(FilesMeaning{files_row::kLine, authoring_.step, {}, field.name});
            row = at < 0 ? 0 : at;
        } else {
            const FileRow* here = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
            const std::int64_t at =
                here == nullptr
                    ? -1
                    : map_.row_of(FilesMeaning{files_row::kEntry,
                                               static_cast<std::size_t>(state_.cursor), {},
                                               here->name});
            row = at < 0 ? 0 : at;
        }
        offer_at(row, 0, correlation, mail);
    }

    void offer_at(std::int64_t row, std::int64_t column, std::uint64_t correlation,
                  loom::Mail& mail) {
        if (chooser_.open) {
            offer_candidate(row, column, correlation, mail);
        } else if (authoring_.open) {
            offer_field(authoring_.step, row, column, correlation, mail);
        } else {
            offer_entry(row, column, correlation, mail);
        }
    }

    void offer_entry(std::int64_t row, std::int64_t column, std::uint64_t correlation,
                     loom::Mail& mail) {
        const FileRow* here = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
        const std::string name = here == nullptr ? std::string() : ws::shown_name(here->name);
        pane_menu::Offer offer(files::kProjectFilesPane,
                               join_subject(state_.current_dir,
                                            here == nullptr ? std::string() : here->name));
        offer.at(row, column);
        if (here != nullptr) {
            offer.row(files::kMenuOpen,
                      (here->directory ? "enter `" : "open `") + name + "`");
            if (!here->directory) {
                offer.row(files::kMenuUseRecipes, "use `" + name + "` as this project's recipes");
            }
        }
        offer.row(files::kMenuPickBuildable, "pick something buildable here");
        offer.row(files::kMenuMark, marks_.marked(state_.current_dir)
                                        ? "forget this place"
                                        : "mark this place");
        offer.row(files::kMenuParent, "up a directory");
        offer.row(files::kMenuRefresh, "look at this directory again");
        offer.row(files::kMenuPreviousMark, "go to the previous mark");
        offer.row(files::kMenuNextMark, "go to the next mark");
        offer.row(files::kMenuManage, "manage this pane...");
        asked_menu_ = offer.continuing(mail, files::kFilesRole, correlation);
    }

    void offer_candidate(std::int64_t row, std::int64_t column, std::uint64_t correlation,
                         loom::Mail& mail) {
        pane_menu::Offer offer(
            files::kProjectFilesPane,
            join_subject(chooser_.dir, chooser_.cursor < chooser_.candidates.size()
                                           ? chooser_.candidates[chooser_.cursor].name
                                           : std::string()));
        offer.at(row, column);
        if (chooser_.cursor < chooser_.candidates.size()) {
            offer.row(files::kMenuChoose,
                      "author a recipe for `" + chooser_.candidates[chooser_.cursor].name + "`");
        }
        offer.row(files::kMenuCancel, "pick nothing -- back to the listing");
        offer.row(files::kMenuManage, "manage this pane...");
        asked_menu_ = offer.continuing(mail, files::kFilesRole, correlation);
    }

    /// The authoring menu: every field but the one in hand, then the mode's own controls. A row
    /// per field is what makes a short pane workable: the room may hold only the field being typed
    /// into, and the others are reachable by no press unless the menu names them. Each row carries
    /// its field in its id (`files::menu_edit_field`), since one answer echoes one subject, and
    /// that subject is the candidate.
    void offer_field(std::size_t which, std::int64_t row, std::int64_t column,
                     std::uint64_t correlation, loom::Mail& mail) {
        pane_menu::Offer offer(files::kProjectFilesPane,
                               join_subject(authoring_.dir, authoring_.chosen.name));
        offer.at(row, column);
        // THE FIELD THE HAND WAS ON LEADS, and the rest follow in their own order: a menu
        // opened ON a field is still about that field first.
        const auto field_row = [&](std::size_t i) {
            offer.row(files::menu_edit_field(i),
                      std::string("type the ") +
                          field_menu_label(authoring_.chosen.tree, authoring_.chosen.multi_config,
                                          i));
        };
        if (which < field_count() && which != authoring_.step) {
            field_row(which);
        }
        for (std::size_t i = 0; i < field_count(); ++i) {
            if (i == authoring_.step || i == which) {
                continue; // the line is already standing on it, or it led
            }
            field_row(i);
        }
        // The strip's own controls, row for row, including an unavailable one: a narrow strip
        // drops what will not fit and says `+N in menu`, so the promise is kept here. On the last
        // field the row still appears, as the strip draws `(next field)`, and `next_field`
        // refuses in words there and never writes the recipe.
        if (authoring_.step + 1 < field_count()) {
            offer.row(files::kMenuNextField,
                      std::string("keep this field and type the ") +
                          field_menu_label(authoring_.chosen.tree, authoring_.chosen.multi_config,
                                          authoring_.step + 1));
        } else {
            // A short label naming no field, clear of two bounds: `refusal_of`
            // (`menu-presenter/presenter.cpp`) refuses the whole offer once any label exceeds
            // `kMaxPaneMenuLabelLen` (64 bytes), and below that the popup clips a row to its
            // display room. The line names the field; `next_field`'s notice says the rest.
            offer.row(files::kMenuNextField, "this is the last field");
        }
        offer.row(files::kMenuWriteRecipe,
                  "write the recipe for `" + authoring_.chosen.name + "`");
        offer.row(files::kMenuCancel, "abandon this recipe");
        offer.row(files::kMenuManage, "manage this pane...");
        asked_menu_ = offer.continuing(mail, files::kFilesRole, correlation);
    }

    /// WHAT A CHOSEN ROW MEANS -- judged here, against what this pane holds NOW. The subject
    /// the presenter echoed is the place or the candidate the menu was opened about; a menu
    /// answered after the browser walked somewhere else acts on nothing.
    void chose(const std::string& said, loom::Mail& mail) {
        const std::vector<std::string> parts = split_subject(said);
        if (parts.size() != 2) {
            return; // not a subject this pane ever wrote
        }
        const std::string& place = parts[0];
        const std::string& subject = parts[1];
        const std::string id = chosen_id_;
        if (id == files::kMenuManage) {
            // THE HOST'S OWN PANE MENU ON THIS PANE -- the deliberate route to arranging,
            // ordering, editing this pane's code and removing it, continuing this choice.
            (void)pane_menu::manage(mail, files::kFilesRole, files::kProjectFilesPane,
                                    files::kFilesRole, files::kProjectFilesPane);
            return;
        }
        std::size_t field = 0;
        if (files::is_menu_edit_field(id, &field)) {
            if (authoring_.open && field < field_count() && subject == authoring_.chosen.name &&
                place == authoring_.dir) {
                edit_field(field, mail);
                // And the choice takes the keys: a menu leaves the keyboard where it was, so a
                // row that begins an edit in an unfocused pane would leave a line no character
                // could reach. The host grants them only while this choice is the latest act.
                take_keys(mail);
            } else {
                notice_ = "that field is not open any more -- nothing was typed";
                say(mail);
            }
            return;
        }
        if (id == files::kMenuNextField) {
            if (authoring_.open && subject == authoring_.chosen.name && place == authoring_.dir) {
                next_field(mail);
                take_keys(mail);
            } else {
                notice_ = "that draft is not open any more -- nothing was typed";
                say(mail);
            }
            return;
        }
        static const struct {
            const char* menu;
            const char* action;
        } kRows[] = {{files::kMenuOpen, files::kActionOpen},
                     {files::kMenuUseRecipes, files::kActionUseRecipes},
                     {files::kMenuPickBuildable, files::kActionPickBuildable},
                     {files::kMenuMark, files::kActionMark},
                     {files::kMenuParent, files::kActionParent},
                     {files::kMenuRefresh, files::kActionRefresh},
                     {files::kMenuNextMark, files::kActionNextMark},
                     {files::kMenuPreviousMark, files::kActionPreviousMark},
                     {files::kMenuChoose, files::kActionChoose},
                     {files::kMenuWriteRecipe, files::kActionWriteRecipe},
                     {files::kMenuCancel, files::kActionCancel}};
        for (const auto& row : kRows) {
            if (id != row.menu) {
                continue;
            }
            // BOTH HALVES OF THE SUBJECT ARE ESTABLISHED AGAIN, and either one failing is a
            // refusal rather than a retarget. A menu stands open across the maker's other
            // acts and across anything that moves this pane's own rows -- a finished build
            // re-walks the directory (WL-FILES-12) -- so the place it was opened in and the
            // row it was opened on are both facts that can have stopped being true.
            const std::string& where_it_was = chooser_.open ? chooser_.dir : state_.current_dir;
            if (!authoring_.open && place != where_it_was) {
                notice_ = "that menu was about " + place + " -- nothing was done";
                say(mail);
                return;
            }
            const bool was_authoring = authoring_.open;
            perform_on(row.action, subject, mail);
            // A CHOICE THAT OPENED THE AUTHORING LINE BEGAN AN EDIT, and an edit needs the
            // keys the menu left where they were (`take_keys`). `chooser_choose` is the one
            // row here that can do it; a row that merely walked the browser takes nothing.
            if (!was_authoring && authoring_.open) {
                take_keys(mail);
            }
            return;
        }
    }

    /// ASK THE HOST FOR THE KEYBOARD, CONTINUING THE CHOICE THIS DELIVERY BROUGHT. Spent only
    /// where a chosen row actually began an edit: a right press by itself stays focus-neutral
    /// (WL-CTX-08), and the host refuses a grab that is no longer the maker's latest act.
    void take_keys(loom::Mail& mail) {
        (void)pane_menu::take_keyboard(mail, files::kFilesRole, files::kProjectFilesPane);
    }

    /// A FILE THE PANE ASKED THE HOST TO USE OR AUTHOR (`zengine.recipes`), answered.
    void on(const RecipeOutcome& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !recipes_.awaiting || mail.correlation() != recipes_.pending) {
            return;
        }
        recipes_.awaiting = false;
        // Both halves in one sentence, in the order that survives the cut: what went wrong and
        // what is still in force are both owed, and the notice row is cut at the band's width, so
        // the short fixed statements go first and the owner's sentence and the path take the
        // tail. `said.path` is the office's answer about what is in force after the attempt.
        if (!said.accepted) {
            notice_ = recipes_.was_author
                          ? ws::authoring_refused_words(said.refusal, said.path)
                          : ws::catalog_refused_words(said.refusal, said.path);
            say(mail);
            return;
        }
        notice_ = recipes_.was_author
                      ? ws::authored_words(recipes_.id, recipes_.artifact, said.path,
                                           said.recipes)
                      : ws::catalog_taken_words(said.path, said.recipes);
        // The Builder is asked to say what it is: the tool re-reads the catalog in force and
        // republishes it, and the Builder pane hears the new rows.
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(zengine::builder::kBuilderRole, zengine::builder::StatusRequested{});
        say(mail);
    }

    /// The opening office answered (`zengine.opening`). A refused open is said in the pane's own
    /// row; an accepted one moved the keyboard to the Editor and needs no word here.
    void on(const SourceOpened& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !open_.awaiting || mail.correlation() != open_.pending) {
            return;
        }
        open_ = Ask{};
        if (!said.accepted) {
            notice_ = said.refusal;
            say(mail);
        }
    }

    /// THE BUS'S WORD THAT ONE OF THIS PANE'S ATTEMPTS WAS REFUSED BEFORE ANY HANDLER RAN
    /// (Loom's `zen.DispatchRefused`; WL-OPEN-07). The shape alone is ordinary speech, so
    /// Loom's provenance is checked first; then the exact attempt is matched against the one
    /// ask this pane can still be waiting on, and only that ask is cleared. A forged, stale,
    /// duplicate or mismatched notice settles nothing. Delivered silence is not a refusal and
    /// is not ended here: the ask stays awaited, and the next Return is a fresh attempt.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused()) {
            return;
        }
        const loom::Ticket attempt = refused.refused_attempt();
        if (!attempt.valid() || !open_.awaiting || !open_.attempt.valid() ||
            attempt.seq != open_.attempt.seq) {
            return;
        }
        const std::string subject = open_.subject;
        open_ = Ask{};
        notice_ = "`" + ws::shown_name(subject) + "` was not opened -- the open could not reach " +
                  ws::kOpeningRole + " (" + refused.reason + ")";
        say(mail);
    }

    /// A build this process ran has finished (published `to_any`), so what is on disk may have
    /// changed: take a fresh listing and keep the cursor. Gated on an outcome a build will not
    /// leave, and on a build having happened: `BuildStatus` also answers anyone asking what the
    /// state is (this pane asks, after an accepted catalog choice), and taking that for a finished
    /// build would re-list and erase the sentence just written. `builds` is the tool's count of
    /// builds asked for, so an answer with the same count describes the same world.
    void on(const zengine::builder::BuildStatus& said, loom::Mail& mail) {
        const std::int64_t built_before = builds_seen_;
        const bool first = !saw_status_;
        saw_status_ = true;
        builds_seen_ = said.builds;
        if (!settled_ || !granted_ || zengine::builder::still_going(said.outcome)) {
            return;
        }
        if (first || said.builds == built_before) {
            return; // a description, not news: nothing on disk changed because of this
        }
        relist();
        say(mail);
    }

    void on(const surface::ClipboardCopy& said, loom::Mail&) {
        if (admissible(said.text)) {
            clip_.text = said.text;
        }
    }

    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        const std::optional<loom::PendingAsk> settled = clip_asks_.settle(mail.correlation(), mail.sender());
        if (!settled || !pasting_ || settled->id != paste_ask_) {
            return;
        }
        pasting_ = false;
        if (!authoring_.open || authoring_.line.draft_epoch() != paste_epoch_) {
            return;
        }
        if (a.readable) {
            if (!admissible(a.text)) {
                return;
            }
            clip_.text = a.text;
        }
        authoring_.line.paste(clip_);
        say(mail);
    }

private:
    // ---- Announcing ---------------------------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(kWorkshopRole,
                          PaneOffered{files::kProjectFilesPane, files::kProjectFilesName,
                                      files::kProjectFilesSummary, 9, 56});
        declare(mail);
    }

    /// What this pane answers to right now, re-declared whenever the mode changes. A mode is a
    /// declaration, not a keyboard context of Workshop's: a pane's rows join one map under its
    /// runtime handle and the collision law refuses a second row on a taken gesture, so the pane
    /// declares what is true now. While a maker types into the authoring line, every key the mode
    /// leaves unclaimed reaches the line as a `PaneKey` (Backspace deletes a character).
    /// `PaneActions` is a replacement (WL-KEY-15), and an id is one operation in every mode.
    void declare(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = files::kProjectFilesPane;
        actions.rows = action_rows();
        (void)mail.as_role(files::kFilesRole).send_to_role(kWorkshopRole, actions);
    }

    /// THE ROWS OF THE MODE THIS PANE IS IN -- what `declare` tells Workshop, and what `answers`
    /// reads, so what the pane acts on and what it said it acts on are one list.
    std::vector<PaneActionRow> action_rows() const {
        std::vector<PaneActionRow> rows;
        const auto row = [&rows](const char* id, const char* label, std::int64_t sc,
                                 std::int64_t mods = input::mod::kNone) {
            rows.push_back(PaneActionRow{id, label, sc, mods});
        };
        // The authoring line owns the keyboard except for these rows. The two arrows are the
        // field walk, free because a single line has no row above or below; `files.write-recipe`
        // declares no default key (`kUnknown`, WL-KEY-13), reachable from its control and the
        // menu, or from a key the maker names.
        if (authoring_.open) {
            row(files::kActionUp, "previous field", input::scan::kUp);
            row(files::kActionDown, "next field", input::scan::kDown);
            row(files::kActionCommitField, "commit this field", input::scan::kReturn);
            row(files::kActionNextField, "keep this field and step", input::scan::kUnknown);
            row(files::kActionWriteRecipe, "write the recipe", input::scan::kUnknown);
            // And the menu declares no default key while a line is open: Workshop resolves a key
            // transition before the character it produced arrives, so `Shift+M` would open the
            // menu and lose the `M` of `Main`. The `[menu]` control, first in every strip, and the
            // second button stay the route; a maker who wants a key names `files.menu`.
            row(files::kActionMenu, "this pane's menu", input::scan::kUnknown);
            row(files::kActionCancel, "abandon", input::scan::kEscape);
            return rows;
        }
        // ---- The chooser is a list: it needs the two arrows and the mode's own rows ------
        if (chooser_.open) {
            row(files::kActionUp, "row up", input::scan::kUp);
            row(files::kActionDown, "row down", input::scan::kDown);
            row(files::kActionChoose, "choose this", input::scan::kReturn);
            row(files::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
            row(files::kActionCancel, "cancel", input::scan::kEscape);
            return rows;
        }
        // ---- Browsing: the ids and default keys a maker's keymap already names -----------
        row(files::kActionUp, "row up", input::scan::kUp);
        row(files::kActionDown, "row down", input::scan::kDown);
        row(files::kActionOpen, "enter or edit", input::scan::kReturn);
        row(files::kActionParent, "up a directory", input::scan::kBackspace);
        row(files::kActionRefresh, "look again", input::scan::kR);
        row(files::kActionUseRecipes, "use as recipes", input::scan::kU);
        row(files::kActionMark, "mark this place", input::scan::kM);
        row(files::kActionNextMark, "next mark", input::scan::kN);
        row(files::kActionPreviousMark, "previous mark", input::scan::kN, input::mod::kShift);
        row(files::kActionPickBuildable, "pick buildable", input::scan::kA);
        // THE KEYBOARD'S WAY TO THE ROWS A RIGHT PRESS OFFERS. `m` is already this pane's
        // mark, so the menu takes the shifted one rather than moving a key a maker has.
        row(files::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
        return rows;
    }

    bool answers(const std::string& id) const {
        for (const PaneActionRow& row : action_rows()) {
            if (row.id == id) {
                return true;
            }
        }
        return false;
    }

    // ---- The doors ----------------------------------------------------------------------

    struct Ask {
        std::uint64_t pending = 0;
        bool awaiting = false;
        /// THE QUEUED ATTEMPT (Loom's sequence), kept so the bus's later word that exactly
        /// this send was refused before any handler ran can be matched to it -- and only it.
        loom::Ticket attempt{};
        std::string subject; ///< what the ask was about, for the sentence a refusal needs
    };

    void ask_project_root(loom::Mail& mail) {
        root_.pending = ++asked_;
        root_.awaiting = true;
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(ws::kProjectRole, ProjectRootRequested{}, root_.pending);
    }

    // ---- Marks (the durable file the pane owns) -----------------------------------------

    void load_marks() {
        if (marks_path_.empty() || !std::filesystem::exists(marks_path_)) {
            return;
        }
        const ws::marks_persist::LoadedMarks loaded = ws::marks_persist::load_file(marks_path_);
        if (!loaded.outcome.accepted) {
            marks_refused_ = true;
            notice_ = "marks refused -- this run remembers no places: " + loaded.outcome.refusal;
            return;
        }
        marks_.maker = loaded.maker;
        if (!loaded.skipped.empty()) {
            notice_ = loaded.skipped;
        }
    }

    void save_marks() {
        if (marks_path_.empty() || marks_refused_) {
            return;
        }
        const ws::Written done = ws::marks_persist::save_file(marks_path_, marks_.maker);
        if (!done.accepted) {
            notice_ = "could not write your marks: " + done.refusal;
        }
    }

    // ---- Browsing -----------------------------------------------------------------------

    std::string where() const { return state_.current_dir.empty() ? std::string("nowhere") : state_.current_dir; }

    void refresh() {
        if (state_.current_dir.empty()) {
            listing_ = Listing{};
            listing_.refusal =
                "this run began nowhere -- Workshop could not tell where it was launched from";
            state_.cursor = 0;
            return;
        }
        listing_ = ws::enumerate_directory(state_.current_dir);
        state_.cursor = 0;
        wheel_accum_ = 0.0;
    }

    /// LOOK AT THE SAME PLACE AGAIN FOR A REASON THAT IS NOT THE MAKER'S -- a room granted, a
    /// build finished -- and keep what the maker selected: the entry the cursor named is found
    /// again by name, and only an entry the fresh listing no longer has leaves the cursor at the
    /// top. A move to another place (enter, parent, a mark) and `files.refresh` start from
    /// `refresh()` itself.
    void relist() {
        const FileRow* row = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
        const std::string was = row != nullptr ? row->name : std::string();
        refresh();
        if (!was.empty()) {
            point_at(was);
        }
    }

    void move(std::int64_t by) {
        const std::int64_t total = static_cast<std::int64_t>(listing_.rows.size());
        if (total == 0) {
            state_.cursor = 0;
            return;
        }
        std::int64_t at = (state_.cursor < total ? state_.cursor : 0) + by;
        if (at < 0) {
            at = 0;
        }
        if (at >= total) {
            at = total - 1;
        }
        state_.cursor = at;
    }

    void point_at(const std::string& name) {
        for (std::size_t i = 0; i < listing_.rows.size(); ++i) {
            if (listing_.rows[i].name == name) {
                state_.cursor = static_cast<std::int64_t>(i);
                return;
            }
        }
    }

    void parent(loom::Mail& mail) {
        if (state_.current_dir.empty()) {
            notice_ = "there is nowhere to go up from -- this run began nowhere";
            say(mail);
            return;
        }
        const std::string up = ws::parent_location(state_.current_dir);
        if (up.empty()) {
            notice_ = state_.current_dir + " is the top of this filesystem";
            say(mail);
            return;
        }
        const std::filesystem::path was(state_.current_dir);
        const AdmittedName leaf = ws::admit_filename(was.filename());
        state_.current_dir = up;
        refresh();
        if (leaf.exact) {
            point_at(leaf.name);
        }
        say_where();
        say(mail);
    }

    void say_where() {
        const std::string why = ws::provenance_words(marks_.provenance(state_.current_dir));
        notice_ = why.empty() ? "in " + where() : "in " + where() + " (" + why + ")";
    }

    void open(loom::Mail& mail) {
        const FileRow* row = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
        if (row == nullptr) {
            notice_ = "no row is selected -- nothing was opened";
            say(mail);
            return;
        }
        if (!row->openable) {
            notice_ = "`" + ws::shown_name(row->name) +
                      "` has bytes this Workshop cannot carry in a path -- it cannot be opened";
            say(mail);
            return;
        }
        if (state_.current_dir.empty()) {
            notice_ = "this run began nowhere -- there is no location to act in";
            say(mail);
            return;
        }
        if (row->directory) {
            const std::string into = ws::admit_location(
                ws::persist::resolved_against(state_.current_dir, row->name));
            if (into.empty()) {
                notice_ = "`" + ws::shown_name(row->name) + "` cannot be reached from here";
                say(mail);
                return;
            }
            state_.current_dir = into;
            refresh();
            say_where();
            say(mail);
            return;
        }
        // A file: ask the opening office, which arranges the document and the desk together
        // (WL-OPEN-01); every refusal lands in this pane's own row. The ticket is kept
        // (WL-OPEN-07): a valid one says only that the send was queued, and the bus's later word
        // that exactly this attempt was refused reaches `on(loom::DispatchRefused)`. An invalid
        // ticket means nothing was queued, refused now rather than awaited forever.
        open_.pending = ++asked_;
        open_.awaiting = true;
        open_.subject = row->name;
        open_.attempt = mail.as_role(files::kFilesRole)
                            .send_to_role(ws::kOpeningRole,
                                          OpenSourceRequested{ws::persist::resolved_against(
                                              state_.current_dir, row->name)},
                                          open_.pending);
        if (!open_.attempt.valid()) {
            open_ = Ask{};
            notice_ = "`" + ws::shown_name(row->name) +
                      "` was not opened -- nothing was queued to the opening office";
            say(mail);
        }
    }


    void mark(loom::Mail& mail) {
        if (state_.current_dir.empty()) {
            notice_ = "there is nowhere to mark -- this run began nowhere";
            say(mail);
            return;
        }
        const bool removed = marks_.forget(state_.current_dir);
        if (!removed) {
            marks_.remember(state_.current_dir);
        }
        save_marks();
        notice_ = (removed ? "no longer marked: " : "marked: ") + state_.current_dir;
        say(mail);
    }

    void jump_mark(std::int64_t by) {
        const std::vector<MarkedPlace> stops = marks_.destinations(ws::host_filesystem_roots());
        if (stops.empty()) {
            notice_ = "there is nowhere to jump to -- no origin, no marks, and no filesystem roots";
            return;
        }
        const std::int64_t total = static_cast<std::int64_t>(stops.size());
        std::int64_t at = by > 0 ? -1 : 0;
        for (std::int64_t i = 0; i < total; ++i) {
            if (stops[static_cast<std::size_t>(i)].path == state_.current_dir) {
                at = i;
                break;
            }
        }
        const std::int64_t to = ((at + by) % total + total) % total;
        const MarkedPlace& went = stops[static_cast<std::size_t>(to)];
        state_.current_dir = went.path;
        refresh();
        const std::string why = ws::provenance_words(went.from);
        notice_ = "at " + went.path + (why.empty() ? std::string() : " (" + why + ")");
    }

    // ---- Use as recipes, and pick buildable ---------------------------------------------

    void use_recipes(loom::Mail& mail) {
        const FileRow* row = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
        if (row == nullptr || !row->openable || row->directory || state_.current_dir.empty()) {
            notice_ = ws::catalog_row_refusal(row, !state_.current_dir.empty());
            say(mail);
            return;
        }
        recipes_.pending = ++asked_;
        recipes_.awaiting = true;
        recipes_.was_author = false;
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(ws::kRecipesRole,
                          RecipeUseRequested{ws::persist::resolved_against(state_.current_dir, row->name)},
                          recipes_.pending);
    }

    void pick_buildable(loom::Mail& mail) {
        if (!listing_.known || state_.current_dir.empty()) {
            notice_ = "nothing is listed here -- nothing to pick from";
            say(mail);
            return;
        }
        Chooser chooser;
        chooser.dir = state_.current_dir;
        for (const FileRow& row : listing_.rows) {
            if (!row.openable) {
                continue;
            }
            if (row.directory) {
                std::error_code ec;
                const std::filesystem::path cache =
                    std::filesystem::path(state_.current_dir) / row.name / "CMakeCache.txt";
                const bool configured = std::filesystem::exists(cache, ec);
                if (configured && !ec) {
                    chooser.candidates.push_back(
                        BuildCandidate{row.name, true, cache_is_multi_config(cache)});
                }
            } else if (source_name(row.name)) {
                chooser.candidates.push_back(BuildCandidate{row.name, false});
            }
        }
        if (chooser.candidates.empty()) {
            notice_ = "nothing buildable in " + state_.current_dir +
                      " -- no source file, and no directory holding a configured CMake tree";
            say(mail);
            return;
        }
        chooser.open = true;
        chooser_ = std::move(chooser);
        declare(mail); // this mode's own rows, and no others
        notice_ = "pick something buildable -- Return authors a recipe, Escape cancels";
        say(mail);
    }

    void chooser_choose(loom::Mail& mail) {
        if (chooser_.cursor >= chooser_.candidates.size()) {
            chooser_ = Chooser{};
            declare(mail);
            say(mail);
            return;
        }
        Authoring a;
        a.open = true;
        a.chosen = chooser_.candidates[chooser_.cursor];
        a.dir = chooser_.dir;
        a.values.assign(field_count(a.chosen.tree, a.chosen.multi_config), std::string());
        a.filled.assign(field_count(a.chosen.tree, a.chosen.multi_config), false);
        authoring_ = std::move(a);
        chooser_ = Chooser{};
        field_hint_ = 0;
        load_field(0);
        // THE LINE TAKES THE KEYBOARD: the mode's own rows are declared, and everything else
        // reaches the line as an ordinary key.
        declare(mail);
        notice_ = std::string(authoring_.chosen.tree ? "a configured tree: " : "a source file: ") +
                  authoring_.chosen.name + " -- Return commits a field, Escape cancels";
        say(mail);
    }

    /// What a field is seeded with when it has never been answered: the candidate's stem for the
    /// recipe name, and the artifact stem following whatever names the thing built. An answered
    /// field is seeded with the maker's answer, never re-suggested over it.
    std::string suggestion_for(std::size_t which) const {
        const bool tree = authoring_.chosen.tree;
        if (which == 0) {
            return tree ? authoring_.chosen.name : stem_of(authoring_.chosen.name);
        }
        if (!tree && which == 1) {
            return authoring_.values[0];
        }
        if (tree && which == 2) {
            return authoring_.values[1];
        }
        return std::string();
    }

    /// STAND THE LINE ON A FIELD: its prompt, its text, the caret at the end of it.
    void load_field(std::size_t which) {
        authoring_.step = which;
        const Field& field = field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, which);
        const std::string text =
            authoring_.filled[which] ? authoring_.values[which] : suggestion_for(which);
        authoring_.prompt = std::string(field.name) + "> ";
        authoring_.line.set(text, text.size());
    }

    /// KEEP WHAT THE LINE HOLDS, WITHOUT JUDGING IT. Leaving a field is not writing anything,
    /// so an empty required field is kept empty here and refused where it matters -- at the
    /// write. Judging it here would make a maker unable to look at the next field.
    void stash_field() {
        const std::string typed = trimmed(authoring_.line.text());
        authoring_.values[authoring_.step] = typed;
        if (!typed.empty()) {
            authoring_.filled[authoring_.step] = true;
        }
    }

    /// Commit the field the line is standing on, refusing an empty required one. Returns whether
    /// it took.
    bool record_field(loom::Mail& mail) {
        const std::string typed = trimmed(authoring_.line.text());
        const Field& field =
            field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, authoring_.step);
        if (field.required && typed.empty()) {
            notice_ = std::string(field.name) + " is required -- nothing was written";
            say(mail);
            return false;
        }
        authoring_.values[authoring_.step] = typed;
        authoring_.filled[authoring_.step] = true;
        return true;
    }

    /// STAND ON ANOTHER FIELD, keeping what the current one holds.
    void edit_field(std::size_t which, loom::Mail& mail) {
        if (!authoring_.open || which >= field_count()) {
            return;
        }
        if (which != authoring_.step) {
            stash_field();
            load_field(which);
        }
        say(mail);
    }

    /// IS EVERY REQUIRED FIELD ANSWERED? Read for the write control's face, and asked again by
    /// the write itself.
    bool filled_in() const {
        if (!authoring_.open) {
            return false;
        }
        for (std::size_t i = 0; i < field_count(); ++i) {
            const Field& field = field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, i);
            const bool answered = i == authoring_.step
                                      ? !trimmed(authoring_.line.text()).empty()
                                      : authoring_.filled[i] && !authoring_.values[i].empty();
            if (field.required && !answered) {
                return false;
            }
        }
        return true;
    }

    /// COMMIT THIS FIELD AND STAND ON THE NEXT -- AND NOTHING ELSE. The whole of what the
    /// `[next field]` control reads as, refusing in its own words on the last field, where the
    /// face is already drawn `(next field)` to say the operation does not apply. Writing the
    /// recipe is the control beside it, deliberately (`kActionNextField`).
    void next_field(loom::Mail& mail) {
        if (!authoring_.open) {
            return; // the mode closed under the gesture: no field to keep, and nothing to step
        }
        if (authoring_.step + 1 >= field_count()) {
            notice_ = std::string(field_name(authoring_.chosen.tree, authoring_.chosen.multi_config,
                                             authoring_.step)) +
                      " is the last field -- `write the recipe` writes the draft";
            say(mail);
            return;
        }
        if (!record_field(mail)) {
            return;
        }
        load_field(authoring_.step + 1);
        say(mail);
    }

    /// Commit this field and step to the next, and from the last one write the recipe: Return.
    void authoring_commit(loom::Mail& mail) {
        if (!authoring_.open) {
            return;
        }
        if (!record_field(mail)) {
            return;
        }
        if (authoring_.step + 1 < field_count()) {
            load_field(authoring_.step + 1);
            say(mail);
            return;
        }
        compose_recipe(mail);
    }

    /// WRITE THE WHOLE DRAFT FROM WHEREVER THE LINE IS STANDING. The field in hand is committed
    /// first, then every required one is asked for; a missing one is named and nothing is sent.
    void write_recipe(loom::Mail& mail) {
        if (!authoring_.open) {
            return; // the draft is gone: a second ask for the same write writes nothing twice
        }
        if (!record_field(mail)) {
            return;
        }
        for (std::size_t i = 0; i < field_count(); ++i) {
            const Field& field = field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, i);
            if (field.required && (!authoring_.filled[i] || authoring_.values[i].empty())) {
                notice_ = std::string(field.name) + " is required -- nothing was written";
                say(mail);
                return;
            }
        }
        compose_recipe(mail);
    }

    /// THE DRAFT, COMPOSED AND HANDED TO THE RECIPES DOOR. What a maker typed is a DRAFT; the
    /// host composes, checks and installs it (WL-AUTH-01), and this pane hears the outcome.
    void compose_recipe(loom::Mail& mail) {
        Authoring& a = authoring_;
        RecipeAuthorRequested draft;
        draft.tree = a.chosen.tree;
        draft.id = a.values[0];
        const std::string place = ws::persist::resolved_against(a.dir, a.chosen.name);
        if (!draft.tree) {
            draft.artifact = a.values[1];
            draft.source = place;
            draft.packages = split_list(a.values[2]);
            draft.links = split_list(a.values[3]);
        } else {
            draft.target = a.values[1];
            draft.artifact = a.values[2];
            draft.build_dir = place;
            draft.artifact_dir = a.values[3];
            if (a.chosen.multi_config) {
                draft.config = a.values[4];
            }
        }
        authoring_ = Authoring{};
        declare(mail); // the browser's rows are in force again
        recipes_.pending = ++asked_;
        recipes_.awaiting = true;
        recipes_.was_author = true;
        recipes_.id = draft.id;
        recipes_.artifact = draft.artifact;
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(ws::kRecipesRole, draft, recipes_.pending);
    }

    // ---- The chooser's typed fields ----------------------------------------------------------

    struct Field {
        const char* name;
        bool required;
        // Empty means the same as `name`: every field but one is short enough to be its own menu
        // row (`kMaxPaneMenuLabelLen`, 64 bytes, with the longest prefix this pane composes onto
        // it). Read through `field_menu_label`, the one place the fallback is spelled.
        const char* menu_label = nullptr;
    };
    // The fifth field is offered only when the tree says several configurations coexist there
    // (`cache_is_multi_config`): ask for the few things nothing can detect. `name` carries the
    // why and is pane-local text, clipped to the pane's width; `menu_label` carries the what and
    // crosses as a `PaneMenuRow`, where one row over 64 bytes makes the presenter refuse the
    // whole offer (`menu-presenter/presenter.cpp`'s `refusal_of`) -- every menu this form offers.
    static const Field& field_at(bool tree, bool multi_config, std::size_t step) {
        static constexpr Field kSource[] = {{"recipe name", true},
                                            {"artifact stem", true},
                                            {"package prefix (comma-separated)", true},
                                            {"link targets (comma-separated)", true}};
        static constexpr Field kTree[] = {{"recipe name", true},
                                          {"cmake target", true},
                                          {"artifact stem", true},
                                          {"artifact directory (optional)", false}};
        static constexpr Field kConfig = {"configuration (this tree builds several; cmake "
                                          "--build needs one)",
                                          true, "configuration"};
        if (tree && multi_config && step == 4) {
            return kConfig;
        }
        return tree ? kTree[step] : kSource[step];
    }
    static const char* field_name(bool tree, bool multi_config, std::size_t step) {
        return field_at(tree, multi_config, step).name;
    }
    /// THE SHORT FORM FOR A MENU ROW -- `field.menu_label` when the field declared one, `name`
    /// otherwise. The one place the fallback is spelled; every menu-row site reads through this,
    /// never `field.name` or `.menu_label` directly.
    static const char* field_menu_label(bool tree, bool multi_config, std::size_t step) {
        const Field& field = field_at(tree, multi_config, step);
        return field.menu_label ? field.menu_label : field.name;
    }
    static std::size_t field_count(bool tree, bool multi_config) {
        return (tree && multi_config) ? 5 : 4;
    }
    /// THE CANDIDATE THE AUTHORING FORM IS OPEN ON OWNS ITS OWN FIELD COUNT -- every call site
    /// below reads `authoring_.chosen`, so this is the one place that says so.
    std::size_t field_count() const {
        return field_count(authoring_.chosen.tree, authoring_.chosen.multi_config);
    }

    // ---- Clipboard (the authoring line's paste) -----------------------------------------

    static bool admissible(std::string_view text) {
        for (const char c : text) {
            const unsigned char b = static_cast<unsigned char>(c);
            if (b < 0x20u || b >= 0x7Fu) {
                return false;
            }
        }
        return true;
    }

    void begin_paste(loom::Mail& mail) {
        const loom::AskOpened opened = clip_asks_.open_to_role(
            surface::kSkinRole, surface::ClipboardTextRequested::zen_name,
            surface::ClipboardTextRequested::zen_version);
        if (!opened) {
            return;
        }
        paste_ask_ = opened.id;
        paste_epoch_ = authoring_.line.draft_epoch();
        pasting_ = true;
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(surface::kSkinRole, surface::ClipboardTextRequested{}, opened.correlation);
    }

    // ---- Saying what the pane shows -----------------------------------------------------

    /// One row of the picture, with what it means recorded as it is written: a press is answered
    /// from the record the composition made, never from a second calculation. Spelled before it
    /// is fit (`ascii_spelling`, `workshop/pane_text.hpp`): a notice can relay another owner's
    /// diagnostic verbatim, which may carry UTF-8, and `judge_content` admits a publication whole
    /// or not at all, so one unspelled byte would refuse every row this pane sent.
    void push_row(const std::string& text, std::int64_t role, FilesMeaning meaning = FilesMeaning{}) {
        if (static_cast<std::int64_t>(composing_.size()) >= rows_) {
            return; // the room ran out: a row nobody can see names nothing
        }
        if (meaning.kind != files_row::kNone) {
            map_.row(static_cast<std::int64_t>(composing_.size()), std::move(meaning));
        }
        composing_.push_back(surface::SurfaceTextRow{fit(ascii_spelling(text), columns_), role});
    }

    /// The whole picture. The notice leads and is composed first, since the row map records
    /// absolute rows (`body_budget` already asked the mode for one fewer). It is cleared by the
    /// maker's next act, not by being said (`agents/panes.md`): one gesture can publish several
    /// times in one drain, Workshop keeps the last picture, and a notice cleared by the first
    /// `say` would be one no maker ever reads.
    void say(loom::Mail& mail) {
        map_.begin();
        composing_.clear();
        if (!granted_ || rows_ <= 0 || columns_ <= 0) {
            map_.settle(); // nothing is published, so no row can name anything
            return;
        }
        // THE SENTENCE IS NEVER THE THING THAT DOES NOT FIT: it is the answer to the maker's
        // last act. A one-row room keeps its header instead, which is the pane's identity and
        // where it is standing; there is nothing useful to say in one row twice.
        if (!notice_.empty() && rows_ > 1) {
            push_row(notice_, surface::role::kAccent);
        }
        if (chooser_.open) {
            say_chooser();
        } else if (authoring_.open) {
            say_authoring();
        } else {
            say_browser();
        }
        ++published_;
        ws::v3::PaneContent said;
        said.pane = files::kProjectFilesPane;
        said.rows = std::move(composing_);
        composing_.clear();
        said.picture = map_.settle();
        (void)mail.as_role(files::kFilesRole).send_to_role(kWorkshopRole, said);
    }

    /// HOW MANY ROWS THE LISTING MAY SPEND: the room, less this pane's own header, less the
    /// notice row `say` puts in front of it, less the control strip's own rows. A pane has no
    /// band to write a notice on and no chrome to hang buttons off, so both come out of the
    /// same budget -- and the composition is asked for fewer rows rather than having its last
    /// row silently dropped after the fact. The Builder's `publish` spends the same subtraction
    /// for the same reason.
    std::int64_t body_budget(std::int64_t strip_rows) const {
        return rows_ - kHeaderRows - (notice_.empty() ? 0 : 1) - strip_rows;
    }

    // ---- The controls a maker can press -------------------------------------------------

    /// ONE CONTROL: the operation it asks for, what it reads as, and whether this pane believes
    /// the operation applies. What it acts ON is `subject_of`'s answer, the one place that says
    /// so, which is why the face, the recorded span and the spend cannot disagree.
    struct ControlRow {
        const char* id;
        std::string label;
        bool available = true;
    };

    /// The browser's controls, in the order a maker reads them; `[menu]` is first, as the route to
    /// everything, and `pack_controls` never drops the first control that fits. Nothing here asks
    /// an operating system anything (WL-FILES-07): availability is read off what this pane holds,
    /// so the mark jumps are always offered and refuse in words at the gesture. An unavailable
    /// control is still drawn, as `(open)`, and pressing it answers with the operation's own
    /// refusal.
    std::vector<ControlRow> browser_controls() const {
        const FileRow* row = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
        const bool somewhere = !state_.current_dir.empty();
        const bool usable = row != nullptr && row->openable;
        std::vector<ControlRow> controls;
        controls.push_back(ControlRow{files::kActionMenu, "menu", true});
        controls.push_back(ControlRow{files::kActionOpen,
                                      row != nullptr && row->directory ? "enter" : "open",
                                      usable && somewhere});
        controls.push_back(ControlRow{files::kActionParent, "up a directory",
                                      somewhere && !ws::at_filesystem_root(state_.current_dir)});
        controls.push_back(ControlRow{files::kActionRefresh, "look again", somewhere});
        controls.push_back(ControlRow{files::kActionUseRecipes, "use as recipes",
                                      usable && !row->directory && somewhere});
        controls.push_back(ControlRow{files::kActionPickBuildable, "pick buildable",
                                      listing_.known && somewhere});
        controls.push_back(ControlRow{files::kActionMark,
                                      marks_.marked(state_.current_dir) ? "unmark here"
                                                                        : "mark here",
                                      somewhere});
        controls.push_back(ControlRow{files::kActionPreviousMark, "previous mark", true});
        controls.push_back(ControlRow{files::kActionNextMark, "next mark", true});
        return controls;
    }

    std::vector<ControlRow> chooser_controls() const {
        std::vector<ControlRow> controls;
        controls.push_back(ControlRow{files::kActionMenu, "menu", true});
        controls.push_back(ControlRow{files::kActionChoose, "author a recipe for this",
                                      chooser_.cursor < chooser_.candidates.size()});
        controls.push_back(ControlRow{files::kActionCancel, "cancel", true});
        return controls;
    }

    std::vector<ControlRow> authoring_controls() const {
        std::vector<ControlRow> controls;
        controls.push_back(ControlRow{files::kActionMenu, "menu", true});
        controls.push_back(ControlRow{files::kActionNextField, "next field",
                                      authoring_.step + 1 < field_count()});
        controls.push_back(ControlRow{files::kActionWriteRecipe, "write the recipe", filled_in()});
        controls.push_back(ControlRow{files::kActionCancel, "abandon", true});
        return controls;
    }

    /// How many rows the strip may spend in the room this pane has. The controls do not get to
    /// eat the pane: the strip grows with the room -- one row until the pane has eight, two until
    /// eleven, three after that -- and what does not fit is counted and reachable through
    /// `[menu]`. A room too small for one strip row leaves the right press, which opens the same
    /// rows wherever the hand is.
    std::int64_t strip_budget() const {
        if (rows_ < 2) {
            return 0;
        }
        const std::int64_t want = (rows_ - 2) / 3;
        return want < 1 ? 1 : (want > kMaxControlRows ? kMaxControlRows : want);
    }

    component::ControlStrip packed(const std::vector<ControlRow>& controls) const {
        std::vector<component::Control> faces;
        faces.reserve(controls.size());
        for (const ControlRow& control : controls) {
            faces.push_back(component::Control{control.label, control.available});
        }
        return component::pack_controls(faces, columns_, strip_budget());
    }

    /// HOW MANY ROWS A STRIP OF THESE CONTROLS WOULD TAKE -- asked before the body is laid out,
    /// so the listing is given what is genuinely left rather than losing its tail afterwards.
    std::int64_t strip_rows_for(const std::vector<ControlRow>& controls) const {
        return static_cast<std::int64_t>(packed(controls).rows.size());
    }

    /// DRAW THE STRIP AND RECORD EVERY FACE AS A TARGET. A face the width cut is not recorded
    /// (`RowMap::span` refuses it): a press on the `...` a cut left behind must not operate a
    /// control the maker cannot read. What did not fit is counted on the last strip row, and
    /// the route to it is `[menu]`, never only a key.
    void say_controls(const std::vector<ControlRow>& controls) {
        const component::ControlStrip strip = packed(controls);
        for (std::size_t i = 0; i < strip.rows.size(); ++i) {
            std::string text = strip.rows[i];
            if (i + 1 == strip.rows.size() && strip.dropped > 0) {
                text += "  +" + std::to_string(strip.dropped) + " in menu";
            }
            const std::int64_t row = static_cast<std::int64_t>(composing_.size());
            push_row(text, surface::role::kFill);
            if (static_cast<std::int64_t>(composing_.size()) == row) {
                return; // the room ran out before this strip row: nothing below it is a target
            }
            const std::int64_t solid = component::solid_columns(
                composing_[static_cast<std::size_t>(row)].text, text.size());
            for (const component::PlacedControl& placed : strip.placed) {
                if (placed.row != static_cast<std::int64_t>(i)) {
                    continue;
                }
                const ControlRow& control = controls[placed.index];
                map_.span(row, placed.first, placed.width, solid,
                          FilesMeaning{files_row::kControl, 0, control.id, {}});
            }
        }
    }

    void say_browser() {
        const std::string why = ws::provenance_words(marks_.provenance(state_.current_dir));
        std::string header = "Files";
        const std::size_t total = listing_.rows.size();
        if (!settled_) {
            header += " (waiting)";
        } else if (!listing_.known) {
            header += " --";
        } else if (total == 0) {
            header += " empty";
        } else {
            const std::size_t at = static_cast<std::size_t>(state_.cursor) < total
                                       ? static_cast<std::size_t>(state_.cursor) + 1 : total;
            header += " " + std::to_string(at) + "/" + std::to_string(total);
            if (listing_.bounded) {
                header += "+ (stopped counting)";
            }
        }
        if (!why.empty()) {
            header += "  " + why;
        }
        header += "  " + where();
        push_row(header, surface::role::kAccent);
        const std::vector<ControlRow> controls = browser_controls();
        const std::int64_t body_rows = body_budget(strip_rows_for(controls));
        if (body_rows > 0) {
            if (!listing_.known) {
                push_row(listing_.refusal.empty() ? "nothing has been listed yet"
                                                  : listing_.refusal,
                         surface::role::kMuted);
            } else if (total == 0) {
                push_row("this directory is empty", surface::role::kMuted);
            } else {
                say_entries(total, static_cast<std::size_t>(body_rows));
            }
        }
        say_controls(controls);
    }

    /// The listing through a window that moves as little as it can (`component::cursor_window`).
    /// Least motion, not centring: a window re-laid on every selection scrolled the rows out from
    /// under the pressing hand, and the picture fence would refuse a double-click's second press.
    /// The markers are rows of the same budget, and one entry is always seated.
    void say_entries(std::size_t total, std::size_t body_rows) {
        const component::ListWindow win = component::cursor_window(
            total, static_cast<std::size_t>(state_.cursor), window_hint_, body_rows);
        window_hint_ = win.first;
        // A marker is a row of the same budget, said only where the window reserved one
        // (`ListWindow::markers`): saying it anyway overran the budget and pushed the control
        // strip, the one row a maker with a mouse cannot lose, out of the room.
        if (win.before > 0 && win.markers > 0) {
            push_row("  ... " + std::to_string(win.before) + " earlier", surface::role::kMuted);
        }
        for (std::size_t i = win.first; i < win.end(); ++i) {
            const bool here = i == static_cast<std::size_t>(state_.cursor);
            const FileRow& row = listing_.rows[i];
            push_row(std::string(here ? "> " : "  ") + row_text(row),
                     here ? surface::role::kAccent
                          : (row.openable ? surface::role::kFill : surface::role::kMuted),
                     FilesMeaning{files_row::kEntry, i, {}, row.name});
        }
        if (win.after > 0 && win.markers > 0) {
            push_row("  ... " + std::to_string(win.after) + " more", surface::role::kMuted);
        }
    }

    void say_chooser() {
        push_row("pick something buildable -- " + std::to_string(chooser_.candidates.size()) +
                     (chooser_.candidates.size() == 1 ? " candidate" : " candidates"),
                 surface::role::kAccent);
        const std::vector<ControlRow> controls = chooser_controls();
        const std::int64_t body_rows = body_budget(strip_rows_for(controls));
        if (body_rows > 0) {
            const component::ListWindow win =
                component::cursor_window(chooser_.candidates.size(), chooser_.cursor,
                                         chooser_hint_, static_cast<std::size_t>(body_rows));
            chooser_hint_ = win.first;
            if (win.before > 0 && win.markers > 0) {
                push_row("  ... " + std::to_string(win.before) + " earlier",
                         surface::role::kMuted);
            }
            for (std::size_t i = win.first; i < win.end(); ++i) {
                const bool here = i == chooser_.cursor;
                const BuildCandidate& c = chooser_.candidates[i];
                push_row(std::string(here ? "> " : "  ") + c.name +
                             (c.tree ? "/  (configured tree)" : ""),
                         here ? surface::role::kAccent : surface::role::kFill,
                         FilesMeaning{files_row::kCandidate, i, {}, c.name});
            }
            if (win.after > 0 && win.markers > 0) {
                push_row("  ... " + std::to_string(win.after) + " more", surface::role::kMuted);
            }
        }
        say_controls(controls);
    }

    /// The authoring line and the fields it walks. Every field is shown and any may be stood on;
    /// what is written is still one draft the host composes, checks and installs (WL-AUTH-01,
    /// WL-FILES-15). The pane protocol carries no caret, so the line shows none, but its window
    /// follows the caret column and a press places it. The fields go through the listing's own
    /// window, which keeps the field being typed into on screen; where the room cannot seat a
    /// neighbour, the menu offers a row for every field (`offer_field`).
    void say_authoring() {
        push_row(std::string("author `") + authoring_.chosen.name + "` -- " +
                     (authoring_.chosen.tree ? "a configured tree" : "a source file"),
                 surface::role::kAccent);
        const std::vector<ControlRow> controls = authoring_controls();
        const std::int64_t body_rows = body_budget(strip_rows_for(controls));
        if (body_rows > 0) {
            const component::ListWindow win =
                component::cursor_window(field_count(), authoring_.step, field_hint_,
                                         static_cast<std::size_t>(body_rows));
            field_hint_ = win.first;
            std::int64_t spent = 0;
            if (win.before > 0 && win.markers > 0) {
                push_row("  ... " + std::to_string(win.before) + " earlier", surface::role::kMuted);
                ++spent;
            }
            for (std::size_t i = win.first; i < win.end(); ++i) {
                say_field(i);
                ++spent;
            }
            if (win.after > 0 && win.markers > 0) {
                push_row("  ... " + std::to_string(win.after) + " more", surface::role::kMuted);
                ++spent;
            }
            // A cut the window could not reserve a marker for (`ListWindow::unsaid_cut`) still has
            // a row here, and it names the route: every field is in the menu. It wears no
            // control's face: brackets are the face vocabulary, and only `pack_controls` writes
            // them (`component/control_strip.hpp`).
            if (win.unsaid_cut() && spent < body_rows) {
                push_row("  ... " + std::to_string(win.before + win.after) +
                             " more fields -- this pane's menu names every one",
                         surface::role::kMuted);
            }
        }
        say_controls(controls);
    }

    /// The active field's prompt, as drawn: the full label wherever the room leaves the value
    /// `kMinFieldValueColumns` beside it, and a shortened label in a narrower room, since the
    /// value is what a maker is reading. Read here and nowhere else, so painting (`say_field`)
    /// and the press that turns a column back into a caret measure from the same text.
    std::string active_prompt() const {
        return fitted_label(authoring_.prompt, columns_, kMinFieldValueColumns);
    }

    /// ONE AUTHORING FIELD'S ROW: the line itself where the maker is standing, and what the
    /// field holds everywhere else. Either is a press target -- the line places the caret, a
    /// held field stands the line on itself.
    void say_field(std::size_t i) {
        const Field& field = field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, i);
        if (i == authoring_.step) {
            const std::string prompt = active_prompt();
            const std::int64_t prompt_cols = static_cast<std::int64_t>(prompt.size());
            const std::int64_t cols = columns_ > prompt_cols + 1 ? columns_ - prompt_cols - 1 : 1;
            authoring_.line.keep_caret_visible(cols);
            push_row(prompt + authoring_.line.visible(cols), surface::role::kAccent,
                     FilesMeaning{files_row::kLine, i, {}, field.name});
            return;
        }
        const std::string held = authoring_.filled[i] && !authoring_.values[i].empty()
                                     ? authoring_.values[i]
                                     : std::string(field.required ? "(required)" : "(none)");
        push_row("  " + std::string(field.name) + ": " + held, surface::role::kMuted,
                 FilesMeaning{files_row::kField, i, {}, field.name});
    }

    // ---- State not in the shape ---------------------------------------------------------

    zengine::ActivationCursor activation_;
    std::uint64_t asked_ = 0;

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
    static constexpr std::int64_t kHeaderRows = 1;

    bool settled_ = false;
    /// HOW MANY BUILDS THE TOOL HAD BEEN ASKED FOR when this pane last heard from it, and
    /// whether it has heard at all -- the two facts that tell a finished build from an answer.
    std::int64_t builds_seen_ = 0;
    bool saw_status_ = false;
    std::string marks_path_;
    bool marks_refused_ = false;
    LocationMarks marks_;
    Listing listing_;
    double wheel_accum_ = 0.0;
    std::string notice_;
    /// HOW MANY PICTURES THIS PANE HAS PUBLISHED -- counted by `say` at the send, so a handler
    /// can tell whether the act it ran already said its rows.
    std::uint64_t published_ = 0;
    /// WHAT EACH ROW AND EACH RUN OF COLUMNS IN THE LAST PICTURE MEANS, and the number of that
    /// picture -- replaced whole by every `say`, the room grant's included. A press is answered
    /// from this record and from nowhere else, and one that names an older number is refused.
    component::RowMap<FilesMeaning> map_;
    /// THE ROWS BEING COMPOSED, held while `say` runs so each mode's composer and the control
    /// strip write into one list and the map records the row each of them landed on.
    std::vector<surface::SurfaceTextRow> composing_;
    /// WHERE THE LISTING'S WINDOW BEGAN LAST TIME, and the chooser's -- what makes the window
    /// move by the least it can rather than re-centring under a maker's hand. Derived, never
    /// kept across a reload: a fresh image re-derives it from the cursor on its first paint.
    std::size_t window_hint_ = 0;
    std::size_t chooser_hint_ = 0;
    /// ...and where the authoring fields' window began, for the same reason: a short pane
    /// re-laying its field rows under a typing hand would move the line on every character.
    std::size_t field_hint_ = 0;
    /// THIS IMAGE'S ONE OUTSTANDING MENU. Deliberately not reload-kept state: a successor that
    /// inherited it would accept its predecessor's menu as its own (`pane_menu::Asked`).
    pane_menu::Asked asked_menu_;
    /// THE ROW A SETTLED MENU ANSWER NAMED, read by `chose` -- held for the length of one
    /// delivery and never longer.
    std::string chosen_id_;

    Ask root_;
    Ask open_;
    struct Recipes : Ask {
        bool was_author = false;
        std::string id;       ///< what the maker called the row, for the accepted sentence
        std::string artifact; ///< and the stem it produces
    } recipes_;

    struct Chooser {
        bool open = false;
        std::size_t cursor = 0;
        std::string dir;
        std::vector<BuildCandidate> candidates;
    } chooser_;

    /// The draft a maker is typing: which candidate it is about, every field's value, which have
    /// been answered at least once, and which one the line is standing on. Values are held for
    /// every field from the start (`field_count`), not pushed per commit, so a maker can go back
    /// to a field and still write the same one draft.
    struct Authoring {
        bool open = false;
        BuildCandidate chosen;
        std::string dir;
        std::string prompt;
        component::TextBox line;
        std::size_t step = 0;
        std::vector<std::string> values;
        std::vector<bool> filled;
    } authoring_;

    component::Clipboard clip_;
    loom::AskBook clip_asks_{1};
    std::uint64_t paste_ask_ = 0;
    std::uint64_t paste_epoch_ = 0;
    bool pasting_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(FilesWeave)
