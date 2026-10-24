// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Files tool: a loadable weave that offers Workshop one pane -- a browser over the machine,
// the marks a weaver keeps, and the two authored files a weaver writes from it. What it needs of
// the host (where this run began, where its marks live, whether a file is a recipe catalog, one
// path opened in the Editor) it asks for through the doors `workshop/pane_seam_vocabulary.hpp`
// spells (WL-FILES-17); what crosses is values. It owns its listing, its marks and its two
// modes, and spends the pure half (`files.hpp`, `marks.hpp`, `marks_persist.hpp`).
// Files law: agents/workshop/files.md

#include "files/vocabulary.hpp"

#include "files/files.hpp"
#include "files/filesystem_roots.hpp"
#include "workshop/actor_scope.hpp" // the class an act that opens or writes is judged as
#include "workshop/open_seam_vocabulary.hpp" // the opening office the open is asked of
#include "workshop/pane_causes.hpp"
#include "workshop/pane_facets.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_settings.hpp"
#include "workshop/pane_settings_taken.hpp"
#include "workshop/pane_text.hpp"
#include "files/marks_persist.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/path_admission.hpp"
#include "workshop/persist.hpp"

#include "workshop/pane_menu.hpp"
#include "workshop/pane_escape.hpp"

#include "app/vocabulary.hpp"
#include "builder/vocabulary.hpp"
#include "launcher/configured.hpp"
#include "launcher/vocabulary.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "component/text_box.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "workshop/pane_rows.hpp"

#include "manuals/zengine.files.hpp" // this pane's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
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
namespace gfx = zengine::graphics;
namespace ws = zengine::workshop;
namespace files = zengine::files;
namespace pane_menu = zengine::workshop::pane_menu;

using ws::AdmittedName;
using ws::FileRow;
using ws::Listing;
using ws::LocationMarks;
using ws::MarkedPlace;
using ws::OpenSourceRequested;
using ws::v3::PaneActionRow;
using ws::PaneCatalogRequested;
using ws::PaneKey;
using ws::v2::PaneOffered;
using ws::PanePassRequested;
using ws::PaneTextInput;
using ws::ProjectRoot;
using ws::ProjectRootRequested;
using ws::RecipeAuthorRequested;
using ws::RecipeOutcome;
using ws::RecipeUseRequested;
using ws::SourceOpened;

/// The office Workshop holds, named as a string rather than through `workshop/panes.hpp`: a
/// provider is a stranger to Workshop's internals.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// How many rows the wheel is worth per notch in a cursor-windowed list, unless a layout keeps
/// another, and the fewest and most it may keep.
constexpr std::int64_t kFilesWheelRows = 3;
constexpr std::int64_t kFilesWheelRowsLow = 1;
constexpr std::int64_t kFilesWheelRowsHigh = 24;

/// The shared pane text helpers (`workshop/pane_text.hpp`); `judge_content` is why the fit must
/// be applied at all.
using zengine::workshop::pane_text::ascii_spelling;
using zengine::workshop::pane_text::fit;

/// A build candidate the chooser holds: a place, and which of the two recipe kinds it may be
/// tried as. Local: it is the pane's mode state and crosses no wire.
struct BuildCandidate {
    std::string name;
    bool tree = false;
    bool multi_config = false;
};

/// How a menu's subject carries two facts across the seam: `PaneMenuAnswered` echoes one string,
/// and an answer must re-establish both the place the menu was opened in and its row. A unit
/// separator is a byte no name a press can open carries (`carried_name` refuses ASCII control
/// bytes), and a subject that splits otherwise names nothing; the desktop spells the same join.
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
/// at what a weaver could see, so rows that moved between the aim and the delivery are said,
/// never spent on whatever slid into that place.

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

/// ONE THING THE PANE ASKED AND STILL WAITS ON (`AskFacet`): an act waiting on Workshop's word,
/// with everything it acts on captured at the ask, so the answer does exactly what the gesture
/// asked for wherever the browser has walked since; or an ask of an office, with what its answer
/// is read against.
struct FilesAsk {
    enum class Kind {
        kOpen, kUseRecipes, kMark, kAuthor, kStart, ///< acts waiting on Workshop's word
        kRoot, kOpening, kRecipes, kLaunch          ///< the project, opening and launcher's asks
    };
    Kind kind = Kind::kRoot;
    std::string subject;         ///< the entry or candidate a refusal names
    std::string path;            ///< the file opened or taken as recipes, resolved
    std::string place;           ///< the place marked, the directory the draft is about, or the
                                 ///< kind a start names
    RecipeAuthorRequested draft; ///< the draft as it stood at the ask
    bool was_author = false;     ///< the recipes ask: a written row, not a catalog taken
    std::string id;              ///< ...what the weaver called the row, for the accepted sentence
    std::string artifact;        ///< ...and the stem it produces
};

/// An ask of `kind`, nothing captured yet.
inline FilesAsk asking(FilesAsk::Kind kind) {
    FilesAsk a;
    a.kind = kind;
    return a;
}

/// Which asks a kind is: the matcher every lookup in the table reads.
inline auto of_kind(FilesAsk::Kind kind) {
    return [kind](const FilesAsk& a) { return a.kind == kind; };
}

// =============================================================================
// The weave
// =============================================================================

using Rows = ws::PaneRowsFacet<files::kProjectFilesPane, files::kFilesRole,
                               zengine::manual::pages::zengine_files::kManual, FilesMeaning>;
using Asks = ws::AskFacet<files::kProjectFilesPane, files::kFilesRole, FilesAsk>;
using Actions = ws::PaneActionFacet<files::kProjectFilesPane, files::kFilesRole>;
using Menu = ws::PaneMenuFacet<files::kProjectFilesPane, files::kFilesRole>;

class FilesWeave
    : public zengine::With<Rows, Asks, Actions, Menu>::Weave<
          FilesWeave, files::FilesState,
          loom::Accept<PaneKey, PaneTextInput, ProjectRoot, RecipeOutcome, SourceOpened,
                       zengine::builder::BuildStatus, zengine::launcher::Launched, gfx::ClipboardCopy,
                       gfx::ClipboardText, ws::v2::PaneSettings>,
          loom::Emit<ProjectRootRequested, RecipeUseRequested, RecipeAuthorRequested,
                     OpenSourceRequested, zengine::builder::StatusRequested, zengine::launcher::Launch,
                     ws::PaneKeyboardRequested, ws::PaneEscapeUnspent, gfx::ClipboardCopy,
                     gfx::ClipboardTextRequested, ws::v2::PaneSettingsDeclared,
                     ws::PaneSettingUnused>> {
public:
    PaneOffered offering() const {
        return PaneOffered{files::kProjectFilesPane, files::kProjectFilesName,
                           files::kProjectFilesSummary, 9, 56};
    }

    /// The rows the pane answers to, and the settings it takes.
    void offered(loom::Mail& mail) {
        declare(mail);
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(kWorkshopRole,
                          ws::v2::PaneSettingsDeclared{files::kProjectFilesPane, setting_rows()});
    }

    /// THE SETTINGS THIS LAYOUT KEEPS FOR THE PANE, taken whole; what it cannot use is said back
    /// against the revision it was handed at. A listing held to fewer or more entries is walked
    /// again, keeping the row the weaver was on. Held as plain members, never reload state.
    void on(const ws::v2::PaneSettings& handed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || handed.pane != files::kProjectFilesPane) {
            return;
        }
        const ws::SettingsTaken taken = ws::take_pane_settings(setting_rows(), handed);
        ws::say_settings_unused(mail, files::kFilesRole, taken);
        wheel_rows_ = taken.number(files::kSettingWheelRows);
        const auto listed = static_cast<std::size_t>(taken.number(files::kSettingListedEntries));
        if (listed != listed_entries_) {
            listed_entries_ = listed;
            if (settled_) {
                relist();
                say(mail); // draws nothing until a room is granted, so the first picture has it
            }
        }
    }

    /// The window gives the pane's area its room, the one beat on which this tool draws. The listing is re-enumerated on it
    /// (WL-FILES-12); with no origin yet, it is when the browser first asks the host where it
    /// began. A grant is not a weaver's act, so it keeps the cursor: a room moves for resizes,
    /// dragged edges and a title row returned right behind a selecting press.
    void on_room(loom::Mail& mail) {
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
    /// when this run has nowhere else, and the durable marks are read. A place a reload carried
    /// keeps the row the weaver was on: only a seeded place starts at its top.
    void on(const ProjectRoot& said, loom::Mail& mail) {
        if (!asks().answered(mail, of_kind(FilesAsk::Kind::kRoot))) {
            return;
        }
        settled_ = true;
        marks_path_ = said.marks_path;
        marks_.origin = ws::admit_location(said.project_dir);
        load_marks();
        const bool carried = !state_.current_dir.empty();
        const std::int64_t cursor = state_.cursor;
        if (!carried) {
            state_.current_dir = marks_.origin;
        }
        refresh();
        if (carried) {
            state_.cursor = cursor;
        }
        say(mail);
    }


    // WL-FOCUS-04 -- agents/workshop/focus.md
    /// A press the window landed on another row than the one the rows show there now is refused in
    /// words and spends nothing.
    void press(const ws::RowPress& at, loom::Mail& mail) {
        if (at.moved) {
            notice_ = "the rows moved -- press again";
            say(mail);
            return;
        }
        const std::int64_t row = at.row, column = at.column;
        const bool keys_went_here = at.keys_went_here;
        // THE PRESS IS READ AGAINST THE PICTURE IT WAS AIMED AT, before anything below changes the
        // next one: spending the notice moves every row up, and a new cursor moves the window.
        const FilesMeaning* m = room().map().at(row, column);
        if (m == nullptr || m->kind == files_row::kNone) {
            return; // the notice, the header, a marker row, or blank space names nothing
        }
        // THE WEAVER HAS ACTED, SO THE LAST ACT'S ANSWER IS SPENT -- in the rows Workshop holds,
        // too (`on_action` says why an open needs the saying).
        room().spend_notice(
            notice_,
            [&] {
                if (m->kind == files_row::kControl) {
                    perform(m->id, mail);
                } else if (m->kind == files_row::kEntry && !chooser_.open && !authoring_.open) {
                    // A PRESS ON THE ALREADY-SELECTED ROW ACTIVATES IT, AND ONLY WHERE THE KEYS
                    // ALREADY WERE: the press that brings the keys to this pane is a weaver
                    // pointing at it, not an act in it.
                    if (keys_went_here && m->index == static_cast<std::size_t>(state_.cursor)) {
                        open(mail);
                    } else {
                        state_.cursor = static_cast<std::int64_t>(m->index);
                        say(mail);
                    }
                } else if (m->kind == files_row::kCandidate && chooser_.open) {
                    // THE CHOOSER'S OWN SECOND PRESS: the first names the candidate, the second
                    // authors it -- the browser's rule, one mode over, so no press means two
                    // things at once. And only where the keys already were (WL-FOCUS-04): the
                    // press that brings them here is a weaver pointing at the pane, not a weaver
                    // choosing a candidate in it.
                    if (keys_went_here && m->index == chooser_.cursor) {
                        chooser_choose(mail);
                    } else {
                        chooser_.cursor = m->index;
                        say(mail);
                    }
                } else if (m->kind == files_row::kField && authoring_.open) {
                    edit_field(m->index, mail);
                } else if (m->kind == files_row::kLine && authoring_.open) {
                    // THE CARET GOES WHERE THE HAND IS. The prompt is drawn in front of the text,
                    // so the column a press names is measured from the first column the text
                    // occupies -- the prompt AS DRAWN (`active_prompt`), which a narrow room may
                    // have shortened from `authoring_.prompt`'s full label.
                    const std::int64_t prompt = static_cast<std::int64_t>(active_prompt().size());
                    authoring_.line.place(authoring_.line.position_at_column(column - prompt));
                    say(mail);
                }
            },
            [&] { say(mail); });
    }

    /// THE SECOND BUTTON. A right press on a row this pane owns OFFERS that row's menu, beside
    /// the press, continuing it; a right press on anything else -- the header, a marker, blank
    /// space -- is handed back to the host, whose own pane menu answers (WL-CTX-08). A middle
    /// press means nothing here and is consumed.
    bool right_press(const FilesMeaning& m, const ws::RowPress& at, loom::Mail& mail) {
        if (m.kind == files_row::kNone) {
            return false;
        }
        notice_.clear();
        offer_for(m, at.row, at.column, mail.correlation(), mail);
        say(mail);
        return true;
    }

    /// WHAT A MENU CAME TO -- one of THIS image's own asks, settled by `PaneMenuFacet`: a choice
    /// counts only from the presenter's office, under the number of an ask this image sent and
    /// has not heard answered, about the pane and subject it asked about, once. What the row
    /// MEANS is judged here, against what this pane holds now -- the listing may have been walked
    /// again while the menu was open.
    void on_menu(const std::string& id, const std::string& subject, loom::Mail& mail) {
        chose(id, subject, mail);
    }

    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != files::kProjectFilesPane) {
            return;
        }
        // Escape's default: the cursor is where the list rests, not a selection, so Escape goes
        // back to Workshop.
        if (ws::pane_escape::answer(key, mail, files::kFilesRole, [] { return false; })) {
            return;
        }
        // ONLY THE AUTHORING LINE READS RAW KEYS. Everything else this pane does arrives as
        // a resolved id (`on_action`); a component's editing gestures are the component's, not
        // the pane's commands.
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
            mail.publish(gfx::ClipboardCopy{clip_.text});
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

    void wheel(double dy, loom::Mail& mail) {
        if (authoring_.open) {
            return; // a line has one row: there is nothing for a notch to walk
        }
        const std::size_t population =
            chooser_.open ? chooser_.candidates.size()
                          : (listing_.known ? listing_.rows.size() : std::size_t{0});
        if (population == 0) {
            return;
        }
        const std::int64_t notches = component::take_wheel_notches(
            wheel_accum_, dy * static_cast<double>(wheel_rows_));
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

    /// ONE OF THE PANE'S DECLARED ACTIONS, ASKED FOR BY NAME (WL-KEY-15): the id Workshop resolved
    /// the keystroke to against the effective keymap, admitted by `answers` in the pane's current
    /// mode. Each mode's operations have ids of their own (Return is `files.choose`,
    /// `files.commit-field` or `files.open`), so an id resolved in one mode and delivered in the
    /// next is refused, never the next mode's operation.
    void on_action(const std::string& id, loom::Mail& mail) {
        // A key and a control reach one operation through the same `perform`, against the subject
        // `subject_of` names, so a remapped key and the button beside it cannot mean two things.
        // THE LAST ACT'S ANSWER IS SPENT, and gone from the rows Workshop holds: an act whose
        // answer is still on its way (Workshop's word on an act that opens or writes, then the
        // opening or project office's), or that meant nothing, says no picture, and the rows are
        // said once without the notice.
        room().spend_notice(
            notice_,
            [&] {
                if (id == files::kActionUp) {
                    step(-1, mail);
                } else if (id == files::kActionDown) {
                    step(1, mail);
                } else {
                    perform(id, mail);
                }
            },
            [&] { say(mail); });
    }

    /// A MODE OWNS THE PANE'S ACTIONS FIRST, AND AN ID IT DOES NOT ANSWER TO IS NO ACT. The
    /// declaration and the keystroke race across two messages -- Escape twice in one poll
    /// resolves to two cancels against the authoring line's rows, and the second arrives when
    /// the browser declares none -- so a stale id, or one nobody declared, spends nothing: no
    /// act, and the notice stands (`agents/panes.md`).
    bool answers(const std::string& id) const {
        for (const PaneActionRow& row : action_rows()) {
            if (row.id == id) {
                return true;
            }
        }
        return false;
    }

    /// WHAT AN ID ACTS ON, RIGHT NOW -- the one place that says so, read when a control is
    /// painted, when a menu is offered, and again when either is spent. An id with no subject
    /// of its own answers empty and is never subject-checked.
    std::string subject_of(const std::string& id) const {
        if (id == files::kActionOpen || id == files::kActionUseRecipes ||
            id == files::kActionStart) {
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
    /// control is not: a control acts on what the pane shows as selected, which only the weaver's
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
               id == files::kActionChoose || id == files::kActionStart;
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
        } else if (id == files::kActionStart) {
            start(mail);
        } else if (id == files::kActionDemos) {
            demos(mail);
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
            // A WRITE WAITING ON WORKSHOP GOES WITH ITS DRAFT.
            (void)asks().forget(of_kind(FilesAsk::Kind::kAuthor));
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
            const std::int64_t at = room().map().row_of(
                FilesMeaning{files_row::kCandidate, chooser_.cursor, {},
                             chooser_.cursor < chooser_.candidates.size()
                                 ? chooser_.candidates[chooser_.cursor].name
                                 : std::string()});
            row = at < 0 ? 0 : at;
        } else if (authoring_.open) {
            const Field& field =
                field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, authoring_.step);
            const std::int64_t at = room().map().row_of(
                FilesMeaning{files_row::kLine, authoring_.step, {}, field.name});
            row = at < 0 ? 0 : at;
        } else {
            const FileRow* here = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
            const std::int64_t at =
                here == nullptr
                    ? -1
                    : room().map().row_of(FilesMeaning{files_row::kEntry,
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
            if (const auto launch = startable()) {
                offer.row(files::kMenuStart, "start the " + launch->kind + " `" + name + "`");
            }
        }
        if (*zengine::launcher::configured::kDemosFolder != '\0') {
            offer.row(files::kMenuDemos, "go to the demos");
        }
        offer.row(files::kMenuPickBuildable, "pick something buildable here");
        offer.row(files::kMenuMark, marks_.marked(state_.current_dir)
                                        ? "forget this place"
                                        : "mark this place");
        offer.row(files::kMenuParent, "up a directory");
        offer.row(files::kMenuRefresh, "look at this directory again");
        offer.row(files::kMenuPreviousMark, "go to the previous mark");
        offer.row(files::kMenuNextMark, "go to the next mark");
        menu().send_continuing(mail, offer, correlation);
    }

    // The chooser's and the recipe form's menus, defined in `files/recipe_authoring.hpp`.
    void offer_candidate(std::int64_t row, std::int64_t column, std::uint64_t correlation,
                         loom::Mail& mail);
    void offer_field(std::size_t which, std::int64_t row, std::int64_t column,
                     std::uint64_t correlation, loom::Mail& mail);

    /// WHAT A CHOSEN ROW `id` MEANS -- judged here, against what this pane holds NOW. The subject
    /// the presenter echoed is the place or the candidate the menu was opened about; a menu
    /// answered after the browser walked somewhere else acts on nothing.
    void chose(const std::string& id, const std::string& said, loom::Mail& mail) {
        const std::vector<std::string> parts = split_subject(said);
        if (parts.size() != 2) {
            return; // not a subject this pane ever wrote
        }
        const std::string& place = parts[0];
        const std::string& subject = parts[1];
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
                     {files::kMenuStart, files::kActionStart},
                     {files::kMenuDemos, files::kActionDemos},
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
            // refusal rather than a retarget. A menu stands open across the weaver's other
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
    /// (WL-CTX-08), and the host refuses a grab that is no longer the weaver's latest act.
    void take_keys(loom::Mail& mail) {
        (void)pane_menu::take_keyboard(mail, files::kFilesRole, files::kProjectFilesPane);
    }

    /// A FILE THE PANE ASKED THE HOST TO USE OR AUTHOR (`zengine.project`), answered.
    void on(const RecipeOutcome& said, loom::Mail& mail) {
        const std::optional<FilesAsk> asked =
            asks().answered(mail, of_kind(FilesAsk::Kind::kRecipes));
        if (!asked) {
            return;
        }
        const FilesAsk& recipes = *asked;
        // Both halves in one sentence, in the order that survives the cut: what went wrong and
        // what is still in force are both owed, and the notice row is cut at the band's width, so
        // the short fixed statements go first and the owner's sentence and the path take the
        // tail. `said.path` is the office's answer about what is in force after the attempt.
        if (!said.accepted) {
            notice_ = recipes.was_author
                          ? ws::authoring_refused_words(said.refusal, said.path)
                          : ws::catalog_refused_words(said.refusal, said.path);
            say(mail);
            return;
        }
        notice_ = recipes.was_author
                      ? ws::authored_words(recipes.id, recipes.artifact, said.path,
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
        if (!asks().answered(mail, of_kind(FilesAsk::Kind::kOpening))) {
            return;
        }
        if (!said.accepted) {
            notice_ = said.refusal;
            say(mail);
        }
    }

    /// WORKSHOP'S WORD ON THE ACT THIS PANE ASKED ABOUT (`ask_approval`), judged for the hand of
    /// the gesture that brought it. Allowed, the act captured at the ask is done now and is never
    /// asked about again; refused, nothing of it happens, and the pane says why in its own row.
    void on_approval(const FilesAsk& approved, const ws::PaneOperationAnswered& answer,
                     loom::Mail& mail) {
        if (!answer.allowed) {
            notice_ = not_done(approved) + " -- " + answer.reason;
            say(mail);
            return;
        }
        switch (approved.kind) {
        case FilesAsk::Kind::kOpen: open_file(approved, mail); return;
        case FilesAsk::Kind::kUseRecipes: take_recipes(approved, mail); return;
        case FilesAsk::Kind::kMark: toggle_mark(approved.place, mail); return;
        case FilesAsk::Kind::kAuthor: send_recipe(approved, mail); return;
        case FilesAsk::Kind::kStart: launch(approved, mail); return;
        default: return;
        }
    }

    /// THE LAUNCHER'S ANSWER to this pane's own start: the process it started, or why not.
    void on(const zengine::launcher::Launched& said, loom::Mail& mail) {
        if (!asks().answered(mail, of_kind(FilesAsk::Kind::kLaunch))) {
            return;
        }
        const std::string after = said.kind == zengine::launcher::kKindApp
                                      ? zengine::launcher::kStartedAppGoesOn
                                      : "it goes on without Workshop";
        notice_ = said.ok ? "started the " + said.kind + " `" + said.name + "`, process " +
                                std::to_string(said.pid) + ": " + after
                          : "nothing was started -- " + said.reason;
        say(mail);
    }

    /// THE BUS'S WORD THAT ONE OF THIS PANE'S ATTEMPTS WAS REFUSED BEFORE ANY HANDLER RAN
    /// (Loom's `zen.DispatchRefused`; WL-OPEN-07), matched by `AskFacet` to the exact ask it
    /// ended: an act waiting on Workshop never asked, or the open never sent, is said in the
    /// pane's row; the project's and the recipes' asks are asked again by the next act.
    void on_refused(const FilesAsk& asked, const loom::DispatchRefused& refused,
                    loom::Mail& mail) {
        switch (asked.kind) {
        case FilesAsk::Kind::kOpening:
            notice_ = "`" + ws::shown_name(asked.subject) +
                      "` was not opened -- the open could not reach " + ws::kOpeningRole + " (" +
                      refused.reason + ")";
            break;
        case FilesAsk::Kind::kLaunch:
            notice_ = "nothing was started -- the launcher could not be asked (" + refused.reason + ")";
            break;
        case FilesAsk::Kind::kRoot:
        case FilesAsk::Kind::kRecipes: return;
        default:
            notice_ = not_done(asked) + " -- Workshop could not be asked (" + refused.reason + ")";
            break;
        }
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
        if (!settled_ || !room().granted() || zengine::builder::still_going(said.outcome)) {
            return;
        }
        if (first || said.builds == built_before) {
            return; // a description, not news: nothing on disk changed because of this
        }
        relist();
        say(mail);
    }

    void on(const gfx::ClipboardCopy& said, loom::Mail&) {
        if (admissible(said.text)) {
            clip_.text = said.text;
        }
    }

    void on(const gfx::ClipboardText& a, loom::Mail& mail) {
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

    /// THE SETTINGS THE PANE TAKES, each with its default, how a weaver finds it and what it does:
    /// the wheel's rows a notch, pinned, and the entries one listing holds.
    static std::vector<ws::v2::PaneSettingRow> setting_rows() {
        ws::v2::PaneSettingRow wheel;
        wheel.key = files::kSettingWheelRows;
        wheel.number = kFilesWheelRows;
        wheel.low = kFilesWheelRowsLow;
        wheel.high = kFilesWheelRowsHigh;
        wheel.tags = {"input", "scroll"};
        wheel.pinned = true;
        wheel.summary = "rows scrolled per wheel notch";
        ws::v2::PaneSettingRow listed;
        listed.key = files::kSettingListedEntries;
        listed.number = static_cast<std::int64_t>(ws::kMaxListedEntries);
        listed.low = static_cast<std::int64_t>(ws::kFewestListedEntries);
        listed.high = static_cast<std::int64_t>(ws::kMaxListedEntries);
        listed.tags = {"limits"};
        listed.summary = "the most entries one directory listing holds";
        return {wheel, listed};
    }

    /// What this pane answers to right now, re-declared whenever the mode changes. A mode is a
    /// declaration, not a keyboard context of Workshop's: a pane's rows join one map under its
    /// runtime handle and the collision law refuses a second row on a taken gesture, so the pane
    /// declares what is true now. While a weaver types into the authoring line, every key the mode
    /// leaves unclaimed reaches the line as a `PaneKey` (Backspace deletes a character).
    /// `PaneActions` is a replacement (WL-KEY-15), and an id is one operation in every mode.
    void declare(loom::Mail& mail) { this->template facet<Actions>().declare(mail, action_rows()); }

    /// THE ROWS OF THE MODE THIS PANE IS IN -- what `declare` tells Workshop, and what `answers`
    /// reads, so what the pane acts on and what it said it acts on are one list -- each with what
    /// its press may send once Workshop allows it: a file to open, the recipes to use or a recipe
    /// to write, and the Builder asked to say what it now holds.
    std::vector<PaneActionRow> action_rows() const {
        std::vector<PaneActionRow> rows;
        const auto row = [&rows](const char* id, const char* label, std::int64_t sc,
                                 std::int64_t mods = input::mod::kNone,
                                 std::vector<ws::ActionCause> causes = {}) {
            rows.push_back(PaneActionRow{id, label, sc, mods, {}, std::move(causes)});
        };
        const ws::ActionCause status = ws::cause_of<zengine::builder::StatusRequested>();
        const std::vector<ws::ActionCause> authored{ws::cause_of<RecipeAuthorRequested>(), status};
        // The authoring line owns the keyboard except for these rows. The two arrows are the
        // field walk, free because a single line has no row above or below; `files.write-recipe`
        // declares no default key (`kUnknown`, WL-KEY-13), reachable from its control and the
        // menu, or from a key the weaver names.
        if (authoring_.open) {
            row(files::kActionUp, "previous field", input::scan::kUp);
            row(files::kActionDown, "next field", input::scan::kDown);
            row(files::kActionCommitField, "commit this field", input::scan::kReturn,
                input::mod::kNone, authored);
            row(files::kActionNextField, "keep this field and step", input::scan::kUnknown);
            row(files::kActionWriteRecipe, "write the recipe", input::scan::kUnknown,
                input::mod::kNone, authored);
            // And the menu declares no default key while a line is open: Workshop resolves a key
            // transition before the character it produced arrives, so `Shift+M` would open the
            // menu and lose the `M` of `Main`. The `[menu]` control, first in every strip, and the
            // second button stay the route; a weaver who wants a key names `files.menu`.
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
        // ---- Browsing: the ids and default keys a weaver's keymap already names -----------
        row(files::kActionUp, "row up", input::scan::kUp);
        row(files::kActionDown, "row down", input::scan::kDown);
        row(files::kActionOpen, "enter or edit", input::scan::kReturn, input::mod::kNone,
            {ws::cause_of<OpenSourceRequested>()});
        row(files::kActionParent, "up a directory", input::scan::kBackspace);
        row(files::kActionRefresh, "look again", input::scan::kR);
        row(files::kActionUseRecipes, "use as recipes", input::scan::kU, input::mod::kNone,
            {ws::cause_of<RecipeUseRequested>(), status});
        row(files::kActionMark, "mark this place", input::scan::kM);
        row(files::kActionNextMark, "next mark", input::scan::kN);
        row(files::kActionPreviousMark, "previous mark", input::scan::kN, input::mod::kShift);
        row(files::kActionPickBuildable, "pick buildable", input::scan::kA);
        row(files::kActionStart, "start this app or demo", input::scan::kUnknown, input::mod::kNone,
            {ws::cause_of<zengine::launcher::Launch>()});
        row(files::kActionDemos, "go to the demos", input::scan::kUnknown);
        // THE KEYBOARD'S WAY TO THE ROWS A RIGHT PRESS OFFERS. `m` is already this pane's
        // mark, so the menu takes the shifted one rather than moving a key a weaver has.
        row(files::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
        return rows;
    }

    // ---- The doors ----------------------------------------------------------------------

    /// ASK WORKSHOP WHETHER THE HAND BEHIND THIS DELIVERY MAY DO AN ACT OF `action_class`, before
    /// any of it happens, under this delivery's correlation, whichever route brought the act. The
    /// ask names no send, so the class alone is judged; `subject` is what a class `open` opens.
    /// Each act waits in a bounded book under its own correlation, so another hand's act between
    /// the ask and the answer replaces nothing; a full book refuses the newest act in words.
    void ask_approval(FilesAsk next, const char* action_class, const std::string& subject,
                      loom::Mail& mail) {
        ask_approval(std::move(next), std::vector<std::string>{action_class}, subject, mail);
    }
    void ask_approval(FilesAsk next, std::vector<std::string> classes, const std::string& subject,
                      loom::Mail& mail) {
        const std::string undone = not_done(next);
        switch (
            asks().approve(mail, std::move(next), mail.correlation(), std::move(classes), subject)) {
        case Asks::Asked::kHeld: return;
        case Asks::Asked::kFull:
            notice_ = undone + " -- " + std::to_string(Asks::kApprovals) +
                      " acts are already waiting on Workshop";
            break;
        case Asks::Asked::kUnqueued: notice_ = undone + " -- Workshop could not be asked"; break;
        }
        say(mail);
    }

    /// WHAT DID NOT HAPPEN: the head of every sentence that ends an act Workshop did not approve.
    std::string not_done(const FilesAsk& a) const {
        switch (a.kind) {
        case FilesAsk::Kind::kOpen: return "`" + ws::shown_name(a.subject) + "` was not opened";
        case FilesAsk::Kind::kUseRecipes: return "the recipes in force are unchanged";
        case FilesAsk::Kind::kMark:
            return marks_.marked(a.place) ? "no mark was forgotten" : "nothing was marked";
        case FilesAsk::Kind::kAuthor: return "no recipe was written";
        case FilesAsk::Kind::kStart: return "nothing was started";
        default: break;
        }
        return "nothing was done";
    }

    /// WHERE THIS RUN BEGAN, asked of the project office; a newer ask replaces one unanswered.
    void ask_project_root(loom::Mail& mail) {
        (void)asks().forget(of_kind(FilesAsk::Kind::kRoot));
        (void)asks().ask(mail, ws::kProjectRole, ProjectRootRequested{},
                         asking(FilesAsk::Kind::kRoot));
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
        marks_.weaver = loaded.weaver;
        if (!loaded.skipped.empty()) {
            notice_ = loaded.skipped;
        }
    }

    void save_marks() {
        if (marks_path_.empty() || marks_refused_) {
            return;
        }
        const ws::Written done = ws::marks_persist::save_file(marks_path_, marks_.weaver);
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
        listing_ = ws::enumerate_directory(state_.current_dir, listed_entries_);
        state_.cursor = 0;
        wheel_accum_ = 0.0;
    }

    /// LOOK AT THE SAME PLACE AGAIN FOR A REASON THAT IS NOT THE WEAVER'S -- a room granted, a
    /// build finished -- and keep what the weaver selected: the entry the cursor named is found
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
            notice_ = ws::unopenable_name(*row) + " -- it cannot be opened";
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
        // A FILE OPENS ONLY ONCE WORKSHOP SAYS THIS GESTURE'S HAND MAY OPEN IT, judged by the
        // path the open would send; a directory above is walking, and asks nothing.
        FilesAsk next;
        next.kind = FilesAsk::Kind::kOpen;
        next.subject = row->name;
        next.path = ws::persist::resolved_against(state_.current_dir, row->name);
        const std::string path = next.path;
        ask_approval(std::move(next), ws::scope::kOpen, path, mail);
    }

    /// THE APPROVED OPEN, asked of the opening office, which arranges the document and the desk
    /// together (WL-OPEN-01); every refusal lands in this pane's own row. The ticket is kept
    /// (WL-OPEN-07): a valid one says only that the send was queued, and the bus's later word
    /// that exactly this attempt was refused reaches `on_refused`. An invalid
    /// ticket means nothing was queued, refused now rather than awaited forever.
    void open_file(const FilesAsk& approved, loom::Mail& mail) {
        (void)asks().forget(of_kind(FilesAsk::Kind::kOpening));
        FilesAsk opening = asking(FilesAsk::Kind::kOpening);
        opening.subject = approved.subject;
        if (asks().ask(mail, ws::kOpeningRole, OpenSourceRequested{approved.path},
                       std::move(opening)) != Asks::Asked::kHeld) {
            notice_ = "`" + ws::shown_name(approved.subject) +
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
        // THE MARKS FILE IS WRITTEN ONLY ONCE WORKSHOP SAYS THIS GESTURE'S HAND MAY WRITE A FILE.
        FilesAsk next;
        next.kind = FilesAsk::Kind::kMark;
        next.place = state_.current_dir;
        ask_approval(std::move(next), ws::scope::kWrite, std::string(), mail);
    }

    /// THE APPROVED MARK: the place the gesture stood in is marked, or forgotten, and saved.
    void toggle_mark(const std::string& place, loom::Mail& mail) {
        const bool removed = marks_.forget(place);
        if (!removed) {
            marks_.remember(place);
        }
        save_marks();
        notice_ = (removed ? "no longer marked: " : "marked: ") + place;
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

    // ---- Start an app or a demo (zengine.launcher) ---------------------------------------

    /// WHAT THE SELECTED ROW STARTS, if anything: a folder directly in the project's `apps/` is
    /// an app, and a setup's own folder the demo service lists is a demo. The launcher judges
    /// again whatever it is asked.
    std::optional<zengine::launcher::Launch> startable() const {
        namespace fs = std::filesystem;
        const FileRow* row = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
        if (row == nullptr || !row->directory || state_.current_dir.empty()) {
            return std::nullopt;
        }
        const auto same = [](const fs::path& a, const fs::path& b) {
            return a.lexically_normal().generic_string() == b.lexically_normal().generic_string();
        };
        const fs::path here(state_.current_dir);
        if (!marks_.origin.empty() && same(here, fs::path(marks_.origin) / zengine::app::kAppsFolder)) {
            return zengine::launcher::Launch{zengine::launcher::kKindApp, row->name};
        }
        for (const auto& [name, dir] : zengine::launcher::configured::demo_setups()) {
            if (row->name == name && same(here / row->name, fs::path(dir))) {
                return zengine::launcher::Launch{zengine::launcher::kKindDemo, row->name};
            }
        }
        return std::nullopt;
    }

    /// START WHAT THE ROW IS, asked of Workshop first as a Save and a Build are, classes `build`
    /// and `write` for the hand that pressed it; allowed, asked of the launcher.
    void start(loom::Mail& mail) {
        const auto launch = startable();
        if (!launch) {
            notice_ = "nothing here starts: an app is a folder in the project's apps/, and a demo is "
                      "one of the demos";
            say(mail);
            return;
        }
        FilesAsk next = asking(FilesAsk::Kind::kStart);
        next.subject = launch->name;
        next.place = launch->kind;
        ask_approval(std::move(next), std::vector<std::string>{ws::scope::kBuild, ws::scope::kWrite},
                     std::string(), mail);
    }

    void launch(const FilesAsk& approved, loom::Mail& mail) {
        FilesAsk asked = asking(FilesAsk::Kind::kLaunch);
        asked.subject = approved.subject;
        asked.place = approved.place;
        if (asks().ask(mail, zengine::launcher::kLauncherRole,
                       zengine::launcher::Launch{approved.place, approved.subject},
                       std::move(asked)) != Asks::Asked::kHeld) {
            notice_ = "nothing was started -- the launcher could not be asked";
        } else {
            notice_ = "starting the " + approved.place + " `" + approved.subject + "`";
        }
        say(mail);
    }

    /// THE WAY TO THE DEMOS: the folder of the demo service's own setups, where a right press on
    /// one starts it.
    void demos(loom::Mail& mail) {
        const std::string folder = zengine::launcher::configured::kDemosFolder;
        if (folder.empty()) {
            notice_ = "this Workshop was configured with no demo service";
        } else {
            state_.current_dir = ws::admit_location(folder);
            refresh();
            notice_ = "at the demos: start one from its menu";
        }
        say(mail);
    }

    // ---- Use as recipes -----------------------------------------------------------------

    void use_recipes(loom::Mail& mail) {
        const FileRow* row = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
        if (row == nullptr || !row->openable || row->directory || state_.current_dir.empty()) {
            notice_ = ws::catalog_row_refusal(row, !state_.current_dir.empty());
            say(mail);
            return;
        }
        // THE CATALOG IN FORCE IS WHAT THE NEXT BUILD RUNS, so choosing it is asked as a write:
        // nothing is sent until Workshop says this gesture's hand may.
        FilesAsk next;
        next.kind = FilesAsk::Kind::kUseRecipes;
        next.subject = row->name;
        next.path = ws::persist::resolved_against(state_.current_dir, row->name);
        ask_approval(std::move(next), ws::scope::kWrite, std::string(), mail);
    }

    /// THE APPROVED CATALOG, asked of the project office, which installs it or says why not.
    void take_recipes(const FilesAsk& approved, loom::Mail& mail) {
        (void)asks().forget(of_kind(FilesAsk::Kind::kRecipes));
        (void)asks().ask(mail, ws::kProjectRole, RecipeUseRequested{approved.path},
                         asking(FilesAsk::Kind::kRecipes));
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

    /// A paste is asked of the window that shows the pane's area, and of nobody while none does.
    void begin_paste(loom::Mail& mail) {
        const std::string office = room().clipboard_office();
        const loom::AskOpened opened = clip_asks_.open_to_role(
            office, gfx::ClipboardTextRequested::zen_name,
            gfx::ClipboardTextRequested::zen_version);
        if (!opened) {
            return;
        }
        paste_ask_ = opened.id;
        paste_epoch_ = authoring_.line.draft_epoch();
        pasting_ = true;
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(office, gfx::ClipboardTextRequested{}, opened.correlation);
    }

    // ---- Saying what the pane shows -----------------------------------------------------

    /// One row's text as this pane draws it: spelled before it is fit (`ascii_spelling`,
    /// `workshop/pane_text.hpp`), since a notice can relay another owner's diagnostic verbatim,
    /// which may carry UTF-8, and `judge_content` admits a publication whole or not at all, so
    /// one unspelled byte would refuse every row this pane sent.
    std::string fitted(const std::string& text) const {
        return fit(ascii_spelling(text), columns());
    }
    /// ...as the facet's strip and window take it (`PaneRowsFacet::push_controls`).
    auto fit_row() const {
        return [this](const std::string& text) { return fitted(text); };
    }

    /// One row of the picture, with what it means recorded as it is written: a press is answered
    /// from the record the composition made, never from a second calculation.
    void push_row(const std::string& text, std::int64_t role, FilesMeaning meaning = FilesMeaning{}) {
        std::optional<FilesMeaning> means;
        if (meaning.kind != files_row::kNone) {
            means = std::move(meaning);
        }
        (void)room().push(zengine::workshop::TextRow{fitted(text), role}, std::move(means));
    }

    /// The whole picture. The notice leads and is composed first, since the row map records
    /// absolute rows (`body_budget` already asked the mode for one fewer). It is cleared by the
    /// weaver's next act, not by being said (`agents/panes.md`): one gesture can publish several
    /// times in one drain, Workshop keeps the last picture, and a notice cleared by the first
    /// `say` would be one no weaver ever reads.
    void say(loom::Mail& mail) {
        room().begin();
        if (!room().granted() || rows() <= 0 || columns() <= 0) {
            room().hold(); // nothing is published, so no row can name anything
            return;
        }
        // THE SENTENCE IS NEVER THE THING THAT DOES NOT FIT: it is the answer to the weaver's
        // last act. A one-row room keeps its header instead, which is the pane's identity and
        // where it is standing; there is nothing useful to say in one row twice.
        if (!notice_.empty() && rows() > 1) {
            push_row(notice_, gfx::role::kAccent);
        }
        if (chooser_.open) {
            say_chooser();
        } else if (authoring_.open) {
            say_authoring();
        } else {
            say_browser();
        }
        // The authoring line's caret beside the rows, said every time, so a caret that went goes.
        room().publish(
            mail, [](const FilesMeaning& m) { return part_name(m); },
            [](const FilesMeaning&) { return true; });
    }

    /// WHAT FILES CALLS ITS PARTS: a listing's entry by its name, `entry:<name>`; a buildable
    /// candidate, `candidate:<name>`; an authoring field, line or not, `field:<name>`; and a
    /// control by its operation, `control:<id>`.
    static std::string part_name(const FilesMeaning& m) {
        switch (m.kind) {
        case files_row::kEntry: return "entry:" + m.subject;
        case files_row::kCandidate: return "candidate:" + m.subject;
        case files_row::kField:
        case files_row::kLine: return "field:" + m.subject;
        case files_row::kControl: return "control:" + m.id;
        default: return std::string();
        }
    }

    // ---- The browser's picture and its controls: files/browser_view.hpp -----------------

    /// ONE CONTROL: the operation it asks for, what it reads as, and whether this pane believes
    /// the operation applies. What it acts ON is `subject_of`'s answer, the one place that says
    /// so, which is why the face, the recorded span and the spend cannot disagree.
    struct ControlRow {
        const char* id;
        std::string label;
        bool available = true;
    };

    std::int64_t body_budget(std::int64_t strip_rows) const;
    std::vector<ControlRow> browser_controls() const;
    void say_controls(const std::vector<ControlRow>& controls);
    void say_browser();
    void say_entries(std::size_t total, std::size_t body_rows);

    // ---- Pick buildable, and the recipe form: files/recipe_authoring.hpp ----------------

    void pick_buildable(loom::Mail& mail);
    void chooser_choose(loom::Mail& mail);
    std::string suggestion_for(std::size_t which) const;
    void load_field(std::size_t which);
    void stash_field();
    bool record_field(loom::Mail& mail);
    void edit_field(std::size_t which, loom::Mail& mail);
    bool filled_in() const;
    void next_field(loom::Mail& mail);
    void authoring_commit(loom::Mail& mail);
    void write_recipe(loom::Mail& mail);
    void compose_recipe(loom::Mail& mail);
    void send_recipe(const FilesAsk& approved, loom::Mail& mail);

    struct Field {
        const char* name;
        bool required;
        // Empty means the same as `name`: every field but one is short enough to be its own menu
        // row (`kMaxPaneMenuLabelLen` bytes, with the longest prefix this pane composes onto
        // it). Read through `field_menu_label`, the one place the fallback is spelled.
        const char* menu_label = nullptr;
    };
    static const Field& field_at(bool tree, bool multi_config, std::size_t step);
    static const char* field_name(bool tree, bool multi_config, std::size_t step);
    static const char* field_menu_label(bool tree, bool multi_config, std::size_t step);
    static std::size_t field_count(bool tree, bool multi_config);
    std::size_t field_count() const;

    std::vector<ControlRow> chooser_controls() const;
    std::vector<ControlRow> authoring_controls() const;
    void say_chooser();
    void say_authoring();
    std::string active_prompt() const;
    void say_field(std::size_t i);

    // ---- State not in the shape ---------------------------------------------------------

    Rows& room() { return this->template facet<Rows>(); }
    const Rows& room() const { return this->template facet<Rows>(); }
    /// The rows and columns the pane composes for (`PaneFacet`).
    std::int64_t rows() const { return room().rows(); }
    std::int64_t columns() const { return room().columns(); }
    Asks& asks() { return this->template facet<Asks>(); }
    /// This image's one outstanding menu (`PaneMenuFacet`).
    Menu& menu() { return this->template facet<Menu>(); }

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
    /// THE LAYOUT'S SETTINGS, handed again after a reload, so never reload state: the rows a wheel
    /// notch moves, and the entries one listing holds.
    std::int64_t wheel_rows_ = kFilesWheelRows;
    std::size_t listed_entries_ = ws::kMaxListedEntries;
    /// WHERE THE LISTING'S WINDOW BEGAN LAST TIME, and the chooser's -- what makes the window
    /// move by the least it can rather than re-centring under a weaver's hand. Derived, never
    /// kept across a reload: a fresh image re-derives it from the cursor on its first paint.
    std::size_t window_hint_ = 0;
    std::size_t chooser_hint_ = 0;
    /// ...and where the authoring fields' window began, for the same reason: a short pane
    /// re-laying its field rows under a typing hand would move the line on every character.
    std::size_t field_hint_ = 0;

    struct Chooser {
        bool open = false;
        std::size_t cursor = 0;
        std::string dir;
        std::vector<BuildCandidate> candidates;
    } chooser_;

    /// The draft a weaver is typing: which candidate it is about, every field's value, which have
    /// been answered at least once, and which one the line is standing on. Values are held for
    /// every field from the start (`field_count`), not pushed per commit, so a weaver can go back
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

// The browser's picture and the two authoring modes: members of `FilesWeave` defined in parts
// of this file, which need the whole class before them.
#include "files/browser_view.hpp"
#include "files/recipe_authoring.hpp"

ZEN_EXPORT_WEAVE(FilesWeave)
