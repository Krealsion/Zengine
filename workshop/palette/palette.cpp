// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Command Palette: every action, pane, layout, manual, setup and app a hand can reach, found by
// name. It reads what their owners say -- the keymap and inventory, the layouts, the launcher's
// listing -- keeps no catalog of its own, and asks Workshop to run what is chosen by its key's or
// press's path, as that hand (`CommandRunRequested`); a setup or an app it starts itself, once
// Workshop allowed the hand. It draws only in its own area of the window that shows it.
// Reference: workshop/docs/zengine.palette.md; the door: WL-DESK-18, agents/workshop/desktop.md.

#include "workshop/palette/entries.hpp"

#include "workshop/command_vocabulary.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_rows_block.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/actor_scope.hpp"
#include "workshop/settings_seam_vocabulary.hpp"

#include "launcher/vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/text_box.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace {

namespace ws = zengine::workshop;
namespace gfx = zengine::graphics;
namespace input = zengine::input;
namespace pal = zengine::palette;

constexpr const char* kWorkshop = "zengine.workshop";
constexpr const char* kOffice = ws::kPaletteRole;
constexpr const char* kPane = ws::kPalettePane;

/// The palette's own rows: the list's cursor, the run and putting it down.
constexpr const char* kUp = "palette.up";
constexpr const char* kDown = "palette.down";
constexpr const char* kRun = "palette.run";
constexpr const char* kClose = "palette.close";

/// THE ENTRIES CHOSEN THIS RUN, newest last: kept across a reload in place, never written.
struct PaletteState {
    std::vector<std::string> history;
    ZEN_SHAPE(PaletteState, 1, ZEN_FIELD(history));
};

// WL-DESK-18 -- agents/workshop/desktop.md
class Palette
    : public loom::WeaveBase<
          Palette, PaletteState,
          loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneKey,
                       ws::PaneTextInput, ws::PaneActionRequested, ws::KeymapShown,
                       ws::PaneInventory, ws::CommandRunAnswered, ws::LayoutSettingsAnswered,
                       ws::PaneOperationAnswered, zengine::launcher::LaunchCatalog,
                       zengine::launcher::Launched, gfx::AreaRoom, gfx::DrawnRefused,
                       gfx::AreaButtonChanged>,
          loom::Emit<ws::v2::PaneOffered, ws::v3::PaneActions, ws::KeymapRequested,
                     ws::PaneInventoryRequested, ws::CommandRunRequested,
                     ws::LayoutSettingsRequested, ws::v2::PaneOperationRequested,
                     zengine::launcher::LaunchCatalogRequested, zengine::launcher::Launch,
                     gfx::Drawn, gfx::RoomAsked>> {
public:
    /// EVERY ACTIVATION: the pane offered and its rows declared, the two lists asked for, and the
    /// area's room asked while no window has said it.
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        offer(mail);
        ask_lists(mail);
        if (!window_.shown()) {
            (void)mail.as_role(kOffice).send_to_role(gfx::kGraphicsRole,
                                                     gfx::RoomAsked{window_.area()});
        }
    }
    void on(const ws::PaneCatalogRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(kWorkshop)) {
            offer(mail);
        }
    }

    void on(const ws::KeymapShown& keys, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop)) {
            return;
        }
        keys_ = keys;
        relist(mail);
    }
    void on(const ws::PaneInventory& inventory, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop)) {
            return;
        }
        inventory_ = inventory;
        relist(mail);
    }

    /// THE FILTER'S KEYS: what is typed, and the text box's own editing keys.
    void on(const ws::PaneTextInput& text, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || text.pane != kPane) {
            return;
        }
        std::string printable;
        for (const char c : text.text) {
            if (c >= 0x20 && c < 0x7f) {
                printable += c;
            }
        }
        if (filter_.text().size() + printable.size() > pal::kMaxFilterBytes) {
            notice_ = "the filter holds at most " + std::to_string(pal::kMaxFilterBytes) + " characters";
            show(mail);
            return;
        }
        filter_.type(printable);
        refiltered(mail);
    }
    void on(const ws::PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || key.pane != kPane) {
            return;
        }
        if (filter_.consume(key.scancode, key.modifiers, clipboard_)) {
            refiltered(mail);
        }
    }

    /// ITS ROWS, as Workshop sent them for a hand's key: the run goes out under that key's own
    /// number, which Workshop recorded as that hand's act.
    void on(const ws::PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || asked.pane != kPane) {
            return;
        }
        const std::vector<std::size_t> list = visible();
        if (asked.id == kUp || asked.id == kDown) {
            if (!list.empty()) {
                cursor_ = asked.id == kUp ? (cursor_ == 0 ? 0 : cursor_ - 1)
                                          : std::min(cursor_ + 1, list.size() - 1);
            }
            show(mail);
            return;
        }
        if (asked.id == kClose) {
            run(mail, ws::CommandRunRequested{ws::kCommandClose, "", "", ""}, std::string());
            return;
        }
        if (asked.id != kRun) {
            return;
        }
        if (list.empty() || cursor_ >= list.size()) {
            notice_ = "nothing matches the filter";
            show(mail);
            return;
        }
        const pal::Entry& e = listing_.entries[list[cursor_]];
        if (e.off) {
            notice_ = "switched off in your keymap -- `" + e.id + "` is `none`";
            show(mail);
            return;
        }
        if (e.gone) {
            notice_ = e.label + " is not here now: nothing offers it";
            show(mail);
            return;
        }
        if (e.kind == pal::kKindSetup || e.kind == pal::kKindApp) {
            start(mail, e);
            return;
        }
        run(mail, ws::CommandRunRequested{e.kind, e.office, e.pane, e.id}, e.address());
    }

    /// THE LAYOUTS ON THE SHELF, the answer to this weave's own ask.
    void on(const ws::LayoutSettingsAnswered& said, loom::Mail& mail) {
        if (!answers(mail, layouts_ask_)) {
            return;
        }
        layouts_ask_ = 0;
        others_.layouts = said.layouts;
        relist(mail);
    }
    /// WHAT THE LAUNCHER CAN START, the answer to this weave's own ask.
    void on(const zengine::launcher::LaunchCatalog& said, loom::Mail& mail) {
        if (!answers(mail, listing_ask_)) {
            return;
        }
        listing_ask_ = 0;
        others_.startable = said.entries;
        relist(mail);
    }

    /// WORKSHOP'S WORD ON A START THIS PALETTE ASKED ABOUT, for the hand that pressed its row:
    /// allowed, the launcher is asked; refused, nothing starts and the refusal is said.
    void on(const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!answers(mail, approval_ask_)) {
            return;
        }
        approval_ask_ = 0;
        if (!answer.allowed) {
            notice_ = "nothing was started -- " + answer.reason;
            in_flight_ = false;
            running_.clear();
            show(mail);
            return;
        }
        launch_ask_ = ++asks_;
        if (!mail.as_role(kOffice)
                 .send_to_role(zengine::launcher::kLauncherRole, starting_, launch_ask_)
                 .valid()) {
            launch_ask_ = 0;
            in_flight_ = false;
            running_.clear();
            notice_ = "nothing was started -- the launcher could not be asked";
        } else {
            started_ = "starting the " + starting_.kind + " `" + starting_.name + "`";
        }
        show(mail);
    }
    /// THE LAUNCHER'S ANSWER to this palette's own start: the process, or why nothing started. The
    /// palette stays open to say it, and remembers the entry when it started.
    void on(const zengine::launcher::Launched& said, loom::Mail& mail) {
        if (!answers(mail, launch_ask_)) {
            return;
        }
        launch_ask_ = 0;
        in_flight_ = false;
        if (said.ok) {
            state_.history = pal::remembered(state_.history, running_);
            started_ = "started the " + said.kind + " `" + said.name + "`, process " +
                       std::to_string(said.pid) + ": it goes on without Workshop";
        } else {
            notice_ = "nothing was started -- " + said.reason;
        }
        running_.clear();
        show(mail);
    }

    /// WORKSHOP'S WORD ON THE RUN, the answer to the ask in flight and no other: the choice
    /// remembered when it ran; its refusal said.
    void on(const ws::CommandRunAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || !in_flight_ || mail.correlation() != asked_) {
            return;
        }
        in_flight_ = false;
        if (answer.ran) {
            if (!running_.empty()) {
                state_.history = pal::remembered(state_.history, running_);
            }
            notice_.clear();
        } else {
            notice_ = answer.refusal;
        }
        running_.clear();
        show(mail);
    }

    /// THE AREA'S ROOM, as the window that shows it says it: drawn whole there. An area the window
    /// shows afresh is the palette opened, which starts afresh -- an empty filter, the cursor on
    /// the first entry -- over the lists as they stand now.
    void on(const gfx::AreaRoom& r, loom::Mail& mail) {
        if (!window_.room(r, mail)) {
            return;
        }
        if (r.empty) {
            filter_.set(std::string(), 0);
            cursor_ = 0;
            first_ = 0;
            notice_.clear();
            started_.clear();
            ask_lists(mail);
        }
        show(mail);
    }
    void on(const gfx::DrawnRefused& refused, loom::Mail& mail) {
        (void)window_.refused(refused, mail);
    }

    /// A PRESS ON AN ENTRY'S LINE puts the cursor there; Return runs it. A window's press is not a
    /// key Workshop numbered, so it runs nothing itself. The entry is the one the line the press
    /// reached means (`RowsBlock::named`), never a row counted from the top.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        const auto p = window_.pointer(b, mail, rows(), columns());
        if (!p || !b.pressed || b.button != 1) {
            return;
        }
        if (p->moved) { // the window landed it on an entry the list has since moved
            notice_ = "the list moved -- press again";
            show(mail);
            return;
        }
        const Line* line = entries_.named(window_.part_at(p->x, p->y));
        if (line == nullptr) {
            return;
        }
        const std::vector<std::size_t> list = visible();
        const auto at = std::find_if(list.begin(), list.end(), [&](std::size_t i) {
            return listing_.entries[i].address() == line->address;
        });
        if (at != list.end()) {
            cursor_ = static_cast<std::size_t>(at - list.begin());
            show(mail);
        }
    }

private:
    /// The two lines above the entries: the filter, and the count or the notice.
    static constexpr std::int64_t kHead = 2;

    /// WHAT AN ENTRY'S LINE MEANS: the entry, by its address, and the short name its line keeps
    /// while the entry stays -- a relist of the same entries renames nothing, so a press landed on
    /// an entry still standing under the hand is the same element (#387's rule).
    struct Line {
        std::string address;
        std::string name;
        bool operator==(const Line&) const = default;
    };
    static std::string part_of(const Line& line) { return line.name; }
    /// THE NAME AN ENTRY'S LINE KEEPS: its address, hashed to 16 hex digits (well inside
    /// `kMaxPanePartNameLen`), and a count past it in the rare picture where two hash alike.
    static std::string line_name(const std::string& address, std::set<std::string>& taken) {
        std::uint64_t h = 1469598103934665603ULL;
        for (const char c : address) {
            h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
        }
        static constexpr char kHex[] = "0123456789abcdef";
        std::string name = "entry:";
        for (int shift = 60; shift >= 0; shift -= 4) {
            name += kHex[(h >> shift) & 0xF];
        }
        std::string out = name;
        for (int n = 2; !taken.insert(out).second; ++n) {
            out = name + "." + std::to_string(n);
        }
        return out;
    }

    /// The palette's own colours, each with the role a window that cannot show it paints instead.
    static gfx::Paint head_ground() { return gfx::exact(gfx::role::kMediumGround, 0x1E2A3C); }
    static gfx::Paint cursor_bar() { return gfx::exact(gfx::role::kMediumGround, 0x2C4A6E); }

    void offer(loom::Mail& mail) {
        (void)mail.as_role(kOffice).send_to_role(
            kWorkshop, ws::v2::PaneOffered{kPane, "Palette", "every action and pane, found by name",
                                           18, 64});
        ws::v3::PaneActions actions;
        actions.pane = kPane;
        const std::vector<ws::ActionCause> runs{ws::cause_of<ws::CommandRunRequested>()};
        actions.rows.push_back(ws::v3::PaneActionRow{kUp, "previous entry", input::scan::kUp,
                                                     input::mod::kNone, "", {}});
        actions.rows.push_back(ws::v3::PaneActionRow{kDown, "next entry", input::scan::kDown,
                                                     input::mod::kNone, "", {}});
        actions.rows.push_back(ws::v3::PaneActionRow{
            kRun, "run the entry", input::scan::kReturn, input::mod::kNone, "",
            {ws::cause_of<ws::CommandRunRequested>(), ws::cause_of<zengine::launcher::Launch>()}});
        actions.rows.push_back(ws::v3::PaneActionRow{kClose, "put the palette down",
                                                     input::scan::kEscape, input::mod::kNone, "",
                                                     runs});
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, actions);
    }

    void ask_lists(loom::Mail& mail) {
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::KeymapRequested{});
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::PaneInventoryRequested{});
        layouts_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(ws::kLayoutsRole, ws::LayoutSettingsRequested{},
                                                 layouts_ask_);
        listing_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(zengine::launcher::kLauncherRole,
                                                 zengine::launcher::LaunchCatalogRequested{},
                                                 listing_ask_);
    }

    /// Whether `mail` answers the ask that went out under `asked`, this weave's own number.
    static bool answers(const loom::Mail& mail, std::uint64_t asked) {
        return asked != 0 && mail.answers_ask() && mail.correlation() == asked;
    }

    /// A SETUP OR AN APP, started as Files starts one: Workshop asked first whether the hand of
    /// this row's key may build and write, under that key's own number; then the launcher. One
    /// start, as one run, in flight.
    void start(loom::Mail& mail, const pal::Entry& e) {
        if (in_flight_) {
            notice_ = "the last entry chosen is still being run";
            show(mail);
            return;
        }
        ws::v2::PaneOperationRequested asked;
        asked.pane = kPane;
        asked.gesture = static_cast<std::int64_t>(mail.correlation());
        asked.classes = {ws::scope::kBuild, ws::scope::kWrite};
        approval_ask_ = ++asks_;
        if (!mail.as_role(kOffice).send_to_role(kWorkshop, asked, approval_ask_).valid()) {
            approval_ask_ = 0;
            notice_ = "nothing was started -- Workshop could not be asked";
            show(mail);
            return;
        }
        started_.clear();
        notice_.clear();
        starting_ = zengine::launcher::Launch{
            e.kind == pal::kKindSetup ? zengine::launcher::kKindDemo : zengine::launcher::kKindApp,
            e.id};
        in_flight_ = true;
        running_ = e.address();
    }

    /// ONE RUN IN FLIGHT: a second waits for the first's answer.
    void run(loom::Mail& mail, const ws::CommandRunRequested& asked, std::string address) {
        if (in_flight_) {
            notice_ = "the last entry chosen is still being run";
            show(mail);
            return;
        }
        const loom::Ticket sent =
            mail.as_role(kOffice).send_to_role(kWorkshop, asked, mail.correlation());
        if (!sent.valid()) {
            notice_ = "Workshop could not be asked to run it";
            show(mail);
            return;
        }
        in_flight_ = true;
        asked_ = mail.correlation();
        running_ = std::move(address);
    }

    void relist(loom::Mail& mail) {
        listing_ = pal::entries_of(keys_, inventory_, others_);
        const std::size_t n = visible().size();
        cursor_ = n == 0 ? 0 : std::min(cursor_, n - 1);
        show(mail);
    }
    void refiltered(loom::Mail& mail) {
        cursor_ = 0;
        first_ = 0;
        notice_.clear();
        started_.clear();
        show(mail);
    }
    std::vector<std::size_t> visible() const {
        return pal::shown(listing_, filter_.text(), state_.history);
    }

    std::int64_t rows() const { return window_.rows_room().rows(); }
    std::int64_t columns() const { return window_.rows_room().columns(); }

    /// ONE ENTRY'S LINE, `width` wide: the marker, the label, whose it is, and its key at the end.
    static std::string line_of(const pal::Entry& e, bool here, std::int64_t width) {
        std::string key = e.key + (e.more_keys > 0 ? " +" + std::to_string(e.more_keys) : "");
        if (e.gone) {
            key = "[gone]";
        }
        std::string left = std::string(here ? "> " : "  ") + e.label + "  " + e.owner;
        const auto w = static_cast<std::size_t>(std::max<std::int64_t>(width, 1));
        if (left.size() + key.size() + 1 > w) {
            const std::size_t room = w > key.size() + 1 ? w - key.size() - 1 : 0;
            left = left.substr(0, room);
        }
        return left + std::string(w - std::min(w, left.size() + key.size()), ' ') + key;
    }

    /// THE PALETTE DRAWN in its area, a picture of its own: the filter with its caret and the count
    /// or the notice on a band of their own colour, a bar under the cursor's entry, and the entries
    /// the room holds around the cursor, each line meaning its entry -- rows inside the picture
    /// (`RowsBlock`).
    void show(loom::Mail& mail) {
        if (!window_.shown() || rows() <= 0 || columns() <= 0) {
            return;
        }
        const std::vector<std::size_t> list = visible();
        const auto room = static_cast<std::size_t>(std::max<std::int64_t>(rows() - kHead, 1));
        if (cursor_ < first_) {
            first_ = cursor_;
        } else if (cursor_ >= first_ + room) {
            first_ = cursor_ + 1 - room;
        }
        const gfx::TextMeasure& text = window_.text();
        const bool cells = window_.rows_room().cells;
        gfx::Sketch sketch;
        sketch.fill_rect("room", 0, 0, window_.width(), window_.height(),
                         gfx::by_role(gfx::role::kGround));
        sketch.fill_rect("head", 0, 0, window_.width(), kHead * text.line, head_ground());

        ws::RowsBlock<Line> head;
        head.begin(kHead, columns());
        const std::string prefix = "find: ";
        (void)head.push({prefix + filter_.text(), gfx::role::kAccent});
        std::string count = std::to_string(list.size()) + " of " +
                            std::to_string(listing_.entries.size());
        if (listing_.left_out > 0) {
            count += " -- " + std::to_string(listing_.left_out) + " " + listing_.left_from +
                     " left out past " + std::to_string(pal::kMaxEntries);
        }
        (void)head.push(!notice_.empty()    ? ws::TextRow{notice_, gfx::role::kAlert}
                        : !started_.empty() ? ws::TextRow{started_, gfx::role::kAccent}
                                            : ws::TextRow{count, gfx::role::kMuted});
        head.caret().row = 0;
        head.caret().column = static_cast<std::int64_t>(prefix.size() + filter_.caret());
        const auto always = [](const Line&) { return true; };
        (void)head.draw(sketch, "find", 0, 0, text, cells, part_of, always, true, false);

        if (cursor_ >= first_ && cursor_ < list.size() && cursor_ < first_ + room) {
            const auto y = static_cast<std::int64_t>(kHead + cursor_ - first_) * text.line;
            sketch.fill_rect("cursor", 0, y, window_.width(), text.line, cursor_bar());
        }
        entries_.begin(rows() - kHead, columns());
        std::set<std::string> taken;
        // NUMBERED BY WHAT ITS ROWS MEAN: the entries shown, from the first, so a press is read
        // against the entries it was drawn with.
        std::uint64_t meaning = 1469598103934665603ULL ^ first_;
        for (std::size_t i = first_; i < list.size() && i < first_ + room; ++i) {
            const pal::Entry& e = listing_.entries[list[i]];
            const bool here = i == cursor_;
            Line line{e.address(), ""};
            line.name = line_name(line.address, taken);
            for (const char c : line.name) {
                meaning = (meaning ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
            }
            (void)entries_.push({line_of(e, here, columns()),
                                 here ? gfx::role::kAccent
                                      : (e.off || e.gone) ? gfx::role::kMuted : gfx::role::kFill},
                                std::move(line));
        }
        (void)entries_.draw(sketch, "entries", 0, kHead * text.line, text, cells, part_of, always,
                            false, false);
        sketch.number(static_cast<std::int64_t>(meaning >> 1));
        window_.draw(sketch, mail);
    }

    // NOT STATE: the lists are Workshop's and asked again; the filter, the cursor and the run in
    // flight belong to one opening.
    ws::KeymapShown keys_;
    ws::PaneInventory inventory_;
    pal::Others others_;
    /// THIS WEAVE'S OWN NUMBERS for its asks, and the ask each answer is read against.
    std::uint64_t asks_ = 0, layouts_ask_ = 0, listing_ask_ = 0, approval_ask_ = 0, launch_ask_ = 0;
    zengine::launcher::Launch starting_; ///< the start waiting on Workshop's word
    pal::Listing listing_;
    zengine::component::TextBox filter_;
    zengine::component::Clipboard clipboard_;
    std::size_t cursor_ = 0, first_ = 0;
    ws::RowsBlock<Line> entries_;   ///< the entries' lines last drawn, and what each means
    std::string notice_;
    std::string started_; ///< the palette's own start, said where no refusal stands
    bool in_flight_ = false;
    std::uint64_t asked_ = 0;
    std::string running_;
    ws::WindowDrawing window_{kOffice, ws::pane_area(kOffice, kPane)};
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Palette)
