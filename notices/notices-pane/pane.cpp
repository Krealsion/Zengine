// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The Notices pane, a loadable weave, offers Workshop one pane: what Workshop said, the newest at
// the bottom, read from the notices record a page at a time (notices/vocabulary.hpp). It only
// shows: no row is selected and nothing is pressed into. `notices.up` and `notices.down`, and the
// wheel's notches, which ask the same two, move the rows it shows. Its laws are
// notices/docs/notices-laws.md.

// The pane owns one thing, where the view ends. The notices are the record's, asked for again each
// time the record moves or the view does.

#include "notices/notices-pane/vocabulary.hpp"
#include "notices/vocabulary.hpp"

#include "pane/causes.hpp"
#include "pane/facets.hpp"
#include "pane/rows.hpp"
#include "pane/text.hpp"
#include "pane/vocabulary.hpp"

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
namespace nt = zengine::notices;
namespace pane = zengine::notices_pane;

using zengine::pane::TextRow;
using zengine::pane::v2::PaneOffered;
using zengine::pane::v3::PaneActionRow;
using zengine::pane::pane_text::omitted_text;
using zengine::pane::pane_text::wrap;

/// The pane's area, its rows and their presses; a notice's first row means its number.
using Rows = zengine::pane::PaneRowsFacet<pane::kNoticesPane, pane::kNoticesPaneRole,
                               zengine::manual::pages::zengine_notices_pane::kManual,
                               std::int64_t>;
using Actions = zengine::pane::PaneActionFacet<pane::kNoticesPane, pane::kNoticesPaneRole>;

class NoticesPaneWeave
    : public zengine::With<Rows, Actions>::Weave<
          NoticesPaneWeave, pane::NoticesPaneState,
          loom::Accept<nt::NoticesMoved, nt::NoticesShown, loom::AskEnded>,
          loom::Emit<nt::NoticesRequested>> {
public:
    /// Offers the pane and asks the record for a page. A pane offered before anything was said
    /// shows that it is waiting, never an empty box.
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

    /// An area granted: the rows are drawn there, and a page that fills it is asked for.
    void on_room(loom::Mail& mail) {
        say(mail);
        ask(mail);
    }

    /// One of the pane's two actions, asked by name: from a key, through Workshop's keymap, or from
    /// a wheel's notch. A step asked while a page is on its way waits for it.
    void on_action(const std::string& id, loom::Mail& mail) {
        const int step = id == pane::kActionUp ? 1 : id == pane::kActionDown ? -1 : 0;
        if (step == 0) {
            return;
        }
        if (asking_) {
            owed_ += step;
            return;
        }
        (void)move(step, mail);
    }

    /// An id this pane never declared is no act, and spends nothing.
    static bool answers(const std::string& id) {
        return id == pane::kActionUp || id == pane::kActionDown;
    }

    /// Each of the wheel's notches asks the action a key would: away from the weaver is older.
    void wheel(double dy, loom::Mail& mail) {
        std::int64_t notches = zengine::component::take_wheel_notches(wheel_, dy);
        for (; notches > 0; --notches) {
            on_action(pane::kActionUp, mail);
        }
        for (; notches < 0; ++notches) {
            on_action(pane::kActionDown, mail);
        }
    }

    /// The record moved: the page is asked for again, and the view stays where it was.
    void on(const nt::NoticesMoved&, loom::Mail& mail) {
        if (mail.authored_from_role(nt::kNoticesRole)) {
            ask(mail);
        }
    }

    /// The page asked for ended with no answer. Nothing is coming, so nothing waits on it: the
    /// steps asked meanwhile are let go, and the record's next move asks again.
    void on(const loom::AskEnded&, loom::Mail& mail) {
        if (mail.ask_ended() && asking_ && mail.correlation() == ask_) {
            asking_ = false;
            owed_ = 0;
        }
    }

    /// The page this pane last asked for, and no other, is shown; then the next step a key or notch
    /// asked while it was coming is taken.
    void on(const nt::NoticesShown& shown, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != ask_) {
            return;
        }
        asking_ = false;
        page_ = shown;
        heard_ = true;
        if (state_.newest != 0 && shown.rows.empty() && shown.after > 0) {
            // The notice the view ended at was let go, so the view follows the newest from here.
            state_.newest = 0;
            ask(mail);
            return;
        }
        say(mail);
        if (owed_ != 0) {
            const int step = owed_ > 0 ? 1 : -1;
            owed_ -= step;
            if (!move(step, mail)) {
                // The view is at the end those steps go toward. They are spent, and none waits to
                // move it when the record moves later.
                owed_ = 0;
            }
        }
    }

private:
    /// The two rows that `offered` declares and `answers` reads. Each press asks the record for the
    /// page the view moved to.
    static std::vector<PaneActionRow> action_rows() {
        const std::vector<zengine::pane::ActionCause> asks = {
            zengine::pane::cause_of<nt::NoticesRequested>()};
        return {PaneActionRow{pane::kActionUp, "older notice", input::scan::kUp, input::mod::kNone,
                              {}, asks},
                PaneActionRow{pane::kActionDown, "newer notice", input::scan::kDown,
                              input::mod::kNone, {}, asks}};
    }

    Rows& room() { return this->template facet<Rows>(); }

    /// Moves the view one notice older (`step` 1) or newer (-1) and asks for that page. It moves
    /// older only while the view leaves an older one unshown, and newer only while one stands after
    /// it; at that end it asks nothing and answers false.
    bool move(int step, loom::Mail& mail) {
        if (!heard_) {
            return false;
        }
        const std::vector<nt::KeptNotice>& rows = page_.rows;
        if (step > 0) {
            if (older_hidden_ <= 0 || rows.empty()) {
                return false;
            }
            state_.newest = rows.size() >= 2 ? rows[rows.size() - 2].number : rows.front().number - 1;
        } else {
            if (page_.after <= 0) {
                return false;
            }
            state_.newest = page_.after == 1 ? 0 : page_.next;
        }
        ask(mail);
        return true;
    }

    /// Asks the record for the page the area can show: as many notices as the area has rows, at
    /// least one and at most a page, ending where the view ends. A later ask makes an earlier one's
    /// answer stale.
    void ask(loom::Mail& mail) {
        const std::int64_t count = std::clamp<std::int64_t>(room().rows(), 1, nt::kMaxNoticesPage);
        ask_ = ++asks_;
        asking_ = true;
        (void)mail.as_role(pane::kNoticesPaneRole)
            .ask_role(nt::kNoticesRole, nt::NoticesRequested{state_.newest, count}, ask_);
    }

    /// Draws what the pane shows: the notices of the page, the newest at the bottom, each wrapped
    /// to the area's width with its continuations indented, and a refusal in the alert's colour;
    /// above them, how many are kept and not shown, or how many the record let go; below them, how
    /// many are newer.
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
            (void)r.push(TextRow{"waiting for the notices record", gfx::colors::quiet});
            publish(mail);
            return;
        }
        if (page_.rows.empty() && page_.before == 0 && page_.after == 0) {
            (void)r.push(TextRow{"nothing said yet", gfx::colors::quiet});
            publish(mail);
            return;
        }
        const std::int64_t newer_row = page_.after > 0 && rows >= 2 ? 1 : 0;
        // The notices that fit, newest first and each whole; the newest is cut to the area when it
        // alone is taller. They are measured once without a row above them, and again with one
        // when that row is owed.
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
            (void)r.push(TextRow{"  " + said, gfx::colors::quiet});
        }
        for (std::size_t b = blocks.size(); b-- > 0;) {
            const Block& block = blocks[b];
            const gfx::Color ink =
                block.notice->refusal ? gfx::colors::alert : gfx::colors::text;
            for (std::size_t line = 0; line < block.lines.size(); ++line) {
                std::optional<std::int64_t> meaning;
                if (line == 0) {
                    meaning = block.notice->number;
                }
                (void)r.push(TextRow{block.lines[line], ink}, meaning);
            }
        }
        if (newer_row > 0) {
            (void)r.push(TextRow{"  " + omitted_text(static_cast<std::size_t>(page_.after), "newer"),
                                 gfx::colors::quiet});
        }
        publish(mail);
    }

    /// Sends the rows composed, a notice's first row named `notice:<number>`, with no caret.
    void publish(loom::Mail& mail) {
        room().publish(
            mail, [](std::int64_t number) { return "notice:" + std::to_string(number); },
            [](std::int64_t) { return true; }, false);
    }

    // ---- State not in the shape --------------------------------------------------------

    /// The record's last page, as answered to this pane's last ask. It is never kept across a
    /// reload.
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
