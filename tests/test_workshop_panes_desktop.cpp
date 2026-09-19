// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE SHIPPED DESKTOP BY MOUSE, AND THE KEYMAP EDITED FROM ITS HOTKEYS PANE -- driven through
// the real Workshop weave and the real desktop IMAGE, never a stand-in (the laws WL-DESK-13 and
// 14, WL-KEY-17 and 18). The Pane Manager's mark, name, second press, wheel, menu and toggle;
// the Hotkeys table's columns, cursor, wheel, menu, capture, spelling, and every edit; the edit
// door's collision refusal, its persistence contract, and the picture a press names.

#include "doctest.h"

#include "workshop_support.hpp"

#include "desktop-pane/vocabulary.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace dp = zengine::desktop_pane;

loom::WeaveId load_real_desktop(PaneRig& r) {
    const loom::WeaveId id = r.load(dp::kDesktopStem, WORKSHOP_SO_DESKTOP_PANE, kDesktopRole);
    REQUIRE(id.valid());
    REQUIRE(r.load_refusals.empty());
    return id;
}

std::int64_t kind_of(PaneRig& r, const char* office, const char* pane) {
    const RuntimePane* row = r.session().panels.runtime.find(office, pane);
    return row == nullptr ? kNoPaneKind : row->kind;
}

/// THE ROWS A PANE IS SHOWING, as Workshop admitted them.
std::vector<std::string> shown_rows(PaneRig& r, std::int64_t kind) {
    const ExternalPane* shown = r.session().panels.external_pane(kind);
    REQUIRE(shown != nullptr);
    CAPTURE(shown->refusal_why);
    CHECK(shown->refusal.empty());
    std::vector<std::string> out;
    for (const surface::SurfaceTextRow& line : shown->shown) {
        out.push_back(line.text);
    }
    return out;
}

std::string joined(const std::vector<std::string>& rows) {
    std::string text;
    for (const std::string& row : rows) {
        text += row + "\n";
    }
    return text;
}

/// THE ROW INDEX (in the pane's body) OF THE FIRST SHOWN ROW CONTAINING `needle`, or -1.
std::int64_t row_containing(const std::vector<std::string>& rows, const std::string& needle) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].find(needle) != std::string::npos) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

std::string marked(const std::vector<std::string>& rows) {
    for (const std::string& row : rows) {
        if (row.rfind("> ", 0) == 0 || row.rfind("? ", 0) == 0) {
            return row;
        }
    }
    return std::string();
}

void button_cell(PaneRig& r, std::int64_t button, bool pressed, std::int64_t cx, std::int64_t cy) {
    r.publish(loom::to_value(input::PointerButton{button, pressed, cx, cy + surface::kTuiCanvasTopRow,
                                                  input::space::kCells, input::mod::kNone}));
}

void queue_key(PaneRig& r, std::int64_t scancode, std::int64_t modifiers) {
    (void)r.bus.publish(loom::Message(loom::to_value(input::KeyPressed{scancode, std::string(), modifiers}),
                                      loom::WeaveId{}, loom::WeaveId{}, 0));
}

void queue_button(PaneRig& r, std::int64_t button, bool pressed, std::int64_t cx, std::int64_t cy) {
    (void)r.bus.publish(loom::Message(
        loom::to_value(input::PointerButton{button, pressed, cx, cy + surface::kTuiCanvasTopRow,
                                            input::space::kCells, input::mod::kNone}),
        loom::WeaveId{}, loom::WeaveId{}, 0));
}

ui::Rect body_of(PaneRig& r, std::int64_t kind) { return external_body_rect(r.session(), kind); }
std::int64_t body_x(PaneRig& r, std::int64_t kind, std::int64_t col) { return body_of(r, kind).x + col; }
std::int64_t body_y(PaneRig& r, std::int64_t kind, std::int64_t row) {
    return body_of(r, kind).y + kExternalHeaderRows + row;
}

/// A RIG WITH THE REAL DESKTOP, three tool panes offered by a native seat, and the Pane Manager
/// open and holding the keys.
struct Desk {
    PaneRig r;
    ProviderSeat* tools = nullptr;
    loom::WeaveId desktop{};
    loom::WeaveId presenter{};
    std::int64_t launcher = kNoPaneKind;

    explicit Desk(const std::string& keymap_path = std::string(), std::int64_t width = 160,
                  std::int64_t height = 60, const char* presenter_image = WORKSHOP_SO_MENU_PRESENTER) {
        if (!keymap_path.empty()) {
            r.host.keymap_path = keymap_path;
        }
        r.mount_workshop();
        r.ready();
        r.extent(width, height);
        tools = r.mount_provider("zengine.test.tools");
        r.drive(tools, [](ProviderSeat& s, loom::Mail& m) {
            s.offer(m, PaneOffered{"alpha", "Alpha", "a fixture"});
            s.offer(m, PaneOffered{"beta", "Beta", "a fixture"});
            s.offer(m, PaneOffered{"gamma", "Gamma", "a fixture"});
        });
        desktop = load_real_desktop(r);
        presenter = r.load_presenter(presenter_image); // the shipped one unless a case replaces it
        r.key(input::scan::kP, input::mod::kCtrl);
        launcher = kind_of(r, kDesktopRole, dp::kLauncherPane);
        REQUIRE(is_runtime_kind(launcher));
        REQUIRE(r.session().panels.keyboard == launcher);
    }

    std::vector<std::string> rows() { return shown_rows(r, launcher); }
    std::int64_t row_of(const std::string& name) { return row_containing(rows(), name); }
    /// THE ROW SHOWING `name`, walked into the window with the marker when it is out of it.
    std::int64_t walk_to(const std::string& name) {
        for (int i = 0; i < 40; ++i) {
            if (row_of(name) >= 0) {
                return row_of(name);
            }
            r.key(input::scan::kUp);
        }
        for (int i = 0; i < 40; ++i) {
            if (row_of(name) >= 0) {
                return row_of(name);
            }
            r.key(input::scan::kDown);
        }
        return -1;
    }
    bool open(const char* pane) {
        return has_pane(r.session().setup.active, PaneRef{"zengine.test.tools", pane});
    }
    void press(std::int64_t row, std::int64_t col) {
        r.press_cell(body_x(r, launcher, col), body_y(r, launcher, row));
    }
    void right(std::int64_t row, std::int64_t col) {
        button_cell(r, 3, true, body_x(r, launcher, col), body_y(r, launcher, row));
        button_cell(r, 3, false, body_x(r, launcher, col), body_y(r, launcher, row));
    }
};

constexpr std::int64_t kMarkCol = 3;  // inside `[    ]`
constexpr std::int64_t kNameCol = 11; // on the name, past the mark

} // namespace

// =============================================================================
// The Pane Manager by mouse (WL-DESK-14)
// =============================================================================

TEST_CASE("WL-DESK-14: a press on a row's mark shows or hides that pane; a press on its name only moves the marker") {
    Desk d;
    const std::int64_t alpha = d.row_of("Alpha");
    REQUIRE(alpha >= 0);
    REQUIRE_FALSE(d.open("alpha"));
    const std::string marked_before = marked(d.rows());
    REQUIRE(marked_before.find("Alpha") == std::string::npos);
    d.press(alpha, kMarkCol);
    CHECK(d.open("alpha"));
    CHECK(d.rows()[static_cast<std::size_t>(alpha)].find("[open]") != std::string::npos);
    // THE MARKER DID NOT MOVE FOR A MARK PRESS.
    CHECK(marked(d.rows()) == marked_before);
    d.press(alpha, kMarkCol);
    CHECK_FALSE(d.open("alpha"));
    // A NAME PRESS CHOOSES, AND OPENS NOTHING.
    const std::int64_t beta = d.row_of("Beta");
    d.press(beta, kNameCol);
    CHECK(marked(d.rows()).find("Beta") != std::string::npos);
    CHECK_FALSE(d.open("beta"));
}

TEST_CASE("WL-DESK-14: a deliberate second press on the marked name, with the keys already here, opens it -- or focuses and lifts it when it is open and covered") {
    Desk d;
    const std::int64_t beta = d.row_of("Beta");
    d.press(beta, kNameCol); // chooses
    CHECK_FALSE(d.open("beta"));
    d.press(beta, kNameCol); // the same row, the keys already here: Return's meaning
    CHECK(d.open("beta"));
    const std::int64_t beta_kind = kind_of(d.r, "zengine.test.tools", "beta");
    CHECK(d.r.session().panels.keyboard == beta_kind);
    // BACK IN THE MANAGER, a second press on Beta's marked name focuses it again.
    d.r.key(input::scan::kP, input::mod::kCtrl); // hides the manager (strict toggle)
    REQUIRE_FALSE(has_pane(d.r.session().setup.active, PaneRef{kDesktopRole, dp::kLauncherPane}));
    d.r.key(input::scan::kP, input::mod::kCtrl); // shows it, with the keys
    REQUIRE(has_pane(d.r.session().setup.active, PaneRef{kDesktopRole, dp::kLauncherPane}));
    REQUIRE(d.r.session().panels.keyboard == d.launcher);
    // THE MARKER STILL HOLDS BETA (a choice is kept by identity), so a press on its name now is
    // the deliberate second press: Beta, open and covered, is focused and lifted at once.
    REQUIRE(marked(d.rows()).find("Beta") != std::string::npos);
    const std::int64_t beta_again = d.row_of("Beta");
    REQUIRE(beta_again >= 0);
    d.press(beta_again, kNameCol);
    CHECK(d.r.session().panels.keyboard == beta_kind);
    // ...AND FROM ANOTHER ROW, TWO PRESSES AGAIN: the first chooses Alpha, the second on Beta
    // only moves the marker (a different row), the third on Beta opens it.
    d.r.key(input::scan::kP, input::mod::kCtrl);
    d.r.key(input::scan::kP, input::mod::kCtrl);
    REQUIRE(d.r.session().panels.keyboard == d.launcher);
    d.press(d.row_of("Alpha"), kNameCol);
    CHECK(marked(d.rows()).find("Alpha") != std::string::npos);
    CHECK(d.r.session().panels.keyboard == d.launcher);
    d.press(d.row_of("Beta"), kNameCol);
    CHECK(marked(d.rows()).find("Beta") != std::string::npos);
    CHECK(d.r.session().panels.keyboard == d.launcher);
    d.press(d.row_of("Beta"), kNameCol);
    CHECK(d.r.session().panels.keyboard == beta_kind);
}

TEST_CASE("WL-DESK-14: the wheel walks the marker one row per notch, and a press names the picture it was aimed at -- a press queued behind a change of the list is refused, never resolved against the moved rows") {
    Desk d;
    const std::string first = marked(d.rows());
    d.r.wheel_cell(-1.0, body_x(d.r, d.launcher, kNameCol), body_y(d.r, d.launcher, 1));
    CHECK(marked(d.rows()) != first);
    d.r.wheel_cell(1.0, body_x(d.r, d.launcher, kNameCol), body_y(d.r, d.launcher, 1));
    CHECK(marked(d.rows()) == first);

    // THE QUEUED PRESS. Six cursor steps and a press on Gamma's row are queued together, as a
    // hand that scrolls and clicks produces them. The host forwards each step to the desktop and
    // handles the press BEFORE the desktop has composed any of the pictures the steps make, so
    // the press carries the number of the picture the maker was looking at; by the time the
    // desktop reads it the window has scrolled and another pane sits on that row. The picture
    // number does not match, and the press is refused in words -- never resolved against the
    // pane that moved into its place.
    const std::int64_t gamma = d.row_of("Gamma");
    REQUIRE(gamma >= 0);
    for (int i = 0; i < 6; ++i) {
        queue_key(d.r, input::scan::kDown, input::mod::kNone);
    }
    queue_button(d.r, 1, true, body_x(d.r, d.launcher, kMarkCol), body_y(d.r, d.launcher, gamma));
    d.r.bus.drain_until_idle();
    const std::string text = joined(d.rows());
    CAPTURE(text);
    CHECK(text.find("more above") != std::string::npos); // the window did scroll
    for (const char* pane : {"alpha", "beta", "gamma"}) {
        CAPTURE(pane);
        CHECK_FALSE(d.open(pane));
    }
    CHECK_FALSE(has_pane(d.r.session().setup.active, PaneRef{kDesktopRole, dp::kHotkeysPane}));
    CHECK(text.find("the list moved -- press again") != std::string::npos);
    // ...AND A PRESS AGAINST THE PICTURE NOW SHOWN ACTS: the repaint that said the notice moved
    // no row the maker could press, so it kept the picture's number.
    const std::int64_t gamma_now = d.walk_to("Gamma");
    REQUIRE(gamma_now >= 0);
    d.press(gamma_now, kMarkCol);
    CHECK(d.open("gamma"));
}

TEST_CASE("WL-DESK-13: Ctrl+P is a strict visibility toggle judged by the host -- open becomes closed, closed becomes open and focused, whatever the desktop last heard") {
    Desk d;
    const PaneRef manager{kDesktopRole, dp::kLauncherPane};
    REQUIRE(has_pane(d.r.session().setup.active, manager));
    // COVERED AND UNFOCUSED IS STILL OPEN: the toggle closes it.
    const std::int64_t alpha = d.row_of("Alpha");
    d.press(alpha, kMarkCol);
    const std::int64_t alpha_kind = kind_of(d.r, "zengine.test.tools", "alpha");
    d.r.press_cell(body_x(d.r, alpha_kind, 1), body_y(d.r, alpha_kind, 0));
    REQUIRE(d.r.session().panels.keyboard == alpha_kind);
    d.r.key(input::scan::kP, input::mod::kCtrl);
    CHECK_FALSE(has_pane(d.r.session().setup.active, manager));
    d.r.key(input::scan::kP, input::mod::kCtrl);
    CHECK(has_pane(d.r.session().setup.active, manager));
    CHECK(d.r.session().panels.keyboard == kind_of(d.r, kDesktopRole, dp::kLauncherPane));
    // TWO TOGGLES QUEUED BEFORE ANY INVENTORY REACHES THE DESKTOP: the host judges each against
    // the desk as it is, so the pair ends where it began -- never both the same way.
    queue_key(d.r, input::scan::kP, input::mod::kCtrl);
    queue_key(d.r, input::scan::kP, input::mod::kCtrl);
    d.r.bus.drain_until_idle();
    CHECK(has_pane(d.r.session().setup.active, manager));
}

TEST_CASE("WL-DESK-14: a right press on a row offers its menu -- open, manage and inspect -- and `manage...` opens the host's own pane menu on THAT pane; the menu key offers the marked row's") {
    Desk d;
    const std::int64_t beta = d.row_of("Beta");
    d.right(beta, kNameCol);
    REQUIRE(menu_shown(d.r.session()));
    const std::vector<std::string> painted = context_rows_on(d.r.last_canvas(), d.r.session());
    REQUIRE(painted.size() == 3);
    CHECK(painted[0] == "> open Beta");
    CHECK(painted[1] == "  manage...");
    CHECK(painted[2] == "  inspect in Info");
    // `manage...`: the host's menu, on Beta -- a pane that is not even on the desk.
    d.r.key(input::scan::kDown);
    d.r.key(input::scan::kReturn);
    REQUIRE(d.r.session().context.open);
    CHECK_FALSE(d.r.session().presented.open);
    CHECK(d.r.session().context.subject == context_subject::kPane);
    CHECK(d.r.session().context.pane == PaneRef{"zengine.test.tools", "beta"});
    d.r.key(input::scan::kEscape);
    // `open Beta` BY MOUSE ON THE MENU ROW.
    d.right(beta, kNameCol);
    REQUIRE(menu_shown(d.r.session()));
    d.r.press_cell(context_cell_x(d.r.session()), context_entry_cell_y(d.r.session(), 0));
    CHECK(d.open("beta"));
    // THE MENU KEY, on the marked row, from the keyboard: the same rows, now with close.
    d.r.press_cell(body_x(d.r, d.launcher, kNameCol), body_y(d.r, d.launcher, d.row_of("Beta")));
    REQUIRE(d.r.session().panels.keyboard == d.launcher);
    d.r.key(input::scan::kM);
    REQUIRE(menu_shown(d.r.session()));
    const std::vector<std::string> again = context_rows_on(d.r.last_canvas(), d.r.session());
    REQUIRE(again.size() == 4);
    CHECK(again[0] == "> focus Beta");
    CHECK(again[1] == "  close Beta");
    d.r.key(input::scan::kDown);
    d.r.key(input::scan::kReturn);
    CHECK_FALSE(d.open("beta"));
    // A RIGHT PRESS ON THE HEADING IS HANDED BACK: the host's menu on the Manager itself.
    d.right(0, 2);
    REQUIRE(d.r.session().context.open);
    CHECK_FALSE(d.r.session().presented.open);
    CHECK(d.r.session().context.pane == PaneRef{kDesktopRole, dp::kLauncherPane});
}

TEST_CASE("WL-DESK-14: the Pane Manager renders within every budget the screen grants -- down to a heading alone -- and never says an omission marker it did not seat") {
    Desk d;
    for (const std::int64_t height : {8, 10, 12, 16}) {
        d.r.extent(160, height);
        const ExternalPane* pane = d.r.session().panels.external_pane(d.launcher);
        REQUIRE(pane != nullptr);
        const std::vector<std::string> shown = d.rows();
        CAPTURE(height);
        CAPTURE(pane->rows);
        CAPTURE(joined(shown));
        CHECK(static_cast<std::int64_t>(shown.size()) <= pane->rows);
        if (!shown.empty()) {
            CHECK(shown[0].rfind("PANES", 0) == 0);
        }
        // A MARKER ROW IS SAID ONLY WHEN A ROW WAS RESERVED FOR IT: a room too small for a list
        // row beside a marker shows the list row and counts nothing it cannot say.
        std::size_t markers = 0;
        std::size_t list_rows = 0;
        for (const std::string& row : shown) {
            markers += row.find(" more ") != std::string::npos ? 1u : 0u;
            list_rows += row.find("[") != std::string::npos ? 1u : 0u;
        }
        CHECK((markers == 0 || list_rows >= 1));
    }
    d.r.extent(160, 60);
    CHECK(d.rows().size() > 4);
}

// =============================================================================
// The Hotkeys pane: columns, cursor, wheel, menu, and every edit (WL-KEY-17)
// =============================================================================

namespace {

/// A RIG WITH THE REAL DESKTOP, a keymap file, and the Hotkeys pane open, tall, with the keys.
struct Keys {
    TempDir dir;
    std::string path;
    PaneRig r;
    loom::WeaveId presenter{};
    std::int64_t hotkeys = kNoPaneKind;

    explicit Keys(const char* name, const std::string& file_text = std::string(),
                  bool isolated = false, const char* presenter_image = WORKSHOP_SO_MENU_PRESENTER)
        : dir(name), path(dir.file("keymap.json")) {
        if (!file_text.empty()) {
            write_keymap_file(path, file_text);
        }
        if (!isolated) {
            r.host.keymap_path = path;
        }
        r.mount_workshop();
        r.ready();
        r.extent(200, 60);
        load_real_desktop(r);
        presenter = r.load_presenter(presenter_image);
        r.key(input::scan::kK, input::mod::kCtrl);
        const Written tall = author_pane_size(r.session().setup.active,
                                              PaneRef{kDesktopRole, dp::kHotkeysPane},
                                              PaneSize{pane_unit::kSubcells, subs(120)},
                                              PaneSize{pane_unit::kSubcells, subs(40)});
        REQUIRE_MESSAGE(tall.accepted, tall.refusal);
        r.extent(200, 60);
        hotkeys = kind_of(r, kDesktopRole, dp::kHotkeysPane);
        REQUIRE(is_runtime_kind(hotkeys));
        REQUIRE(r.session().panels.keyboard == hotkeys);
    }

    std::vector<std::string> rows() { return shown_rows(r, hotkeys); }
    std::string text() { return joined(rows()); }
    /// THE ROW SHOWING `id`, walked into the window with the cursor when it is below it.
    std::int64_t row_of(const std::string& id) {
        r.key(input::scan::kHome);
        for (int step = 0; step < 200; ++step) {
            const std::int64_t at = row_containing(rows(), id);
            if (at >= 0) {
                return at;
            }
            r.key(input::scan::kDown);
        }
        return -1;
    }
    void right(std::int64_t row, std::int64_t col = 4) {
        button_cell(r, 3, true, body_x(r, hotkeys, col), body_y(r, hotkeys, row));
        button_cell(r, 3, false, body_x(r, hotkeys, col), body_y(r, hotkeys, row));
    }
    /// CHOOSE A MENU ROW BY ITS LABEL, through the keyboard.
    void choose(const std::string& label) {
        REQUIRE(menu_shown(r.session()));
        // THE PRESENTER'S LINES, READ AS A MAKER READS THEM: the cursor starts on the first row,
        // and Down walks it to the line that reads `label`.
        const std::int64_t at = presented_line_of(r.session(), label);
        REQUIRE_MESSAGE(at >= 0, "no menu line reads ", label);
        for (std::int64_t i = 0; i < at; ++i) {
            r.key(input::scan::kDown);
        }
        r.key(input::scan::kReturn);
    }
    std::string file() {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    /// WHAT THE FILE'S ROWS SAY FOR AN ID, in order.
    std::vector<std::string> file_rows(const std::string& id) {
        const keymap_persist::LoadedKeymap loaded = keymap_persist::from_text(file());
        REQUIRE_MESSAGE(loaded.outcome.accepted, loaded.outcome.refusal);
        std::vector<std::string> out;
        for (const AuthoredOverride& o : loaded.keymap.authored) {
            if (o.action == id) {
                out.push_back(o.gesture);
            }
        }
        return out;
    }
};

} // namespace

TEST_CASE("WL-KEY-17: right-click a binding, Modify (press a key): the change is live at once, written to the file, listed in the table, and the floor teaches it; the keys stayed where they were") {
    Keys k("hotkeys-modify-press");
    const std::int64_t row = k.row_of("desktop.terminal");
    REQUIRE(row >= 0);
    CHECK(k.rows()[static_cast<std::size_t>(row)].find("ctrl+t") != std::string::npos);
    k.right(row);
    REQUIRE(menu_shown(k.r.session()));
    const std::vector<std::string> painted = context_rows_on(k.r.last_canvas(), k.r.session());
    REQUIRE(painted.size() == 7);
    CHECK(painted[0] == "> Modify (press a key)");
    CHECK(painted[4] == "  Remove `ctrl+t`");
    k.choose("Modify (press a key)");
    CHECK_FALSE(k.r.session().presented.open);
    CHECK_FALSE(k.r.session().context.open);
    CHECK(k.text().find("press the key for `desktop.terminal`") != std::string::npos);
    k.r.key(input::scan::kG, input::mod::kCtrl);
    // LIVE: the application row answers to the new key.
    const AppRow* moved = k.r.session().keymap.app_row_of_id("desktop.terminal");
    REQUIRE(moved != nullptr);
    CHECK(moved->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
    // WRITTEN: the file holds it, as version 2.
    CHECK(k.file_rows("desktop.terminal") == std::vector<std::string>{"ctrl+g"});
    CHECK(loom::compat::parse(k.file()).claimed_version() == 2);
    // LISTED: the row reads the new key, marked, and the sentence says both facts.
    const std::string text = k.text();
    CAPTURE(text);
    const std::int64_t now = k.row_of("desktop.terminal");
    REQUIRE(now >= 0);
    CHECK(k.rows()[static_cast<std::size_t>(now)].find("ctrl+g") != std::string::npos);
    CHECK(k.rows()[static_cast<std::size_t>(now)].find("*") != std::string::npos);
    CHECK(text.find("now `ctrl+g` -- written to the keymap file") != std::string::npos);
    CHECK(k.r.session().panels.keyboard == k.hotkeys);
}

TEST_CASE("WL-KEY-17: Add a key, Remove one, remove the last (disabled, aloud), Reset -- by menu and by the same door from the keyboard; two consecutive writes; the table lists every key") {
    Keys k("hotkeys-add-remove");
    std::int64_t row = k.row_of("layout.next");
    REQUIRE(row >= 0);
    // ADD BY TYPING A SPELLING.
    k.right(row);
    k.choose("Add a key (type)");
    CHECK(k.text().find("type the key for `layout.next`") != std::string::npos);
    k.r.text("ctrl+n");
    k.r.key(input::scan::kReturn);
    CHECK(k.r.session().keymap.matches(Act::kLayoutNext, input::scan::kN, input::mod::kCtrl));
    CHECK(k.r.session().keymap.matches(Act::kLayoutNext, input::scan::kPeriod, input::mod::kNone));
    CHECK(k.file_rows("layout.next") == std::vector<std::string>{".", "ctrl+n"});
    // BOTH LISTED, the default first.
    std::vector<std::string> rows = k.rows();
    std::size_t listed = 0;
    for (const std::string& r : rows) {
        listed += r.find("next layout") != std::string::npos ? 1u : 0u;
    }
    CHECK(listed == 2);
    // REMOVE THE DEFAULT FROM ITS OWN ROW: the second key stands alone (a second write, judged
    // against the baseline the first write refreshed).
    row = row_containing(rows, "layout.next");
    REQUIRE(rows[static_cast<std::size_t>(row)].find(".") != std::string::npos);
    k.right(row);
    k.choose("Remove `.`");
    CHECK_FALSE(k.r.session().keymap.matches(Act::kLayoutNext, input::scan::kPeriod, input::mod::kNone));
    CHECK(k.r.session().keymap.matches(Act::kLayoutNext, input::scan::kN, input::mod::kCtrl));
    CHECK(k.file_rows("layout.next") == std::vector<std::string>{"ctrl+n"});
    CHECK(k.text().find("written to the keymap file") != std::string::npos);
    // REMOVE THE LAST: disabled, said, never the default silently back.
    row = k.row_of("layout.next");
    k.right(row);
    k.choose("Remove `ctrl+n`");
    CHECK_FALSE(k.r.session().keymap.matches(Act::kLayoutNext, input::scan::kN, input::mod::kCtrl));
    CHECK_FALSE(k.r.session().keymap.matches(Act::kLayoutNext, input::scan::kPeriod, input::mod::kNone));
    CHECK(k.file_rows("layout.next") == std::vector<std::string>{"none"});
    CHECK(k.text().find("no key requests it now; reset restores the default") != std::string::npos);
    // RESET, FROM THE KEYBOARD: `m` on the cursor's row offers the same menu.
    row = k.row_of("layout.next");
    k.r.press_cell(body_x(k.r, k.hotkeys, 4), body_y(k.r, k.hotkeys, row));
    k.r.key(input::scan::kM);
    k.choose("Reset to default");
    CHECK(k.r.session().keymap.matches(Act::kLayoutNext, input::scan::kPeriod, input::mod::kNone));
    CHECK(k.file_rows("layout.next").empty());
    CHECK(k.text().find("reset to its default") != std::string::npos);
}

TEST_CASE("WL-KEY-17: a collision refuses the edit in the collision law's own words, and nothing changes -- not the live map, not the file, not the rows") {
    Keys k("hotkeys-collision", keymap_file_text("default", {{"layout.new", "ctrl+n"}}));
    const std::string before = k.file();
    const std::int64_t row = k.row_of("workshop.quit");
    REQUIRE(row >= 0);
    k.right(row);
    k.choose("Modify (type a spelling)");
    k.r.text("w"); // `workshop.manage`'s key, in command mode
    k.r.key(input::scan::kReturn);
    const std::string text = k.text();
    CAPTURE(text);
    CHECK(text.find("workshop.quit` unchanged") != std::string::npos);
    CHECK(text.find("workshop.manage") != std::string::npos);
    CHECK(k.r.session().keymap.matches(Act::kQuit, input::scan::kQ, input::mod::kNone));
    CHECK_FALSE(k.r.session().keymap.matches(Act::kQuit, input::scan::kW, input::mod::kNone));
    CHECK(k.file() == before);
    // ...AND A PANE'S ROW IS JUDGED TOO: a key a pane in force declares is a collision.
    k.right(k.row_of("workshop.quit"));
    k.choose("Modify (type a spelling)");
    k.r.text("ctrl+k"); // the desktop's own application row
    k.r.key(input::scan::kReturn);
    CHECK(k.text().find("desktop.hotkeys") != std::string::npos);
    CHECK(k.file() == before);
}

TEST_CASE("WL-KEY-18: persistence -- an isolated run applies live and says nothing was written; a file changed by another hand is not overwritten; a file refused at launch refuses every edit") {
    SUBCASE("isolated") {
        Keys k("hotkeys-isolated", std::string(), true);
        k.right(k.row_of("desktop.terminal"));
        k.choose("Modify (type a spelling)");
        k.r.text("ctrl+g");
        k.r.key(input::scan::kReturn);
        const AppRow* moved = k.r.session().keymap.app_row_of_id("desktop.terminal");
        REQUIRE(moved != nullptr);
        CHECK(moved->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
        CHECK(k.text().find("isolated run -- no keymap file") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(k.path));
        bool standing = false;
        for (const Condition& c : k.r.session().conditions.rows) {
            standing = standing || c.key == kKeymapUnwrittenKey;
        }
        CHECK(standing);
    }
    SUBCASE("changed by another hand") {
        Keys k("hotkeys-external", keymap_file_text("default", {{"layout.new", "ctrl+n"}}));
        write_keymap_file(k.path, keymap_file_text("default", {{"layout.new", "ctrl+b"}}));
        const std::string hand = k.file();
        k.right(k.row_of("desktop.terminal"));
        k.choose("Modify (type a spelling)");
        k.r.text("ctrl+g");
        k.r.key(input::scan::kReturn);
        const AppRow* moved = k.r.session().keymap.app_row_of_id("desktop.terminal");
        REQUIRE(moved != nullptr);
        CHECK(moved->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
        CHECK(k.text().find("changed since this Workshop read it") != std::string::npos);
        CHECK(k.file() == hand);
    }
    SUBCASE("refused at launch") {
        Keys k("hotkeys-bad-file", "{\"zen\":\"1\",\"schema\":\"WorkshopKeymap\",\"version\":\"7\",\"value\":{}}");
        CAPTURE(keymap_persist::from_text(k.file()).outcome.refusal);
        REQUIRE(k.text().find("refused") != std::string::npos);
        k.right(k.row_of("desktop.terminal"));
        k.choose("Modify (type a spelling)");
        k.r.text("ctrl+g");
        k.r.key(input::scan::kReturn);
        const AppRow* row = k.r.session().keymap.app_row_of_id("desktop.terminal");
        REQUIRE(row != nullptr);
        CHECK(row->gesture == Gesture{input::scan::kT, input::mod::kCtrl});
        CHECK(k.text().find("refused at launch") != std::string::npos);
    }
    SUBCASE("a write that fails is said, and the change stands for this run") {
        Keys k("hotkeys-write-fails");
        // THE PATH IS A DIRECTORY NOW: the sibling write cannot be renamed over it.
        std::filesystem::create_directory(k.path);
        k.right(k.row_of("desktop.terminal"));
        k.choose("Modify (type a spelling)");
        k.r.text("ctrl+g");
        k.r.key(input::scan::kReturn);
        const AppRow* moved = k.r.session().keymap.app_row_of_id("desktop.terminal");
        REQUIRE(moved != nullptr);
        CHECK(moved->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
        CHECK(k.text().find("changed since this Workshop read it") != std::string::npos);
    }
}

TEST_CASE("WL-KEY-18: a version-1 file is imported explicitly, unrelated and unknown rows are preserved as values in order, and the next write is version 2") {
    keymap_persist::v1::WorkshopKeymap old;
    old.format = keymap_persist::kFormat;
    old.format_version = 1;
    old.legend = "compact";
    old.overrides = {keymap_persist::WorkshopKeymapRow{"layout.new", "ctrl+n"},
                     keymap_persist::WorkshopKeymapRow{"future.unknown", "ctrl+u"},
                     keymap_persist::WorkshopKeymapRow{"desktop.terminal", "ctrl+t"}};
    const std::string v1 = loom::compat::serialize(loom::to_value(old));
    REQUIRE(loom::compat::parse(v1).claimed_version() == 1);
    Keys k("hotkeys-v1-import", v1);
    CHECK(k.r.session().keymap.matches(Act::kLayoutNew, input::scan::kN, input::mod::kCtrl));
    CHECK(k.r.session().keymap.legend == legend_mode::kCompact);
    CHECK(k.text().find("applied -- 3 authored rows") != std::string::npos);
    k.right(k.row_of("desktop.terminal"));
    k.choose("Add a key (type)");
    k.r.text("ctrl+g");
    k.r.key(input::scan::kReturn);
    const std::string written = k.file();
    CAPTURE(written);
    CHECK(loom::compat::parse(written).claimed_version() == 2);
    const keymap_persist::LoadedKeymap loaded = keymap_persist::from_text(written);
    REQUIRE(loaded.outcome.accepted);
    REQUIRE(loaded.keymap.authored.size() == 4);
    CHECK(loaded.keymap.authored[0].action == "layout.new");
    CHECK(loaded.keymap.authored[0].gesture == "ctrl+n");
    CHECK(loaded.keymap.authored[1].action == "future.unknown");
    CHECK(loaded.keymap.authored[1].gesture == "ctrl+u");
    CHECK(loaded.keymap.authored[2].action == "desktop.terminal");
    CHECK(loaded.keymap.authored[2].gesture == "ctrl+t");
    CHECK(loaded.keymap.authored[3].action == "desktop.terminal");
    CHECK(loaded.keymap.authored[3].gesture == "ctrl+g");
    CHECK(loaded.keymap.legend == legend_mode::kCompact);
    // ...AND A VERSION-2 FILE WITH REPEATS READ BY THE VERSION-1 PATH IS REFUSED as version 1 did.
    keymap_persist::v1::WorkshopKeymap twice = old;
    twice.overrides = {keymap_persist::WorkshopKeymapRow{"layout.new", "ctrl+n"},
                       keymap_persist::WorkshopKeymapRow{"layout.new", "ctrl+b"}};
    const keymap_persist::LoadedKeymap refused =
        keymap_persist::from_text(loom::compat::serialize(loom::to_value(twice)));
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("version 1 keymap") != std::string::npos);
}

TEST_CASE("WL-KEY-17: the table has coherent columns, a visible cursor the wheel and the keys walk, a press that chooses a row, and a heading press that is handed back; a small room keeps the notice") {
    Keys k("hotkeys-table");
    // COHERENT COLUMNS: the label column begins at the same place on two rows -- laid out over
    // the whole population, so it holds across the window's moves.
    const std::int64_t a = k.row_of("desktop.terminal");
    REQUIRE(a >= 0);
    const std::size_t label_at = k.rows()[static_cast<std::size_t>(a)].find("terminal");
    const std::int64_t b = k.row_of("desktop.panes");
    REQUIRE(b >= 0);
    CHECK(k.rows()[static_cast<std::size_t>(b)].find("panes") == label_at);
    // THE CURSOR: the first binding is marked; Down moves it; a press chooses; the wheel walks.
    k.r.key(input::scan::kHome);
    CHECK(marked(k.rows()).find("desktop.terminal") != std::string::npos);
    k.r.key(input::scan::kDown);
    CHECK(marked(k.rows()).find("desktop.panes") != std::string::npos);
    const std::int64_t terminal = k.row_of("desktop.terminal");
    k.r.press_cell(body_x(k.r, k.hotkeys, 4), body_y(k.r, k.hotkeys, terminal));
    CHECK(marked(k.rows()).find("desktop.terminal") != std::string::npos);
    k.r.wheel_cell(-1.0, body_x(k.r, k.hotkeys, 4), body_y(k.r, k.hotkeys, terminal));
    CHECK(marked(k.rows()).find("desktop.panes") != std::string::npos);
    // A HEADING IS HANDED BACK: the host's own menu on the Hotkeys pane.
    k.right(0, 2);
    REQUIRE(k.r.session().context.open);
    CHECK_FALSE(k.r.session().presented.open);
    CHECK(k.r.session().context.pane == PaneRef{kDesktopRole, dp::kHotkeysPane});
    k.r.key(input::scan::kEscape);
    // A ROW THAT IS NOT REMAPPABLE (the text box's own keys) is handed back too.
    k.r.key(input::scan::kEnd);
    const std::vector<std::string> tail = k.rows();
    const std::int64_t fixed = row_containing(tail, "(not remappable)");
    REQUIRE(fixed >= 0);
    k.right(fixed);
    REQUIRE(k.r.session().context.open);
    CHECK_FALSE(k.r.session().presented.open);
    k.r.key(input::scan::kEscape);
    // A TINY ROOM: whatever the screen grants, the heading first and nothing past the budget.
    for (const std::int64_t height : {8, 10, 12}) {
        k.r.extent(200, height);
        const ExternalPane* pane = k.r.session().panels.external_pane(k.hotkeys);
        REQUIRE(pane != nullptr);
        const std::vector<std::string> tiny = k.rows();
        CAPTURE(height);
        CAPTURE(pane->rows);
        CAPTURE(joined(tiny));
        CHECK(static_cast<std::int64_t>(tiny.size()) <= pane->rows);
        if (!tiny.empty()) {
            CHECK(tiny[0].rfind("HOTKEYS", 0) == 0);
        }
    }
}

// =============================================================================
// The corrections' reproduced defects, now green (WL-DESK-14, WL-CTX-09, WL-KEY-17)
// =============================================================================

TEST_CASE("WL-DESK-14: a same-length inventory swap changes the picture, so a press stamped with the old number opens nothing -- the meaning carries the subject, not just the slot") {
    Desk d;
    const auto row = d.row_of("Alpha");
    REQUIRE(row >= 0);
    const auto old_picture = d.r.session().panels.external_pane(d.launcher)->picture;
    PaneInventory inventory = d.r.w->inventory_reading();
    std::size_t alpha = inventory.panes.size();
    std::size_t gamma = inventory.panes.size();
    for (std::size_t i = 0; i < inventory.panes.size(); ++i) {
        if (inventory.panes[i].office == "zengine.test.tools") {
            if (inventory.panes[i].pane == "alpha") alpha = i;
            if (inventory.panes[i].pane == "gamma") gamma = i;
        }
    }
    REQUIRE(alpha < inventory.panes.size());
    REQUIRE(gamma < inventory.panes.size());
    std::swap(inventory.panes[alpha], inventory.panes[gamma]);
    const auto updated = d.r.bus.office_send_to_role_as(d.r.workshop_id, kWorkshopProvider,
        kDesktopRole, loom::Message(loom::to_value(inventory), d.r.workshop_id,
                                   d.r.workshop_id, 0));
    REQUIRE(updated.valid());
    const auto pressed = d.r.bus.office_send_to_role_as(d.r.workshop_id, kWorkshopProvider,
        kDesktopRole, loom::Message(loom::to_value(ws::v3::PanePressed{dp::kLauncherPane, row,
            kMarkCol, true, old_picture}), d.r.workshop_id, d.r.workshop_id, 0));
    REQUIRE(pressed.valid());
    d.r.bus.drain_until_idle();
    CHECK_FALSE(d.open("gamma"));
}

TEST_CASE("WL-DESK-14: content queued ahead of a raw press cannot retarget the row the hand aimed at -- the press is stamped with the picture the medium had, and refused as moved") {
    // THE PRODUCTION BOUNDARY, NOT AN IDEAL STAMP. The inventory swap reaches the real desktop,
    // which composes picture B and queues it; a RAW press is queued behind it, captured while the
    // host still admits A and the medium still shows A. Nothing here supplies a picture number.
    bool shown_first = false;
    bool handed_out = false;
    SUBCASE("the press is read before B reaches the medium: refused") {}
    SUBCASE("the press is read after B was admitted and handed out, before the medium handled it: refused") {
        handed_out = true;
    }
    SUBCASE("control -- the press is read after B was handed to the medium: it acts on B") {
        shown_first = true;
    }
    Desk d;
    const auto row = d.row_of("Alpha");
    REQUIRE(row >= 0);
    const auto old_picture = d.r.session().panels.external_pane(d.launcher)->stamp.aimed;
    REQUIRE(old_picture == d.r.session().panels.external_pane(d.launcher)->picture);
    PaneInventory inventory = d.r.w->inventory_reading();
    std::size_t alpha = inventory.panes.size();
    std::size_t gamma = inventory.panes.size();
    for (std::size_t i = 0; i < inventory.panes.size(); ++i) {
        if (inventory.panes[i].office == "zengine.test.tools") {
            if (inventory.panes[i].pane == "alpha") alpha = i;
            if (inventory.panes[i].pane == "gamma") gamma = i;
        }
    }
    REQUIRE(alpha < inventory.panes.size());
    REQUIRE(gamma < inventory.panes.size());
    std::swap(inventory.panes[alpha], inventory.panes[gamma]);
    REQUIRE(d.r.bus.office_send_to_role_as(d.r.workshop_id, kWorkshopProvider, kDesktopRole,
        loom::Message(loom::to_value(inventory), d.r.workshop_id, d.r.workshop_id, 0)).valid());
    // Deliver the inventory to the real desktop. Its newly composed B waits in the next batch.
    REQUIRE(d.r.bus.pump_pending() >= 1);
    REQUIRE(d.r.session().panels.external_pane(d.launcher)->picture == old_picture);
    REQUIRE(d.rows()[static_cast<std::size_t>(row)].find("Alpha") != std::string::npos);
    if (handed_out) {
        // THE HOST ADMITS B AND HANDS IT OUT: the canvas and the fence's first hop are queued, and
        // the medium has not handled the canvas yet when the press is read. The second hop is
        // what keeps this press on the picture the medium still held.
        REQUIRE(d.r.bus.pump_pending() >= 1);
        REQUIRE(d.r.session().panels.external_pane(d.launcher)->picture != old_picture);
        REQUIRE(d.r.session().panels.external_pane(d.launcher)->stamp.aimed == old_picture);
    }
    if (shown_first) {
        d.r.bus.drain_until_idle(); // B admitted, painted, and the fence round twice behind it
        REQUIRE(d.rows()[static_cast<std::size_t>(row)].find("Gamma") != std::string::npos);
        REQUIRE(d.r.session().panels.external_pane(d.launcher)->stamp.aimed !=
                old_picture);
    }
    queue_button(d.r, 1, true, body_x(d.r, d.launcher, kMarkCol), body_y(d.r, d.launcher, row));
    queue_button(d.r, 1, false, body_x(d.r, d.launcher, kMarkCol), body_y(d.r, d.launcher, row));
    d.r.bus.drain_until_idle();
    if (shown_first) {
        CHECK(d.open("gamma")); // the row the medium showed under the hand
        CHECK_FALSE(d.open("alpha"));
    } else {
        CHECK_FALSE(d.open("gamma")); // never the row that moved in behind the hand
        CHECK_FALSE(d.open("alpha"));
        CHECK(joined(d.rows()).find("the list moved -- press again") != std::string::npos);
    }
}

TEST_CASE("WL-KEY-17: a mouse choice and its own release take the keyboard for an UNFOCUSED Hotkeys edit -- batched or separated, capture or typed spelling; a newer act still defeats it") {
    bool typed = false;
    bool separated = false;
    bool newer = false;
    SUBCASE("capture; the choosing press and its release in one batch") {}
    SUBCASE("capture; the release on a later turn") { separated = true; }
    SUBCASE("typed spelling; one batch") { typed = true; }
    SUBCASE("typed spelling; the release on a later turn") {
        typed = true;
        separated = true;
    }
    SUBCASE("a newer key queued behind the click defeats the late keyboard grab") { newer = true; }
    Keys k("corr-mouse-choice-keyboard");
    ProviderSeat* other = k.r.mount_provider("review.other");
    k.r.drive(other, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"other", "Other", "the prior keyboard owner"});
    });
    (void)hand_launch(k.r, PaneRef{"review.other", "other"});
    const std::int64_t others = kind_of(k.r, "review.other", "other");
    REQUIRE(k.r.session().panels.keyboard == others);
    const auto row = row_containing(k.rows(), "desktop.terminal");
    REQUIRE(row >= 0);
    k.right(row);
    REQUIRE(menu_shown(k.r.session()));
    const std::size_t entry = typed ? 1 : 0;
    REQUIRE(presented_line_of(k.r.session(), typed ? "Modify (type a spelling)"
                                                    : "Modify (press a key)") ==
            static_cast<std::int64_t>(entry));
    const auto x = context_cell_x(k.r.session());
    const auto y = context_entry_cell_y(k.r.session(), entry);
    // ONE ACTUAL CLICK'S TWO TRANSITIONS; nothing else a maker did comes between them.
    queue_button(k.r, 1, true, x, y);
    if (separated) {
        k.r.bus.drain_until_idle();
    }
    queue_button(k.r, 1, false, x, y);
    if (newer) {
        queue_key(k.r, input::scan::kDown, input::mod::kNone); // a genuinely newer act
    }
    k.r.bus.drain_until_idle();
    REQUIRE_FALSE(k.r.session().presented.open);
    CHECK(k.text().find(typed ? "type the key for `desktop.terminal`"
                              : "press the key for `desktop.terminal`") != std::string::npos);
    if (newer) {
        CHECK(k.r.session().panels.keyboard == others); // the late grab was refused
        return;
    }
    REQUIRE(k.r.session().panels.keyboard == k.hotkeys);
    if (typed) {
        k.r.text("ctrl+g");
        k.r.key(input::scan::kReturn);
    } else {
        k.r.key(input::scan::kG, input::mod::kCtrl);
    }
    const AppRow* bound = k.r.session().keymap.app_row_of_id("desktop.terminal");
    REQUIRE(bound != nullptr);
    CHECK(bound->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
}

TEST_CASE("WL-CTX-09: a printable menu shortcut and the text its own key produced are one gesture, so the menu opens") {
    Desk d;
    queue_key(d.r, input::scan::kM, input::mod::kNone);
    (void)d.r.bus.publish(loom::Message(loom::to_value(input::TextEntered{"m"}),
        loom::WeaveId{}, loom::WeaveId{}, 0));
    d.r.bus.drain_until_idle();
    CHECK(menu_shown(d.r.session()));
}

TEST_CASE("WL-KEY-17: Modify on an UNFOCUSED Hotkeys pane takes the keyboard through the guarded transition and captures the key") {
    Keys k("corr-unfocused-hotkeys");
    const auto row = k.row_of("desktop.terminal");
    REQUIRE(row >= 0);
    ProviderSeat* other = k.r.mount_provider("review.other");
    k.r.drive(other, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"other", "Other", "the prior keyboard owner"});
    });
    (void)hand_launch(k.r, PaneRef{"review.other", "other"});
    REQUIRE(k.r.session().panels.keyboard != k.hotkeys);
    const auto refreshed_row = row_containing(k.rows(), "desktop.terminal");
    REQUIRE(refreshed_row >= 0);
    k.right(refreshed_row);
    REQUIRE(menu_shown(k.r.session()));
    k.choose("Modify (press a key)");
    k.r.key(input::scan::kG, input::mod::kCtrl);
    const AppRow* bound = k.r.session().keymap.app_row_of_id("desktop.terminal");
    REQUIRE(bound != nullptr);
    CHECK(bound->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
}

// =============================================================================
// The presenter, replaced and lost -- with the real desktop's own operations (WL-CTX-09)
// =============================================================================

namespace {

/// ANOTHER PANE TAKES THE KEYS, so an edit begun from a menu has to ask for them.
std::int64_t keys_elsewhere(PaneRig& r) {
    ProviderSeat* other = r.mount_provider("review.other");
    r.drive(other, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"other", "Other", "the prior keyboard owner"});
    });
    (void)hand_launch(r, PaneRef{"review.other", "other"});
    const std::int64_t kind = kind_of(r, "review.other", "other");
    REQUIRE(r.session().panels.keyboard == kind);
    return kind;
}

/// A DIGIT AS THE TERMINAL AND SDL DELIVER IT: the key and the text it made, queued together.
void queue_digit(PaneRig& r, std::int64_t scancode, const char* text) {
    queue_key(r, scancode, input::mod::kNone);
    (void)r.bus.publish(loom::Message(loom::to_value(input::TextEntered{text}), loom::WeaveId{},
                                      loom::WeaveId{}, 0));
}

/// SAY SOMETHING TO THE DESKTOP AS `role`, from the weave that holds it -- a host-root forgery of
/// office speech, so a case can put an AUTHENTICATED but wrong answer in front of the requester.
void say_as(PaneRig& r, loom::WeaveId as, const char* role, const ws::PaneMenuAnswered& a,
            std::uint64_t number) {
    REQUIRE(r.bus.office_send_to_role_as(as, role, kDesktopRole,
                                         loom::Message(loom::to_value(a), as, as, number))
                .valid());
    r.bus.drain_until_idle();
}

} // namespace

TEST_CASE("WL-CTX-10: a reloaded desktop cancels its predecessor's menu -- withdrawn when the successor offers its pane again, and a choice about it acts on nothing") {
    // THE REVIEW'S LIFECYCLE CASE, ON THE FINAL ARCHITECTURE: Alpha's menu open, the real desktop
    // image reloaded through the control door, its new activation verified, then the surviving
    // menu chosen if it survived. The policy this pane ships is CANCELLATION.
    Desk d;
    REQUIRE_FALSE(d.open("alpha"));
    d.right(d.row_of("Alpha"), kNameCol);
    REQUIRE(menu_shown(d.r.session()));
    REQUIRE(presented_texts(d.r.session())[0] == "> open Alpha");
    std::size_t activations = 0;
    const auto tap = d.r.bus.add_observer([&](const loom::BusEvent& event) {
        if (event.kind == loom::EventKind::Delivered && event.target == d.desktop &&
            event.schema_name == loom::Activated::zen_name) {
            ++activations;
        }
    });
    d.r.enqueue_reload(dp::kDesktopStem, WORKSHOP_SO_DESKTOP_PANE);
    d.r.bus.drain_until_idle();
    d.r.bus.remove_observer(tap);
    REQUIRE(d.r.load_refusals.empty());
    REQUIRE(activations == 1); // the real control door activated the reloaded incarnation
    // WITHDRAWN: the successor offered its pane again, and a menu about a pane a predecessor
    // presented is not the successor's. Nothing is left open for a hand to choose from.
    CHECK_FALSE(d.r.session().presented.open);
    if (d.r.session().presented.open) {
        d.r.key(input::scan::kReturn); // the review's own step, were the menu to have survived
    }
    CHECK_FALSE(d.open("alpha"));
}

TEST_CASE("WL-CTX-10: the desktop acts only on the presenter's answer to an ask of this image's own -- not the host's choice, not another subject, not another number, not a predecessor's, and not twice") {
    Desk d;
    d.right(d.row_of("Alpha"), kNameCol);
    REQUIRE(menu_shown(d.r.session()));
    const std::uint64_t asked = d.r.session().presented.correlation;
    const std::string alpha = d.r.session().presented.subject;
    REQUIRE(alpha == std::string("zengine.test.tools\x1f") + "alpha");
    const ws::PaneMenuAnswered open_alpha{dp::kLauncherPane, alpha, true, dp::kMenuOpen, ""};
    // PROVENANCE: the host's office cannot choose, however well the answer is shaped.
    say_as(d.r, d.r.workshop_id, kWorkshopProvider, open_alpha, asked);
    CHECK_FALSE(d.open("alpha"));
    // THE SUBJECT: an answer about a subject this ask was not about acts on nothing.
    say_as(d.r, d.presenter, kPresenterRole,
           ws::PaneMenuAnswered{dp::kLauncherPane, std::string("zengine.test.tools\x1f") + "gamma",
                                true, dp::kMenuOpen, ""},
           asked);
    CHECK_FALSE(d.open("gamma"));
    // THE LIFETIME: a number this image never asked under settles nothing.
    say_as(d.r, d.presenter, kPresenterRole, open_alpha, asked + 1000);
    CHECK_FALSE(d.open("alpha"));
    // THE GENUINE ANSWER, from the presenter's office, under this image's own number: it acts.
    say_as(d.r, d.presenter, kPresenterRole, open_alpha, asked);
    CHECK(d.open("alpha"));
    // ONCE: the same answer again finds the ask settled -- close Alpha, and a duplicate does not
    // reopen it.
    (void)hand_close(d.r, PaneRef{"zengine.test.tools", "alpha"});
    REQUIRE_FALSE(d.open("alpha"));
    say_as(d.r, d.presenter, kPresenterRole, open_alpha, asked);
    CHECK_FALSE(d.open("alpha"));
    // A PREDECESSOR'S ASK: a fresh ask, the desktop reloaded, and that ask's number answered by the
    // presenter's office -- the successor never asked it, so the choice is cancelled.
    d.r.key(input::scan::kEscape); // the real presenter's menu, still open from above, answered
    d.right(d.row_of("Beta"), kNameCol);
    REQUIRE(menu_shown(d.r.session()));
    const std::uint64_t before_reload = d.r.session().presented.correlation;
    d.r.enqueue_reload(dp::kDesktopStem, WORKSHOP_SO_DESKTOP_PANE);
    d.r.bus.drain_until_idle();
    REQUIRE(d.r.load_refusals.empty());
    say_as(d.r, d.presenter, kPresenterRole,
           ws::PaneMenuAnswered{dp::kLauncherPane, std::string("zengine.test.tools\x1f") + "beta",
                                true, dp::kMenuOpen, ""},
           before_reload);
    CHECK_FALSE(d.open("beta"));
}

TEST_CASE("WL-CTX-10: an ordinary replacement presenter holds the office -- the Pane Manager and Hotkeys keep their own operations while it presents their menus its way: numbered, a digit chooses, a release chooses") {
    bool by_mouse = false;
    bool slide_off = false;
    SUBCASE("Hotkeys, from another focused pane: a digit and the text it made, batched") {}
    SUBCASE("Hotkeys, from another focused pane: the press arms a row and its own release chooses it") {
        by_mouse = true;
    }
    SUBCASE("Hotkeys: a hand that slides off before letting go chooses nothing") {
        by_mouse = true;
        slide_off = true;
    }
    Keys k("presenter-replaced-hotkeys", std::string(), false, WORKSHOP_SO_NUMBERED_PRESENTER);
    const std::int64_t others = keys_elsewhere(k.r);
    const auto row = row_containing(k.rows(), "desktop.terminal");
    REQUIRE(row >= 0);
    k.right(row);
    REQUIRE(menu_shown(k.r.session()));
    // PRESENTED ITS WAY: numbered rows, the cursor marked -- the rows are still the desktop's.
    const std::vector<std::string> lines = presented_texts(k.r.session());
    REQUIRE(lines.size() == 7);
    CHECK(lines[0] == "> 1 Modify (press a key)");
    CHECK(lines[1] == "  2 Modify (type a spelling)");
    if (!by_mouse) {
        queue_digit(k.r, input::scan::k1, "1");
        k.r.bus.drain_until_idle();
    } else {
        const auto x = context_cell_x(k.r.session());
        queue_button(k.r, 1, true, x, context_entry_cell_y(k.r.session(), 0));
        queue_button(k.r, 1, false, x, context_entry_cell_y(k.r.session(), slide_off ? 1 : 0));
        k.r.bus.drain_until_idle();
        if (slide_off) {
            // NOTHING CHOSEN: the press armed row 1 and the release came up on row 2. The menu is
            // still open, and the keys are still where they were.
            CHECK(menu_shown(k.r.session()));
            CHECK(k.r.session().panels.keyboard == others);
            CHECK(k.text().find("press the key for `desktop.terminal`") == std::string::npos);
            return;
        }
    }
    // THE HOTKEYS PANE'S OWN OPERATION: capture, the keyboard asked for on the choice's terms --
    // the digit's text and the click's release are that act, not a newer one.
    CHECK_FALSE(k.r.session().presented.open);
    CHECK(k.text().find("press the key for `desktop.terminal`") != std::string::npos);
    REQUIRE(k.r.session().panels.keyboard == k.hotkeys);
    k.r.key(input::scan::kG, input::mod::kCtrl);
    const AppRow* bound = k.r.session().keymap.app_row_of_id("desktop.terminal");
    REQUIRE(bound != nullptr);
    CHECK(bound->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
}

TEST_CASE("WL-CTX-10: the replacement presenter presents the Pane Manager's menu too, and a digit opens the row -- through the desktop's own launch") {
    Desk d(std::string(), 160, 60, WORKSHOP_SO_NUMBERED_PRESENTER);
    d.right(d.row_of("Alpha"), kNameCol);
    REQUIRE(menu_shown(d.r.session()));
    const std::vector<std::string> lines = presented_texts(d.r.session());
    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == "> 1 open Alpha");
    CHECK(lines[2] == "  3 inspect in Info");
    // WRAPPING IS THIS PRESENTER'S: up from the first row lands on the last.
    d.r.key(input::scan::kUp);
    CHECK(presented_texts(d.r.session())[2] == "> 3 inspect in Info");
    queue_digit(d.r, input::scan::k1, "1");
    d.r.bus.drain_until_idle();
    CHECK(d.open("alpha"));
}

TEST_CASE("WL-CTX-10: the presenter reloaded in place by another image while a menu is open hands the menu over -- shown again the new way with its cursor, answered under the same number, and the Hotkeys edit completes") {
    Keys k("presenter-handoff");
    const std::int64_t others = keys_elsewhere(k.r);
    const auto row = row_containing(k.rows(), "desktop.terminal");
    REQUIRE(row >= 0);
    k.right(row);
    REQUIRE(menu_shown(k.r.session()));
    REQUIRE(presented_texts(k.r.session())[0] == "> Modify (press a key)");
    k.r.key(input::scan::kDown); // the cursor on "Modify (type a spelling)"
    const std::int64_t menu = k.r.session().presented.menu;
    const std::uint64_t asked = k.r.session().presented.correlation;
    // THE ORDINARY REPLACEMENT, LIVE: the numbered presenter's image reloaded through the control
    // door in the shipped presenter's place -- the same accepted set, the same reload state.
    k.r.enqueue_reload("zengine-menu-presenter", WORKSHOP_SO_NUMBERED_PRESENTER);
    k.r.bus.drain_until_idle();
    REQUIRE(k.r.load_refusals.empty());
    // HANDED OVER: the same menu, still open, shown the new image's way, its cursor kept.
    REQUIRE(k.r.session().presented.open);
    CHECK(k.r.session().presented.menu == menu);
    CHECK(k.r.session().presented.correlation == asked);
    const std::vector<std::string> lines = presented_texts(k.r.session());
    REQUIRE(lines.size() == 7);
    CHECK(lines[0] == "  1 Modify (press a key)");
    CHECK(lines[1] == "> 2 Modify (type a spelling)");
    CHECK(k.r.session().panels.keyboard == others); // a reload moved no keys
    // THE CHOICE, answered by the successor under the predecessor's number, is the desktop's own
    // ask -- it acts, and the edit takes the keys on the choice's terms.
    k.r.key(input::scan::kReturn);
    CHECK_FALSE(k.r.session().presented.open);
    CHECK(k.text().find("type the key for `desktop.terminal`") != std::string::npos);
    REQUIRE(k.r.session().panels.keyboard == k.hotkeys);
    k.r.text("ctrl+g");
    k.r.key(input::scan::kReturn);
    const AppRow* bound = k.r.session().keymap.app_row_of_id("desktop.terminal");
    REQUIRE(bound != nullptr);
    CHECK(bound->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
}

TEST_CASE("WL-CTX-10: a presenter that leaves ends its menu, answered by the host; with none in the office a menu is refused in words; one loaded afresh does not carry the old menu, which ends answered") {
    Keys k("presenter-lost");
    const auto row = row_containing(k.rows(), "desktop.terminal");
    REQUIRE(row >= 0);
    k.right(row);
    REQUIRE(menu_shown(k.r.session()));
    // LOST: the presenter unloaded while its menu is open. The host learns it when the next act it
    // forwards cannot be delivered, ends the menu and answers the requester itself -- unchosen.
    REQUIRE(k.r.unload("zengine-menu-presenter"));
    k.r.key(input::scan::kDown);
    CHECK_FALSE(k.r.session().presented.open);
    CHECK(k.text().find("press the key for") == std::string::npos); // nothing was chosen
    CHECK(k.r.session().notice.find("the menu closed -- the presenter") != std::string::npos);
    // NOBODY IN THE OFFICE: the next menu is refused where it would open, nothing opens, and the
    // band says why -- a right-click that opened nothing unexplained would read as a dead mouse.
    k.right(row_containing(k.rows(), "desktop.terminal"));
    CHECK_FALSE(k.r.session().presented.open);
    CHECK_FALSE(k.r.session().context.open);
    CHECK(k.r.session().notice.find("menu did not open -- no presenter holds `zengine.presenter`") !=
          std::string::npos);
    // A PRESENTER LOADED AGAIN presents menus again...
    (void)k.r.load_presenter();
    k.right(row_containing(k.rows(), "desktop.terminal"));
    REQUIRE(menu_shown(k.r.session()));
    // ...AND ONE THAT ARRIVES WITHOUT THE OPEN MENU -- unloaded and loaded afresh, so it carries no
    // state -- cannot answer it: the host ends it, in words, and a choice can no longer be made.
    REQUIRE(k.r.unload("zengine-menu-presenter"));
    (void)k.r.load_presenter();
    CHECK_FALSE(k.r.session().presented.open);
    CHECK(k.text().find("press the key for") == std::string::npos);
    CHECK(k.r.session().notice.find("the menu closed -- the presenter was replaced") !=
          std::string::npos);
}
