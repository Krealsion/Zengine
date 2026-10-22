// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Command Palette: every action and every pane a hand can reach, found by name and run by
// Workshop. It reads what Workshop says to any presenter -- the keymap in force and the pane
// inventory -- keeps no catalog of its own, and asks Workshop to run what is chosen
// (`CommandRunRequested`), which performs it by the path its key or press takes, as the hand
// that chose it. It draws only in its own area of the window that shows it.
// Reference: workshop/docs/zengine.palette.md; the door: WL-DESK-18, agents/workshop/desktop.md.

#include "workshop/palette/entries.hpp"

#include "workshop/command_vocabulary.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/text_box.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
          loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom, ws::PaneKey,
                       ws::PaneTextInput, ws::PaneActionRequested, ws::KeymapShown,
                       ws::PaneInventory, ws::CommandRunAnswered, gfx::AreaRoom, gfx::DrawnRefused,
                       gfx::AreaButtonChanged>,
          loom::Emit<ws::v2::PaneOffered, ws::v3::PaneActions, ws::KeymapRequested,
                     ws::PaneInventoryRequested, ws::CommandRunRequested, gfx::Drawn,
                     gfx::RoomAsked>> {
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

    /// THE PALETTE OPENED: Workshop granted it its room. It starts afresh -- an empty filter, the
    /// cursor on the first entry -- over the lists as they stand now.
    void on(const ws::PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || room.pane != kPane) {
            return;
        }
        filter_.set(std::string(), 0);
        cursor_ = 0;
        first_ = 0;
        notice_.clear();
        ask_lists(mail);
        show(mail);
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
        run(mail, ws::CommandRunRequested{e.kind, e.office, e.pane, e.id}, e.address());
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

    /// THE AREA'S ROOM, as the window that shows it says it: drawn whole there.
    void on(const gfx::AreaRoom& r, loom::Mail& mail) {
        if (window_.room(r, mail)) {
            show(mail);
        }
    }
    void on(const gfx::DrawnRefused& refused, loom::Mail& mail) {
        (void)window_.refused(refused, mail);
    }

    /// A PRESS ON AN ENTRY'S LINE puts the cursor there; Return runs it. A window's press is not a
    /// key Workshop numbered, so it runs nothing itself.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        const auto p = window_.pointer(b, mail, rows(), columns());
        if (!p || !b.pressed || b.button != 1 || !p->at.shown) {
            return;
        }
        const std::int64_t entry = p->at.row - kHead + static_cast<std::int64_t>(first_);
        const std::vector<std::size_t> list = visible();
        if (p->at.row >= kHead && entry >= 0 && static_cast<std::size_t>(entry) < list.size()) {
            cursor_ = static_cast<std::size_t>(entry);
            show(mail);
        }
    }

private:
    /// The two lines above the entries: the filter, and the count or the notice.
    static constexpr std::int64_t kHead = 2;

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
        actions.rows.push_back(ws::v3::PaneActionRow{kRun, "run the entry", input::scan::kReturn,
                                                     input::mod::kNone, "", runs});
        actions.rows.push_back(ws::v3::PaneActionRow{kClose, "put the palette down",
                                                     input::scan::kEscape, input::mod::kNone, "",
                                                     runs});
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, actions);
    }

    void ask_lists(loom::Mail& mail) {
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::KeymapRequested{});
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::PaneInventoryRequested{});
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
        listing_ = pal::entries_of(keys_, inventory_);
        const std::size_t n = visible().size();
        cursor_ = n == 0 ? 0 : std::min(cursor_, n - 1);
        show(mail);
    }
    void refiltered(loom::Mail& mail) {
        cursor_ = 0;
        first_ = 0;
        notice_.clear();
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

    /// THE PALETTE DRAWN in its area: the filter with its caret, the count or the notice, and the
    /// entries the room holds around the cursor.
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
        std::vector<zengine::surface::SurfaceTextRow> lines;
        const std::string prefix = "find: ";
        lines.push_back({prefix + filter_.text(), gfx::role::kAccent});
        std::string count = std::to_string(list.size()) + " of " +
                            std::to_string(listing_.entries.size());
        if (listing_.left_out > 0) {
            count += " -- " + std::to_string(listing_.left_out) + " " + listing_.left_from +
                     " left out past " + std::to_string(pal::kMaxEntries);
        }
        lines.push_back(notice_.empty() ? zengine::surface::SurfaceTextRow{count, gfx::role::kMuted}
                                        : zengine::surface::SurfaceTextRow{notice_, gfx::role::kAlert});
        for (std::size_t i = first_; i < list.size() && i < first_ + room; ++i) {
            const pal::Entry& e = listing_.entries[list[i]];
            const bool here = i == cursor_;
            lines.push_back({line_of(e, here, columns()),
                             here ? gfx::role::kAccent
                                  : (e.off || e.gone) ? gfx::role::kMuted : gfx::role::kFill});
        }
        ws::RowsCaret caret;
        caret.row = 0;
        caret.column = static_cast<std::int64_t>(prefix.size() + filter_.caret());
        const ws::RowsDrawing drawn = ws::rows_drawing(
            lines, std::vector<ws::PaneRowPart>{}, window_.rows_room(), columns(), caret, rows());
        window_.draw(drawn.sketch, mail);
    }

    // NOT STATE: the lists are Workshop's and asked again; the filter, the cursor and the run in
    // flight belong to one opening.
    ws::KeymapShown keys_;
    ws::PaneInventory inventory_;
    pal::Listing listing_;
    zengine::component::TextBox filter_;
    zengine::component::Clipboard clipboard_;
    std::size_t cursor_ = 0, first_ = 0;
    std::string notice_;
    bool in_flight_ = false;
    std::uint64_t asked_ = 0;
    std::string running_;
    ws::WindowDrawing window_{kOffice, kOffice};
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Palette)
