// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPOSER_VIEW_HPP
#define ZENGINE_COMPOSER_VIEW_HPP

// What the Compose pane shows, and what each row of it means: one value, built once, by the
// function that draws it. A row and its meaning are one `RenderedRow`, appended together, so a
// row without a meaning is unsayable; provider-local, not a Surface shape or a component.
// Pane law: agents/panes.md

// One measurer, twice over: the geometry that draws a thing and the geometry that hits it are
// one function -- the row map `project` builds is what `meaning_at_row` reads, and
// `value_capacity` is asked by both the projector and the caret-window reconciliation. It
// measures nothing itself (`surface::fit_region` did, on Workshop's side), holds no state and
// knows no bus: a test can hand it a draft and a budget and ask what a maker would see.

#include "composer/draft.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kind.hpp>
#include <zen/schema.hpp>
#include <zen/terminal/composer.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::composer {

/// WHERE THIS PANE IS IN THE ONE CONVERSATION IT HAS. Five states, and every one
/// of them is a fact about what this pane has OBSERVED rather than a phase of a
/// workflow somebody designed.
namespace stage {
/// Nothing has been selected. The pane has no target and says how to get one.
inline constexpr std::int64_t kNoTarget = 0;
/// A library was selected and the kernel had bound it NO ROLE. That is an observed
/// absence, not a missing reading, and there is nothing to address.
inline constexpr std::int64_t kNoRole = 1;
/// A discovery request was SUBMITTED and no answer has been observed. Not
/// "loading": nothing promises that an answer will arrive.
inline constexpr std::int64_t kAsking = 2;
/// An answer arrived and was decoded. These are the target's accepted roots.
inline constexpr std::int64_t kCatalog = 3;
/// One root was chosen and a draft is open on it.
inline constexpr std::int64_t kForm = 4;
} // namespace stage

/// WHAT A ROW OF THIS PANE NAMES.
///
/// `kNothing` is NEGATIVE for `role::kNone`'s and `kNoEntry`'s reason: every other
/// meaning is a non-negative index into something, so an absence cannot collide
/// with a row anybody meant, and a consumer that forgot to test would fall out of
/// range rather than into the first item.
namespace meaning {
inline constexpr std::int64_t kNothing = -1; ///< a heading, a note, an omission marker, a blank
inline constexpr std::int64_t kMessage = 0;  ///< an accepted root; `which` indexes the roots
inline constexpr std::int64_t kField = 1;    ///< a draft field; `which` indexes the schema's fields
inline constexpr std::int64_t kSubmit = 2;   ///< the control that composes, assembles and sends
inline constexpr std::int64_t kBack = 3;     ///< the control that returns to the catalog
} // namespace meaning

struct RowMeaning {
    std::int64_t what = meaning::kNothing;
    std::int64_t which = 0;
};

struct RenderedRow {
    surface::SurfaceTextRow row;
    RowMeaning meaning;
};

/// THE PANE, AS ROWS THAT KNOW WHAT THEY ARE. Bounded by the granted room, replaced
/// whole on every projection, and retained by the provider for exactly one reason:
/// to answer which item a press named.
struct ComposerView {
    std::vector<RenderedRow> rows;
};

/// THE MARK AT THE HEAD OF EVERY SELECTABLE ROW, and the two characters cost no
/// columns: an unselected row is already indented by two, so selecting one exchanges
/// two spaces for a mark and a space and no row changes width. `>` is the statement
/// and the accent role is the second signal, never the other way round -- a terminal
/// with no colour must still be able to say "this one".
inline constexpr const char* kSelectedMark = "> ";
inline constexpr const char* kUnselectedMark = "  ";

/// The mark this view leaves where it could not show everything -- three plain
/// characters, because this canvas is plain ASCII by contract.
inline constexpr const char* kElided = "...";

/// What a caret looks like in a provider's row: `surface::kCaretGlyph`, the character a cell
/// medium inserts for a region's caret, so typing here looks like typing into the Terminal. A
/// character, not a region caret, because `PaneContent` carries rows and no caret; it costs one
/// column of the value's room, since a caret sits between characters and after the last one.
inline constexpr char kCaret = surface::kCaretGlyph;

/// Fit `text` into `columns`, and say so when it did not fit. A copy, as introspection's is: a
/// provider is a stranger to Workshop's own composition. `workshop/pane_text.hpp` now shares one
/// between the pane weaves, and this pane has not moved to it. Not a second measurer: the
/// measuring was `surface::fit_region`'s, and this spends the number.
inline std::string fit(std::string text, std::int64_t columns) {
    if (columns <= 0) {
        return {};
    }
    // PaneContent is printable ASCII. Preserve the draft bytes; substitute only in
    // this preview so copied UTF-8 or control bytes cannot silence the whole pane.
    for (char& c : text) if (static_cast<unsigned char>(c) < 32 ||
                            static_cast<unsigned char>(c) > 126) c = '?';
    const std::size_t room = static_cast<std::size_t>(columns);
    if (text.size() <= room) {
        return text;
    }
    const std::size_t mark = std::char_traits<char>::length(kElided);
    if (room <= mark) {
        return std::string(kElided).substr(0, room);
    }
    text.resize(room - mark);
    text += kElided;
    return text;
}

/// Which run of a list a budget can show while keeping the focus visible, and how much is
/// hidden each side: `list_window`'s rules (a population that fits is shown whole, the focus is
/// always inside, every omission is counted and spends a row), with one omission row naming both
/// sides, since two rows of small print are a quarter of an eight-row pane. A one-row budget
/// shows the marker and no items. Total over every budget and focus.
struct Window {
    std::int64_t first = 0;  ///< the first item shown
    std::int64_t count = 0;  ///< how many are shown
    std::int64_t before = 0; ///< how many are hidden above the window
    std::int64_t after = 0;  ///< ...and below it
};

inline Window window_of(std::int64_t population, std::int64_t focus, std::int64_t rows) {
    Window w;
    if (population <= 0) {
        return w;
    }
    if (rows <= 0) {
        // THE ACCOUNTING IS TOTAL EVEN WHERE NOBODY CAN SPEND IT. A zero-row budget
        // hides the whole population, and this says so rather than answering with
        // three zeros that add up to nothing -- the caller has no row to write the
        // marker in, which is a fact about the caller and not about the arithmetic.
        // Answering `{0,0,0,0}` here would make "shown plus hidden is the population"
        // false for exactly the case where the most is hidden.
        w.after = population;
        return w;
    }
    if (population <= rows) {
        w.count = population;
        return w;
    }
    if (rows == 1) {
        w.after = population; // the marker, and no room for an item beside it
        return w;
    }
    w.count = rows - 1; // one row of the budget is the omission marker's
    if (focus < 0) {
        focus = 0;
    }
    if (focus >= population) {
        focus = population - 1;
    }
    w.first = focus - w.count / 2;
    if (w.first < 0) {
        w.first = 0;
    }
    if (w.first > population - w.count) {
        w.first = population - w.count;
    }
    w.before = w.first;
    w.after = population - w.first - w.count;
    return w;
}

/// The one sentence an omission is ever said in. Both sides, in one row.
inline std::string omission_text(const Window& w) {
    if (w.before > 0 && w.after > 0) {
        return "  " + std::string(kElided) + " " + std::to_string(w.before) + " above, " +
               std::to_string(w.after) + " below";
    }
    if (w.before > 0) {
        return "  " + std::string(kElided) + " " + std::to_string(w.before) + " more above";
    }
    return "  " + std::string(kElided) + " " + std::to_string(w.after) + " more below";
}

/// HOW MANY ITEMS A FORM'S CURSOR WALKS: every declared field, then Submit, then
/// Back. The two controls are items of the same ring so a keyboard reaches them
/// without a second gesture, and they are PLACED separately (see `project`) so a
/// pointer always finds them in the same place.
inline std::int64_t submit_index(const MessageDraft& d) noexcept {
    return static_cast<std::int64_t>(d.size());
}
inline std::int64_t back_index(const MessageDraft& d) noexcept {
    return static_cast<std::int64_t>(d.size()) + 1;
}
inline std::int64_t form_items(const MessageDraft& d) noexcept { return back_index(d) + 1; }

/// The columns a field's value gets, and there is exactly one answer: the row is `mark + name +
/// ":" + type + "  " + [ value ]`, so the value's room is what the fixed part left, less two
/// brackets and the caret's column, floored at zero. The projector and the window
/// reconciliation both call it, so `keep_caret_visible` gets the number the painter cuts with.
inline std::int64_t value_capacity(const MessageDraft& draft, std::size_t which,
                                   std::int64_t columns) {
    if (!draft.valid() || which >= draft.size()) {
        return 0;
    }
    const loom::Field& f = draft.field(which);
    const std::int64_t fixed =
        static_cast<std::int64_t>(2 + f.name.size() + 1 + draft.type_of(which).size() + 2 + 2 + 1);
    return columns > fixed ? columns - fixed : 0;
}

/// One field row: its name, its declared type (the schema's own spelling, the only thing the
/// row says about meaning), and what the maker authored, presence kept apart from value --
/// `[hello_]` present (caret while editing), `[]` present and empty, `(required)` absent and
/// needed, `(absent)` absent, `[false]` a chosen false, `(not composable in this version)`.
inline std::string field_row_text(const MessageDraft& draft, std::size_t which, bool chosen,
                                  bool editing, std::int64_t columns) {
    const loom::Field& f = draft.field(which);
    std::string row =
        std::string(chosen ? kSelectedMark : kUnselectedMark) + f.name + ":" + draft.type_of(which);
    const FieldDraft& d = draft.fields[which];
    switch (composability(f.type.kind)) {
    case Composability::kNotFlat:
        return fit(row + (d.present && d.typed ? "  [copied message/list]" : "  (drop compatible value)"), columns);
    case Composability::kNoSpelling:
        return fit(row + (d.present && d.typed ? "  [copied bytes]" : "  (no text form; drop complete message)"), columns);
    case Composability::kScalar:
        break;
    }
    if (!d.present) {
        return fit(row + (f.required ? "  (required)" : "  (absent)"), columns);
    }
    const std::int64_t room = value_capacity(draft, which, columns);
    // A resting value is fitted and a live one is windowed: `fit` marks what it cut, since a
    // committed value has no caret to say it moved; `TextBox::visible` does not, because the
    // edited value has one.
    if (!editing) {
        return fit(row + "  [" + fit(d.value.text(), room) + "]", columns);
    }
    std::string shown = d.value.visible(room);
    const std::size_t at = d.value.caret_column();
    shown.insert(at <= shown.size() ? at : shown.size(), 1, kCaret);
    return fit(row + "  [" + shown + "]", columns);
}

/// What this pane is looking at and what the maker has done to it: the provider's whole
/// presentation state, read by the projector and written by nothing else. No inventory and no
/// second copy: `library` and `role` are what a `LoadedSelected` said, `snapshot` is one
/// target's decoded vocabulary replaced whole, and `draft` is the maker's own work.
struct Composing {
    std::int64_t stage = stage::kNoTarget;
    std::string library; ///< the loaded-library name the maker pressed -- DIAGNOSTIC identity
    std::string role;    ///< the office a message would be addressed to -- MESSAGING identity
    Snapshot snapshot;
    MessageDraft draft;
    std::int64_t cursor = 0;   ///< into the roots in the catalog, into `form_items` in the form
    std::string notice;        ///< the last thing this pane has to say about what it did
    std::int64_t notice_role = surface::role::kMuted;
};

/// WHICH ITEM A ROW OF THIS VIEW NAMES, or nothing.
///
/// TOTAL over every row index, including ones no press can produce: a row outside the
/// view means nothing, exactly as a heading does. A provider is handed a row off a
/// wire and must not have to bound it twice.
inline RowMeaning meaning_at_row(const ComposerView& view, std::int64_t row) {
    if (row < 0 || row >= static_cast<std::int64_t>(view.rows.size())) {
        return RowMeaning{};
    }
    return view.rows[static_cast<std::size_t>(row)].meaning;
}

/// THE ROWS, for `PaneContent`. The meanings stay here.
inline std::vector<surface::SurfaceTextRow> rows_of(const ComposerView& view) {
    std::vector<surface::SurfaceTextRow> out;
    out.reserve(view.rows.size());
    for (const RenderedRow& r : view.rows) {
        out.push_back(r.row);
    }
    return out;
}

/// The target, in the pane's own words, with two identities kept apart: `to @zengine.timer` is
/// the messaging address, resolved at delivery (whoever holds the office then; no `WeaveId` is
/// pinned), and `from zengine-timer` is the diagnostic identity -- the library the kernel
/// loaded, which addresses nothing.
inline std::string target_row_text(const Composing& c, std::int64_t columns) {
    return fit("to @" + c.role, columns);
}
inline std::string library_row_text(const Composing& c, std::int64_t columns) {
    return fit("from " + c.library, columns);
}

namespace detail {

/// One row and its meaning, appended together. The closure exists so that a row with
/// no meaning is not a thing this file can produce by forgetting a line.
struct Sayer {
    ComposerView& view;
    void operator()(std::string text, std::int64_t role, RowMeaning meaning = RowMeaning{},
                    std::int64_t ground = surface::role::kNone) const {
        view.rows.push_back(RenderedRow{
            surface::SurfaceTextRow{std::move(text), role, ground}, meaning});
    }
};

} // namespace detail

/// The whole view, spent against the room Workshop granted, most-protected first: the target
/// line, the notice (a refusal a maker cannot see is worse than a list they can scroll), the
/// heading stating the population, the list or form windowed with every omission counted, and
/// the library line out of genuine slack. The form's two controls are a fixed demand, subtracted
/// first and anchored to the foot, so they never move under the hand. Exactly inside the grant
/// (Workshop refuses an over-budget update whole); zero rows or columns is an empty projection.
inline ComposerView project(const Composing& c, std::int64_t rows, std::int64_t columns) {
    ComposerView view;
    const detail::Sayer say{view};
    if (rows <= 0 || columns <= 0) {
        return view;
    }
    std::int64_t left = rows;

    // ---- the target line, always -------------------------------------------
    if (c.stage == stage::kNoTarget) {
        say(fit("no weave selected", columns), surface::role::kMuted);
        --left;
        if (left > 0) {
            say(fit("press a row in the Loaded pane", columns), surface::role::kMuted);
            --left;
        }
        return view;
    }
    if (c.stage == stage::kNoRole) {
        say(fit("selected " + c.library, columns), surface::role::kFill);
        --left;
        if (left > 0) {
            // AN OBSERVED ABSENCE, AND NOT A GUESS. The kernel binds a role at load
            // only when one was named, so an empty role is the kernel saying this
            // library holds none. Nothing here manufactures a WeaveId, addresses the
            // library by name, or sweeps for a participant that might answer.
            say(fit("(no messaging role observed -- nothing to address)", columns),
                surface::role::kMuted);
            --left;
        }
        return view;
    }
    say(target_row_text(c, columns), surface::role::kAccent);
    --left;

    // ---- the notice, reserved before anything variable ---------------------
    const std::int64_t notice = !c.notice.empty() && left >= 2 ? 1 : 0;
    left -= notice;

    if (c.stage == stage::kAsking) {
        if (left > 0) {
            // Transport refusal is observed separately. A delivered request still
            // need not receive an application answer; elapsed time invents no outcome.
            say(fit("asked what it accepts -- no answer observed yet", columns),
                surface::role::kMuted);
            --left;
        }
    } else if (c.stage == stage::kCatalog) {
        const std::int64_t population = static_cast<std::int64_t>(c.snapshot.roots.size());
        if (left > 0) {
            say(fit("accepted messages -- " + std::to_string(population), columns),
                surface::role::kAccent);
            --left;
        }
        const Window w = window_of(population, c.cursor, left);
        for (std::int64_t i = w.first; i < w.first + w.count; ++i) {
            const auto& root = c.snapshot.roots[static_cast<std::size_t>(i)];
            const bool chosen = i == c.cursor;
            // (name, version) AND NEVER A MERGED NAME. `Foo v1` and `Foo v2` are two
            // message identities and this pane draws no conclusion about either from
            // the other -- no compatibility, no supersession, no "latest".
            say(fit(std::string(chosen ? kSelectedMark : kUnselectedMark) + root->name() + " v" +
                        std::to_string(root->version()),
                    columns),
                chosen ? surface::role::kAccent : surface::role::kFill,
                RowMeaning{meaning::kMessage, i},
                chosen ? surface::role::kMuted : surface::role::kNone);
        }
        left -= w.count;
        // THE MARKER IS A ROW OF THE BUDGET, so a budget with no rows left has no
        // marker -- `window_of` accounts for the whole population whether or not
        // anybody can spend the accounting, and this is where that is spent. What
        // says the list is incomplete at that size is the HEADING, which stated the
        // population before the list was offered anything.
        if (left > 0 && (w.before > 0 || w.after > 0)) {
            say(fit(omission_text(w), columns), surface::role::kMuted);
            --left;
        }
    } else if (c.stage == stage::kForm && c.draft.valid()) {
        if (left > 0) {
            say(fit(c.draft.schema->name() + " v" + std::to_string(c.draft.schema->version()) +
                        " -> @" + c.role,
                    columns),
                surface::role::kAccent);
            --left;
        }
        // The two controls are a fixed demand, subtracted before the fields are offered
        // anything; `controls` is 2 or 0, since half a footer would show Submit with no way back.
        const std::int64_t controls = left >= 3 ? 2 : 0;
        const std::int64_t field_rows = left - controls;
        const std::int64_t population = static_cast<std::int64_t>(c.draft.size());
        const Window w = window_of(population, c.cursor, field_rows);
        for (std::int64_t i = w.first; i < w.first + w.count; ++i) {
            const std::size_t which = static_cast<std::size_t>(i);
            const bool chosen = i == c.cursor;
            const loom::Field& f = c.draft.field(which);
            const FieldDraft& d = c.draft.fields[which];
            const bool editing = chosen && d.present && typeable(f.type.kind);
            std::int64_t role = surface::role::kFill;
            if (chosen) {
                role = surface::role::kAccent;
            } else if (composability(f.type.kind) != Composability::kScalar) {
                role = surface::role::kMuted;
            } else if (!d.present && f.required) {
                // A REQUIRED FIELD NOBODY HAS AUTHORED IS SOMETHING THE MAKER MUST
                // SEE -- it is the one thing standing between this draft and a send.
                role = surface::role::kAlert;
            }
            say(field_row_text(c.draft, which, chosen, editing, columns), role,
                RowMeaning{meaning::kField, i},
                chosen ? surface::role::kMuted : surface::role::kNone);
        }
        left -= w.count;
        if (left > 0 && (w.before > 0 || w.after > 0)) {
            say(fit(omission_text(w), columns), surface::role::kMuted);
            --left;
        }
        if (controls > 0) {
            // Anchored to the foot, so spare room falls between the fields and the controls,
            // and the two targets do not move as a maker scrolls.
            while (left > controls) {
                say(std::string(), surface::role::kFill);
                --left;
            }
            const bool on_submit = c.cursor == submit_index(c.draft);
            const bool on_back = c.cursor == back_index(c.draft);
            // `[ ... ]` is this tool's word for a pressable thing, and the muted ground a second
            // signal, never the only one: a terminal has no ground to tint.
            say(fit(std::string(on_submit ? kSelectedMark : kUnselectedMark) + "[ Submit ]",
                    columns),
                on_submit ? surface::role::kAccent : surface::role::kFill,
                RowMeaning{meaning::kSubmit, 0}, surface::role::kMuted);
            say(fit(std::string(on_back ? kSelectedMark : kUnselectedMark) + "[ Back ]", columns),
                on_back ? surface::role::kAccent : surface::role::kFill,
                RowMeaning{meaning::kBack, 0}, surface::role::kMuted);
            left -= controls;
        }
    }

    if (notice > 0) {
        say(fit(c.notice, columns), c.notice_role);
    }
    // ...and the library line only out of GENUINE slack: it is the DIAGNOSTIC
    // identity, useful and never necessary, so it takes a row nothing else wanted.
    if (left > 0 && !c.library.empty()) {
        say(library_row_text(c, columns), surface::role::kMuted);
    }
    return view;
}

} // namespace zengine::composer

#endif // ZENGINE_COMPOSER_VIEW_HPP
