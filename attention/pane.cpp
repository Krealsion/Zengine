// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Attention pane: a loadable weave that offers Workshop one pane -- what is true right now
// and worth a weaver's attention. Every row is the host's reading of its own state (files it
// could not use, a pane that refused an update, an authored pane no cell shows, the row
// realization stopped at), published whenever it changes
// (`workshop/attention_seam_vocabulary.hpp`), so there is nothing to poll (WL-ATTN-01).
// Workshop law: agents/workshop/attention.md

// This pane owns one thing, which statements the weaver has hidden; everything else is held for
// as long as the last publication and replaced whole by the next.

#include "attention/vocabulary.hpp"

#include "workshop/attention_seam_vocabulary.hpp"
#include "workshop/pane_facets.hpp"
#include "workshop/pane_settings.hpp"
#include "workshop/pane_settings_taken.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/list_window.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include "manuals/zengine.attention.hpp" // this pane's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace gfx = zengine::graphics;
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace pane = zengine::attention_pane;

using ws::v3::PaneActionRow;
using ws::v2::PaneOffered;
using ws::StandingCondition;
using ws::StandingConditions;

/// WHO THIS PANE IS TALKING TO. The host's office, spelled as a literal exactly as
/// `files.cpp` and `builder/builder-pane/pane.cpp` spell it: a provider is a stranger to Workshop's
/// internals and says who it is talking to the way a third party would.
constexpr const char* kWorkshopRole = "zengine.workshop";

// ---- The text helpers the composition spends ------------------------------------------
//
// `fit`, `wrap` and `omitted_text` come from `workshop/pane_text.hpp`; the list's window is
// `component/list_window.hpp`'s.

using zengine::component::ListWindow;
using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;
using zengine::workshop::pane_text::omitted_text;
using zengine::workshop::pane_text::wrap;
constexpr std::int64_t kWrapIndent = zengine::workshop::pane_text::kWrapIndent;

/// WHAT A PRESS ON A CONDITION'S ROW ASKS: `condition:<key>`, the key as the host said it.
constexpr const char* kConditionAction = "condition:";

/// THE NAME OF A CONDITION'S ROW, `condition:<key>`, with `/` and `%` spelled `%2F` and `%25` so
/// the key stays one segment of the drawing's path; empty when the name is longer than a path may
/// be, and the row is then drawn unnamed.
std::string condition_path(const std::string& key) {
    std::string out = kConditionAction;
    for (const char c : key) {
        out += c == '/' ? std::string("%2F") : c == '%' ? std::string("%25") : std::string(1, c);
    }
    return out.size() <= gfx::kMaxPathBytes ? out : std::string();
}

/// WHAT A DISMISSAL IS MEASURED AGAINST -- the condition's content, as one opaque token.
/// `Condition::stamp()`'s composition, over the fields that cross: the fifth field is the
/// resolved suggestion where the host's own was the action's id, and it is the same
/// question either way ("is this the same statement I hid?").
std::string stamp_of(const StandingCondition& c) {
    return c.compact + '\n' + c.detail + '\n' + std::to_string(c.role) + '\n' + c.suggestion;
}

// =============================================================================
// The weave
// =============================================================================

/// The pane's room and its one picture, shown in Workshop and in a graphics window; a press on a
/// condition's row asks `condition:<key>`.
using Drawing = ws::PaneDrawingFacet<pane::kAttentionPane, pane::kAttentionPaneRole,
                                     zengine::manual::pages::zengine_attention::kManual>;
using Actions = ws::PaneActionFacet<pane::kAttentionPane, pane::kAttentionPaneRole>;

class AttentionPaneWeave
    : public zengine::With<Drawing, Actions>::Weave<
          AttentionPaneWeave, pane::AttentionPaneState,
          loom::Accept<StandingConditions, ws::v2::PaneSettings>,
          loom::Emit<ws::v2::PaneSettingsDeclared, ws::PaneSettingUnused>> {
public:
    PaneOffered offering() const {
        return PaneOffered{pane::kAttentionPane, pane::kAttentionPaneName,
                           pane::kAttentionPaneSummary, 7, 62};
    }

    /// The rows the pane answers to, and the setting it takes.
    void offered(loom::Mail& mail) {
        declare(mail);
        (void)mail.as_role(pane::kAttentionPaneRole)
            .send_to_role(kWorkshopRole,
                          ws::v2::PaneSettingsDeclared{pane::kAttentionPane, setting_rows()});
    }

    /// Workshop, or the graphics window that shows the pane's area, grants it a room: the one beat
    /// on which this view draws unasked. It asks for nothing here: what it shows arrives when it
    /// changes, so a pane granted a room before the host has said anything shows "waiting" rather
    /// than an empty list reading as "all is well".
    void on_room(loom::Mail& mail) { say(mail); }

    /// ONE OF THE PANE'S DECLARED ACTIONS, ASKED FOR BY NAME (WL-KEY-15), OR A PRESS ON A
    /// CONDITION'S ROW. Workshop resolved the keystroke against the effective keymap -- the
    /// weaver's override where one is authored, this office's declared default otherwise -- so
    /// what arrives is the id, one `answers` admits; a press on a row, in Workshop or in a window,
    /// asks `condition:<key>`, which puts the cursor on it. Every act says its rows below, so what
    /// an act spends is always published.
    void on_action(const std::string& id, loom::Mail& mail) {
        notice_.clear(); // the weaver has acted; the last act's answer is spent
        const std::vector<StandingCondition> shown = visible();
        if (cursor_ >= shown.size()) {
            cursor_ = shown.empty() ? 0 : shown.size() - 1;
        }
        if (id.rfind(kConditionAction, 0) == 0) {
            const std::string key = id.substr(std::string(kConditionAction).size());
            for (std::size_t i = 0; i < shown.size(); ++i) {
                if (shown[i].key == key) {
                    cursor_ = i;
                }
            }
        } else if (id == pane::kActionUp) {
            if (cursor_ > 0) {
                --cursor_;
            }
        } else if (id == pane::kActionDown) {
            if (cursor_ + 1 < shown.size()) {
                ++cursor_;
            }
        } else if (id == pane::kActionDismiss) {
            if (cursor_ < shown.size()) {
                // AN EVENT, SAID AS ONE. What just happened is that a weaver hid a
                // presentation; what remains true is the condition, which is why the
                // sentence is about the gesture and not about the subject. A pane has only
                // its own room, so it leads with it and it stands until the weaver's next act
                // (`agents/panes.md`).
                notice_ = "hidden -- " + shown[cursor_].compact + " is still true";
                dismiss(shown[cursor_]);
            }
        }
        say(mail);
    }

    /// AN ID THIS PANE NEVER DECLARED IS NO ACT (`agents/panes.md`), so it spends nothing: the
    /// notice, the cursor and every dismissal stay as they were.
    static bool answers(const std::string& id) {
        for (const PaneActionRow& row : action_rows()) {
            if (row.id == id) {
                return true;
            }
        }
        return false;
    }

    /// WHAT IS TRUE RIGHT NOW, SAID BY THE HOST. Replaced WHOLE, never merged: the host
    /// publishes the current reading and a condition that stopped being returned stopped
    /// being true. Half of a previous reading and half of this one would be a picture of a
    /// moment that never existed -- the same republish-the-picture discipline
    /// `builder::BuildStatus` is under.
    void on(const StandingConditions& said, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return; // a stranger's opinion about what is true is not a reading of the host
        }
        known_ = said.rows;
        heard_ = true;
        // A DISMISSAL WHOSE CONDITION IS GONE GOES WITH IT. The set stays one entry per key
        // and nothing here outlives its subject: a keymap that starts being readable takes
        // the weaver's decision not to look at the refusal with it, and if the same key ever
        // comes back it comes back visible.
        forget_resolved();
        const std::size_t total = visible().size();
        if (cursor_ >= total) {
            cursor_ = total == 0 ? 0 : total - 1;
        }
        say(mail);
    }

    /// THE SETTING THIS LAYOUT KEEPS FOR THE PANE, taken whole; what it cannot use is said back
    /// against the revision it was handed at. Held as a plain member, never reload state.
    void on(const ws::v2::PaneSettings& handed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || handed.pane != pane::kAttentionPane) {
            return;
        }
        const ws::SettingsTaken taken = ws::take_pane_settings(setting_rows(), handed);
        ws::say_settings_unused(mail, pane::kAttentionPaneRole, taken);
        const bool was = detail_shown_;
        detail_shown_ = taken.flag(pane::kSettingDetail);
        if (detail_shown_ != was) {
            say(mail); // draws nothing until a room is granted, so the first picture has it
        }
    }

private:
    // ---- Offering and declaring ---------------------------------------------------------

    /// THE SETTING THE PANE TAKES, with its default, how a weaver finds it and what it does.
    static std::vector<ws::v2::PaneSettingRow> setting_rows() {
        ws::v2::PaneSettingRow detail;
        detail.key = pane::kSettingDetail;
        detail.flag = true;
        detail.tags = {"help", "chrome"};
        detail.pinned = true;
        detail.summary = "show the selected item detail block";
        return {detail};
    }

    /// What this pane answers to: three rows in every state, `attention.up`, `attention.down` and
    /// `attention.dismiss` (Up, Down, `d`), the ids a weaver's keymap names. No modes, so one
    /// declaration: nothing in this pane takes text, which is also why a bare letter is legal.
    void declare(loom::Mail& mail) { this->template facet<Actions>().declare(mail, action_rows()); }

    /// THE THREE ROWS -- what `declare` tells Workshop, and what `answers` reads, so what the
    /// pane acts on and what it said it acts on are one list.
    static std::vector<PaneActionRow> action_rows() {
        return {PaneActionRow{pane::kActionUp, "row up", input::scan::kUp, input::mod::kNone},
                PaneActionRow{pane::kActionDown, "row down", input::scan::kDown,
                              input::mod::kNone},
                PaneActionRow{pane::kActionDismiss, "hide this one", input::scan::kD,
                              input::mod::kNone}};
    }

    // ---- The weaver's own half: what they have chosen not to look at ----------------------

    bool hides(const StandingCondition& c) const {
        const std::string mark = stamp_of(c);
        for (const pane::Dismissal& d : state_.dismissed) {
            if (d.key == c.key) {
                return d.stamp == mark;
            }
        }
        return false;
    }

    /// Hide this statement. Re-dismissing a condition that has since changed REPLACES the
    /// old stamp rather than adding a row, so the set stays one entry per key and a weaver
    /// who hides the same condition twice has hidden it once.
    void dismiss(const StandingCondition& c) {
        const std::string mark = stamp_of(c);
        for (pane::Dismissal& d : state_.dismissed) {
            if (d.key == c.key) {
                d.stamp = mark;
                return;
            }
        }
        state_.dismissed.push_back(pane::Dismissal{c.key, mark});
    }

    /// DROP EVERY DISMISSAL WHOSE CONDITION IS NO LONGER TRUE. The set is the pane's own state
    /// and crosses a reload, so a dismissal that outlived its subject would be a decision a
    /// weaver made about a fact that no longer exists, silently re-applied if it ever came
    /// back. Dismiss is still not resolve: this drops the HIDING, not the condition.
    void forget_resolved() {
        std::vector<pane::Dismissal> kept;
        for (const pane::Dismissal& d : state_.dismissed) {
            for (const StandingCondition& c : known_) {
                if (c.key == d.key) {
                    kept.push_back(d);
                    break;
                }
            }
        }
        state_.dismissed = std::move(kept);
    }

    /// THE ONE POPULATION EVERY ROW BELOW SPENDS -- what the host says is true, less what
    /// this weaver has hidden. `attention_shown`'s job, on the pane's side of the seam.
    std::vector<StandingCondition> visible() const {
        std::vector<StandingCondition> out;
        for (const StandingCondition& c : known_) {
            if (!hides(c)) {
                out.push_back(c);
            }
        }
        return out;
    }

    // ---- Saying what the pane shows ------------------------------------------------------

    Drawing& room() { return this->template facet<Drawing>(); }
    const Drawing& room() const { return this->template facet<Drawing>(); }

    /// The picture shown, wherever the pane stands.
    void say(loom::Mail& mail) { room().show(*this, mail); }

public:
    /// WHAT THE PANE SHOWS, one row a line of the room's text high: the notice, the glance, and the
    /// list `say_view` composes. A row the room has no place for is neither drawn nor named.
    void picture(gfx::Sketch& f) {
        const std::int64_t rows = room().rows(), columns = room().columns();
        const std::int64_t height = room().text().line;
        std::int64_t row = 0;
        const auto line = [&](const std::string& path, const std::string& text,
                              std::int64_t role) {
            f.text(path, 0, row * height, drawable(fit(text, columns)), gfx::by_role(role));
            ++row;
        };
        // A notice, when there is one, leads: a pane has only its own room, so its first row
        // carries it, and it is cleared by the weaver's next act rather than by being said
        // (`agents/panes.md`).
        if (!notice_.empty() && rows > 1) {
            line("notice", notice_, surface::role::kAccent);
        }
        // A CONDITION IS NAMED BY ITS KEY, `condition:<key>`, on the row that says it, and a
        // press there asks for it; the notice and the glance are named for what they are, and
        // every other row for its place.
        bool glance = true, lines = false;
        say_view([&](const std::string& text, std::int64_t role,
                     std::optional<std::string> key = std::nullopt) {
            if (row >= rows) {
                return;
            }
            if (glance) {
                glance = false;
                line("glance", text, role);
                return;
            }
            const std::string named = key ? condition_path(*key) : std::string();
            if (!named.empty()) {
                line(named, text, role);
                f.action(named, kConditionAction + *key);
                return;
            }
            if (!lines) {
                lines = true;
                f.group("line", 0, 0);
            }
            line("line/" + std::to_string(row), text, role);
        });
    }

private:
    /// HOW MANY ROWS THE LIST MAY SPEND: the room, less the glance, less the notice row
    /// `picture` puts in front of it.
    std::int64_t body_budget() const {
        return room().rows() - 1 - (notice_.empty() ? 0 : 1);
    }

    // WL-ATTN-06 -- agents/workshop/attention.md
    template <class Push>
    void say_view(Push&& push) {
        const std::int64_t columns = room().columns();
        // NO ROW SPELLS THE PANE'S OWN KEYS: a pane's declared rows are in the band's legend and
        // in the hotkey view under this pane's own heading, resolved through the weaver's
        // effective keymap. Saying them here would put this pane in the business of reading a
        // keymap it cannot see.
        if (!heard_) {
            // THE HOST HAS NOT SAID ANYTHING YET, WHICH IS NOT THE SAME AS NOTHING BEING
            // WRONG. A pane opened before the first publication has no reading at all, and
            // an empty list here would read as an all-clear this pane has no grounds for.
            push("ATTENTION (waiting)", surface::role::kMuted);
            return;
        }
        if (known_.empty()) {
            // NOTHING IS WRONG, SAID IN WORDS. A weaver who put this pane on their desk is
            // owed an answer, and an empty box is not one.
            push("nothing needs your attention right now", surface::role::kMuted);
            return;
        }
        // THE GLANCE LEADS: the loudest condition the host says is true and how many more, in
        // that condition's role -- every true one, hidden or not, since hiding is a choice about
        // what this weaver reads and not about what is the case. A pane one row tall is this row.
        push(ws::attention_glance(known_), known_.front().role);
        const std::vector<StandingCondition> shown = visible();
        const std::int64_t budget_rows = body_budget();
        if (budget_rows <= 0) {
            return;
        }
        const std::size_t budget = static_cast<std::size_t>(budget_rows);
        if (shown.empty()) {
            // EVERYTHING TRUE IS HIDDEN, WHICH IS NOT NOTHING BEING TRUE. Hiding is what this
            // weaver chose to read; the host still holds each condition, the glance still counts
            // it and the publication still carries it, so the list says it is hiding them rather
            // than saying the all-clear.
            push("  all conditions hidden -- " + std::to_string(known_.size()) +
                     (known_.size() == 1 ? " is" : " are") + " still true",
                 surface::role::kMuted);
            return;
        }
        // The cursor's own block is composed and reserved before the list is windowed: a window
        // computed over the compact rows alone would, once the kept row spends three more
        // beneath it, push off the omission marker -- a bound that grows when exceeded is none.
        // The cursor is resolved once, here, since the population can shrink between a keystroke
        // and the next publication, and every question below spends the same answer.
        const std::size_t cursor = cursor_ < shown.size() ? cursor_ : shown.size() - 1;
        const StandingCondition& at = shown[cursor];
        // THE LAYOUT'S `detail` SETTING OFF: no block, and its rows are the list's.
        std::vector<std::string> block =
            detail_shown_ ? wrap(at.detail, columns - kWrapIndent) : std::vector<std::string>{};
        if (detail_shown_ && !at.suggestion.empty()) {
            // WHAT A WEAVER COULD PRESS SOMEWHERE ELSE, in the host's own words. Nothing here
            // can press it, which is WL-ATTN-10 unchanged: the row is a sentence, and this
            // pane could not resolve an action id even if it were given one.
            block.push_back(at.suggestion);
        }
        // THE LIST KEEPS A FLOOR OF THREE ROWS, so the cursor's row is always in the window it is
        // being explained inside of. Below four rows there is no explanation at all rather than
        // an explanation with no statement over it, and below three no row is seated between two
        // markers: the list says only how many it leaves out, since it never drops a condition
        // quietly. At three and more the window is the cursor's, nearest the list's top.
        const std::size_t reserve =
            budget >= 4 ? (block.size() < budget - 3 ? block.size() : budget - 3) : 0;
        const std::size_t list_rows = budget - reserve;
        ListWindow win = zengine::component::cursor_window(shown.size(), cursor, 0, list_rows);
        if (list_rows < 3 && shown.size() > list_rows) {
            win = ListWindow{};
            win.after = shown.size();
            win.markers = 1;
        }
        if (win.before > 0) {
            push("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
        }
        for (std::size_t i = win.first; i < win.end(); ++i) {
            const StandingCondition& c = shown[i];
            const bool here = i == cursor;
            push(std::string(here ? "> " : "  ") + c.compact, c.role, c.key);
            if (!here || reserve == 0) {
                continue;
            }
            const std::size_t said = block.size() > reserve ? reserve - 1 : block.size();
            for (std::size_t line = 0; line < said; ++line) {
                push("    " + block[line], surface::role::kMuted);
            }
            if (said < block.size()) {
                push("    " + omitted_text(block.size() - said, "more"), surface::role::kMuted);
            }
        }
        if (win.after > 0) {
            push("  " + omitted_text(win.after, "more"), surface::role::kMuted);
        }
    }

    // ---- State not in the shape ----------------------------------------------------------

    /// THE HOST'S LAST READING, held between publications and owned by nobody here. Replaced
    /// whole; never merged; never persisted; deliberately NOT in the state shape, because a
    /// reloaded image that carried it would present a picture derived before it existed.
    std::vector<StandingCondition> known_;
    bool heard_ = false;

    std::size_t cursor_ = 0;
    std::string notice_;

    /// WHETHER THE CURSOR'S CONDITION IS EXPLAINED BENEATH IT: the layout's `detail` setting,
    /// handed again after a reload, so never reload state.
    bool detail_shown_ = true;
};

} // namespace

ZEN_EXPORT_WEAVE(AttentionPaneWeave)
