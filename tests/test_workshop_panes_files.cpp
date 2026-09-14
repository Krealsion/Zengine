// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite — THE PROJECT BROWSER, AS A LOADED WEAVE.
//
// THIS FILE OWNS the pane that used to be compiled into this host. Everything the browser
// does a maker can see -- listing a place, walking it, activating a row, choosing a recipe
// catalog, marking a place, picking something buildable and authoring its row -- is driven
// here through the REAL `zengine-files` image, over the REAL pane protocol, against the
// REAL doors this host mounts for it. Nothing in this file constructs the weave, reaches
// into its state, or calls one of its functions: there is a shared library on disk, a plan
// row that loads it, an office it holds, and a maker's hand.
//
// ---- WHY THE CLAIMS MOVED HERE ---------------------------------------------------
//
// They were `workshop_files` cases, and they drove the same gestures against a built-in
// pane through `Session`. The pane left; the gestures did not. What changed is the SEAM
// they cross -- a keystroke is a resolved action id now (WL-KEY-15), a row is a
// `SurfaceTextRow` in a granted room rather than a painter's output, and the three facts
// the browser reads from the host are asks to offices -- so the evidence belongs where
// that seam is, which is here. The PURE half (the listing, the marks, the sentences)
// moved with the code, to `files_weave`.
//
// ⚠ AND THE CASES ARE STRONGER FOR IT. A built-in's case could assert `pane.cursor`; these
// can only assert what a maker can see, which is the row on the screen. Where a claim
// really is about the pane's private state, it is asked of the ROWS the pane published,
// because that is the only honest picture of it from out here.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "files/vocabulary.hpp"
#include "workshop/authoring.hpp"
#include "workshop/pane_doors.hpp"
#include "workshop/recipe_persist.hpp"
#include "workshop/recipes.hpp"

#include <algorithm>
#include <fstream>

namespace {

namespace files = zengine::files;

/// The office and pane a saved setup names, spelled through the package's own header --
/// the durable names, not literals, so a case cannot agree with a typo.
inline PaneRef files_ref() {
    return PaneRef{files::kFilesRole, files::kProjectFilesPane};
}

inline void put_file(const std::filesystem::path& at, const std::string& bytes) {
    std::ofstream out(at, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
}

/// A recipe catalog on disk, in the shape the owner reads.
inline void put_catalog(const std::filesystem::path& at,
                        const std::vector<zengine::builder::Recipe>& rows) {
    put_file(at, recipe_persist::to_text(rows));
}

inline zengine::builder::Recipe authored_recipe(const std::string& id,
                                                const std::string& source) {
    zengine::builder::SingleSourceRecipe one;
    one.source = source;
    one.links.push_back("loom::kernel");
    zengine::builder::Recipe r;
    r.id = id;
    r.artifact = id;
    r.single_source = one;
    return r;
}

/// THE INDEX OF THE FIRST ROW THAT BEGINS WITH `head`, or -1. A row is located by what it says
/// at its start, as a maker reads it: a refusal that quotes a file's name contains that name,
/// and is never that file's row.
inline std::int64_t row_beginning(const std::vector<std::string>& rows, const std::string& head) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].rfind(head, 0) == 0) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

/// A PICTURE, ONE NUMBERED ROW PER LINE -- what a failed press case prints, so a reader sees the
/// rows the press was aimed at.
inline std::string picture(const std::vector<std::string>& rows) {
    std::string out;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        out += std::to_string(i) + "| " + rows[i] + "\n";
    }
    return out;
}

/// A LONG LISTING WHOSE NAMES ARE THEIR ORDER: `entry-07.txt` is the eighth row of the listing.
inline constexpr std::int64_t kLongListing = 40;
inline std::string entry_name(std::int64_t i) {
    return std::string("entry-") + (i < 10 ? "0" : "") + std::to_string(i) + ".txt";
}

/// WHICH ENTRY A PAINTED ROW SHOWS, or -1 for a row that shows none -- the notice, the header, a
/// count of entries not shown, a blank. Only the long listing's names are read.
inline std::int64_t entry_shown(const std::string& row) {
    if (row.rfind("> entry-", 0) != 0 && row.rfind("  entry-", 0) != 0) {
        return -1;
    }
    return std::stoll(row.substr(8, 2));
}

/// WHERE THE CURSOR IS, as the pane's own header counts it (`Files 21/40 ...` is entry 20). Read
/// off the header because a small room can cut the cursor's own row away and still count it.
inline std::int64_t cursor_said(const std::vector<std::string>& rows) {
    const std::int64_t at = row_beginning(rows, "Files ");
    REQUIRE(at >= 0);
    const std::string& header = rows[static_cast<std::size_t>(at)];
    const std::size_t slash = header.find('/');
    REQUIRE(slash != std::string::npos);
    return std::stoll(header.substr(6, slash - 6)) - 1;
}

/// WHAT CROSSED THE SEAM, READ OFF THE BUS'S OWN TAP: the row each `PanePressed` delivered to the
/// weave carried, the version it crossed in and the routing fact a second version states, how
/// many opens the weave attempted -- delivered, or refused at dispatch -- and the exact path of
/// each one delivered, every action id delivered to it, and the recipe id of every authored row
/// it sent. A case states the provider row it aimed at, where Workshop said the keys were, the id
/// a key was resolved as and the file an activation asked for, rather than inferring any of them
/// from the pane's answer. A press is read by field name, so one tap reads either version.
struct SeamTap {
    loom::Switchboard& bus;
    loom::WeaveId browser;
    loom::ObserverId id{};
    std::vector<std::int64_t> pressed;
    std::vector<std::uint32_t> versions;  ///< per delivered press
    std::vector<int> keys_went_here;      ///< per delivered press: 1, 0, or -1 for a v1 (no fact)
    std::vector<std::string> heard;       ///< every shape delivered to the weave, in order
    std::size_t refused = 0;              ///< deliveries to the weave Loom refused
    std::size_t keys = 0;                 ///< raw keys the weave was sent (`PaneKey`)
    std::size_t actions = 0;              ///< resolved ids (`PaneActionRequested`)
    std::size_t rooms = 0;
    std::size_t attempts = 0;
    std::vector<std::string> requested;
    std::vector<std::string> ids;      ///< every `PaneActionRequested` id delivered, in order
    std::vector<std::string> authored; ///< every delivered `RecipeAuthorRequested`, by recipe id

    SeamTap(loom::Switchboard& b, loom::WeaveId weave) : bus(b), browser(weave) {
        id = bus.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Refused && ev.target == browser) {
                ++refused;
            }
            if (ev.kind == loom::EventKind::Delivered && ev.target == browser) {
                heard.push_back(ev.schema_name);
                keys += ev.schema_name == PaneKey::zen_name ? std::size_t{1} : std::size_t{0};
                actions += ev.schema_name == PaneActionRequested::zen_name ? std::size_t{1}
                                                                           : std::size_t{0};
                rooms += ev.schema_name == PaneRoom::zen_name ? std::size_t{1} : std::size_t{0};
            }
            if (ev.kind == loom::EventKind::Delivered && ev.target == browser &&
                ev.schema_name == PanePressed::zen_name && ev.payload != nullptr) {
                const loom::Cell* row = ev.payload->get("row");
                const loom::Cell* fact = ev.payload->get("keys_went_here");
                pressed.push_back(row != nullptr && row->is(loom::Kind::Int) ? row->as_int() : -1);
                versions.push_back(ev.schema_version);
                keys_went_here.push_back(fact != nullptr && fact->is(loom::Kind::Bool)
                                             ? (fact->as_bool() ? 1 : 0)
                                             : -1);
            }
            if (ev.sender == browser && ev.schema_name == OpenSourceRequested::zen_name &&
                (ev.kind == loom::EventKind::Delivered || ev.kind == loom::EventKind::Refused)) {
                ++attempts;
                if (ev.kind == loom::EventKind::Delivered && ev.payload != nullptr) {
                    requested.push_back(loom::from_value<OpenSourceRequested>(*ev.payload).path);
                }
            }
            if (ev.kind == loom::EventKind::Delivered && ev.payload != nullptr) {
                if (ev.target == browser && ev.schema_name == PaneActionRequested::zen_name) {
                    ids.push_back(loom::from_value<PaneActionRequested>(*ev.payload).id);
                }
                if (ev.sender == browser && ev.schema_name == RecipeAuthorRequested::zen_name) {
                    authored.push_back(loom::from_value<RecipeAuthorRequested>(*ev.payload).id);
                }
            }
        });
    }
    ~SeamTap() { bus.remove_observer(id); }
    SeamTap(const SeamTap&) = delete;
    SeamTap& operator=(const SeamTap&) = delete;
};

/// A LIVE WORKSHOP WITH THE REAL BROWSER LOADED INTO IT.
///
/// The order is the host's, and it is the whole arrangement under test: the doors are
/// mounted BEFORE the plan runs, because the pane asks `zengine.project` where this run
/// began on the very beat it is first granted room -- a door mounted afterwards would be
/// absent exactly when the only ask that matters is made.
struct FilesRig {
    TempDir dir;
    std::filesystem::path root;
    std::string marks_path;
    CurrentRecipes recipes;
    PaneRig r;
    std::int64_t kind = 0;

    explicit FilesRig(const char* tag) : dir(tag) {
        root = dir.path();
        r.host.project_dir = root.generic_string();
        marks_path = (root / "workshop-marks.json").generic_string();
    }

    /// Mount the two host doors, load the image, open the pane, and put the keyboard on it.
    ///
    /// `with_editor` LOADS THE REAL EDITOR IMAGE BESIDE THE BROWSER, for the two cases about
    /// the one door: the Editor is a weave (VD-25), so a Return on a source row is answered by
    /// nobody unless its image is in the room.
    /// `with_manager` MOUNTS THE OPENING MANAGER BESIDE WORKSHOP, as the host does: the host
    /// names the managed pane before Workshop is mounted, and the manager's office is what
    /// the pane's Return asks (WL-OPEN-01). Without it the ask reaches nobody, which is the
    /// refusal-at-dispatch cases' whole subject.
    void open(std::int64_t width = 160, std::int64_t height = 48, bool with_editor = false,
              bool with_manager = true) {
        r.host.managed_pane = PaneRef{"zengine.editor", "editor"};
        r.mount_workshop();
        if (with_manager) {
            r.mount_opening();
        }
        mount_project_door();
        mount_recipes_door();
        load::LoadPlan plan;
        load::ArtifactIntent tool;
        tool.stem = files::kFilesStem;
        tool.weave = load::WeaveIntent{files::kFilesRole};
        plan.artifacts.push_back(tool);
        if (with_editor) {
            load::ArtifactIntent editor;
            editor.stem = "zengine-editor-pane";
            editor.weave = load::WeaveIntent{"zengine.editor"};
            plan.artifacts.push_back(editor);
        }
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(width, height);
        REQUIRE_MESSAGE(row() != nullptr, "the loaded image offered no `project-files` pane");
        r.pick(files_ref());
        kind = row()->kind;
        // A PRESS ON WORKSHOP'S TITLE ROW POINTS THE KEYS AT THE PANE AND SELECTS NOTHING: the
        // title sits above every row the pane was granted, so the pane is sent no press at all,
        // and this is the one gesture that focuses without also selecting or activating. A
        // provider row -- the pane's own header is its row 0 -- is reached through `press_pane`.
        r.press_cell(external_body_rect(r.session(), kind).x,
                     external_body_rect(r.session(), kind).y);
    }

    const RuntimePane* row() {
        return r.session().panels.runtime.find(files::kFilesRole, files::kProjectFilesPane);
    }

    /// The Editor pane's handle, when its image was loaded beside the browser.
    std::int64_t editor_kind() {
        const RuntimePane* editor = r.session().panels.runtime.find("zengine.editor", "editor");
        REQUIRE(editor != nullptr);
        return editor->kind;
    }
    /// The Editor pane's first row: its status row, which carries the dirty word and the path.
    std::string editor_status() {
        const std::vector<std::string> rows = pane_rows(r, editor_kind());
        REQUIRE_FALSE(rows.empty());
        return rows[0];
    }

    std::vector<std::string> shown() { return pane_rows(r, kind); }

    /// The pane's first row: its header, or -- from the maker's act until their next one --
    /// the notice the pane leads with.
    std::string first() {
        const std::vector<std::string> rows = shown();
        REQUIRE_FALSE(rows.empty());
        return rows[0];
    }

    /// The last row with anything on it: the pane fills its room, and a room taller than
    /// the composition is padded with blanks nobody wrote.
    std::string last_written() {
        const std::vector<std::string> rows = shown();
        for (std::size_t i = rows.size(); i > 0; --i) {
            if (!rows[i - 1].empty() && rows[i - 1].find_first_not_of(' ') != std::string::npos) {
                return rows[i - 1];
            }
        }
        return std::string();
    }

    /// The row the cursor is on, as a maker sees it: the one the pane marked with `>`.
    std::string at_cursor() {
        for (const std::string& text : shown()) {
            if (text.rfind("> ", 0) == 0) {
                return text.substr(2);
            }
        }
        return std::string();
    }

    /// Walk the cursor to a named entry the way a maker does, and fail loudly otherwise.
    void point_at(const std::string& name) {
        for (int guard = 0; guard < 64; ++guard) {
            r.key(input::scan::kUp);
        }
        for (int guard = 0; guard < 64; ++guard) {
            if (at_cursor().rfind(name, 0) == 0) {
                return;
            }
            r.key(input::scan::kDown);
        }
        REQUIRE_MESSAGE(at_cursor().rfind(name, 0) == 0, "no row called ", name);
    }

    /// A KEY QUEUED AND NOT DRAINED, so a case can place a real message at an exact interval
    /// of the conversation the key begins; the case pumps.
    void enqueue_key(std::int64_t sc) {
        (void)r.bus.publish(loom::Message(
            loom::to_value(input::KeyPressed{sc, "", input::mod::kNone}), loom::WeaveId{},
            loom::WeaveId{}, 0));
    }
    /// ...AND A PRESS ON ONE OF THE PANE'S PROVIDER ROWS, queued the same way: `press_pane`'s
    /// arithmetic against the picture as it stands when the press is queued.
    void enqueue_press(std::int64_t row) {
        const ui::Rect body = external_body_rect(r.session(), kind);
        (void)r.bus.publish(loom::Message(
            loom::to_value(input::PointerButton{1, true, body.x,
                                                body.y + kExternalHeaderRows + row +
                                                    surface::kTuiCanvasTopRow,
                                                input::space::kCells, input::mod::kNone}),
            loom::WeaveId{}, loom::WeaveId{}, 0));
    }

    /// A STRANGER THAT CAN SAY `zen.DispatchRefused` AS A SHAPE -- granted so the forgery
    /// case proves the PANE's refusal to act on ordinary speech, not the bus's grant refusal.
    DoorAsker* stranger = nullptr;
    loom::WeaveId stranger_id{};
    void mount_stranger() {
        auto held = std::make_unique<DoorAsker>(std::string(kDoorAskerOffice));
        stranger = held.get();
        loom::Grant grant;
        grant.allow_to_any(loom::DispatchRefused::zen_name, loom::DispatchRefused::zen_version);
        stranger_id = r.bus.register_weave(std::move(held), std::move(grant),
                                           std::string(kDoorAskerOffice));
        stranger->zen_set_self(stranger_id);
        stranger->id = stranger_id;
    }
    /// Queue a forged refusal notice to the browser naming `attempt`; nothing is drained.
    void forge_refusal(loom::WeaveId to, std::uint64_t attempt) {
        REQUIRE(stranger != nullptr);
        stranger->next = [to, attempt](DoorAsker&, loom::Mail& mail) {
            loom::DispatchRefused forged;
            forged.attempt = std::to_string(attempt);
            forged.role = kOpeningRole;
            forged.shape = OpenSourceRequested::zen_name;
            forged.version = OpenSourceRequested::zen_version;
            forged.reason = "NoSuchTarget";
            (void)mail.as_role(kDoorAskerOffice).send(to, forged);
        };
        (void)r.bus.send(stranger_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                    loom::WeaveId{}, 0));
    }

    /// A bare-letter gesture, as a backend really reports one: the key transition AND the
    /// character it produced.
    void letter(std::int64_t scancode, const char* typed) {
        r.key(scancode);
        r.text(typed);
    }

    void settle() { r.bus.drain_until_idle(); }

    /// A NEW ROOM AND NOTHING ELSE: the surface changes size, Workshop grants the pane its room
    /// again, and the pane says its rows -- the ordinary repaint that exposes a notice cleared in
    /// private. Required to be a real grant, so a deduplicated extent cannot pass for one.
    void regrant() {
        const ExternalPane* seat = r.session().panels.external_pane(kind);
        REQUIRE(seat != nullptr);
        const std::int64_t rows = seat->rows;
        const std::int64_t columns = seat->columns;
        wide_ = !wide_;
        r.extent(wide_ ? 160 : 150, wide_ ? 48 : 44);
        const ExternalPane* after = r.session().panels.external_pane(kind);
        REQUIRE(after != nullptr);
        REQUIRE_MESSAGE((after->rows != rows || after->columns != columns),
                        "the surface changed and the pane's room did not");
    }
    bool wide_ = true;

    /// The ids the pane declares right now, sorted.
    std::vector<std::string> declared() {
        std::vector<std::string> ids;
        const RuntimePane* seat = row();
        REQUIRE(seat != nullptr);
        for (const v2::PaneActionRow& a : seat->actions) {
            ids.push_back(a.id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    /// The browser's weave, for a tap that reads what crosses its seam.
    loom::WeaveId files_id() {
        const loom::WeaveId id = r.kernel.weave_id(files::kFilesStem);
        REQUIRE(id.value != 0);
        return id;
    }

    /// GIVE THE PANE AN AUTHORED HEIGHT, in canvas cells of the pane's whole window, and repaint
    /// at a new extent -- a room grant, which also takes the listing again and puts the cursor
    /// back on its first row.
    void author_height(std::int64_t cells, std::int64_t width, std::int64_t height) {
        const Written wrote = author_pane_size(r.session().setup.active, files_ref(), PaneSize{},
                                               PaneSize{pane_unit::kSubcells, subs(cells)});
        REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
        r.extent(width, height);
    }

    /// HOW MANY ROWS THE PANE WAS GRANTED, read through the body geometry `press_pane` spends. The
    /// picture can be shorter: a pane publishes the rows it has, and the rest of its room is blank.
    std::int64_t granted_rows() {
        return external_body_rect(r.session(), kind).h - kExternalHeaderRows;
    }

    /// PUT THE LONG LISTING'S CURSOR ON ENTRY `at`, SPEND WHATEVER NOTICE STANDS, AND -- when
    /// asked -- LEAD WITH A REFUSAL. Every step is a maker's key: an arrow is an act, so it spends
    /// a standing notice, and Return on a file with no opening office held is refused, which gives
    /// the pane a notice naming that file without moving its cursor.
    void settle(std::int64_t at, bool refusal_leads) {
        std::int64_t cursor = cursor_said(shown());
        r.key(cursor > 0 ? input::scan::kUp : input::scan::kDown);
        r.key(cursor > 0 ? input::scan::kDown : input::scan::kUp);
        for (; cursor < at; ++cursor) {
            r.key(input::scan::kDown);
        }
        for (; cursor > at; --cursor) {
            r.key(input::scan::kUp);
        }
        if (refusal_leads) {
            r.key(input::scan::kReturn);
        }
        const std::vector<std::string> rows = shown();
        const std::string seen = picture(rows);
        INFO("settled on entry ", at, " as\n", seen);
        REQUIRE(cursor_said(rows) == at);
        REQUIRE(row_beginning(rows, "Files ") == (refusal_leads ? 1 : 0));
        if (refusal_leads) {
            REQUIRE(rows[0].rfind("`" + entry_name(at) + "` was not opened", 0) == 0);
        }
    }

    /// PRESS ONE PAINTED ROW AND JUDGE THE PRESS BY WHAT THAT ROW SAID. An entry the cursor is not
    /// on is selected; the cursor's own entry is opened, because the pane already holds the keys;
    /// the notice, the header, a count of entries not shown and a blank are no act, so nothing is
    /// selected, nothing is asked and the picture does not move.
    void press_and_judge(SeamTap& tap, std::int64_t row) {
        const std::vector<std::string> before = shown();
        const std::string seen = picture(before);
        const std::int64_t cursor = cursor_said(before);
        const std::string said =
            row < static_cast<std::int64_t>(before.size()) ? before[static_cast<std::size_t>(row)]
                                                           : std::string();
        const std::int64_t entry = entry_shown(said);
        const std::size_t presses = tap.pressed.size();
        const std::size_t attempts = tap.attempts;
        press_pane(r, kind, row, 0);
        const std::vector<std::string> after = shown();
        const std::string now = picture(after);
        INFO("pressed row ", row, " of\n", seen, "and the pane then showed\n", now);
        REQUIRE(tap.pressed.size() == presses + 1);
        CHECK(tap.pressed.back() == row);
        if (entry >= 0 && said.rfind("> ", 0) == 0) {
            CHECK(tap.attempts == attempts + 1);
            CHECK(cursor_said(after) == cursor);
            CHECK(after[0].rfind("`" + entry_name(entry) + "` was not opened", 0) == 0);
        } else if (entry >= 0) {
            CHECK(tap.attempts == attempts);
            CHECK(cursor_said(after) == entry);
            CHECK(row_beginning(after, "Files ") == 0);
        } else {
            CHECK(tap.attempts == attempts);
            CHECK(after == before);
        }
    }

    /// EVERY ROW THE ROOM HOLDS, PRESSED ONCE FROM THE SAME PICTURE: the cursor on `at`, with a
    /// refusal leading or not, settled again before each press because a press can change it.
    /// The rig's title press gave the pane the keys and the arrows that settle it keep them
    /// there, so every press here is made by a maker whose keys are already Files': a press on
    /// the cursor's own row is the activation gesture.
    void sweep(std::int64_t at, bool refusal_leads) {
        REQUIRE(typing_pane(r.session()) == kind);
        SeamTap tap(r.bus, files_id());
        for (std::int64_t row = 0; row < granted_rows(); ++row) {
            settle(at, refusal_leads);
            press_and_judge(tap, row);
        }
    }

    void mount_project_door() {
        auto door = std::make_unique<ProjectDoor>(r.host.project_dir, marks_path);
        ProjectDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
        const loom::WeaveId id =
            r.bus.register_weave(std::move(door), std::move(say), std::string(kProjectRole));
        raw->zen_set_self(id);
    }

    void mount_recipes_door() {
        r.host.use_recipes = [this](const std::string& path) {
            HostContext::RecipeSwap done;
            const Written read = install_recipes(recipes, path, root.generic_string(),
                                                 r.host.project_dir, &HostContext::so_in);
            done.accepted = read.accepted;
            done.refusal = read.refusal;
            done.path = recipes.source();
            done.recipes = recipes.all().size();
            return done;
        };
        static std::vector<std::unique_ptr<authoring::RecipeAuthor>> kept;
        kept.push_back(std::make_unique<authoring::RecipeAuthor>(authoring::RecipeAuthor{
            root.generic_string(), r.host.project_dir, &recipes, r.host.use_recipes}));
        authoring::RecipeAuthor* author = kept.back().get();
        r.host.author_recipe = [author](const HostContext::RecipeDraft& draft) {
            return authoring::author_recipe(*author, draft);
        };
        auto door = std::make_unique<RecipesDoor>(r.host.use_recipes, r.host.author_recipe);
        RecipesDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(RecipeOutcome::zen_name, RecipeOutcome::zen_version);
        const loom::WeaveId id =
            r.bus.register_weave(std::move(door), std::move(say), std::string(kRecipesRole));
        raw->zen_set_self(id);
    }
};

} // namespace

// ============================================================================
// FILES-WEAVE — the pane arrives, and it is a stranger
// ============================================================================

TEST_CASE("FILES-WEAVE: the browser arrives by a plan row, under an office of its own") {
    // ⭐ THE PHASE'S CENTRAL CLAIM, MEASURED AT THE SEAM. Workshop compiled nothing for this
    // pane, minted no kind for it and holds no branch on it: what puts it on a maker's
    // screen is a row in an editable file naming an artifact, and an offer this host learns
    // about at runtime like any other.
    FilesRig f("files-arrive");
    f.open();

    // A RUNTIME HANDLE, MINTED FROM A LIVE OFFER -- never a compile-time kind.
    REQUIRE(f.row() != nullptr);
    CHECK(is_runtime_kind(f.kind));
    CHECK(std::string(f.row()->name) == files::kProjectFilesName);
    // ...AND THE PICKER LISTS IT UNDER THE OFFICE THAT OFFERED IT, which is the only
    // answer to "whose pane is this" (WL-CAT-03).
    bool listed = false;
    for (const CatalogRow& row : combined_catalog(f.r.session().panels)) {
        listed = listed || row.ref == files_ref();
    }
    CHECK(listed);
    // ...AND THE HOST'S OWN CATALOG DOES NOT OFFER IT, from the other side: there is one
    // Files pane in this process and it belongs to the image that was loaded.
    for (const PanelKind& built_in : kPanelCatalog) {
        CHECK(std::string(built_in.pane) != files::kProjectFilesPane);
    }
}

TEST_CASE("FILES-WEAVE: the pane declares its rows with the ids a maker's keymap already knows") {
    // ⭐ WHY A MAKER'S AUTHORED KEYMAP KEEPS WORKING ACROSS THIS MIGRATION. The built-in
    // declared `files.up`, `files.open`, `files.use-recipes` and the rest as host action
    // rows; the weave declares the SAME ids through `PaneActions`, so a file that moved one
    // of them moves it still -- and nothing about the file's grammar changed.
    FilesRig f("files-actions");
    f.open();
    REQUIRE(f.row() != nullptr);
    const std::vector<v2::PaneActionRow>& declared = f.row()->actions;
    REQUIRE_MESSAGE(!declared.empty(), "the pane declared no actions at all");
    std::vector<std::string> ids;
    for (const v2::PaneActionRow& row : declared) {
        ids.push_back(row.id);
    }
    for (const char* id : {files::kActionUp, files::kActionDown, files::kActionOpen,
                           files::kActionParent, files::kActionRefresh,
                           files::kActionUseRecipes, files::kActionMark,
                           files::kActionPickBuildable}) {
        CHECK_MESSAGE(any_row(ids, id), "the pane declares no row for `", id, "`");
    }
    // ⚠ AND `files.cancel` IS NOT AMONG THEM WHILE BROWSING, because there is nothing to
    // cancel. A pane is ONE keyboard context -- its rows are joined into one map under its
    // runtime handle -- so it declares what is true NOW and re-declares when its mode
    // changes; the case below is the other half of that.
    CHECK_FALSE(any_row(ids, files::kActionCancel));
    // ...AND THE HOST DECLARES NONE OF THEM ITSELF, which is what makes the join above the
    // whole answer rather than half of one.
    for (const std::string& id : ids) {
        CHECK_MESSAGE(row_of_id(id.c_str()) == nullptr,
                      "the host still declares an action row for `", id, "`");
    }
}

// ============================================================================
// FILES-WEAVE — browsing, through the room and the resolved ids
// ============================================================================

TEST_CASE("FILES-WEAVE: the pane lists the place this run began, asked of the host") {
    // THE READ-ONLY DOOR, END TO END. The pane has no `HostContext`; where this run began
    // is a value it asked an office for on the beat it was first granted room.
    FilesRig f("files-listing");
    put_file(f.root / "alpha.cpp", "int a;\n");
    put_file(f.root / "beta.cpp", "int b;\n");
    std::filesystem::create_directory(f.root / "src");
    f.open();

    const std::vector<std::string> rows = f.shown();
    REQUIRE_FALSE(rows.empty());
    // THE HEADER NAMES THE PANE, HOW FAR THROUGH THE LISTING THE CURSOR IS, AND WHERE.
    CHECK(rows[0].rfind("Files ", 0) == 0);
    CHECK(rows[0].find("/3") != std::string::npos);
    // DIRECTORIES FIRST, THEN FILES, BYTEWISE -- the pure half's order, seen from outside.
    CHECK(any_row(rows, "src/"));
    CHECK(any_row(rows, "alpha.cpp"));
    CHECK(any_row(rows, "beta.cpp"));
    const std::int64_t dir_at = row_with_text(rows, "src/");
    const std::int64_t file_at = row_with_text(rows, "alpha.cpp");
    REQUIRE(dir_at >= 0);
    REQUIRE(file_at >= 0);
    CHECK(dir_at < file_at);
}

TEST_CASE("FILES-WEAVE: an arrow is a resolved action id, and the cursor moves") {
    // ⭐ THE KEYSTROKE'S WHOLE ROUTE. A maker presses Down; Workshop resolves it against
    // the effective keymap -- which now holds this pane's own declared rows -- and sends
    // the ID, not the key. The pane never sees a scancode for a command.
    FilesRig f("files-arrows");
    put_file(f.root / "alpha.cpp", "int a;\n");
    put_file(f.root / "beta.cpp", "int b;\n");
    f.open();

    CHECK(f.at_cursor().rfind("alpha.cpp", 0) == 0);
    f.r.key(input::scan::kDown);
    CHECK(f.at_cursor().rfind("beta.cpp", 0) == 0);
    f.r.key(input::scan::kUp);
    CHECK(f.at_cursor().rfind("alpha.cpp", 0) == 0);
    // ...AND IT STOPS AT THE ENDS RATHER THAN WRAPPING OR RUNNING OFF.
    f.r.key(input::scan::kUp);
    CHECK(f.at_cursor().rfind("alpha.cpp", 0) == 0);
    for (int i = 0; i < 8; ++i) {
        f.r.key(input::scan::kDown);
    }
    CHECK(f.at_cursor().rfind("beta.cpp", 0) == 0);
}

TEST_CASE("FILES-WEAVE: Return walks into a directory, Backspace walks out") {
    FilesRig f("files-walk");
    std::filesystem::create_directory(f.root / "src");
    put_file(f.root / "src" / "inner.cpp", "int i;\n");
    f.open();

    f.point_at("src/");
    f.r.key(input::scan::kReturn);
    CHECK(any_row(f.shown(), "inner.cpp"));
    CHECK(any_row(f.shown(), (f.root / "src").generic_string()));

    f.r.key(input::scan::kBackspace);
    CHECK(any_row(f.shown(), "src/"));
    CHECK(any_row(f.shown(), f.root.generic_string()));
}

TEST_CASE("FILES-WEAVE: a press selects, and a second press on the same row activates") {
    // THE TWO-PRESS PROMISE IS THE PANE'S OWN (the focus register's fourth law), decided from the
    // fact Workshop reports with the press. The keys begin elsewhere -- a press outside the pane
    // gives them to Workshop -- so the first press, even on the row the cursor already rests on,
    // only selects it and takes the keys; the second, the keys now Files', activates it.
    FilesRig f("files-press");
    std::filesystem::create_directory(f.root / "src");
    put_file(f.root / "src" / "inner.cpp", "int i;\n");
    put_file(f.root / "zulu.cpp", "int z;\n");
    f.open();
    press_outside(f.r, f.kind);
    REQUIRE(keyboard_pane(f.r.session().panels) == kNoPaneKind);
    SeamTap tap(f.r.bus, f.files_id());

    // Row 0 is the pane's own header; row 1 is the first entry under it, `src/`, where the cursor
    // already is.
    press_pane(f.r, f.kind, 1, 0);
    CHECK(f.at_cursor().rfind("src/", 0) == 0);
    CHECK_FALSE(any_row(f.shown(), "inner.cpp"));
    press_pane(f.r, f.kind, 1, 0);
    CHECK(any_row(f.shown(), "inner.cpp"));
    CHECK(tap.keys_went_here == std::vector<int>{0, 1});
}

TEST_CASE("FILES-WEAVE: the wheel moves the cursor, and a header press names no entry") {
    FilesRig f("files-wheel");
    for (int i = 0; i < 12; ++i) {
        put_file(f.root / ("f" + std::to_string(i) + ".cpp"), "x\n");
    }
    f.open(160, 14);

    const std::string was = f.at_cursor();
    const ui::Rect body = external_body_rect(f.r.session(), f.kind);
    f.r.wheel_cell(-1.0, body.x + 1, body.y + 2);
    CHECK(f.at_cursor() != was);
    // ...AND A HEADER PRESS SELECTS NOTHING, in both places a header is. Workshop's title row sits
    // above every row the pane was granted, so a press there reaches the pane as nothing at all --
    // which is what makes the focus press in `open()` a focus press rather than a hidden
    // selection. The pane's own header is its row 0, located in the picture, and names no entry.
    const std::string now = f.at_cursor();
    SeamTap tap(f.r.bus, f.files_id());
    f.r.press_cell(body.x, body.y);
    CHECK(tap.pressed.empty());
    CHECK(f.at_cursor() == now);
    const std::int64_t header = row_beginning(f.shown(), "Files ");
    REQUIRE(header == 0);
    press_pane(f.r, f.kind, header, 0);
    REQUIRE(tap.pressed.size() == 1);
    CHECK(tap.pressed[0] == header);
    CHECK(f.at_cursor() == now);
}

// ============================================================================
// A press names the entry the pane painted -- whatever leads the room, and however it is cut
// ============================================================================

TEST_CASE("a press on Files' painted header while a refusal leads selects nothing and opens nothing") {
    // THE HEADER MOVES DOWN A ROW WHEN A NOTICE LEADS, and a press is read against the picture it
    // was aimed at. The refusal here is an open no opening office could take, so the cursor stays
    // on the file it names; the header is found in the rows Workshop painted, not assumed at 0.
    FilesRig f("files-press-header");
    put_file(f.root / "alpha.cpp", "int a;\n");
    put_file(f.root / "beta.cpp", "int b;\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/false);
    f.point_at("beta.cpp");
    f.r.key(input::scan::kReturn);
    const std::vector<std::string> before = f.shown();
    const std::string seen = picture(before);
    INFO("the pane showed\n", seen);
    REQUIRE(before[0].find("could not reach") != std::string::npos);
    const std::int64_t header = row_beginning(before, "Files ");
    REQUIRE(header == 1);

    SeamTap tap(f.r.bus, f.files_id());
    press_pane(f.r, f.kind, header, 0);
    REQUIRE(tap.pressed.size() == 1);
    CHECK(tap.pressed[0] == header);
    const std::vector<std::string> after = f.shown();
    const std::string now = picture(after);
    INFO("after the header press the pane showed\n", now);
    CHECK(f.at_cursor().rfind("beta.cpp", 0) == 0);
    CHECK(tap.attempts == 0);
    CHECK(after == before); // no act: the refusal still leads, and nothing else moved

    // ...AND THE BLANK ROOM UNDER THE LISTING IS THE SAME NOTHING: the pane published fewer rows
    // than it was granted, and its last granted row shows nothing. Read again first, in case the
    // press above changed the picture.
    const std::vector<std::string> ahead = f.shown();
    const std::int64_t blank = f.granted_rows() - 1;
    REQUIRE(blank >= static_cast<std::int64_t>(ahead.size()));
    press_pane(f.r, f.kind, blank, 0);
    REQUIRE(tap.pressed.size() == 2);
    CHECK(tap.pressed[1] == blank);
    CHECK(tap.attempts == 0);
    CHECK(f.shown() == ahead);
}

TEST_CASE("a press on Files' painted selected row while a refusal leads opens exactly that row's file") {
    // THE ACTIVATION, UNDER A NOTICE. The rig's title press gave the pane the keys with the cursor
    // on `alpha.cpp`; Return's open is refused for want of an opening office, and the refusal
    // leads. The office is then held, so the press on the row marked `>` -- the keys still
    // Files' -- is the activation gesture, and it must open the file that row shows, not the row
    // below it. What it asked for and what the Editor then holds are both read as exact paths.
    FilesRig f("files-press-selected");
    put_file(f.root / "alpha.cpp", "the alpha source\n");
    put_file(f.root / "beta.cpp", "the beta source\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/false);
    REQUIRE(row_beginning(f.shown(), "> alpha.cpp") == 1);
    REQUIRE(typing_pane(f.r.session()) == f.kind);
    f.r.key(input::scan::kReturn);
    f.r.mount_opening();
    const std::vector<std::string> before = f.shown();
    const std::string seen = picture(before);
    INFO("the pane showed\n", seen);
    REQUIRE(before[0].find("could not reach") != std::string::npos);
    const std::int64_t selected = row_beginning(before, "> alpha.cpp");
    REQUIRE(selected == 2);

    SeamTap tap(f.r.bus, f.files_id());
    press_pane(f.r, f.kind, selected, 0);
    REQUIRE(tap.pressed.size() == 1);
    CHECK(tap.pressed[0] == selected);
    const std::vector<std::string> after = f.shown();
    const std::string now = picture(after);
    INFO("after the press the pane showed\n", now);
    CHECK(row_beginning(after, "> alpha.cpp") >= 0); // the selection did not move to its neighbour
    const std::string alpha = (f.root / "alpha.cpp").lexically_normal().generic_string();
    CHECK(tap.attempts == 1);
    REQUIRE(tap.requested.size() == 1);
    CHECK(tap.requested[0] == alpha);
    const std::int64_t editor = f.editor_kind();
    REQUIRE(f.r.session().panels.has(editor));
    CHECK(f.r.session().panels.keyboard == editor);
    const loom::SenseReading held = f.r.bus.observe(f.r.kernel.weave_id("zengine-editor-pane"),
                                                    EditorDocument::zen_name,
                                                    EditorDocument::zen_version);
    REQUIRE(held.value);
    CHECK(loom::from_value<EditorDocument>(*held.value).path == alpha);
    const std::vector<std::string> source = pane_rows(f.r, editor);
    CHECK(std::find(source.begin(), source.end(), "the alpha source") != source.end());
}

TEST_CASE("every row Files paints names only what it shows, at the head of a long listing and scrolled into it, with a notice leading and without") {
    // THE WINDOW SEATS FEWER ENTRIES THAN IT HAS ROWS: a `... N earlier` and a `... N more` take
    // rows of their own, a notice takes one above the header, and the cursor sits wherever the
    // window centred it. Each painted row is pressed from the same settled picture, and judged by
    // what that row said.
    FilesRig f("files-press-window");
    for (std::int64_t i = 0; i < kLongListing; ++i) {
        put_file(f.root / entry_name(i), "x\n");
    }
    f.open(160, 48, /*with_editor=*/false, /*with_manager=*/false);
    REQUIRE(f.granted_rows() >= 6); // room for a notice, the header, both markers and two entries
    REQUIRE(f.granted_rows() < kLongListing);

    SUBCASE("at the head, with nothing leading") {
        f.sweep(0, false);
    }
    SUBCASE("at the head, with a refusal leading") {
        f.sweep(0, true);
    }
    SUBCASE("scrolled into the middle, with nothing leading") {
        f.sweep(kLongListing / 2, false);
    }
    SUBCASE("scrolled into the middle, with a refusal leading") {
        f.sweep(kLongListing / 2, true);
    }
}

TEST_CASE("in a room too small for the window Files composes, a press names only a row that was painted") {
    // THE COMPOSITION CAN OUTGROW ITS ROOM: one entry is always seated with both its markers, and
    // a notice is put in front before the whole picture is cut to the room. Three rows scrolled
    // into a long listing paint the header, `... N earlier` and the cursor's own entry -- or, with
    // a refusal leading, no entry at all. A press may name nothing the cut took away.
    FilesRig f("files-press-small");
    for (std::int64_t i = 0; i < kLongListing; ++i) {
        put_file(f.root / entry_name(i), "x\n");
    }
    f.open(160, 48, /*with_editor=*/false, /*with_manager=*/false);
    f.author_height(6, 160, 47);
    REQUIRE(f.granted_rows() == 3);

    SUBCASE("with nothing leading") {
        f.sweep(kLongListing / 2, false);
    }
    SUBCASE("with a refusal leading") {
        f.sweep(kLongListing / 2, true);
    }
}

// ============================================================================
// A press opens a row only where the keys already were -- and Workshop says where they were
// ============================================================================

TEST_CASE("a press on Files' selected row after its open moved the keys to the Editor selects that row and takes the keys back, and opens nothing") {
    // THE PRESS THAT POINTS THE KEYS IS NOT AN ACT IN THE PANE (WL-FOCUS-04). Files opened beta
    // and the Editor took the keys; beta is still Files' selection. Pressing it again is a maker
    // coming back to Files, and what it may do is aim the keys -- a second open of beta is the
    // defect. Only the press after that, with the keys Files' own, opens it.
    FilesRig f("files-back-from-open");
    put_file(f.root / "alpha.cpp", "the alpha source\n");
    put_file(f.root / "beta.cpp", "the beta source\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/true);
    const std::string beta = (f.root / "beta.cpp").lexically_normal().generic_string();
    SeamTap tap(f.r.bus, f.files_id());
    press_pane(f.r, f.kind, row_beginning(f.shown(), "  beta.cpp"), 0);
    REQUIRE(f.at_cursor().rfind("beta.cpp", 0) == 0);
    REQUIRE(tap.attempts == 0);
    f.r.key(input::scan::kReturn);
    const std::int64_t editor = f.editor_kind();
    REQUIRE(tap.requested == std::vector<std::string>{beta});
    REQUIRE(keyboard_pane(f.r.session().panels) == editor);

    // THE KEYS ARE THE EDITOR'S: an ordinary key reaches it, and Files hears nothing.
    SeamTap editor_tap(f.r.bus, f.r.kernel.weave_id("zengine-editor-pane"));
    const std::size_t files_heard = tap.heard.size();
    f.r.key(input::scan::kDown);
    CHECK(editor_tap.keys + editor_tap.actions == 1);
    CHECK(tap.heard.size() == files_heard);

    // ONE PRESS ON FILES' STILL-SELECTED ROW: it selects, the keys come back, nothing is asked.
    const std::int64_t selected = row_beginning(f.shown(), "> beta.cpp");
    REQUIRE(selected >= 0);
    const std::size_t presses = tap.pressed.size();
    press_pane(f.r, f.kind, selected, 0);
    REQUIRE(tap.pressed.size() == presses + 1);
    CHECK(tap.versions.back() == 2u);
    CHECK(tap.keys_went_here.back() == 0);
    CHECK(tap.requested == std::vector<std::string>{beta});
    CHECK(tap.attempts == 1);
    CHECK(keyboard_pane(f.r.session().panels) == f.kind);
    CHECK(f.at_cursor().rfind("beta.cpp", 0) == 0);

    // ...AND THE NEXT PRESS ON IT, THE KEYS BEING FILES', IS THE ACTIVATION.
    press_pane(f.r, f.kind, row_beginning(f.shown(), "> beta.cpp"), 0);
    REQUIRE(tap.pressed.size() == presses + 2);
    CHECK(tap.keys_went_here.back() == 1);
    CHECK(tap.requested == std::vector<std::string>{beta, beta});
    CHECK(keyboard_pane(f.r.session().panels) == editor);
}

TEST_CASE("the keys leave Files by a press into the Editor and Files is told nothing, so only Workshop can say a later press on Files' selected row came from elsewhere") {
    // WHY A PANE-LOCAL MEMORY CANNOT ANSWER THIS. The keys leave Files for the Editor by a press
    // on the Editor's own row, and no delivery reaches Files: there is no departure message, by
    // design (the vocabulary's "no focus-changed notification"). The fact has to come with the
    // press, from the party that routes the keys.
    FilesRig f("files-leave-by-press");
    put_file(f.root / "alpha.cpp", "the alpha source\n");
    put_file(f.root / "beta.cpp", "the beta source\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/true);
    const std::string alpha = (f.root / "alpha.cpp").lexically_normal().generic_string();
    press_pane(f.r, f.kind, row_beginning(f.shown(), "  beta.cpp"), 0);
    f.r.key(input::scan::kReturn);
    const std::int64_t editor = f.editor_kind();
    REQUIRE(f.r.session().panels.has(editor));
    press_pane(f.r, f.kind, row_beginning(f.shown(), "  alpha.cpp"), 0);
    REQUIRE(f.at_cursor().rfind("alpha.cpp", 0) == 0);
    REQUIRE(keyboard_pane(f.r.session().panels) == f.kind);

    SeamTap tap(f.r.bus, f.files_id());
    SeamTap editor_tap(f.r.bus, f.r.kernel.weave_id("zengine-editor-pane"));
    press_pane(f.r, editor, 1, 0); // the Editor's first document row
    CHECK(keyboard_pane(f.r.session().panels) == editor);
    CHECK(tap.heard.empty()); // nothing at all reached Files as the keys left it
    // THE EDITOR, A PANE THAT ACCEPTS ONLY THE FIRST VERSION, HEARD EXACTLY ONE PRESS IN IT.
    REQUIRE(editor_tap.pressed.size() == 1);
    CHECK(editor_tap.versions[0] == 1u);
    CHECK(editor_tap.refused == 0);

    press_pane(f.r, f.kind, row_beginning(f.shown(), "> alpha.cpp"), 0);
    REQUIRE(tap.pressed.size() == 1);
    CHECK(tap.keys_went_here[0] == 0);
    CHECK(tap.attempts == 0);
    CHECK(keyboard_pane(f.r.session().panels) == f.kind);
    press_pane(f.r, f.kind, row_beginning(f.shown(), "> alpha.cpp"), 0);
    REQUIRE(tap.pressed.size() == 2);
    CHECK(tap.keys_went_here[1] == 1);
    CHECK(tap.requested == std::vector<std::string>{alpha});
}

TEST_CASE("a press on Files' selected row opens it when the keys were already Files', whether Workshop's title, Files' own header or an arrow left them there") {
    // THE OTHER HALF OF THE SAME LAW. Nothing about how the keys came to Files is Files' to know:
    // Workshop's title is not a provider row and sends no press, Files' header names no entry, and
    // an arrow arrives as a resolved id rather than a key. What Files is told is whether the keys
    // were its own when the maker pressed.
    FilesRig f("files-already-here");
    put_file(f.root / "alpha.cpp", "the alpha source\n");
    put_file(f.root / "beta.cpp", "the beta source\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/true);
    const std::string beta = (f.root / "beta.cpp").lexically_normal().generic_string();

    SUBCASE("the rig's title press left the keys on Files, and an arrow chose the row") {
        SeamTap tap(f.r.bus, f.files_id());
        f.r.key(input::scan::kDown);
        CHECK(tap.actions == 1); // a resolved id...
        CHECK(tap.keys == 0);    // ...and no raw key at all
        REQUIRE(f.at_cursor().rfind("beta.cpp", 0) == 0);
        press_pane(f.r, f.kind, row_beginning(f.shown(), "> beta.cpp"), 0);
        REQUIRE(tap.pressed.size() == 1);
        CHECK(tap.keys_went_here[0] == 1);
        CHECK(tap.requested == std::vector<std::string>{beta});
    }
    SUBCASE("from the Editor, a press on Workshop's title of Files took the keys and sent Files nothing") {
        f.r.key(input::scan::kDown); // chosen and opened by keys: no press of Files' is in its past
        f.r.key(input::scan::kReturn);
        REQUIRE(keyboard_pane(f.r.session().panels) == f.editor_kind());
        SeamTap tap(f.r.bus, f.files_id());
        const ui::Rect body = external_body_rect(f.r.session(), f.kind);
        f.r.press_cell(body.x, body.y);
        CHECK(tap.pressed.empty());
        REQUIRE(keyboard_pane(f.r.session().panels) == f.kind);
        press_pane(f.r, f.kind, row_beginning(f.shown(), "> beta.cpp"), 0);
        REQUIRE(tap.pressed.size() == 1);
        CHECK(tap.keys_went_here[0] == 1);
        CHECK(tap.requested == std::vector<std::string>{beta});
    }
    SUBCASE("from the Editor, a press on Files' own header took the keys and named no entry") {
        f.r.key(input::scan::kDown);
        f.r.key(input::scan::kReturn);
        REQUIRE(keyboard_pane(f.r.session().panels) == f.editor_kind());
        SeamTap tap(f.r.bus, f.files_id());
        const std::int64_t header = row_beginning(f.shown(), "Files ");
        REQUIRE(header == 0);
        press_pane(f.r, f.kind, header, 0);
        REQUIRE(tap.pressed.size() == 1);
        CHECK(tap.keys_went_here[0] == 0);
        CHECK(tap.attempts == 0);
        REQUIRE(keyboard_pane(f.r.session().panels) == f.kind);
        press_pane(f.r, f.kind, row_beginning(f.shown(), "> beta.cpp"), 0);
        REQUIRE(tap.pressed.size() == 2);
        CHECK(tap.keys_went_here[1] == 1);
        CHECK(tap.requested == std::vector<std::string>{beta});
    }
}

TEST_CASE("two presses on Files' selected row queued while the keys were the Editor's cross as elsewhere then here, and open that row's file once") {
    // ONE DRAIN, TWO GESTURES, NO RE-WINDOW BETWEEN THEM. Each press is resolved by Workshop in
    // its own turn, so the second one is read after the first moved the keys: the fact each
    // carries is the routing at ITS press. The first aims, the second acts.
    FilesRig f("files-two-queued");
    put_file(f.root / "alpha.cpp", "the alpha source\n");
    put_file(f.root / "beta.cpp", "the beta source\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/true);
    const std::string beta = (f.root / "beta.cpp").lexically_normal().generic_string();
    press_pane(f.r, f.kind, row_beginning(f.shown(), "  beta.cpp"), 0);
    f.r.key(input::scan::kReturn);
    REQUIRE(keyboard_pane(f.r.session().panels) == f.editor_kind());
    const std::int64_t row = row_beginning(f.shown(), "> beta.cpp");
    REQUIRE(row >= 0);

    SeamTap tap(f.r.bus, f.files_id());
    f.enqueue_press(row);
    f.enqueue_press(row);
    f.r.bus.drain_until_idle();
    REQUIRE(tap.pressed.size() == 2);
    CHECK(tap.pressed[0] == row);
    CHECK(tap.pressed[1] == row);
    CHECK(tap.keys_went_here == std::vector<int>{0, 1});
    CHECK(tap.attempts == 1);
    CHECK(tap.requested == std::vector<std::string>{beta});
}

TEST_CASE("with pane titles hidden, a first press on the row painted gamma selects gamma once every delivery it caused has settled, and a later press opens gamma") {
    // THE PRESS THAT TAKES THE KEYS ALSO BRINGS BACK THE PANE'S TITLE (WL-FOCUS-11): the keyboard's
    // pane keeps its title whatever the preference says. So the picture the maker pressed has no
    // title row and the picture after the press has one. The press is read against the first;
    // the room the title takes is granted after it, and that re-grant must not move the
    // selection the press made.
    FilesRig f("files-hidden-titles");
    put_file(f.root / "alpha.cpp", "the alpha source\n");
    put_file(f.root / "beta.cpp", "the beta source\n");
    put_file(f.root / "gamma.cpp", "the gamma source\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/true);
    const std::string gamma = (f.root / "gamma.cpp").lexically_normal().generic_string();
    press_outside(f.r, f.kind);
    f.letter(input::scan::kT, "t");
    REQUIRE_FALSE(f.r.session().pane_titles);
    REQUIRE(external_title_rows(f.r.session().panels, f.kind, f.r.session().pane_titles) == 0);

    // THE COORDINATES ARE THE PICTURE'S: the region's rows as painted, with no title row in them.
    const ui::Rect body = external_body_rect(f.r.session(), f.kind);
    const std::vector<std::string> painted = external_region_rows(f.r.last_canvas(), body);
    const std::string seen = picture(painted);
    INFO("painted with titles hidden and the keys elsewhere:\n", seen);
    const std::int64_t aimed = row_beginning(painted, "  gamma.cpp");
    REQUIRE(aimed >= 1);
    REQUIRE(row_beginning(painted, "Files ") == 0);

    SeamTap tap(f.r.bus, f.files_id());
    f.r.press_cell(body.x, body.y + aimed);
    const std::vector<std::string> after = f.shown();
    const std::string now = picture(after);
    INFO("after the press settled, the pane showed:\n", now);
    REQUIRE(tap.pressed.size() == 1);
    CHECK(tap.pressed[0] == aimed); // the row painted where the press landed
    CHECK(tap.keys_went_here[0] == 0);
    // ...AND THE ROOM THE TITLE TOOK WAS GRANTED AFTER THE PRESS, and settled.
    const auto at_press = std::find(tap.heard.begin(), tap.heard.end(), std::string(PanePressed::zen_name));
    REQUIRE(at_press != tap.heard.end());
    CHECK(std::find(at_press, tap.heard.end(), std::string(PaneRoom::zen_name)) != tap.heard.end());
    REQUIRE(keyboard_pane(f.r.session().panels) == f.kind);
    REQUIRE(external_title_rows(f.r.session().panels, f.kind, f.r.session().pane_titles) == 1);
    CHECK(f.at_cursor().rfind("gamma.cpp", 0) == 0);
    CHECK(tap.attempts == 0);

    // A LATER PRESS ON GAMMA, WHERE IT IS PAINTED NOW, OPENS GAMMA.
    press_pane(f.r, f.kind, row_beginning(f.shown(), "> gamma.cpp"), 0);
    REQUIRE(tap.pressed.size() == 2);
    CHECK(tap.keys_went_here[1] == 1);
    CHECK(tap.requested == std::vector<std::string>{gamma});
}

TEST_CASE("a press into Files while the picker has the keys never opens its selected row, and the band does not say typing goes to Files; with the picker closed the press opens it") {
    // A MODE ABOVE THE PANE HAS THE KEYS, AND THE PANE IS STILL THE CANDIDATE (WL-FOCUS-06). A Files
    // pane taller than the picker's slot can be pressed below it: the press makes Files the pane the
    // keys return to, but an ordinary key reaches the picker. Workshop reports that, and says it:
    // neither the press nor the band may claim the keys are Files'.
    FilesRig f("files-under-picker");
    for (std::int64_t i = 0; i < 12; ++i) {
        put_file(f.root / entry_name(i), "x\n");
    }
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/true);
    f.author_height(34, 160, 47); // taller than the picker's slot, so rows below it are Files'
    press_outside(f.r, f.kind);
    f.r.key(input::scan::kP);
    REQUIRE(f.r.session().panels.picker.open);

    // AN ENTRY ROW OF FILES THE PICKER DOES NOT COVER, found by the walk a press spends.
    const ui::Rect body = external_body_rect(f.r.session(), f.kind);
    const std::vector<std::string> rows = pane_rows(f.r, f.kind);
    std::int64_t row = -1;
    for (std::int64_t i = static_cast<std::int64_t>(rows.size()) - 1; i >= 0 && row < 0; --i) {
        const Occupancy there =
            occupied_at(f.r.session().panels, f.r.session().setup.active, screen_of(f.r.session()),
                        body.x, body.y + kExternalHeaderRows + i);
        if (there.kind == f.kind && rows[static_cast<std::size_t>(i)].rfind("  entry-", 0) == 0) {
            row = i;
        }
    }
    const std::string seen = picture(rows);
    INFO("Files under the picker showed:\n", seen);
    REQUIRE(row >= 0);
    const std::string entry = rows[static_cast<std::size_t>(row)].substr(2);
    const std::string path =
        (f.root / entry).lexically_normal().generic_string();

    SeamTap tap(f.r.bus, f.files_id());
    press_pane(f.r, f.kind, row, 0); // selects it, and makes Files the candidate
    REQUIRE(f.r.session().panels.picker.open);
    REQUIRE(keyboard_pane(f.r.session().panels) == f.kind);
    REQUIRE(keyboard_context(f.r.session()) == KeyContext::kPicker);
    REQUIRE(f.at_cursor() == entry);
    for (const std::string& line : band_lines(f.r)) {
        CHECK_MESSAGE(line.find("typing goes to") == std::string::npos, line);
    }
    press_pane(f.r, f.kind, row, 0); // ...and again, on the row it now has selected
    REQUIRE(tap.pressed.size() == 2);
    CHECK(tap.keys_went_here == std::vector<int>{0, 0});
    CHECK(tap.attempts == 0);
    CHECK(f.r.session().panels.picker.open);
    // AN ORDINARY KEY IS THE PICKER'S, which is what the fact said.
    const std::size_t picker_was = f.r.session().panels.picker.cursor;
    f.r.key(input::scan::kDown);
    CHECK(f.r.session().panels.picker.cursor != picker_was);
    CHECK(tap.keys + tap.actions == 0);

    // CLOSING THE PICKER HANDS THE KEYS TO FILES, the band says so, and the press opens the row.
    f.r.key(input::scan::kEscape);
    REQUIRE_FALSE(f.r.session().panels.picker.open);
    REQUIRE(keyboard_context(f.r.session()) == KeyContext::kPane);
    CHECK(band_lines(f.r).at(0).find("typing goes to Files @zengine.files") != std::string::npos);
    press_pane(f.r, f.kind, row, 0);
    REQUIRE(tap.pressed.size() == 3);
    CHECK(tap.keys_went_here[2] == 1);
    CHECK(tap.requested == std::vector<std::string>{path});
}

TEST_CASE("a press into Files while the hotkey view has the keys never opens its selected row, and the band does not say typing goes to Files; with the view closed the press opens it") {
    // THE HOTKEY VIEW IS NOT A KEYBOARD CONTEXT (WL-KEY-11): it is keys-modal over whatever context
    // lies beneath, which here is Files itself -- the candidate, the resolved pane and the context
    // all still name Files, and still no ordinary key reaches it. So "where the keys went" is not
    // `keyboard_context` alone.
    FilesRig f("files-under-hotkeys");
    put_file(f.root / "alpha.cpp", "the alpha source\n");
    put_file(f.root / "beta.cpp", "the beta source\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/true);
    const std::string beta = (f.root / "beta.cpp").lexically_normal().generic_string();
    press_pane(f.r, f.kind, row_beginning(f.shown(), "  beta.cpp"), 0); // the keys are Files'
    REQUIRE(f.at_cursor().rfind("beta.cpp", 0) == 0);
    REQUIRE(band_lines(f.r).at(0).find("typing goes to Files @zengine.files") != std::string::npos);

    f.r.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(f.r.session().hotkeys.open);
    REQUIRE(keyboard_pane(f.r.session().panels) == f.kind);
    REQUIRE(keyboard_context(f.r.session()) == KeyContext::kPane);
    for (const std::string& line : band_lines(f.r)) {
        CHECK_MESSAGE(line.find("typing goes to") == std::string::npos, line);
    }
    SeamTap tap(f.r.bus, f.files_id());
    press_pane(f.r, f.kind, row_beginning(f.shown(), "> beta.cpp"), 0);
    REQUIRE(tap.pressed.size() == 1);
    CHECK(tap.keys_went_here[0] == 0);
    CHECK(tap.attempts == 0);
    CHECK(f.r.session().hotkeys.open);
    // AN ORDINARY KEY IS THE VIEW'S.
    f.r.key(input::scan::kUp);
    CHECK(tap.keys + tap.actions == 0);
    CHECK(f.at_cursor().rfind("beta.cpp", 0) == 0);

    f.r.key(input::scan::kEscape);
    REQUIRE_FALSE(f.r.session().hotkeys.open);
    CHECK(band_lines(f.r).at(0).find("typing goes to Files @zengine.files") != std::string::npos);
    press_pane(f.r, f.kind, row_beginning(f.shown(), "> beta.cpp"), 0);
    REQUIRE(tap.pressed.size() == 2);
    CHECK(tap.keys_went_here[1] == 1);
    CHECK(tap.requested == std::vector<std::string>{beta});
}

TEST_CASE("a press from a host that states no routing fact only selects in Files, even on the selected row with the keys Files', and Return still opens it") {
    // AN OLDER HOST AND A NEWER FILES. A host that answers nothing about which version an office
    // accepts sends every press as v1, and v1 says nothing about where the keys were -- so Files
    // does not guess that they were its own. The key a maker already opens with still opens.
    FilesRig f("files-older-host");
    put_file(f.root / "alpha.cpp", "the alpha source\n");
    put_file(f.root / "beta.cpp", "the beta source\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/true);
    f.r.host.holder_accepts = nullptr;
    const std::string beta = (f.root / "beta.cpp").lexically_normal().generic_string();
    SeamTap tap(f.r.bus, f.files_id());
    press_pane(f.r, f.kind, row_beginning(f.shown(), "  beta.cpp"), 0);
    REQUIRE(typing_pane(f.r.session()) == f.kind);
    press_pane(f.r, f.kind, row_beginning(f.shown(), "> beta.cpp"), 0);
    REQUIRE(tap.pressed.size() == 2);
    CHECK(tap.versions == std::vector<std::uint32_t>{1u, 1u});
    CHECK(tap.attempts == 0);
    CHECK(f.at_cursor().rfind("beta.cpp", 0) == 0);
    CHECK(keyboard_pane(f.r.session().panels) == f.kind);
    f.r.key(input::scan::kReturn);
    CHECK(tap.requested == std::vector<std::string>{beta});
}

TEST_CASE("Files takes a press of either version only from Workshop's office and about its own pane, and opens only on a second-version press that says the keys were already there") {
    // THE PROVIDER'S OWN CHECKS, MEASURED FROM THE ONLY SIDE THEY SHOW ON: a weave holding
    // `zengine.workshop` in Workshop's place grants the room and says every press itself, each
    // aimed at the row the cursor is on. Speech that holds the office without speaking as it,
    // and a press about some other pane, act on nothing; a first-version press selects; a
    // second-version press opens exactly when it says the keys were already this pane's.
    FilesRig f("files-press-authors");
    put_file(f.root / "alpha.cpp", "the alpha source\n");
    put_file(f.root / "beta.cpp", "the beta source\n");
    PaneWatcher* watch = f.r.mount_watcher();
    f.mount_project_door();
    REQUIRE(f.r.load(files::kFilesStem, WORKSHOP_SO_FILES, files::kFilesRole).valid());
    f.r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant(m, files::kFilesRole, PaneRoom{files::kProjectFilesPane, 8, 60});
    });
    REQUIRE_FALSE(watch->content.empty());
    REQUIRE(watch->content.back().rows.size() >= 3);
    REQUIRE(watch->content.back().rows[1].text.rfind("> alpha.cpp", 0) == 0);
    SeamTap tap(f.r.bus, f.files_id());
    std::size_t said = watch->content.size();

    f.r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.press_personally(m, files::kFilesRole,
                            v2::PanePressed{files::kProjectFilesPane, 1, 0, true});
    });
    REQUIRE(tap.pressed.size() == 1); // delivered...
    CHECK(tap.attempts == 0);         // ...and not acted on
    CHECK(watch->content.size() == said);

    f.r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.press(m, files::kFilesRole, v2::PanePressed{"somebody-else", 1, 0, true});
    });
    REQUIRE(tap.pressed.size() == 2);
    CHECK(tap.attempts == 0);
    CHECK(watch->content.size() == said);

    f.r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.press(m, files::kFilesRole, PanePressed{files::kProjectFilesPane, 1, 0});
    });
    REQUIRE(tap.pressed.size() == 3);
    CHECK(tap.attempts == 0);
    CHECK(watch->content.size() == said + 1); // it selected, and said so
    said = watch->content.size();

    f.r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.press(m, files::kFilesRole, v2::PanePressed{files::kProjectFilesPane, 1, 0, false});
    });
    REQUIRE(tap.pressed.size() == 4);
    CHECK(tap.attempts == 0);
    CHECK(watch->content.size() == said + 1);

    f.r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.press(m, files::kFilesRole, v2::PanePressed{files::kProjectFilesPane, 1, 0, true});
    });
    REQUIRE(tap.pressed.size() == 5);
    CHECK(tap.attempts == 1);
}

// ============================================================================
// FILES-WEAVE — the three doors, from the pane's side
// ============================================================================

TEST_CASE("FILES-WEAVE: Return on a source opens it in the Editor, through the one door") {
    // ⭐ THE EDITOR DOOR, FROM THE ASKING SIDE. The pane cannot open a file; it asks, and the
    // Editor weave's one door (`OpenSourceRequested` at `zengine.editor`, WL-EDIT-05) does
    // everything the built-in's did -- and then asks Workshop to show the pane it filled.
    FilesRig f("files-open");
    put_file(f.root / "alpha.cpp", "the project\n");
    f.open(160, 48, /*with_editor=*/true);

    f.point_at("alpha.cpp");
    f.r.key(input::scan::kReturn);
    const std::int64_t editor = f.editor_kind();
    REQUIRE(f.r.session().panels.has(editor));
    CHECK(f.r.session().panels.keyboard == editor);
    CHECK(f.editor_status().rfind("saved L1:C1/2", 0) == 0);
    CHECK(f.editor_status().find("alpha.cpp") != std::string::npos); // the path keeps its end
    const std::vector<std::string> rows = pane_rows(f.r, editor);
    CHECK(std::find(rows.begin(), rows.end(), "the project") != rows.end());
}

TEST_CASE("FILES-WEAVE: a dirty Editor's refusal comes back and the pane says it") {
    // ⭐ THE NO-SILENT-LOSS FLOOR, REACHING A PANE THAT IS NOT IN THIS PROCESS -- from a
    // document that is not in this process either. The Editor weave refuses; the refusal
    // travels back as a value; the browser says it in its own first row. Nothing about the
    // maker's unsaved work moved.
    FilesRig f("files-dirty");
    put_file(f.root / "alpha.cpp", "int a;\n");
    put_file(f.root / "beta.cpp", "int b;\n");
    f.open(160, 48, /*with_editor=*/true);

    f.point_at("alpha.cpp");
    f.r.key(input::scan::kReturn);
    const std::int64_t editor = f.editor_kind();
    REQUIRE(f.r.session().panels.has(editor));
    REQUIRE(f.r.session().panels.keyboard == editor); // the reveal pointed the keys here
    press_pane(f.r, editor, 1, 0);                   // the first document row
    f.r.text("x");
    REQUIRE(f.editor_status().rfind("UNSAVED", 0) == 0);

    // Back to the browser, and ask for a different source.
    press_pane(f.r, f.kind, 1, 0);
    f.point_at("beta.cpp");
    f.r.key(input::scan::kReturn);
    // THE PANE LEADS WITH THE REFUSAL rather than with its header, which is how a maker sees
    // that something was said. The refusal's WORDS are the door's and are asserted as a value
    // where the door is (`test_workshop_panes_editor.cpp`); a pane's row is fitted to the room
    // it was granted, and a temporary directory's path is long enough on Windows to cut them.
    CHECK(f.first().rfind("Files ", 0) != 0);
    CHECK(f.editor_status().rfind("UNSAVED", 0) == 0);
    CHECK(f.editor_status().find("alpha.cpp") != std::string::npos);
    CHECK(f.editor_status().find("beta.cpp") == std::string::npos);
}

TEST_CASE("FILES-WEAVE: `u` moves the recipe catalog, and the pane says which and how much") {
    // ⭐ THE ACTING DOOR, FROM THE ASKING SIDE, AND THE WHOLE LOOP IN ONE GESTURE. The pane
    // resolves the row to a path, asks `zengine.recipes`, and the host's one install seam
    // does the rest -- then the pane reads the answer back and says both halves of it.
    FilesRig f("files-userecipes");
    put_catalog(f.root / "recipes.json", {authored_recipe("alpha", "src/alpha.cpp"),
                                          authored_recipe("beta", "src/beta.cpp")});
    f.open();

    f.point_at("recipes.json");
    f.letter(input::scan::kU, "u");

    CHECK(f.recipes.source() == (f.root / "recipes.json").generic_string());
    REQUIRE(f.recipes.all().size() == 2);
    CHECK(f.recipes.all()[0].id == "alpha");
    // The pane's row is fitted to the room it was granted, so what is asserted here is the
    // sentence's HEAD and the file it names; the whole sentence is a value and is asserted
    // as one in `files_weave`.
    CHECK(f.first().rfind("build recipes:", 0) == 0);
}

TEST_CASE("FILES-WEAVE: the answer the pane asked for does not erase what it just said") {
    // ⭐ THE DEFECT THE WHOLE-LOOP WITNESS FOUND, pinned. `BuildStatus` is published for two
    // different reasons -- a build settling, and somebody merely ASKING what the state is --
    // and this pane asks, itself, immediately after an accepted catalog choice. Treating the
    // answer as a finished build made the pane re-list and re-say, so the sentence it had
    // just written about the catalog was gone before a maker could read it. Measured on a
    // real terminal: `u` on a catalog produced no visible row at all.
    //
    // ⚔ MUTATION: dropping the `builds` gate. The first row goes back to being the header.
    FilesRig f("files-status");
    put_catalog(f.root / "recipes.json", {authored_recipe("alpha", "src/alpha.cpp")});
    f.open();

    f.point_at("recipes.json");
    f.letter(input::scan::kU, "u");
    REQUIRE(f.first().rfind("build recipes:", 0) == 0);

    // THE ANSWER TO THE PANE'S OWN ASK: the same world, described again. Nothing was built,
    // so `builds` is unchanged and there is no news in it.
    zengine::builder::BuildStatus described;
    described.outcome = zengine::builder::outcome::kNeverBuilt;
    described.builds = 0;
    f.r.publish(loom::to_value(described));
    CHECK(f.first().rfind("build recipes:", 0) == 0);

    // ...AND A BUILD THAT REALLY FINISHED IS STILL NEWS: the listing is taken again, because
    // a build can create or remove files.
    put_file(f.root / "made-by-the-build.txt", "made by a build\n");
    zengine::builder::BuildStatus finished;
    finished.outcome = zengine::builder::outcome::kSucceeded;
    finished.builds = 1;
    f.r.publish(loom::to_value(finished));
    CHECK(any_row(f.shown(), "made-by-the-build.txt"));
}

TEST_CASE("FILES-WEAVE: a notice stands until the maker's next act") {
    // ⭐ THE GENERAL RULE THE GATE ABOVE IS ONE INSTANCE OF (`agents/panes.md`). This pane
    // used to clear its notice inside `say`, and the Builder's migration proved that wrong
    // one pane over: a gesture makes SEVERAL publications in one drain and Workshop keeps
    // only the last picture, so a sentence spent by the first `say` is a sentence no maker
    // ever reads. The narrow repair above gated ONE re-say; this is the lifetime.
    //
    // ⚔ MUTATION, MEASURED: `notice_.clear()` back at the end of `say`. The standing check
    //   goes red -- the sentence is gone one publication later, and the row is the header
    //   again. The spend check still passes, because a sentence already spent looks exactly
    //   like a sentence the next act spent; that is why the first half is the one that
    //   carries this claim.
    FilesRig f("files-notice-stands");
    put_catalog(f.root / "recipes.json", {authored_recipe("alpha", "src/alpha.cpp")});
    f.open();

    f.point_at("recipes.json");
    f.letter(input::scan::kU, "u");
    REQUIRE(f.first().rfind("build recipes:", 0) == 0);

    // A PUBLICATION THAT IS NOT THE MAKER'S DOING: a build somebody else ordered settles, so
    // this pane takes a fresh listing and says its rows again. The sentence the maker has not
    // read yet survives it, and the new file is there.
    put_file(f.root / "made-by-the-build.txt", "made by a build");
    zengine::builder::BuildStatus described;   // the pane's baseline: what the tool IS
    described.outcome = zengine::builder::outcome::kNeverBuilt;
    described.builds = 0;
    f.r.publish(loom::to_value(described));
    zengine::builder::BuildStatus finished;    // ...and then a build that really happened
    finished.outcome = zengine::builder::outcome::kSucceeded;
    finished.builds = 1;
    f.r.publish(loom::to_value(finished));
    CHECK(f.first().rfind("build recipes:", 0) == 0);
    CHECK(any_row(f.shown(), "made-by-the-build.txt"));

    // ...AND THE MAKER'S NEXT ACT SPENDS IT. One press of Down: the pane leads with its own
    // header again, because the sentence was the answer to an act that is now two acts old.
    f.r.key(input::scan::kDown);
    CHECK(f.first().rfind("Files", 0) == 0);
}

TEST_CASE("FILES-WEAVE: the notice takes its row from the listing, not from the room's end") {
    // ⭐ A NOTICE THAT STANDS HAS TO HAVE SOMEWHERE TO STAND. `say` puts the sentence in
    // front of the composition and cuts the whole thing to the room, so a composition that
    // already filled the room loses its LAST row to the notice -- and the last row of a long
    // listing is `... N more`, the only thing telling a maker the list goes on. Worse, a
    // composition that OVERRAN the room (the window fills the body with entries and the two
    // markers are pushed on top of them) left no room for the notice at all, so the sentence
    // was not merely cut short -- it never appeared.
    //
    // So the listing is asked for the rows that are actually free: the room, less this pane's
    // header, less the notice, less its own markers.
    //
    // ⚔ MUTATIONS, MEASURED. `body_budget()` back to `rows_ - kHeaderRows`: the notice takes
    //   its row from the end of the list instead, and the last row a maker can read becomes
    //   `  entry-03.txt` -- a list that goes on and no longer says so. `fitted_window` back to
    //   `window_of`: the same loss, and it is there BEFORE any notice exists (`  entry-04.txt`),
    //   which is what the first half of this case is for.
    FilesRig f("files-notice-room");
    for (int i = 0; i < 40; ++i) {
        put_file(f.root / ("entry-" + std::string(i < 10 ? "0" : "") + std::to_string(i) + ".txt"),
                 "x");
    }
    f.open();

    // A LONG LISTING WITH NO NOTICE: the room is full and the marker is the last thing in it.
    const std::string full = f.last_written();
    CHECK_MESSAGE(full.find("more") != std::string::npos, "the last row was: ", full);

    // ...AND NOW WITH ONE. `r` re-lists and says so; the sentence leads, the marker survives,
    // and the room is no fuller than it was.
    const std::size_t room = f.shown().size();
    f.letter(input::scan::kR, "r");
    CHECK(f.first().rfind("listed ", 0) == 0);
    const std::string with = f.last_written();
    CHECK_MESSAGE(with.find("more") != std::string::npos, "the last row was: ", with);
    CHECK(f.shown().size() == room);
}

TEST_CASE("FILES-WEAVE: a refused catalog leaves the maker exactly where they were") {
    // THE RECOVERY CLAIM, AT THE NEW SEAM. The browser lists every real file and judges no
    // contents, so pointing at one that is not a catalog is an ordinary thing to do -- and
    // the refusal has to say what went wrong AND what is still running.
    FilesRig f("files-refused");
    put_catalog(f.root / "a.json", {authored_recipe("alpha", "src/alpha.cpp")});
    put_file(f.root / "notes.txt", "not a catalog\n");
    f.open();

    f.point_at("a.json");
    f.letter(input::scan::kU, "u");
    REQUIRE(f.recipes.all().size() == 1);

    f.point_at("notes.txt");
    f.letter(input::scan::kU, "u");
    const std::string said = f.first();
    CHECK(said.rfind("not a recipe catalog", 0) == 0);
    CHECK(said.find("the recipes in force are unchanged") != std::string::npos);
    // ...AND WHAT IS STILL RUNNING IS THE FILE THAT WAS THERE BEFORE, never the one just
    // refused: the owner is asked AFTER the attempt, so the catalog in force is unmoved.
    CHECK(f.recipes.source() == (f.root / "a.json").generic_string());

    // AND THE PANE IS STILL A BROWSER: the next gesture is answered.
    f.letter(input::scan::kR, "r");
    CHECK(f.first().find("listed") != std::string::npos);
}

TEST_CASE("FILES-WEAVE: a directory is refused in the pane's own words, before the host") {
    // THE PRE-CHECK STAYS WITH THE PANE. A directory is not a candidate for a catalog, and
    // the pane knows that from the row it is standing on -- so the owner is never troubled
    // and the sentence is the pane's.
    FilesRig f("files-dirrefused");
    std::filesystem::create_directory(f.root / "somewhere");
    f.open();

    f.point_at("somewhere/");
    f.letter(input::scan::kU, "u");
    const std::string said = f.first();
    CHECK(said.find("is a directory") != std::string::npos);
    CHECK(said.find("one authored file") != std::string::npos);
    CHECK(f.recipes.all().empty());
}

// ============================================================================
// FILES-WEAVE — the places a maker keeps, in the pane's own durable file
// ============================================================================

TEST_CASE("FILES-WEAVE: a marked place is the pane's own file, written by the pane") {
    // THE MARKS ARE THE PANE'S, and the host's only part in them is answering WHERE the
    // file lives. Nothing about a mark reaches `Session`, and the file appears the moment a
    // maker marks something and not before.
    FilesRig f("files-marks");
    std::filesystem::create_directory(f.root / "src");
    // A WIDE SCREEN, DELIBERATELY. The pane's rows are fitted to the room it was granted, and
    // this case reads a PATH out of one; a temporary directory's absolute path on Windows is
    // longer than the ordinary room, so the width is part of the arrangement rather than
    // something the assertion should try to work around.
    f.open(240, 48);

    CHECK_FALSE(std::filesystem::exists(f.marks_path));
    f.point_at("src/");
    f.r.key(input::scan::kReturn);
    f.letter(input::scan::kM, "m");
    CHECK(std::filesystem::exists(f.marks_path));
    const std::string bytes = slurp(f.marks_path);
    CHECK(bytes.find((f.root / "src").generic_string()) != std::string::npos);

    // ...AND `n` STEPS BETWEEN THE PLACES A MAKER KEPT, so the marked one is reachable
    // from anywhere by pressing it. The stops are the origin, the marks and this platform's
    // filesystem roots, so the case walks the ring rather than asserting a position in it.
    f.r.key(input::scan::kBackspace); // out of the marked place, so arriving is a move
    // ⚠ ASKED OF THE PROVENANCE WORD AND THE LEAF, not of the whole path: the header is fitted
    // to the room the pane was granted, and a temporary directory's absolute path does not fit
    // it on Windows. What the case is about is that the marked place is REACHABLE.
    bool came_back = false;
    std::string seen;
    const std::string marked = (f.root / "src").generic_string();
    for (int step = 0; step < 40 && !came_back; ++step) {
        f.letter(input::scan::kN, "n");
        const std::string said = f.first();
        seen += "[" + said + "]";
        came_back = said.rfind("at " + marked, 0) == 0;
    }
    INFO("the ring, row by row: ", seen);
    CHECK(came_back);
}

// ============================================================================
// FILES-WEAVE — picking something buildable, and authoring its row in-pane
// ============================================================================

TEST_CASE("FILES-WEAVE: `a` opens a chooser inside the pane's own room") {
    // ⭐ WHAT USED TO BE A MODAL OVERLAY IS ROWS IN THIS PANE'S ROOM. The chooser is not a
    // second surface, not a popup and not a host panel: the pane simply says different rows
    // and takes the two mode actions it declared for them.
    FilesRig f("files-pick");
    put_file(f.root / "oven.cpp", "// a maker's weave\n");
    std::filesystem::create_directories(f.root / "tree");
    put_file(f.root / "tree" / "CMakeCache.txt", "# configured\n");
    put_file(f.root / "notes.txt", "not buildable\n");
    f.open();

    f.letter(input::scan::kA, "a");
    const std::vector<std::string> rows = f.shown();
    CHECK(any_row(rows, "pick something buildable"));
    CHECK(any_row(rows, "oven.cpp"));
    CHECK(any_row(rows, "tree/"));
    // ...AND WHAT IS NOT BUILDABLE IS NOT OFFERED, which is what makes the list a judgement
    // rather than a second listing.
    CHECK_FALSE(any_row(rows, "notes.txt"));

    // ESCAPE IS THE MODE'S OWN DECLARED ACTION, and it backs out whole.
    f.r.key(input::scan::kEscape);
    CHECK(any_row(f.shown(), "notes.txt"));
}

TEST_CASE("FILES-WEAVE: a maker authors a recipe row in-pane, and the host writes it") {
    // ⭐⭐ THE WHOLE AUTHORING LOOP, THROUGH A LOADED PANE. A maker picks a source, types
    // four fields into a line inside the pane's own room, and the HOST's one authoring
    // writer composes the row, checks it by the recipe law, appends it as written, saves it
    // and installs it. The pane composed no recipe and wrote no file.
    FilesRig f("files-author");
    put_file(f.root / "oven.cpp", "// a maker's weave\n");
    put_catalog(f.root / "recipes.json", {authored_recipe("alpha", "src/alpha.cpp")});
    f.open();

    f.point_at("recipes.json");
    f.letter(input::scan::kU, "u");
    REQUIRE(f.recipes.all().size() == 1);

    f.letter(input::scan::kA, "a");
    REQUIRE(any_row(f.shown(), "pick something buildable"));
    // The chooser's first candidate is the one source here.
    f.r.key(input::scan::kReturn);

    // FOUR FIELDS, EACH A LINE INSIDE THE PANE'S ROOM. The suggested default is cleared the
    // way a maker clears it, so what is written is what was typed.
    const auto answer = [&f](const std::string& text) {
        for (int i = 0; i < 64; ++i) {
            f.r.key(input::scan::kBackspace);
        }
        if (!text.empty()) {
            f.r.text(text);
        }
        f.r.key(input::scan::kReturn);
    };
    answer("oven");           // what a maker calls it
    answer("zengine-oven");   // the stem it produces
    answer("loom");           // the package prefix its build needs
    answer("loom::kernel");   // and the target it links

    // THE HOST WROTE IT, AND THE CATALOG IN FORCE HOLDS BOTH ROWS.
    REQUIRE(f.recipes.all().size() == 2);
    CHECK(f.recipes.all()[1].id == "oven");
    REQUIRE(f.recipes.all()[1].single_source.has_value());
    CHECK(f.recipes.all()[1].single_source->source ==
          (f.root / "oven.cpp").generic_string());
    // ...AND THE FILE ON DISK IS THE ONE THAT WAS IN FORCE, appended as written.
    CHECK(slurp((f.root / "recipes.json").generic_string()).find("oven") != std::string::npos);
    // ...AND THE MAKER IS TOLD WHAT WAS WRITTEN, in the pane's own row.
    const std::string said = f.first();
    CHECK(said.find("authored recipe") != std::string::npos);
    CHECK(said.find("oven") != std::string::npos);
    CHECK(said.find("zengine-oven") != std::string::npos);
}

TEST_CASE("FILES-WEAVE: the authoring line takes raw keys, and Escape abandons it whole") {
    // THE ONE PLACE THIS PANE READS A SCANCODE, and it is a component's editing gestures
    // rather than a command. Everything else the pane does arrives as a resolved id.
    FilesRig f("files-authoring-keys");
    put_file(f.root / "oven.cpp", "// a maker's weave\n");
    f.open();

    f.letter(input::scan::kA, "a");
    f.r.key(input::scan::kReturn); // choose the one candidate
    const std::vector<std::string> prompt = f.shown();
    REQUIRE_FALSE(prompt.empty());
    CHECK(any_row(prompt, ">"));

    for (int i = 0; i < 64; ++i) {
        f.r.key(input::scan::kBackspace);
    }
    f.r.text("ove");
    CHECK(any_row(f.shown(), "ove"));
    f.r.key(input::scan::kBackspace);
    CHECK_FALSE(any_row(f.shown(), "ove"));

    // ESCAPE ABANDONS THE WHOLE PROMPT, not one field: nothing was written and the pane is
    // a browser again.
    f.r.key(input::scan::kEscape);
    CHECK(any_row(f.shown(), "oven.cpp"));
    CHECK(f.recipes.all().empty());
}

TEST_CASE("a key the Files authoring line does not take is no act: the notice stands through a repaint, and a key it takes spends it") {
    // WHAT IS NOT AN ACT SPENDS NOTHING. The line refuses keys it has no meaning for; a refused
    // key used to clear the notice privately and say no rows, so the next unrelated repaint
    // dropped the line's own instructions.
    FilesRig f("files-notice-unspent");
    put_file(f.root / "oven.cpp", "// a maker's weave\n");
    f.open();
    f.letter(input::scan::kA, "a");
    f.r.key(input::scan::kReturn); // the one candidate: the line opens, saying how to use it
    REQUIRE(any_row(f.shown(), "Return commits a field"));
    f.r.key(input::scan::kDown); // a key the line has no meaning for
    f.r.extent(150, 44);         // an unrelated repaint: the room is granted again
    CHECK(any_row(f.shown(), "Return commits a field"));
    f.r.key(input::scan::kLeft); // a key the line takes
    CHECK_FALSE(any_row(f.shown(), "Return commits a field"));
}

TEST_CASE("an id Files does not declare in the mode it is in is no act: an unknown one, a browsing id while the authoring line is open, and a second cancel resolved in the same poll leave the notice standing through a new room, and a declared id that moves nothing still spends it") {
    // WHAT IS NOT AN ACT SPENDS NOTHING, FOR AN ID AS FOR A KEY. The pane cleared its notice
    // before it asked what the id meant in the mode it was in, then said its rows without it --
    // so an id nobody declared erased the authoring line's own instructions, and so did one the
    // line's mode does not declare.
    FilesRig f("files-ids-unspent");
    put_file(f.root / "oven.cpp", "// a maker's weave\n");
    f.open();
    for (int i = 0; i < 8; ++i) {
        f.r.key(input::scan::kUp); // the cursor on the first row, before any notice stands
    }
    const std::string resting = f.at_cursor();
    REQUIRE_FALSE(resting.empty());
    f.letter(input::scan::kA, "a");
    f.r.key(input::scan::kReturn); // the one candidate: the line opens, saying how to use it
    REQUIRE(any_row(f.shown(), "Return commits a field"));
    const std::vector<std::string> line_ids{files::kActionCancel, files::kActionCommitField};
    REQUIRE(f.declared() == line_ids);
    const auto line_stands = [&f, &line_ids] {
        CHECK(any_row(f.shown(), "Return commits a field"));
        CHECK(any_row(f.shown(), "recipe name> "));
        CHECK(f.declared() == line_ids);
        CHECK(f.recipes.all().empty());
    };

    // AN ID NOBODY DECLARED, SAID BY WORKSHOP'S OWN OFFICE.
    const PaneRig::OfficeAction unknown =
        f.r.workshop_action(files::kFilesRole, files::kProjectFilesPane, "files.no-such-action");
    REQUIRE(unknown.authored);
    REQUIRE(unknown.delivered);
    CHECK(unknown.author == kWorkshopProvider);
    line_stands();
    f.regrant();
    line_stands();

    // AN ID THE PANE DECLARES WHEN BROWSING, DELIVERED WHILE THE LINE IS OPEN -- its open among
    // them, which is Return while browsing and never the line's commit.
    for (const char* browsing : {files::kActionRefresh, files::kActionOpen}) {
        const PaneRig::OfficeAction said =
            f.r.workshop_action(files::kFilesRole, files::kProjectFilesPane, browsing);
        REQUIRE(said.delivered);
        line_stands();
        f.regrant();
        line_stands();
    }

    // ESCAPE TWICE IN ONE POLL: the line's cancel, then a cancel Workshop resolved against the
    // line's rows, arriving when the browser declares none. The first cancel's answer stands.
    f.enqueue_key(input::scan::kEscape);
    f.enqueue_key(input::scan::kEscape);
    f.settle();
    REQUIRE_FALSE(f.declared() == line_ids);
    CHECK(f.first().rfind("no recipe was written", 0) == 0);
    f.regrant();
    CHECK(f.first().rfind("no recipe was written", 0) == 0);

    // A DECLARED ID THAT MOVES NOTHING IS STILL AN ACT: `files.up` on the first row spends the
    // notice, in the rows Workshop holds, through a new room too.
    REQUIRE(f.at_cursor() == resting);
    const PaneRig::OfficeAction up =
        f.r.workshop_action(files::kFilesRole, files::kProjectFilesPane, files::kActionUp);
    REQUIRE(up.delivered);
    CHECK(f.at_cursor() == resting);
    CHECK(f.first().rfind("Files", 0) == 0);
    f.regrant();
    CHECK(f.first().rfind("Files", 0) == 0);
    CHECK(f.recipes.all().empty());
}

TEST_CASE("an id Files resolved in one mode is no act in the next") {
    // ⭐ A KEY QUEUED BEHIND A MODE CHANGE IS STILL THE OPERATION IT WAS RESOLVED AS. Workshop
    // resolves every key of a poll against the rows in force before the pane has declared its
    // next ones, so a Return queued behind Escape crosses as the closing mode's own id. While
    // that id was `files.open` in every mode, the browser took it as its own and opened the file
    // under its cursor. Each mode's Return is an id of its own, and one the rows in force do not
    // declare is no act.
    //
    // THE KEYS ARE QUEUED AND THEN DRAINED ONCE (`enqueue_key`, `settle`). A helper that drained
    // between them would let the pane's next declaration arrive first, and nothing would race.
    const std::vector<std::string> chooser_ids{files::kActionCancel, files::kActionChoose,
                                               files::kActionDown, files::kActionUp};
    const std::vector<std::string> line_ids{files::kActionCancel, files::kActionCommitField};
    const auto burst = [](FilesRig& f, std::int64_t first, std::int64_t second) {
        f.enqueue_key(first);
        f.enqueue_key(second);
        f.settle();
    };
    // NOTHING WAS OPENED: no ask left the browser, and the Editor did not come onto the desk.
    const auto opened_nothing = [](FilesRig& f, const SeamTap& tap) {
        CHECK(tap.attempts == 0);
        CHECK(tap.requested.empty());
        CHECK_FALSE(f.r.session().panels.has(f.editor_kind()));
    };
    // THE ANSWER THE PANE LEADS WITH, before and after a new room.
    const auto leads = [](FilesRig& f, const std::string& notice) {
        CHECK(f.first().find(notice) == 0);
        f.regrant();
        CHECK(f.first().find(notice) == 0);
    };

    SUBCASE("Escape then Return over the line: cancelled, and nothing opens") {
        FilesRig f("files-race-line");
        put_file(f.root / "oven.cpp", "// a maker's weave\n");
        f.open(160, 48, /*with_editor=*/true);
        REQUIRE(f.at_cursor().rfind("oven.cpp", 0) == 0); // what a browsing open would open
        f.letter(input::scan::kA, "a");
        f.r.key(input::scan::kReturn); // the one candidate: the line opens
        REQUIRE(f.declared() == line_ids);
        SeamTap tap(f.r.bus, f.files_id());
        burst(f, input::scan::kEscape, input::scan::kReturn);
        CHECK(tap.ids == std::vector<std::string>{files::kActionCancel, files::kActionCommitField});
        opened_nothing(f, tap);
        CHECK(tap.authored.empty());
        CHECK(f.recipes.all().empty());
        CHECK(any_row(f.declared(), files::kActionOpen)); // the browser's rows are in force
        leads(f, "no recipe was written");
        CHECK(f.at_cursor().rfind("oven.cpp", 0) == 0);
    }
    SUBCASE("Escape then Return over the chooser: cancelled, and nothing opens") {
        FilesRig f("files-race-chooser");
        put_file(f.root / "oven.cpp", "// a maker's weave\n");
        f.open(160, 48, /*with_editor=*/true);
        REQUIRE(f.at_cursor().rfind("oven.cpp", 0) == 0);
        f.letter(input::scan::kA, "a");
        REQUIRE(f.declared() == chooser_ids);
        SeamTap tap(f.r.bus, f.files_id());
        burst(f, input::scan::kEscape, input::scan::kReturn);
        CHECK(tap.ids == std::vector<std::string>{files::kActionCancel, files::kActionChoose});
        opened_nothing(f, tap);
        CHECK(tap.authored.empty());
        CHECK(any_row(f.declared(), files::kActionOpen));
        leads(f, "no recipe was authored");
    }
    SUBCASE("two Returns over the chooser: one choice, and the line keeps its first field") {
        FilesRig f("files-race-choose");
        put_file(f.root / "oven.cpp", "// a maker's weave\n");
        f.open(160, 48, /*with_editor=*/true);
        f.letter(input::scan::kA, "a");
        REQUIRE(f.declared() == chooser_ids);
        SeamTap tap(f.r.bus, f.files_id());
        burst(f, input::scan::kReturn, input::scan::kReturn);
        CHECK(tap.ids == std::vector<std::string>{files::kActionChoose, files::kActionChoose});
        CHECK(f.declared() == line_ids);
        CHECK(any_row(f.shown(), "recipe name> oven"));
        CHECK_FALSE(any_row(f.shown(), "artifact stem>"));
        opened_nothing(f, tap);
        CHECK(tap.authored.empty());
        leads(f, "a source file: oven.cpp -- Return commits a field");
        CHECK(any_row(f.shown(), "recipe name> oven"));
    }
    SUBCASE("two Returns on the line's last field: one recipe, and nothing opens") {
        FilesRig f("files-race-last");
        put_file(f.root / "oven.cpp", "// a maker's weave\n");
        put_catalog(f.root / "recipes.json", {authored_recipe("alpha", "src/alpha.cpp")});
        f.open(160, 48, /*with_editor=*/true);
        f.point_at("recipes.json");
        f.letter(input::scan::kU, "u");
        REQUIRE(f.recipes.all().size() == 1);
        f.point_at("oven.cpp");
        f.letter(input::scan::kA, "a");
        f.r.key(input::scan::kReturn);
        for (const char* typed : {"oven", "zengine-oven", "loom", "loom::kernel"}) {
            for (int i = 0; i < 64; ++i) {
                f.r.key(input::scan::kBackspace);
            }
            f.r.text(typed);
            if (std::string(typed) != "loom::kernel") {
                f.r.key(input::scan::kReturn);
            }
        }
        REQUIRE(any_row(f.shown(), "link targets (comma-separated)> loom::kernel"));
        SeamTap tap(f.r.bus, f.files_id());
        burst(f, input::scan::kReturn, input::scan::kReturn);
        CHECK(tap.ids ==
              std::vector<std::string>{files::kActionCommitField, files::kActionCommitField});
        CHECK(tap.authored == std::vector<std::string>{"oven"});
        CHECK(f.recipes.all().size() == 2);
        opened_nothing(f, tap);
        CHECK(any_row(f.declared(), files::kActionOpen));
        leads(f, "authored recipe `oven`");
    }
    SUBCASE("`a` then Return while browsing: the chooser opens, and nothing is chosen") {
        FilesRig f("files-race-pick");
        put_file(f.root / "oven.cpp", "// a maker's weave\n");
        f.open(160, 48, /*with_editor=*/true);
        REQUIRE(f.at_cursor().rfind("oven.cpp", 0) == 0);
        SeamTap tap(f.r.bus, f.files_id());
        burst(f, input::scan::kA, input::scan::kReturn);
        CHECK(tap.ids == std::vector<std::string>{files::kActionPickBuildable, files::kActionOpen});
        CHECK(f.declared() == chooser_ids);
        opened_nothing(f, tap);
        CHECK(tap.authored.empty());
        leads(f, "pick something buildable -- Return authors a recipe");
        CHECK(any_row(f.shown(), "> oven.cpp"));
    }
}

TEST_CASE("each Files mode's Return is its own id, and a keymap moves each alone") {
    // ⭐ THREE OPERATIONS ON ONE KEY, NEVER DECLARED TOGETHER. The collision law judges the rows in
    // force, and the pane replaces them whenever its mode changes, so Return can be `files.open`,
    // `files.choose` or `files.commit-field` without two of them meeting. What a maker's keymap
    // names is the operation: `files.open` is the browser's enter-or-edit, as it was when the
    // browser was this host's, and moves nothing else.
    //
    // WHICH ROW A GESTURE REQUESTS IS ASKED OF THE EFFECTIVE KEYMAP (`pane_action_for`), and what
    // the legend says is read off the band a maker sees: two readers of the one join.
    const std::vector<std::string> chooser_ids{files::kActionCancel, files::kActionChoose,
                                               files::kActionDown, files::kActionUp};
    const std::vector<std::string> line_ids{files::kActionCancel, files::kActionCommitField};
    const auto requests = [](FilesRig& f, std::int64_t scancode, std::int64_t modifiers) {
        const PaneRow* row = f.r.session().keymap.pane_action_for(f.kind, scancode, modifiers);
        return row != nullptr ? row->id : std::string();
    };
    const auto legend_says = [](FilesRig& f, const std::string& pair) {
        const std::vector<std::string> band = band_lines(f.r);
        REQUIRE(band.size() == 2);
        return band[1].find(pair) != std::string::npos;
    };
    // ADMITTED WHOLE: the pane's row keeps a declaration only when the join accepted it, and the
    // keymap then holds exactly those rows for the pane.
    const auto admitted = [](FilesRig& f, const std::vector<std::string>& ids) {
        CHECK(f.declared() == ids);
        const PaneRows* rows = f.r.session().keymap.pane_rows(f.kind);
        REQUIRE(rows != nullptr);
        CHECK(rows->rows.size() == ids.size());
    };

    SUBCASE("the defaults: Return is each mode's own row, and the legend names it") {
        FilesRig f("files-mode-rows");
        put_file(f.root / "oven.cpp", "// a maker's weave\n");
        f.open();
        const auto browsing = [&] {
            const std::vector<std::string> ids = f.declared();
            CHECK(any_row(ids, files::kActionOpen));
            CHECK_FALSE(any_row(ids, files::kActionChoose));
            CHECK_FALSE(any_row(ids, files::kActionCommitField));
            CHECK(requests(f, input::scan::kReturn, input::mod::kNone) == files::kActionOpen);
            CHECK(legend_says(f, "enter enter or edit"));
        };
        browsing();
        f.letter(input::scan::kA, "a");
        admitted(f, chooser_ids);
        CHECK(requests(f, input::scan::kReturn, input::mod::kNone) == files::kActionChoose);
        CHECK(legend_says(f, "enter choose this"));
        CHECK_FALSE(legend_says(f, "enter or edit"));
        f.r.key(input::scan::kReturn);
        admitted(f, line_ids);
        CHECK(requests(f, input::scan::kReturn, input::mod::kNone) == files::kActionCommitField);
        CHECK(legend_says(f, "enter commit this field"));
        f.r.key(input::scan::kEscape);
        browsing();
    }
    SUBCASE("a keymap file: each override moves its own operation, and waits while its mode is shut") {
        TempDir keys("files-mode-keys");
        const std::string path = keys.file("keymap.json");
        const std::string written = keymap_file_text(
            "default", {{files::kActionOpen, "o"},
                        {files::kActionChoose, "c"},
                        {files::kActionCommitField, "ctrl+j"}});
        write_keymap_file(path, written);
        FilesRig f("files-mode-keys");
        put_file(f.root / "oven.cpp", "// a maker's weave\n");
        f.r.host.keymap_path = path;
        f.open(160, 48, /*with_editor=*/true);
        SeamTap tap(f.r.bus, f.files_id());

        // BROWSING: `o` is the open and Return requests nothing. The chooser's and the line's rows
        // name ids nobody declares yet, so they are kept as written and applied to nothing.
        REQUIRE(f.r.session().keymap.authored.size() == 3);
        CHECK(requests(f, input::scan::kO, input::mod::kNone) == files::kActionOpen);
        CHECK(requests(f, input::scan::kReturn, input::mod::kNone).empty());
        CHECK(requests(f, input::scan::kC, input::mod::kNone).empty());
        CHECK(requests(f, input::scan::kJ, input::mod::kCtrl).empty());
        CHECK(legend_says(f, "o enter or edit"));
        CHECK(keymap_persist::to_text(f.r.session().keymap) == written);
        f.r.key(input::scan::kReturn); // the default this maker moved away from: a key, no act
        CHECK(tap.ids.empty());

        // THE CHOOSER: its own override is in force, and neither Return nor the browser's `o` is.
        f.letter(input::scan::kA, "a");
        admitted(f, chooser_ids);
        CHECK(requests(f, input::scan::kC, input::mod::kNone) == files::kActionChoose);
        CHECK(requests(f, input::scan::kReturn, input::mod::kNone).empty());
        CHECK(requests(f, input::scan::kO, input::mod::kNone).empty());
        CHECK(legend_says(f, "c choose this"));
        f.r.key(input::scan::kReturn);
        f.letter(input::scan::kO, "o");
        CHECK(any_row(f.shown(), "pick something buildable"));
        f.letter(input::scan::kC, "c");
        admitted(f, line_ids);
        CHECK(any_row(f.shown(), "recipe name> oven"));

        // THE LINE: `ctrl+j` commits the field, and Return is a key the line does not take.
        CHECK(requests(f, input::scan::kJ, input::mod::kCtrl) == files::kActionCommitField);
        CHECK(requests(f, input::scan::kReturn, input::mod::kNone).empty());
        CHECK(legend_says(f, "^j commit this field"));
        f.r.key(input::scan::kReturn);
        CHECK(any_row(f.shown(), "recipe name> oven"));
        f.r.key(input::scan::kJ, input::mod::kCtrl);
        CHECK(any_row(f.shown(), "artifact stem> oven"));

        // OUT, AND THE BROWSER'S OWN OVERRIDE OPENS THE FILE UNDER THE CURSOR, IN THE EDITOR.
        f.r.key(input::scan::kEscape);
        REQUIRE(f.first().rfind("no recipe was written", 0) == 0);
        CHECK(tap.attempts == 0);
        f.letter(input::scan::kO, "o");
        REQUIRE(tap.requested.size() == 1);
        CHECK(tap.requested[0] == (f.root / "oven.cpp").generic_string());
        CHECK(f.r.session().panels.has(f.editor_kind()));
        CHECK(tap.ids == std::vector<std::string>{files::kActionPickBuildable, files::kActionChoose,
                                                  files::kActionCommitField, files::kActionCancel,
                                                  files::kActionOpen});
        CHECK(tap.authored.empty());
        // ...AND WHAT A SAVE WOULD WRITE IS STILL WHAT THE MAKER WROTE, all three rows in order.
        CHECK(keymap_persist::to_text(f.r.session().keymap) == written);
    }
}

TEST_CASE("deliberate keys in Files still choose, refuse a blank field, write one recipe and cancel, and Return then opens the file") {
    // ⭐ THE ORDINARY PATH UNDER EACH MODE'S OWN ID, OBSERVED WHERE EACH OPERATION LANDS: the id
    // each key crossed as, the one authored row the host's writer received and put in the catalog
    // in force, and the file the browser's open put in the Editor. A key pressed after its mode's
    // rows have arrived means what the legend said it would.
    FilesRig f("files-deliberate");
    put_file(f.root / "oven.cpp", "// a maker's weave\n");
    put_catalog(f.root / "recipes.json", {authored_recipe("alpha", "src/alpha.cpp")});
    f.open(160, 48, /*with_editor=*/true);
    f.point_at("recipes.json");
    f.letter(input::scan::kU, "u");
    REQUIRE(f.recipes.all().size() == 1);
    f.point_at("oven.cpp");
    SeamTap tap(f.r.bus, f.files_id());
    const auto clear_line = [&f] {
        for (int i = 0; i < 64; ++i) {
            f.r.key(input::scan::kBackspace);
        }
    };

    // CHOOSE.
    f.letter(input::scan::kA, "a");
    f.r.key(input::scan::kReturn);
    CHECK(tap.ids == std::vector<std::string>{files::kActionPickBuildable, files::kActionChoose});
    REQUIRE(any_row(f.shown(), "recipe name> oven"));

    // VALIDATE: a required field left blank is refused where it stands, and nothing is sent.
    clear_line();
    f.r.key(input::scan::kReturn);
    CHECK(f.first().rfind("recipe name is required -- nothing was written", 0) == 0);
    CHECK(any_row(f.shown(), "recipe name> "));
    CHECK(tap.authored.empty());

    // EDIT AND PROGRESS: the next field suggests the name, and each Return commits one field.
    f.r.text("oven");
    f.r.key(input::scan::kReturn);
    CHECK(any_row(f.shown(), "artifact stem> oven"));
    for (const char* typed : {"zengine-oven", "loom", "loom::kernel"}) {
        clear_line();
        f.r.text(typed);
        f.r.key(input::scan::kReturn);
    }

    // SUBMIT, EXACTLY ONCE, AND THE HOST WROTE IT.
    CHECK(tap.authored == std::vector<std::string>{"oven"});
    REQUIRE(f.recipes.all().size() == 2);
    CHECK(f.recipes.all()[1].id == "oven");
    CHECK(slurp((f.root / "recipes.json").generic_string()).find("zengine-oven") !=
          std::string::npos);
    CHECK(f.first().find("authored recipe `oven`") != std::string::npos);

    // CANCEL: a second pick, a candidate chosen, and Escape abandons the line whole.
    f.letter(input::scan::kA, "a");
    f.r.key(input::scan::kReturn);
    REQUIRE(any_row(f.shown(), "recipe name> oven"));
    f.r.key(input::scan::kEscape);
    CHECK(f.first().rfind("no recipe was written", 0) == 0);
    CHECK(tap.authored.size() == 1);
    CHECK(f.recipes.all().size() == 2);

    // ...AND RETURN WHILE BROWSING IS THE BROWSER'S OPEN: the file under the cursor, in the Editor.
    REQUIRE(f.at_cursor().rfind("oven.cpp", 0) == 0);
    REQUIRE_FALSE(f.r.session().panels.has(f.editor_kind()));
    CHECK(tap.attempts == 0);
    f.r.key(input::scan::kReturn);
    CHECK(tap.ids.back() == files::kActionOpen);
    REQUIRE(tap.requested.size() == 1);
    CHECK(tap.requested[0] == (f.root / "oven.cpp").generic_string());
    const std::int64_t editor = f.editor_kind();
    REQUIRE(f.r.session().panels.has(editor));
    CHECK(f.editor_status().find("oven.cpp") != std::string::npos);
}

// ============================================================================
// FILES-WEAVE — what a maker's saved desk means now
// ============================================================================

TEST_CASE("FILES-WEAVE: a setup naming the retired reference opens the loaded pane") {
    // ⭐ THE CONVERSION, END TO END AND THROUGH THE REAL IMAGE. A maker's saved desk names
    // `zengine.workshop/project-files`, because that is who used to offer it; the reference
    // is rewritten at load, and the row it produces resolves to the pane that arrived from
    // the plan. The whole migration, from the maker's file to the pane on the screen.
    FilesRig f("files-setup");
    put_file(f.root / "alpha.cpp", "int a;\n");
    f.r.mount_workshop();
    f.mount_project_door();
    f.mount_recipes_door();
    load::LoadPlan plan;
    load::ArtifactIntent tool;
    tool.stem = files::kFilesStem;
    tool.weave = load::WeaveIntent{files::kFilesRole};
    plan.artifacts.push_back(tool);
    const load::Executed done = f.r.run_plan(plan);
    REQUIRE_MESSAGE(done.ok, done.refusal);

    Setup saved;
    saved.name = "Yesterday";
    REQUIRE(add_pane(saved, PaneRef{"zengine.workshop", "project-files"}));
    const std::string path = f.dir.file("desk.json");
    REQUIRE(setup_persist::save_file(path, saved).accepted);
    f.r.host.setup_path = path;

    f.r.ready();
    f.r.extent(160, 48);
    f.r.key(input::scan::kR); // restore the setup this run was pointed at

    // THE DESK CAME BACK NAMING THE OFFICE THAT HOLDS THE PANE NOW...
    CHECK(pane_row(f.r.session().setup.active, files_ref()) != kNoPaneRow);
    CHECK(pane_row(f.r.session().setup.active, PaneRef{"zengine.workshop", "project-files"}) ==
          kNoPaneRow);
    // ...AND THE MAKER WAS TOLD ONCE THAT IT MOVED.
    CHECK(f.r.session().notice.find("zengine.files/project-files") != std::string::npos);
    // ...AND IT IS ON THE SCREEN, listing the place this run began.
    REQUIRE(f.row() != nullptr);
    CHECK(any_row(pane_rows(f.r, f.row()->kind), "alpha.cpp"));
}

TEST_CASE("FILES-WEAVE: an open the desk cannot show opens nothing, and Files says why") {
    // ⭐ THE TRANSACTION'S FAILURE ATOMICITY, THROUGH THE REAL REQUESTER (VD-27). Files asks
    // the Editor's door; the Editor judges the file and asks the desk for a place; the desk
    // has none, so nothing is installed, nothing is authored, and the refusal travels back to
    // Files as the answer to its own request -- which Files says in its own first row.
    FilesRig f("files-noroom");
    put_file(f.root / "alpha.cpp", "the project\n");
    f.open(160, kMinScreen.h, /*with_editor=*/true);
    // THE ONE STACK SLOT THIS SCREEN HAS IS FILES' OWN.
    REQUIRE(f.r.session().panels.has(f.kind));
    REQUIRE_FALSE(f.r.session().panels.has(f.editor_kind()));

    f.point_at("alpha.cpp");
    f.r.key(input::scan::kReturn);
    CHECK_FALSE(f.r.session().panels.has(f.editor_kind()));
    CHECK_FALSE(has_pane(f.r.session().setup.active, PaneRef{"zengine.editor", "editor"}));
    CHECK(f.r.session().panels.keyboard == f.kind); // the keys never left Files
    const std::vector<std::string> rows = pane_rows(f.r, f.kind);
    REQUIRE_FALSE(rows.empty());
    CHECK(rows[0].find("no room for Editor") != std::string::npos);
    CHECK(f.r.session().notice.find("no room for Editor") != std::string::npos);
}

// ============================================================================
// FILES-WEAVE -- the open's refusal at dispatch, consumed by exact attempt (WL-OPEN-07)
// ============================================================================

TEST_CASE("FILES-WEAVE: an open refused at dispatch is said by that exact attempt, and a fresh attempt takes once an opening office is present") {
    // NO OPENING OFFICE IS HELD: the pane's ask is queued, and Loom refuses it before any
    // handler ran -- `NoSuchTarget`, said back to the exact attempt as `zen.DispatchRefused`.
    // The pane names the request that failed in its own row, clears only that ask, moves
    // nothing on the desk and keeps the keys.
    FilesRig f("files-open-refused");
    put_file(f.root / "alpha.cpp", "the project\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/false);
    f.point_at("alpha.cpp");
    f.r.key(input::scan::kReturn);
    CHECK_FALSE(f.r.session().panels.has(f.editor_kind()));
    CHECK(f.r.session().panels.keyboard == f.kind);
    CHECK_MESSAGE(f.first().find("alpha.cpp") != std::string::npos, f.first());
    CHECK_MESSAGE(f.first().find("could not reach") != std::string::npos, f.first());
    CHECK_MESSAGE(f.first().find("NoSuchTarget") != std::string::npos, f.first());
    const std::string refused = f.first();
    // A LATE COPY OF THE NOTICE -- the shape, from a stranger, after the ask has settled --
    // is ordinary speech about nothing this pane is waiting on: the row stands.
    const loom::WeaveId files_id = f.r.kernel.weave_id(files::kFilesStem);
    REQUIRE(files_id.value != 0);
    f.mount_stranger();
    f.forge_refusal(files_id, 1);
    f.r.bus.drain_until_idle();
    CHECK(f.first() == refused);
    // ...AND A FRESH ATTEMPT TAKES once the office is held: the open reaches the manager, the
    // Editor is seated with the file, and the keys move to it.
    f.r.mount_opening();
    f.point_at("alpha.cpp");
    f.r.key(input::scan::kReturn);
    const std::int64_t editor = f.editor_kind();
    REQUIRE(f.r.session().panels.has(editor));
    CHECK(f.r.session().panels.keyboard == editor);
    CHECK(f.editor_status().find("alpha.cpp") != std::string::npos);
}

TEST_CASE("a refused open's row leaves Files' published rows at the next Return on it, while that open is still on its way, and stays gone once the source opens") {
    // THE SAME RETURN, ON THE SAME ROW, WITH NOTHING BETWEEN: no cursor step spends the refusal
    // first, so the only act that can is the open itself -- and an open says nothing of its own
    // until its answer, several turns later. The pane's rows are said at the act, without the
    // spent notice; what is checked is the row Workshop admitted, turn by turn.
    FilesRig f("files-notice-spent");
    put_file(f.root / "alpha.cpp", "the project\n");
    f.open(160, 48, /*with_editor=*/true, /*with_manager=*/false);
    f.point_at("alpha.cpp");
    f.r.key(input::scan::kReturn);
    // (Each row is read into a local before it is a message: `first()` asserts, and an assertion
    // evaluated inside doctest's own report of another deadlocks its reporter under `-s`.)
    const std::string refused = f.first();
    REQUIRE_MESSAGE(refused.find("could not reach") != std::string::npos, refused);
    f.r.mount_opening();
    const loom::WeaveId files_id = f.r.kernel.weave_id(files::kFilesStem);
    REQUIRE(files_id.value != 0);
    loom::Switchboard& bus = f.r.bus;
    bool heard = false;
    const loom::ObserverId tap = bus.add_observer([&heard, &bus, files_id](const loom::BusEvent& ev) {
        if (!heard && ev.kind == loom::EventKind::Delivered && ev.target == files_id &&
            ev.schema_name == PaneActionRequested::zen_name) {
            heard = true; // the turn ends where the pane has acted: its open is queued
            bus.stop();
        }
    });
    f.enqueue_key(input::scan::kReturn);
    for (int turns = 0; turns < 8 && !heard; ++turns) {
        (void)bus.pump_pending();
    }
    bus.remove_observer(tap);
    REQUIRE(heard);
    // TURN BY TURN, until the picture Workshop paints for the pane stops saying the refusal --
    // its content is admitted a turn after the act and painted a turn after that -- which must
    // happen while that open is still on its way, with nothing seated.
    std::string pending = f.first();
    for (int turns = 0; turns < 16 && pending.find("could not reach") != std::string::npos &&
                        !f.r.session().panels.has(f.editor_kind());
         ++turns) {
        (void)bus.pump_pending();
        pending = f.first();
    }
    CHECK_FALSE(f.r.session().panels.has(f.editor_kind()));
    CHECK(f.r.opening->state().op != 0);
    CHECK_MESSAGE(pending.find("could not reach") == std::string::npos, pending);
    // THE OPEN COMPLETES, and the row stays clean.
    bus.drain_until_idle();
    REQUIRE(f.r.session().panels.has(f.editor_kind()));
    CHECK(f.editor_status().find("alpha.cpp") != std::string::npos);
    const std::string opened = f.first();
    CHECK_MESSAGE(opened.find("could not reach") == std::string::npos, opened);
}

TEST_CASE("FILES-WEAVE: a forged refusal naming the pane's own live attempt settles nothing, and the open completes") {
    // THE PROVENANCE IS THE FACT, THE SHAPE IS SPEECH. While the pane's ask is genuinely
    // outstanding at the manager, a stranger says `zen.DispatchRefused` with the RIGHT attempt
    // number -- read off the bus's own tap when the ask was delivered -- and the pane must not
    // settle its request on it: the open goes on to complete, and the row never says refused.
    FilesRig f("files-open-forged");
    put_file(f.root / "alpha.cpp", "the project\n");
    f.open(160, 48, /*with_editor=*/true);
    const loom::WeaveId files_id = f.r.kernel.weave_id(files::kFilesStem);
    REQUIRE(files_id.value != 0);
    f.mount_stranger();
    std::uint64_t attempt = 0;
    const loom::ObserverId tap =
        f.r.bus.add_observer([&attempt, files_id](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Delivered && ev.sender == files_id &&
                ev.schema_name == OpenSourceRequested::zen_name) {
                attempt = ev.seq;
            }
        });
    f.point_at("alpha.cpp");
    f.enqueue_key(input::scan::kReturn);
    // TURN BY TURN, until the ask has been delivered to the manager: the pane is awaiting.
    int turns = 0;
    while (attempt == 0) {
        REQUIRE(++turns < 8);
        REQUIRE(f.r.bus.pump_pending() > 0);
    }
    f.r.bus.remove_observer(tap);
    CHECK(f.r.opening->state().op != 0); // the manager holds the flight
    // THE FORGERY, delivered while the ask is outstanding.
    f.forge_refusal(files_id, attempt);
    (void)f.r.bus.pump_pending();
    (void)f.r.bus.pump_pending();
    CHECK(f.first().find("could not reach") == std::string::npos);
    // ...AND THE OPEN COMPLETES: the real answer settles the ask, the Editor is seated.
    f.r.bus.drain_until_idle();
    const std::int64_t editor = f.editor_kind();
    REQUIRE(f.r.session().panels.has(editor));
    CHECK(f.r.session().panels.keyboard == editor);
    CHECK(f.editor_status().find("alpha.cpp") != std::string::npos);
    CHECK(f.first().find("could not reach") == std::string::npos);
}
