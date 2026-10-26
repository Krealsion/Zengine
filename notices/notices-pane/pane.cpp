// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Notices pane: a loadable weave that offers Workshop one pane -- what Workshop said, the
// newest at the bottom -- walking the notices record a page at a time (notices/vocabulary.hpp).
// It is pure information: no row is selected and nothing is pressed into; `notices.up` and
// `notices.down`, and the wheel's notches, which ask the same two, move the rows it shows.
// Workshop law: agents/workshop/attention.md

// This pane owns one thing, where the view ends; the notices are the record's, asked again each
// time the record moves or the view does.

#include "notices/notices-pane/vocabulary.hpp"
#include "notices/vocabulary.hpp"

#include "workshop/pane_causes.hpp"
#include "workshop/pane_facets.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/list_window.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"

#include "manuals/zengine.notices-pane.hpp" // this pane's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace gfx = zengine::graphics;
namespace input = zengine::input;
namespace ws = zengine::workshop;
namespace nt = zengine::notices;
namespace pane = zengine::notices_pane;

using ws::TextRow;
using ws::v2::PaneOffered;
using ws::v3::PaneActionRow;
using zengine::workshop::pane_text::omitted_text;
using zengine::workshop::pane_text::wrap;

/// The pane's room, its rows and their presses; a notice's first row means its number.
using Rows = ws::PaneRowsFacet<pane::kNoticesPane, pane::kNoticesPaneRole,
                               zengine::manual::pages::zengine_notices_pane::kManual,
                               std::int64_t>;
using Actions = ws::PaneActionFacet<pane::kNoticesPane, pane::kNoticesPaneRole>;

class NoticesPaneWeave
    : public zengine::With<Rows, Actions>::Weave<
          NoticesPaneWeave, pane::NoticesPaneState,
          loom::Accept<nt::NoticesMoved, nt::NoticesShown>, loom::Emit<nt::NoticesRequested>> {
public:
    /// THE OFFER, and the record asked: a pane offered before anything was said shows that it is
    /// waiting, never an empty box.
    void on_activated(loom::Mail& mail) {
        room().announce(*this, mail);
        ask(mail);
    }

    PaneOffered offering() const {
        return PaneOffered{pane::kNoticesPane, pane::kNoticesPaneName, pane::kNoticesPaneSummary,
                           6, 76};
    }

    /// The rows the pane answers to.
    void offered(loom::Mail& mail) { this->template facet<Actions>().declare(mail, action_rows()); }

    /// A room granted: the rows drawn there, and a page that fills it asked.
    void on_room(loom::Mail& mail) {
        say(mail);
        ask(mail);
    }

    /// ONE OF THE PANE'S TWO ACTIONS, asked by name -- from a key, through Workshop's keymap, or
    /// from a wheel's notch. A step asked while a page is being asked waits for it.
    void on_action(const std::string& id, loom::Mail& mail) {
        const int step = id == pane::kActionUp ? 1 : id == pane::kActionDown ? -1 : 0;
        if (step == 0) {
            return;
        }
        if (asking_) {
            owed_ += step;
            return;
        }
        move(step, mail);
    }

    /// AN ID THIS PANE NEVER DECLARED IS NO ACT, and spends nothing.
    static bool answers(const std::string& id) {
        return id == pane::kActionUp || id == pane::kActionDown;
    }

    /// THE WHEEL'S NOTCHES, each the action a key would ask: away from the weaver is older.
    void wheel(double dy, loom::Mail& mail) {
        std::int64_t notches = zengine::component::take_wheel_notches(wheel_, dy);
        for (; notches > 0; --notches) {
            on_action(pane::kActionUp, mail);
        }
        for (; notches < 0; ++notches) {
            on_action(pane::kActionDown, mail);
        }
    }

    /// THE RECORD MOVED: the page asked again, the view where it was.
    void on(const nt::NoticesMoved&, loom::Mail& mail) {
        if (mail.authored_from_role(nt::kNoticesRole)) {
            ask(mail);
        }
    }

    /// THE PAGE THIS PANE LAST ASKED FOR, and no other: shown, then the next step a key or notch
    /// asked while it was coming.
    void on(const nt::NoticesShown& shown, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != ask_) {
            return;
        }
        asking_ = false;
        page_ = shown;
        heard_ = true;
        if (state_.newest != 0 && (shown.after == 0 || shown.rows.empty())) {
            // THE VIEW REACHED THE NEWEST, or the notice it ended at was let go: it follows the
            // newest from here.
            state_.newest = 0;
            if (shown.rows.empty() && shown.after > 0) {
                ask(mail);
                return;
            }
        }
        say(mail);
        if (owed_ != 0) {
            const int step = owed_ > 0 ? 1 : -1;
            owed_ -= step;
            move(step, mail);
        }
    }

private:
    /// THE TWO ROWS -- what `offered` declares and `answers` reads -- each press asking the
    /// record for the page the view moved to.
    static std::vector<PaneActionRow> action_rows() {
        const std::vector<zengine::workshop::ActionCause> asks = {
            ws::cause_of<nt::NoticesRequested>()};
        return {PaneActionRow{pane::kActionUp, "older notice", input::scan::kUp, input::mod::kNone,
                              {}, asks},
                PaneActionRow{pane::kActionDown, "newer notice", input::scan::kDown,
                              input::mod::kNone, {}, asks}};
    }

    Rows& room() { return this->template facet<Rows>(); }

    /// ONE NOTICE OLDER (`step` 1) OR NEWER (-1): the view's last notice moved, and the page asked.
    /// Older only while the view leaves an older one unshown; newer only while one stands after it.
    void move(int step, loom::Mail& mail) {
        if (!heard_) {
            return;
        }
        const std::vector<nt::KeptNotice>& rows = page_.rows;
        if (step > 0) {
            if (older_hidden_ <= 0 || rows.empty()) {
                return;
            }
            state_.newest = rows.size() >= 2 ? rows[rows.size() - 2].number : rows.front().number - 1;
        } else {
            if (page_.after <= 0) {
                return;
            }
            state_.newest = page_.after == 1 ? 0 : page_.next;
        }
        ask(mail);
    }

    /// THE PAGE THE ROOM CAN SHOW, asked of the record: as many notices as the room has rows, each
    /// at least one, ending where the view ends. A later ask makes an earlier one's answer stale.
    void ask(loom::Mail& mail) {
        const std::int64_t count = std::clamp<std::int64_t>(room().rows(), 1, nt::kMaxNoticesPage);
        ask_ = ++asks_;
        asking_ = true;
        (void)mail.as_role(pane::kNoticesPaneRole)
            .send_to_role(nt::kNoticesRole, nt::NoticesRequested{state_.newest, count}, ask_);
    }

    /// WHAT THE PANE SHOWS: the notices of the page, the newest at the bottom, each wrapped to the
    /// room with its continuations indented, a refusal in the alert's colour; above them, what is
    /// kept and not shown, or what the record let go; below them, what is newer.
    void say(loom::Mail& mail) {
        Rows& r = room();
        const std::int64_t rows = r.rows(), columns = r.columns();
        r.begin();
        older_hidden_ = 0;
        if (rows <= 0 || columns <= 0) {
            publish(mail);
            return;
        }
        if (!heard_) {
            (void)r.push(TextRow{"waiting for the notices record", gfx::role::kMuted});
            publish(mail);
            return;
        }
        if (page_.rows.empty() && page_.before == 0 && page_.after == 0) {
            (void)r.push(TextRow{"nothing said yet", gfx::role::kMuted});
            publish(mail);
            return;
        }
        const std::int64_t newer_row = page_.after > 0 && rows >= 2 ? 1 : 0;
        // THE NOTICES THAT FIT, newest first, whole; the newest cut to the room when it alone is
        // taller. Measured once without a row above and again with one when that row is owed.
        struct Block {
            const nt::KeptNotice* notice;
            std::vector<std::string> lines;
        };
        const auto fill = [&](std::int64_t budget) {
            std::vector<Block> blocks;
            std::int64_t used = 0;
            for (std::size_t i = page_.rows.size(); i-- > 0;) {
                std::vector<std::string> lines = wrap(page_.rows[i].text, columns);
                if (lines.empty()) {
                    lines.emplace_back();
                }
                const std::int64_t tall = static_cast<std::int64_t>(lines.size());
                if (used + tall > budget) {
                    if (blocks.empty() && budget > 0) {
                        lines.resize(static_cast<std::size_t>(budget));
                        blocks.push_back(Block{&page_.rows[i], std::move(lines)});
                    }
                    break;
                }
                used += tall;
                blocks.push_back(Block{&page_.rows[i], std::move(lines)});
            }
            return blocks;
        };
        const std::int64_t budget = rows - newer_row;
        std::vector<Block> blocks = fill(budget);
        const auto hidden = [&](const std::vector<Block>& drawn) {
            return page_.before + static_cast<std::int64_t>(page_.rows.size() - drawn.size());
        };
        const bool above = (hidden(blocks) > 0 || page_.let_go > 0) && budget >= 2;
        if (above) {
            blocks = fill(budget - 1);
        }
        older_hidden_ = hidden(blocks);
        if (above) {
            const std::string said =
                older_hidden_ > 0
                    ? omitted_text(static_cast<std::size_t>(older_hidden_), "earlier")
                    : "... " + std::to_string(page_.let_go) + " let go -- the record keeps " +
                          std::to_string(page_.keep);
            (void)r.push(TextRow{"  " + said, gfx::role::kMuted});
        }
        for (std::size_t b = blocks.size(); b-- > 0;) {
            const Block& block = blocks[b];
            const std::int64_t role =
                block.notice->refusal ? gfx::role::kAlert : gfx::role::kFill;
            for (std::size_t line = 0; line < block.lines.size(); ++line) {
                std::optional<std::int64_t> meaning;
                if (line == 0) {
                    meaning = block.notice->number;
                }
                (void)r.push(TextRow{block.lines[line], role}, meaning);
            }
        }
        if (newer_row > 0) {
            (void)r.push(TextRow{"  " + omitted_text(static_cast<std::size_t>(page_.after), "newer"),
                                 gfx::role::kMuted});
        }
        publish(mail);
    }

    /// THE ROWS COMPOSED, sent: a notice's first row named `notice:<number>`; no caret.
    void publish(loom::Mail& mail) {
        room().publish(
            mail, [](std::int64_t number) { return "notice:" + std::to_string(number); },
            [](std::int64_t) { return true; }, false);
    }

    // ---- State not in the shape --------------------------------------------------------

    /// THE RECORD'S LAST PAGE, as answered to this pane's last ask; never reload state.
    nt::NoticesShown page_;
    bool heard_ = false;
    /// The kept notices older than the view's first, as the rows last composed left them.
    std::int64_t older_hidden_ = 0;

    std::uint64_t asks_ = 0, ask_ = 0;
    bool asking_ = false;
    /// Steps a key or notch asked while a page was coming: older positive, newer negative.
    std::int64_t owed_ = 0;
    /// What the wheel turned short of a notch.
    double wheel_ = 0.0;
};

} // namespace

ZEN_EXPORT_WEAVE(NoticesPaneWeave)
