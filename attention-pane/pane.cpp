// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Attention pane -- a loadable weave that offers Workshop one pane: what is true right
// now and worth a maker's attention.
//
// IT USED TO BE CHROME INSIDE THE HOST (`workshop/attention.hpp`'s `AttentionView`,
// `screen_attention.cpp`'s `paint_attention`, a `KeyContext` of its own with four rows, and
// a global chord that opened it from anywhere). It is an ordinary pane now, opened from the
// picker, arranged on the desk, and holding the keyboard only while a maker has pressed
// into it.
//
// ⚠ WHAT IT SHOWS IT DOES NOT DERIVE, AND CANNOT. Every row of this pane is the HOST's
// reading of the host's own state: two files it read at boot and could not use, a pane that
// refused an update, a pane the maker authored that no cell of the screen is showing, and
// the row realization is stopped at. None of it is a fact this image could go and get, and a
// condition "disappears because it resolved, never because something else was said"
// (WL-ATTN-01) -- so there is nothing to poll for either. The host SAYS it, whenever it
// changes, and that publication is the one new sentence this whole arc adds
// (`workshop/attention_seam_vocabulary.hpp`).
//
// ⚠ AND WHAT THIS PANE OWNS IS EXACTLY ONE THING: which statements the maker has hidden.
// Everything else on the screen is somebody else's fact, held for as long as the last
// publication, replaced whole by the next one. A pane that kept a copy would be a second
// owner of the truth, which is the defect the built-in's own projection was written to
// avoid.
//
// THE COMPOSITION DID NOT MOVE ITS MEANING. The header, the empty answer, the reserved
// explanation block, the window's floor of three and the two omission markers are
// `paint_attention`'s, carried here verbatim: what changed is that the rows are SAID as
// values into a room this pane is granted, instead of being written into a popup this pane
// resolved for itself.

#include "attention-pane/vocabulary.hpp"

#include "workshop/attention_seam_vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace pane = zengine::attention_pane;

using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneOffered;
using ws::PaneRoom;
using ws::StandingCondition;
using ws::StandingConditions;

/// WHO THIS PANE IS TALKING TO. The host's office, spelled as a literal exactly as
/// `files.cpp` and `builder-pane/pane.cpp` spell it: a provider is a stranger to Workshop's
/// internals and says who it is talking to the way a third party would.
constexpr const char* kWorkshopRole = "zengine.workshop";

// ---- The text helpers the composition spends ------------------------------------------
//
// `detail::fit`, `detail::wrap`, `omitted_text` and `list_window` are Workshop's own
// (`screen_bindings.cpp`, `screen_gestures.cpp`) and they live behind `screen.hpp`, which is
// the host's presentation and not a header a loaded image may include. They are carried here
// byte-for-byte rather than approximated, because the composition below is a MOVE: a view
// that cut its rows one character differently after the migration would be a view a maker
// could see had changed, for no reason they were told about.
//
// ⚠ THIS IS THE THIRD COPY OF `fit` IN A PANE PACKAGE, and it is named as a seam rather than
// pretended away. `files/files.cpp` carries one, `builder-pane/pane.cpp` carries `fit`,
// `wrap` and `pad`, and this file carries `fit`, `wrap` and two more. A header that owns no
// lifecycle and hides no bus would hold all of them honestly -- and it is a fourth change in
// a migration that already has three, so it is left open with the count written down.

constexpr const char* kElided = "...";
constexpr std::int64_t kWrapIndent = 2;

std::string fit(std::string text, std::int64_t width) {
    if (width <= 0) {
        return {};
    }
    const std::size_t room = static_cast<std::size_t>(width);
    if (text.size() <= room) {
        return text; // it fits, so nothing about it changes -- not even its role
    }
    const std::size_t mark = std::char_traits<char>::length(kElided);
    if (room <= mark) {
        return std::string(kElided).substr(0, room);
    }
    text.resize(room - mark);
    text += kElided;
    return text;
}

std::vector<std::string> wrap(const std::string& text, std::int64_t width) {
    std::vector<std::string> rows;
    if (width <= 0) {
        return rows;
    }
    const std::size_t room = static_cast<std::size_t>(width);
    const std::size_t indent =
        width > kWrapIndent + 1 ? static_cast<std::size_t>(kWrapIndent) : 0;
    std::size_t at = 0;
    while (true) {
        const std::string lead(rows.empty() ? 0 : indent, ' ');
        const std::size_t take = room - lead.size();
        if (text.size() - at <= take) {
            rows.push_back(lead + text.substr(at));
            return rows;
        }
        std::size_t cut = at + take;
        bool broke = false;
        for (std::size_t i = cut; i > at; --i) {
            if (text[i] == ' ') {
                cut = i;
                broke = true;
                break;
            }
        }
        rows.push_back(lead + text.substr(at, cut - at));
        at = cut;
        if (broke) {
            ++at; // the space that broke the line is spent by the break
        }
        if (at >= text.size()) {
            return rows;
        }
    }
}

std::string omitted_text(std::size_t how_many, const char* which) {
    return "... " + std::to_string(how_many) + " " + which;
}

/// WHAT A WINDOW OVER A LIST LOOKS LIKE -- `screen.hpp`'s `ListWindow`, carried.
struct ListWindow {
    std::size_t first = 0;
    std::size_t count = 0;
    std::size_t before = 0;
    std::size_t after = 0;
};

/// THE THREE RULES, CARRIED: everything fits or nothing is hidden quietly; the selection is
/// always inside the window; a wall that exists is said. `screen_gestures.cpp`'s body.
ListWindow list_window(std::size_t total, std::size_t selected_at, std::size_t rows) {
    ListWindow w;
    if (total == 0 || rows == 0) {
        w.after = total; // no room at all: everything there is, is missing
        return w;
    }
    if (total <= rows) {
        w.count = total;
        return w;
    }
    if (rows < 3) {
        // Too few lines to seat one row between two markers, so it spends what it has on
        // the omission: the one thing this view may not do is drop conditions quietly.
        w.after = total;
        return w;
    }
    if (selected_at >= total) {
        selected_at = 0;
    }
    const std::size_t one_marker = rows - 1;
    if (selected_at < one_marker) {
        w.count = one_marker;
        w.after = total - w.count;
        return w;
    }
    const std::size_t tail = total - one_marker;
    if (selected_at >= tail) {
        w.first = tail;
        w.count = one_marker;
        w.before = tail;
        return w;
    }
    w.count = rows - 2;
    w.first = selected_at + 1 - w.count;
    w.before = w.first;
    w.after = total - w.first - w.count;
    return w;
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

class AttentionPaneWeave
    : public loom::WeaveBase<
          AttentionPaneWeave, pane::AttentionPaneState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PaneActionRequested,
                       StandingConditions>,
          loom::Emit<PaneOffered, PaneActions, PaneContent>> {
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

    /// WORKSHOP GRANTS THE PANE ITS ROOM -- the one beat on which this view draws.
    ///
    /// ⚠ AND IT ASKS FOR NOTHING HERE, WHICH IS THIS PANE ALONE AMONG THE THREE. The Files
    /// browser asks where this run began and the Builder asks what the tool is; both have a
    /// question whose answer is stable enough to be worth re-asking at a grant. This pane
    /// has no question at all: what it shows arrives when it changes and nowhere else, so a
    /// pane granted a room before the host has said anything shows the honest "waiting"
    /// state rather than an empty list that would read as "nothing is wrong".
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kAttentionPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        say(mail);
    }

    /// ONE OF THE PANE'S DECLARED ACTIONS, ASKED FOR BY NAME (WL-KEY-15). Workshop resolved
    /// the keystroke against the effective keymap -- the maker's override where one is
    /// authored, this office's declared default otherwise -- so what arrives is the id.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != pane::kAttentionPane) {
            return;
        }
        notice_.clear(); // the maker has acted; the last act's answer is spent
        const std::vector<StandingCondition> shown = visible();
        if (cursor_ >= shown.size()) {
            cursor_ = shown.empty() ? 0 : shown.size() - 1;
        }
        if (asked.id == pane::kActionUp) {
            if (cursor_ > 0) {
                --cursor_;
            }
        } else if (asked.id == pane::kActionDown) {
            if (cursor_ + 1 < shown.size()) {
                ++cursor_;
            }
        } else if (asked.id == pane::kActionDismiss) {
            if (cursor_ < shown.size()) {
                // AN EVENT, SAID AS ONE. What just happened is that a maker hid a
                // presentation; what remains true is the condition, which is why the
                // sentence is about the gesture and not about the subject. The built-in
                // said this on Workshop's notice row; a pane has only its own room, so it
                // leads with it and it stands until the maker's next act
                // (`agents/panes.md`).
                notice_ = "hidden -- " + shown[cursor_].compact + " is still true";
                dismiss(shown[cursor_]);
            }
        } else {
            return;
        }
        say(mail);
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
        // the maker's decision not to look at the refusal with it, and if the same key ever
        // comes back it comes back visible.
        forget_resolved();
        const std::size_t total = visible().size();
        if (cursor_ >= total) {
            cursor_ = total == 0 ? 0 : total - 1;
        }
        say(mail);
    }

private:
    // ---- Offering and declaring ---------------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kAttentionPaneRole)
            .send_to_role(kWorkshopRole,
                          PaneOffered{pane::kAttentionPane, pane::kAttentionPaneName,
                                      pane::kAttentionPaneSummary});
        declare(mail);
    }

    /// WHAT THIS PANE ANSWERS TO -- three rows, in every state it has.
    ///
    /// THE IDS AND THE DEFAULTS ARE THE BUILT-IN'S, unchanged: Up, Down and `d`, spelled
    /// `attention.up`, `attention.down` and `attention.dismiss`. A maker who moved one in
    /// their keymap file finds it moved here, which is the whole promise of the migration.
    ///
    /// ⚠ AND THERE IS NO MODE, SO THERE IS ONE DECLARATION. Files and the Builder re-declare
    /// per mode because each has a line a maker types into; this view has no text and no
    /// second state, so its rows are the same rows always. A bare letter is legal for the
    /// reason it was legal in the built-in's own context: nothing in this pane takes text.
    void declare(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = pane::kAttentionPane;
        actions.rows.push_back(
            PaneActionRow{pane::kActionUp, "row up", input::scan::kUp, input::mod::kNone});
        actions.rows.push_back(PaneActionRow{pane::kActionDown, "row down", input::scan::kDown,
                                             input::mod::kNone});
        actions.rows.push_back(PaneActionRow{pane::kActionDismiss, "hide this one",
                                             input::scan::kD, input::mod::kNone});
        (void)mail.as_role(pane::kAttentionPaneRole).send_to_role(kWorkshopRole, actions);
    }

    // ---- The maker's own half: what they have chosen not to look at ----------------------

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
    /// old stamp rather than adding a row, so the set stays one entry per key and a maker
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

    /// DROP EVERY DISMISSAL WHOSE CONDITION IS NO LONGER TRUE. The built-in kept its
    /// dismissals for the life of the session because they cost nothing to keep and the set
    /// was rebuilt from a host reading every paint; here the set is the pane's own state and
    /// crosses a reload, so a dismissal that outlived its subject would be a decision a
    /// maker made about a fact that no longer exists, silently re-applied if it ever came
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
    /// this maker has hidden. `attention_shown`'s job, on the pane's side of the seam.
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

    void say(loom::Mail& mail) {
        if (!granted_ || rows_ <= 0 || columns_ <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        const auto push = [&out, this](const std::string& text, std::int64_t role) {
            out.push_back(surface::SurfaceTextRow{fit(text, columns_), role});
        };
        say_view(push);
        // A notice, when there is one, leads -- the built-in wrote it on the band; a pane has
        // only its own room, so its first row carries it, and it is cleared by the maker's
        // next act rather than by being said (`agents/panes.md`).
        if (!notice_.empty() && rows_ > 1) {
            if (static_cast<std::int64_t>(out.size()) > rows_ - 1) {
                out.resize(static_cast<std::size_t>(rows_ - 1));
            }
            out.insert(out.begin(),
                       surface::SurfaceTextRow{fit(notice_, columns_), surface::role::kAccent});
        }
        if (static_cast<std::int64_t>(out.size()) > rows_) {
            out.resize(static_cast<std::size_t>(rows_));
        }
        (void)mail.as_role(pane::kAttentionPaneRole)
            .send_to_role(kWorkshopRole, PaneContent{pane::kAttentionPane, std::move(out)});
    }

    /// HOW MANY ROWS THE LIST MAY SPEND: the room, less this pane's own header, less the
    /// notice row `say` puts in front of it.
    std::int64_t body_budget() const {
        return rows_ - 1 - (notice_.empty() ? 0 : 1);
    }

    template <class Push>
    void say_view(Push&& push) {
        // ⚠ THE HEADER NO LONGER SPELLS ITS OWN KEYS, and that is a subtraction rather than
        // a loss. The built-in wrote `d hides one, Escape closes` into its own first row
        // because a maker had no other way to learn a mode's gestures; a pane's declared
        // rows are in the band's legend and in the hotkey view under this pane's own
        // heading, resolved through the maker's effective keymap. Saying it twice would put
        // this pane in the business of reading a keymap it cannot see.
        if (!heard_) {
            // THE HOST HAS NOT SAID ANYTHING YET, WHICH IS NOT THE SAME AS NOTHING BEING
            // WRONG. A pane opened before the first publication has no reading at all, and
            // an empty list here would read as an all-clear this pane has no grounds for.
            push("ATTENTION (waiting)", surface::role::kMuted);
            return;
        }
        const std::vector<StandingCondition> shown = visible();
        push("ATTENTION -- " + std::to_string(shown.size()) +
                 (shown.size() == 1 ? " condition" : " conditions"),
             surface::role::kAccent);
        const std::int64_t budget_rows = body_budget();
        if (budget_rows <= 0) {
            return;
        }
        const std::size_t budget = static_cast<std::size_t>(budget_rows);
        if (shown.empty()) {
            // NOTHING IS WRONG, SAID IN WORDS. A maker who put this pane on their desk is
            // owed an answer, and an empty box is not one.
            push("  nothing needs your attention right now", surface::role::kMuted);
            return;
        }
        // THE CURSOR'S OWN BLOCK IS COMPOSED AND RESERVED BEFORE THE LIST IS WINDOWED, and
        // that ordering is the whole of this composition's honesty. A window computed over
        // the compact rows alone is right until the row it is keeping in view spends three
        // more beneath it -- and what then falls off the bottom is the omission marker,
        // which is the one row that was there to say something had been dropped. A bound
        // that grows when it is exceeded is not a bound.
        //
        // THE CURSOR IS RESOLVED ONCE, HERE, and every question below spends the same
        // answer. The population is the host's and can shrink between a keystroke and the
        // next publication, so a composition that clamped in one place and compared the raw
        // value in another would explain a row it then never marks.
        const std::size_t cursor = cursor_ < shown.size() ? cursor_ : shown.size() - 1;
        const StandingCondition& at = shown[cursor];
        std::vector<std::string> block = wrap(at.detail, columns_ - kWrapIndent);
        if (!at.suggestion.empty()) {
            // WHAT A MAKER COULD PRESS SOMEWHERE ELSE, in the host's own words. Nothing here
            // can press it, which is WL-ATTN-10 unchanged: the row is a sentence, and this
            // pane could not resolve an action id even if it were given one.
            block.push_back(at.suggestion);
        }
        // THE LIST KEEPS `list_window`'S OWN FLOOR OF THREE, so the cursor's row is always
        // in the window it is being explained inside of. Below four rows there is no
        // explanation at all rather than an explanation with no statement over it.
        const std::size_t reserve =
            budget >= 4 ? (block.size() < budget - 3 ? block.size() : budget - 3) : 0;
        const ListWindow win = list_window(shown.size(), cursor, budget - reserve);
        if (win.before > 0) {
            push("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
        }
        for (std::size_t i = win.first; i < win.first + win.count; ++i) {
            const StandingCondition& c = shown[i];
            const bool here = i == cursor;
            push(std::string(here ? "> " : "  ") + c.compact, c.role);
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

    zengine::ActivationCursor activation_;

    /// THE HOST'S LAST READING, held between publications and owned by nobody here. Replaced
    /// whole; never merged; never persisted; deliberately NOT in the state shape, because a
    /// reloaded image that carried it would present a picture derived before it existed.
    std::vector<StandingCondition> known_;
    bool heard_ = false;

    std::size_t cursor_ = 0;
    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
    std::string notice_;
};

} // namespace

ZEN_EXPORT_WEAVE(AttentionPaneWeave)
