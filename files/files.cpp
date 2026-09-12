// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Files tool -- a loadable weave that offers Workshop one pane: a browser over the
// machine, the marks a maker keeps, and the two authored files a maker writes from it.
//
// IT USED TO BE C++ INSIDE THE HOST (`workshop/screen_browser.cpp`, the Files bodies in
// `weave_editor.cpp` and `weave_recipes.cpp`). Now it is a weave beside the Skin and the
// Timer, and everything it once read straight off `HostContext` -- where this run began,
// the file its marks live in, whether a file is a recipe catalog, one path to open in the
// Editor -- it ASKS for, through the four doors `workshop/pane_seam_vocabulary.hpp`
// spells. Three of them are the host's; the fourth is the Editor weave's own
// (`zengine.editor`), since the Editor stopped being the host's built-in. What crosses the
// seam is values; the browser owns its listing, its marks and its two modes, and holds no
// reference to anything in the host.
//
// THE PURE HALF DID NOT MOVE ITS MEANING. `files.hpp`'s `Listing`, `marks.hpp`'s
// `LocationMarks`, `marks_persist.hpp` and `path_admission.hpp` are the same files this
// browser always used; this weave includes them and spends them exactly as the built-in
// did, which is why every WL-FILES law is answered by the same identifiers.

#include "files/vocabulary.hpp"

#include "files/files.hpp"
#include "files/filesystem_roots.hpp"
#include "workshop/open_seam_vocabulary.hpp" // EXPERIMENTAL: the managed door
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "files/marks_persist.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/path_admission.hpp"
#include "workshop/persist.hpp"

#include "activation/activation.hpp"
#include "builder/vocabulary.hpp"
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

using ws::AdmittedName;
using ws::FileRow;
using ws::Listing;
using ws::LocationMarks;
using ws::MarkedPlace;
using ws::OpenSourceRequested;
using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneKey;
using ws::PaneOffered;
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

/// The office Workshop holds, named as a STRING rather than reached through
/// `workshop/panel.hpp`: a provider is a stranger to Workshop's internals and says who it
/// is talking to the way a third party would.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// How many rows the wheel is worth per notch in a cursor-windowed list -- the built-in's
/// `kFilesWheelRows`, carried here so the pane spends the wheel exactly as it always did.
constexpr std::int64_t kFilesWheelRows = 3;

// ---- Small pure helpers the recipe chooser needs (weave_recipes.cpp's own) --------------

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

/// The shared pane text helpers (`workshop/pane_text.hpp`): the fit this file used to carry a
/// copy of, with `judge_content` as the reason it must be applied at all.
using zengine::workshop::pane_text::fit;


/// One directory row's text -- `screen_browser.cpp`'s `files_row_text`, unchanged.
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

/// A build candidate the chooser holds: a place and which of the two recipe kinds it may be
/// tried as. `weave_recipes.cpp`'s `BuildCandidate`, kept local because it is the pane's
/// mode state and crosses no wire.
struct BuildCandidate {
    std::string name;
    bool tree = false;
};

/// One list-window over `total` rows with the cursor on `at`, `count` rows visible -- the
/// windowing `list_window` does, spelled here so the pane can walk it. `before`/`after` are
/// how many rows are hidden each side.
struct Window {
    std::size_t first = 0;
    std::size_t count = 0;
    std::size_t before = 0;
    std::size_t after = 0;
};

Window window_of(std::size_t total, std::size_t at, std::size_t rows) {
    Window w;
    if (total == 0 || rows == 0) {
        return w;
    }
    if (total <= rows) {
        w.count = total;
        return w;
    }
    // Keep the cursor visible, centred where it can be.
    const std::size_t half = rows / 2;
    std::size_t first = at > half ? at - half : 0;
    if (first + rows > total) {
        first = total - rows;
    }
    w.first = first;
    w.count = rows;
    w.before = first;
    w.after = total - (first + rows);
    return w;
}

/// THE WINDOW AND ITS OWN MARKERS TOGETHER, INSIDE THE BUDGET.
///
/// `window_of` fills the rows it is given with ENTRIES; the `... N earlier` and `... N more`
/// rows are pushed on top of that, so a window that filled its budget composed one or two
/// rows MORE than the room had. `say` then cut the overrun off the end -- which took the
/// `... N more` marker with it and left a maker looking at a list with no sign that it went
/// on, and left no room at all for the notice `say` puts in front. Ask for fewer entries
/// instead. At most two markers, so at most two rounds, and one entry is always seated.
Window fitted_window(std::size_t total, std::size_t at, std::int64_t budget) {
    if (budget <= 0) {
        return Window{};
    }
    std::size_t seats = static_cast<std::size_t>(budget);
    Window w = window_of(total, at, seats);
    while (seats > 1 && static_cast<std::int64_t>(w.count) + (w.before > 0 ? 1 : 0) +
                                (w.after > 0 ? 1 : 0) >
                            budget) {
        w = window_of(total, at, --seats);
    }
    return w;
}

// =============================================================================
// The weave
// =============================================================================

class FilesWeave
    : public loom::WeaveBase<
          FilesWeave, files::FilesState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed, PaneKey,
                       PaneTextInput, PaneWheel, PaneActionRequested, ProjectRoot, RecipeOutcome,
                       SourceOpened, zengine::builder::BuildStatus, surface::ClipboardCopy,
                       surface::ClipboardText>,
          loom::Emit<PaneOffered, PaneActions, PaneContent, ProjectRootRequested,
                     RecipeUseRequested, RecipeAuthorRequested, OpenSourceRequested,
                     zengine::builder::StatusRequested, surface::ClipboardCopy,
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

    /// WORKSHOP GRANTS THE PANE ITS ROOM -- the one beat on which this tool draws. The
    /// listing is a snapshot re-enumerated here (WL-FILES-12); if this run has no origin
    /// yet, the room grant is also when the browser first asks the host where it began.
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
        refresh();
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

    void on(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != files::kProjectFilesPane) {
            return;
        }
        if (chooser_.open || authoring_.open) {
            return; // a mode owns its own rows; a press outside a live list selects nothing
        }
        std::size_t which = 0;
        if (!row_of_body_row(press.row, which)) {
            return; // the header, a marker row, or blank space names no entry
        }
        notice_.clear(); // the maker has acted; the last act's answer is spent
        if (had_keyboard_ && which == static_cast<std::size_t>(state_.cursor)) {
            open(mail); // a press on the already-selected row activates it (WL-FOCUS-04)
            return;
        }
        state_.cursor = static_cast<std::int64_t>(which);
        had_keyboard_ = true; // the press that selects has also pointed the keys here
        say(mail);
    }

    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != files::kProjectFilesPane) {
            return;
        }
        had_keyboard_ = true;
        // ONLY THE AUTHORING LINE READS RAW KEYS. Everything else this pane does arrives as
        // a resolved id (`on(PaneActionRequested)`); a component's editing gestures are the
        // component's, not the pane's commands.
        if (!authoring_.open) {
            return;
        }
        notice_.clear();
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!authoring_.line.consume(key.scancode, key.modifiers, clip_)) {
            return;
        }
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
        if (chooser_.open || authoring_.open || !listing_.known || listing_.rows.empty()) {
            return;
        }
        wheel_accum_ += wheel.dy * static_cast<double>(kFilesWheelRows);
        const std::int64_t step = static_cast<std::int64_t>(wheel_accum_);
        wheel_accum_ -= static_cast<double>(step);
        if (step == 0) {
            return;
        }
        const std::int64_t was = state_.cursor;
        move(-step);
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
        notice_.clear(); // the maker has acted; the last act's answer is spent
        // A MODE OWNS THE PANE'S ACTIONS FIRST, AND MEANS ITS OWN THINGS BY THEM. Return
        // (`files.open`) commits a chooser row or an authoring field; Escape
        // (`files.cancel`) backs out whole; the browser's other verbs mean nothing until the
        // mode closes. One gesture map for the pane, because a pane is one keyboard context
        // -- see `vocabulary.hpp` for why there is no second commit id.
        if (chooser_.open) {
            if (asked.id == files::kActionUp) {
                if (chooser_.cursor > 0) {
                    --chooser_.cursor;
                }
            } else if (asked.id == files::kActionDown) {
                if (chooser_.cursor + 1 < chooser_.candidates.size()) {
                    ++chooser_.cursor;
                }
            } else if (asked.id == files::kActionOpen) {
                chooser_choose(mail);
                return;
            } else if (asked.id == files::kActionCancel) {
                chooser_ = Chooser{};
                declare(mail);
                notice_ = "no recipe was authored";
            } else {
                return;
            }
            say(mail);
            return;
        }
        if (authoring_.open) {
            if (asked.id == files::kActionOpen) {
                authoring_commit(mail);
                return;
            }
            if (asked.id == files::kActionCancel) {
                authoring_ = Authoring{};
                declare(mail);
                notice_ = "no recipe was written";
                say(mail);
            }
            return;
        }
        if (asked.id == files::kActionUp) {
            move(-1);
        } else if (asked.id == files::kActionDown) {
            move(1);
        } else if (asked.id == files::kActionOpen) {
            open(mail);
            return;
        } else if (asked.id == files::kActionParent) {
            parent(mail);
            return;
        } else if (asked.id == files::kActionRefresh) {
            refresh();
            notice_ = "listed " + where() + " again";
        } else if (asked.id == files::kActionUseRecipes) {
            use_recipes(mail);
            return;
        } else if (asked.id == files::kActionMark) {
            mark(mail);
            return;
        } else if (asked.id == files::kActionNextMark) {
            jump_mark(1);
        } else if (asked.id == files::kActionPreviousMark) {
            jump_mark(-1);
        } else if (asked.id == files::kActionPickBuildable) {
            pick_buildable(mail);
            return;
        } else {
            return;
        }
        say(mail);
    }

    /// A FILE THE PANE ASKED THE HOST TO USE OR AUTHOR (`zengine.recipes`), answered.
    void on(const RecipeOutcome& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !recipes_.awaiting || mail.correlation() != recipes_.pending) {
            return;
        }
        recipes_.awaiting = false;
        // BOTH HALVES, IN ONE SENTENCE, AND IN THE ORDER THAT SURVIVES THE CUT. What went
        // wrong and what is STILL RUNNING are both owed here -- a refusal that named only
        // the first would leave a maker guessing whether they had just lost the catalog
        // they were using. The notice row is cut at the band's width, so the two SHORT
        // fixed statements go first and the two long variable ones -- the owner's own
        // sentence, then the path -- take the tail in that order. MEASURED by the live
        // witness before the migration: with the reason first, the reassuring half was
        // exactly the half that elided. The composition is the built-in's, unchanged; only
        // the party that owns it moved, and `said.path` is the office's own answer about
        // what is in force AFTER the attempt rather than an echo of what was asked for.
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
        // THE BUILDER IS ASKED TO SAY WHAT IT IS, through the message the built-in sent: the
        // tool re-reads the catalog in force and republishes it, and the Builder pane hears
        // the new rows. No `recipes_moved_to` projection any more (retired with the built-in).
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(zengine::builder::kBuilderRole, zengine::builder::StatusRequested{});
        say(mail);
    }

    /// THE EDITOR DOOR ANSWERED (`zengine.workshop`). A refused open is said in the pane's
    /// own row; an accepted one moved the keyboard to the Editor and needs no word here.
    void on(const SourceOpened& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !open_.awaiting || mail.correlation() != open_.pending) {
            return;
        }
        open_.awaiting = false;
        if (!said.accepted) {
            notice_ = said.refusal;
            say(mail);
        }
    }

    /// A BUILD THIS PROCESS RAN HAS FINISHED (published `to_any`), so what is on disk may
    /// have changed: take a fresh listing and put the cursor back where it was. The
    /// built-in's `files_build_settled`, one image over -- gated on the outcome being one a
    /// build will not leave, so a mid-build status does not re-walk the directory.
    ///
    /// ⚠ AND GATED ON A BUILD HAVING ACTUALLY HAPPENED, which the built-in got for free and
    /// this pane has to ask for. `BuildStatus` is published for two different reasons: a
    /// build settling, and somebody merely ASKING what the state is -- and this pane asks,
    /// itself, right after an accepted catalog choice. Without this gate that answer looked
    /// like a finished build, the pane re-listed and re-said, and the sentence it had just
    /// written about the catalog was gone before a maker could read it. MEASURED on a real
    /// terminal (the whole-loop witness): `u` on a catalog produced no visible row at all.
    /// `builds` is the tool's own count of how many builds it has been asked for, ever, so
    /// an answer that carries the same count is a description of the same world.
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
        const FileRow* row = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
        const std::string was = row != nullptr ? row->name : std::string();
        refresh();
        if (!was.empty()) {
            point_at(was);
        }
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
                                      files::kProjectFilesSummary});
        declare(mail);
    }

    /// WHAT THIS PANE ANSWERS TO RIGHT NOW -- re-declared whenever the mode changes.
    ///
    /// ⭐ A PANE IS ONE KEYBOARD CONTEXT, AND A MODE IS NOT A SECOND ONE. The built-in had
    /// three contexts (`kFiles`, `kRecipeChooser`, `kAuthoring`) and could bind Return and
    /// Backspace differently in each; a pane's rows are joined into ONE map under its
    /// runtime handle, and the collision law refuses a second row on a gesture already
    /// taken. Two ways out existed, and only one of them keeps the maker's keys:
    ///
    ///   - move the defaults apart, so `files.parent` stops being Backspace -- which is
    ///     exactly the promise this migration was made to keep, and
    ///   - DECLARE WHAT IS TRUE NOW, which is what this does.
    ///
    /// So while a maker is typing into the authoring line, this pane declares two rows and
    /// no more, and every other key reaches it as an ordinary `PaneKey` for the line to
    /// consume -- Backspace deletes a character, exactly as it always did, because in that
    /// mode nothing has claimed it. `PaneActions` is a REPLACEMENT (WL-KEY-15): the host
    /// re-joins the map, so what leaves the declaration also leaves the keymap.
    ///
    /// AND THE IDS NEVER MOVE. `files.parent` is `files.parent` in every mode that declares
    /// it, so a maker's authored override for it is applied wherever it is in force.
    void declare(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = files::kProjectFilesPane;
        const auto row = [&actions](const char* id, const char* label, std::int64_t sc,
                                    std::int64_t mods = input::mod::kNone) {
            actions.rows.push_back(PaneActionRow{id, label, sc, mods});
        };
        // ---- The authoring line owns the keyboard, except for two rows -----------------
        if (authoring_.open) {
            row(files::kActionOpen, "commit this field", input::scan::kReturn);
            row(files::kActionCancel, "abandon", input::scan::kEscape);
            (void)mail.as_role(files::kFilesRole).send_to_role(kWorkshopRole, actions);
            return;
        }
        // ---- The chooser is a list: it needs the two arrows and the two mode rows ------
        if (chooser_.open) {
            row(files::kActionUp, "row up", input::scan::kUp);
            row(files::kActionDown, "row down", input::scan::kDown);
            row(files::kActionOpen, "choose this", input::scan::kReturn);
            row(files::kActionCancel, "cancel", input::scan::kEscape);
            (void)mail.as_role(files::kFilesRole).send_to_role(kWorkshopRole, actions);
            return;
        }
        // ---- Browsing: THE SAME IDS THE OVERRIDE FILE ALREADY KNOWS, AND THE SAME
        // DEFAULTS the built-in shipped (workshop/keymap.hpp `kFiles`), so every maker's
        // authored keymap keeps working across the migration.
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
        (void)mail.as_role(files::kFilesRole).send_to_role(kWorkshopRole, actions);
    }

    // ---- The doors ----------------------------------------------------------------------

    struct Ask {
        std::uint64_t pending = 0;
        bool awaiting = false;
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
        // A FILE: ask the Editor's door to open it. The answer says whether it took; the
        // Editor asks Workshop to show its pane itself. The door is at `zengine.editor` now
        // (`ws::kEditorRole`) -- it was at the host's office while the host held the
        // document, and only the address moved.
        open_.pending = ++asked_;
        open_.awaiting = true;
        // EXPERIMENTAL (editor-managed-open-slice): the managed door. The opening manager
        // arranges the document and the desk together; the Editor's own office is the direct
        // door, for a host with no desk.
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(ws::kOpeningRole,
                          OpenSourceRequested{ws::persist::resolved_against(state_.current_dir, row->name)},
                          open_.pending);
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
            // THE BUILT-IN'S OWN FOUR SENTENCES, composed where they are witnessed.
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
                const bool configured = std::filesystem::exists(
                    std::filesystem::path(state_.current_dir) / row.name / "CMakeCache.txt", ec);
                if (configured && !ec) {
                    chooser.candidates.push_back(BuildCandidate{row.name, true});
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
        declare(mail); // this mode answers to four rows, and to no others
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
        const std::string suggested = a.chosen.tree ? a.chosen.name : stem_of(a.chosen.name);
        a.prompt = std::string(field_name(a.chosen.tree, 0)) + "> ";
        a.line.set(suggested, suggested.size());
        authoring_ = std::move(a);
        chooser_ = Chooser{};
        // THE LINE TAKES THE KEYBOARD: two rows declared, everything else an ordinary key.
        declare(mail);
        notice_ = std::string(authoring_.chosen.tree ? "a configured tree: " : "a source file: ") +
                  authoring_.chosen.name + " -- Return commits a field, Escape cancels";
        say(mail);
    }

    void authoring_commit(loom::Mail& mail) {
        Authoring& a = authoring_;
        const std::string typed = trimmed(a.line.text());
        const Field& field = field_at(a.chosen.tree, a.step);
        if (field.required && typed.empty()) {
            notice_ = std::string(field.name) + " is required -- nothing was written";
            say(mail);
            return;
        }
        a.answers.push_back(typed);
        ++a.step;
        if (a.step < kFieldCount) {
            const Field& next = field_at(a.chosen.tree, a.step);
            std::string suggested;
            if (!a.chosen.tree && a.step == 1) {
                suggested = a.answers[0];
            } else if (a.chosen.tree && a.step == 2) {
                suggested = a.answers[1];
            }
            a.prompt = std::string(next.name) + "> ";
            a.line.set(suggested, suggested.size());
            say(mail);
            return;
        }
        // THE LAST FIELD: compose the draft and hand it to the recipes door.
        RecipeAuthorRequested draft;
        draft.tree = a.chosen.tree;
        draft.id = a.answers[0];
        const std::string place = ws::persist::resolved_against(a.dir, a.chosen.name);
        if (!draft.tree) {
            draft.artifact = a.answers[1];
            draft.source = place;
            draft.packages = split_list(a.answers[2]);
            draft.links = split_list(a.answers[3]);
        } else {
            draft.target = a.answers[1];
            draft.artifact = a.answers[2];
            draft.build_dir = place;
            draft.artifact_dir = a.answers[3];
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

    // ---- The chooser's typed fields (weave_recipes.cpp's own table) ---------------------

    struct Field {
        const char* name;
        bool required;
    };
    static const Field& field_at(bool tree, std::size_t step) {
        static constexpr Field kSource[] = {{"recipe name", true},
                                            {"artifact stem", true},
                                            {"package prefix (comma-separated)", true},
                                            {"link targets (comma-separated)", true}};
        static constexpr Field kTree[] = {{"recipe name", true},
                                          {"cmake target", true},
                                          {"artifact stem", true},
                                          {"artifact directory (optional)", false}};
        return tree ? kTree[step] : kSource[step];
    }
    static const char* field_name(bool tree, std::size_t step) { return field_at(tree, step).name; }
    static constexpr std::size_t kFieldCount = 4;

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

    // ---- Presses into the body's window -------------------------------------------------

    /// WHICH LISTING ROW A BODY ROW SHOWS -- the browser's `files_row_of_body_row`, over the
    /// window this pane last drew. Row 0 of the body is the row under the header.
    bool row_of_body_row(std::int64_t body_row, std::size_t& out) {
        if (rows_ <= kHeaderRows || body_row < kHeaderRows) {
            return false;
        }
        const std::size_t total = listing_.rows.size();
        if (total == 0) {
            return false;
        }
        const std::size_t body_rows = static_cast<std::size_t>(rows_ - kHeaderRows);
        const Window win = window_of(total, static_cast<std::size_t>(state_.cursor), body_rows);
        const std::int64_t first_entry = win.before > 0 ? kHeaderRows + 1 : kHeaderRows;
        const std::int64_t offset = body_row - first_entry;
        if (offset < 0 || offset >= static_cast<std::int64_t>(win.count)) {
            return false;
        }
        out = win.first + static_cast<std::size_t>(offset);
        return out < total;
    }

    // ---- Saying what the pane shows -----------------------------------------------------

    void say(loom::Mail& mail) {
        if (!granted_ || rows_ <= 0 || columns_ <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        const auto push = [&out, this](const std::string& text, std::int64_t role) {
            out.push_back(surface::SurfaceTextRow{fit(text, columns_), role});
        };
        if (chooser_.open) {
            say_chooser(push);
        } else if (authoring_.open) {
            say_authoring(push);
        } else {
            say_browser(push);
        }
        // A notice, when there is one, leads -- the built-in wrote it on the band; a pane has
        // only its own room, so its first row carries it.
        //
        // IT IS CLEARED BY THE MAKER'S NEXT ACT, NOT BY BEING SAID (`agents/panes.md`, the
        // pane-weave rules). This pane cleared it inside `say` until the Builder's migration
        // proved that wrong one pane over: one gesture produces SEVERAL publications in one
        // drain -- a notice is written, the rows are said, a door is asked and its answer
        // arrives on the same turn and says them again -- and Workshop keeps only the last
        // picture. Cleared by the first `say`, the sentence is one no maker ever reads. The
        // gate on `BuildStatus` below was this pane's narrow repair for the one instance of
        // it the whole-loop witness caught (`u` on a catalog produced no visible row); this
        // is the general rule, and the gate stays because it is also about a stale listing.
        if (!notice_.empty() && rows_ > 1) {
            // THE SENTENCE IS NEVER THE THING THAT DOES NOT FIT. `body_budget` already asked
            // the composition for one fewer row, so this cut is a backstop and not the
            // mechanism: it fires only where the room cannot seat even one entry and its two
            // markers, and what it drops is the tail of the list rather than the answer to
            // the maker's act. A one-row room keeps its header, which is the pane's identity
            // and where it is standing; there is nothing useful to say in one row twice.
            if (static_cast<std::int64_t>(out.size()) > rows_ - 1) {
                out.resize(static_cast<std::size_t>(rows_ - 1));
            }
            out.insert(out.begin(), surface::SurfaceTextRow{fit(notice_, columns_), surface::role::kAccent});
        }
        if (static_cast<std::int64_t>(out.size()) > rows_) {
            out.resize(static_cast<std::size_t>(rows_));
        }
        (void)mail.as_role(files::kFilesRole)
            .send_to_role(kWorkshopRole, PaneContent{files::kProjectFilesPane, std::move(out)});
    }

    /// HOW MANY ROWS THE LISTING MAY SPEND: the room, less this pane's own header, less the
    /// notice row `say` puts in front of it. A pane has no band to write a notice on, and the
    /// whole content is cut to the room afterwards -- so the composition is asked for one
    /// fewer row rather than having its last row silently dropped after the fact. The
    /// Builder's `publish` spends the same subtraction for the same reason.
    std::int64_t body_budget() const {
        return rows_ - kHeaderRows - (notice_.empty() ? 0 : 1);
    }

    template <class Push>
    void say_browser(Push&& push) {
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
        push(header, surface::role::kAccent);
        const std::int64_t body_rows = body_budget();
        if (body_rows <= 0) {
            return;
        }
        if (!listing_.known) {
            push(listing_.refusal.empty() ? "nothing has been listed yet" : listing_.refusal,
                 surface::role::kMuted);
            return;
        }
        if (total == 0) {
            push("this directory is empty", surface::role::kMuted);
            return;
        }
        const Window win =
            fitted_window(total, static_cast<std::size_t>(state_.cursor), body_rows);
        if (win.before > 0) {
            push("  ... " + std::to_string(win.before) + " earlier", surface::role::kMuted);
        }
        for (std::size_t i = win.first; i < win.first + win.count; ++i) {
            const bool here = i == static_cast<std::size_t>(state_.cursor);
            const FileRow& row = listing_.rows[i];
            push(std::string(here ? "> " : "  ") + row_text(row),
                 here ? surface::role::kAccent
                      : (row.openable ? surface::role::kFill : surface::role::kMuted));
        }
        if (win.after > 0) {
            push("  ... " + std::to_string(win.after) + " more", surface::role::kMuted);
        }
    }

    template <class Push>
    void say_chooser(Push&& push) {
        push("pick something buildable -- " + std::to_string(chooser_.candidates.size()) +
                 (chooser_.candidates.size() == 1 ? " candidate" : " candidates"),
             surface::role::kAccent);
        const std::int64_t body_rows = body_budget();
        if (body_rows <= 0) {
            return;
        }
        const Window win = fitted_window(chooser_.candidates.size(), chooser_.cursor, body_rows);
        for (std::size_t i = win.first; i < win.first + win.count; ++i) {
            const bool here = i == chooser_.cursor;
            const BuildCandidate& c = chooser_.candidates[i];
            push(std::string(here ? "> " : "  ") + c.name + (c.tree ? "/  (configured tree)" : ""),
                 here ? surface::role::kAccent : surface::role::kFill);
        }
    }

    template <class Push>
    void say_authoring(Push&& push) {
        // THE PANE PROTOCOL CARRIES NO CARET (`PaneContent` is `SurfaceTextRow` values, and
        // a caret is a `SurfaceTextRegion` fact a pane cannot send). So the line shows its
        // prompt and its text and no caret -- the same documented loss the Powers query
        // keeps, and the reason the Editor's migration is the contract that would move a
        // caret onto `PaneContent`. The visible window still follows the caret column so a
        // long field scrolls to where the maker is typing.
        const std::int64_t prompt = static_cast<std::int64_t>(authoring_.prompt.size());
        const std::int64_t cols = columns_ > prompt + 1 ? columns_ - prompt - 1 : 1;
        authoring_.line.keep_caret_visible(cols);
        push(authoring_.prompt + authoring_.line.visible(cols), surface::role::kAccent);
    }

    // ---- State not in the shape ---------------------------------------------------------

    zengine::ActivationCursor activation_;
    std::uint64_t asked_ = 0;

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
    bool had_keyboard_ = false;
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

    struct Authoring {
        bool open = false;
        BuildCandidate chosen;
        std::string dir;
        std::string prompt;
        component::TextBox line;
        std::size_t step = 0;
        std::vector<std::string> answers;
    } authoring_;

    component::Clipboard clip_;
    loom::AskBook clip_asks_{1};
    std::uint64_t paste_ask_ = 0;
    std::uint64_t paste_epoch_ = 0;
    bool pasting_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(FilesWeave)
