// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPOSER_VIEW_HPP
#define ZENGINE_COMPOSER_VIEW_HPP

// WHAT THE COMPOSE PANE SHOWS, AND WHAT EACH ROW OF IT MEANS -- one value, built
// once, by the function that draws it.
//
// ---- A ROW AND ITS MEANING ARE ONE THING HERE (MSG-0, on SEL-0's advice) -----
//
// `project_loaded` returns rows and an `entry_of_row` vector parallel to them, and
// SEL-0 recorded the parallelism as a refit risk that had not yet cost anything:
// that list has ONE interactive row kind, so the two vectors are easy to keep in
// step. This pane has FOUR -- a message, a field, Submit, Back -- and two whole
// layouts to keep them in step across. So the pair is a single type:
//
//     RenderedRow { SurfaceTextRow row; RowMeaning meaning; }
//
// One `push_back` per row, carrying both halves, which makes "a row without a
// meaning" and "a meaning without a row" unsayable rather than merely unlikely.
// It is PROVIDER-LOCAL and deliberately not a Surface shape, not a component and
// not a framework: nothing crosses a wire, nothing is granted, and the pane
// beside this one is free to keep answering presses its own way.
//
// ---- ONE MEASURER, TWICE OVER ------------------------------------------------
//
// HD-3's rule is that the geometry that draws a thing and the geometry that hits
// it must be one function, and this file obeys it in two places rather than one:
//
//   the ROW MAP     `project` builds it, `meaning_at_row` reads it, and nothing
//                   recomputes which row is which. A press is one lookup.
//   the VALUE ROOM  `value_capacity` says how many columns a field's value gets,
//                   and BOTH the projector and the caret-window reconciliation
//                   ask it. A second copy would agree until the first value long
//                   enough to scroll.
//
// ---- WHAT IT DOES NOT DO -----------------------------------------------------
//
// It measures nothing. `surface::fit_region` did the measuring, on Workshop's
// side, and the answer arrived as two numbers in a `PaneRoom`. This spends them.
// It also holds no state, decides no gesture, and knows nothing about a bus: a
// test can hand it a draft and a budget and ask what a maker would see.

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

/// WHAT A CARET LOOKS LIKE IN A PROVIDER'S ROW.
///
/// `surface::kCaretGlyph`, which is the character a cell medium's own projection
/// inserts for a `SurfaceTextRegion`'s caret -- so a maker typing into this pane
/// sees the same mark they see typing into the Terminal, in both media.
///
/// IT IS A CHARACTER AND NOT A REGION CARET, and the difference is the seam. A
/// `SurfaceTextRegion` carries `caret_row`/`caret_col` and each medium answers with
/// its own metric (a bar in a window, an inserted glyph in a terminal); `PaneContent`
/// carries ROWS and no caret, so a provider that wanted the graphical bar would need
/// a sixth field on a shape whose whole discipline is that a provider supplies no
/// geometry. A character costs one column of the value's room -- the same column
/// `kTerminalCaretCols` costs the Terminal's line, for the same reason: a caret is
/// BETWEEN characters and the position after the last one has to be somewhere.
inline constexpr char kCaret = surface::kCaretGlyph;

/// Fit `text` into `columns`, AND SAY SO when it did not fit.
///
/// THE THIRD COPY OF THESE NINE LINES IN THIS REPOSITORY, and the third is where it
/// stops being obviously right. `workshop::detail::fit` lives inside Workshop's own
/// quarter-megabyte composition, which a provider is a STRANGER to by design;
/// `introspection::fit` made that trade once and wrote down why. This makes it a
/// second time for the same reason and records the pressure rather than resolving
/// it: two providers copying a cut is a coincidence, three would be a shared
/// presentation helper that belongs somewhere both can see.
///
/// It is not a second MEASURER, which is the thing that would matter. The measuring
/// was done once by `surface::fit_region`, on Workshop's side, and arrived here as a
/// number. This only spends it.
inline std::string fit(std::string text, std::int64_t columns) {
    if (columns <= 0) {
        return {};
    }
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

/// WHICH RUN OF A LIST A BUDGET CAN SHOW WHILE KEEPING THE FOCUS VISIBLE, and how
/// much is hidden on each side.
///
/// THREE RULES, AND THEY ARE `list_window`'S (Workshop's own, re-spelled here for
/// `fit`'s reason): a population that fits is shown whole; the focused item is
/// always inside the window; and every omission is COUNTED and spends a row of the
/// same budget.
///
/// WHERE IT DIFFERS FROM `list_window` IS THE MARKER, AND DELIBERATELY: this uses
/// ONE omission row that names both sides rather than one row per side. Two rows of
/// small print out of an eight-row pane is a quarter of the tool, and a pane whose
/// list is windowed at BOTH ends is exactly the case where rows are scarcest. One
/// row can say `... 3 above, 12 below` and be read at a glance.
///
/// A ONE-ROW BUDGET SHOWS THE MARKER AND NO ITEMS, which is `list_window`'s own
/// answer to the same arithmetic: this place cannot show you an item AND tell you
/// what it is hiding, so it tells you.
///
/// TOTAL over every budget and every focus, including ones no pane produces.
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

/// THE COLUMNS A FIELD'S VALUE GETS, and there is exactly one answer to it.
///
/// The row is `mark + name + ":" + type + "  " + [ value ]`, so the value's room is
/// whatever the fixed part left, less the two brackets and less the one column a
/// caret needs to sit after the last character. Floored at zero, because a pane can
/// legitimately be too narrow for a long field name and a negative capacity is not a
/// thing a `TextBox` should ever be handed.
///
/// BOTH THE PROJECTOR AND THE WINDOW RECONCILIATION CALL IT. That is the whole
/// reason it is a function: `TextBox::keep_caret_visible` must be given the SAME
/// number the painter cuts the slice with, and HD-4 paid for learning that a second
/// copy of a window's capacity is right until the first line long enough to scroll.
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

/// ONE FIELD ROW: what it is called, what it is declared as, and what the maker has
/// authored -- with presence and value kept visibly apart.
///
///     id:Text  [hello_]                present, these bytes, caret shown while editing
///     id:Text  []                      PRESENT AND EMPTY -- a real, sendable value
///     id:Text  (required)              absent, and the message cannot go without it
///     note:Text  (absent)              absent, and that is a complete answer
///     repeat:Bool  [false]             a chosen false, which is not the same as unset
///     items:List<Bytes>  (not composable in this version)
///
/// THE BRACKETS ARE WHAT SAY `PRESENT`. An empty pair is a Text field the maker
/// deliberately set to the empty string, and it does not look like `(absent)` --
/// which is §17's requirement made visible rather than merely represented.
///
/// THE TYPE IS THE SCHEMA'S OWN SPELLING (`describe_schema`), and it is the only
/// thing this row says about what a value MEANS. No unit, no range, no example, no
/// enum, no hint: `delay_ms : Int` is everything the runtime schema proves, and a
/// row reading `milliseconds, min 0` would be this pane inventing a fact.
inline std::string field_row_text(const MessageDraft& draft, std::size_t which, bool chosen,
                                  bool editing, std::int64_t columns) {
    const loom::Field& f = draft.field(which);
    std::string row =
        std::string(chosen ? kSelectedMark : kUnselectedMark) + f.name + ":" + draft.type_of(which);
    const FieldDraft& d = draft.fields[which];
    switch (composability(f.type.kind)) {
    case Composability::kNotFlat:
        return fit(row + "  (not composable in this version)", columns);
    case Composability::kNoSpelling:
        return fit(row + "  (no text form -- not composable)", columns);
    case Composability::kScalar:
        break;
    }
    if (!d.present) {
        return fit(row + (f.required ? "  (required)" : "  (absent)"), columns);
    }
    const std::int64_t room = value_capacity(draft, which, columns);
    // A RESTING VALUE IS FITTED AND A LIVE ONE IS WINDOWED (HD-6's rule, unchanged).
    // `fit` marks what it cut, because a committed value has no caret to tell a maker
    // it moved; `TextBox::visible` does not, because the value being edited has one.
    if (!editing) {
        return fit(row + "  [" + fit(d.value.text(), room) + "]", columns);
    }
    std::string shown = d.value.visible(room);
    const std::size_t at = d.value.caret_column();
    shown.insert(at <= shown.size() ? at : shown.size(), 1, kCaret);
    return fit(row + "  [" + shown + "]", columns);
}

/// WHAT THIS PANE IS LOOKING AT AND WHAT THE MAKER HAS DONE TO IT -- the whole of
/// the provider's presentation state, in one value the projector reads and nothing
/// else writes.
///
/// IT HOLDS NO INVENTORY AND NO SECOND COPY OF ANYTHING. `library` and `role` are
/// what a `LoadedSelected` said; `snapshot` is one target's decoded vocabulary and
/// is replaced whole; `draft` is the maker's own work. Nothing here is a cache of a
/// fact somebody else owns.
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

/// THE TARGET, IN THE PANE'S OWN WORDS -- and the two identities are kept apart on
/// purpose (§5).
///
/// `to @zengine.timer` is the MESSAGING address, and it is what a send resolves at
/// DELIVERY: if the office changes hands between now and then, the message reaches
/// whoever holds it then. That is correct and is why no `WeaveId` is pinned anywhere
/// in this tool. `from zengine-timer` is the DIAGNOSTIC identity -- the library name
/// the kernel loaded, which is what the maker actually pressed and which addresses
/// nothing.
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

/// THE WHOLE VIEW, spent against the room Workshop granted.
///
/// ---- THE PRIORITY ORDER, MOST-PROTECTED FIRST -------------------------------
///
///     the target line     what a send would be addressed to. Without it every row
///                         under it is a list of shapes belonging to nobody.
///     the notice          what this pane last DID -- a refusal, or `SUBMITTED`. It
///                         is reserved before the list because a refusal a maker
///                         cannot see is worse than a row of a list they can scroll.
///     the heading         the population, stated. `accepted messages -- 17` is what
///                         makes the windowed list below it an honest sample.
///     the list / the form windowed, with every omission counted (`window_of`).
///     the library line    only out of GENUINE slack.
///
/// The form's two CONTROLS are a FIXED demand and are subtracted before the fields
/// are offered anything, then anchored to the FOOT -- HD-8's argument in a second
/// place, including its reason for the foot: a control that moves under the hand
/// aiming at it is worse than an empty strip above it.
///
/// ---- IT IS EXACTLY INSIDE THE GRANT ------------------------------------------
///
/// Workshop refuses an over-budget update WHOLE, so a provider that does not measure
/// loses everything it said. Every row is `fit` to `columns` and the count never
/// exceeds `rows`.
///
/// TOTAL over every budget, including ones no pane has: zero rows or zero columns is
/// an empty projection.
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
            // NOT `loading...`. Nothing observed promises that an answer will come:
            // a sender is not told its fate, the target may be gone, and there is no
            // timeout here that could mean "refused". What this pane knows is exactly
            // what this row says.
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
        // The two controls are a FIXED demand, subtracted before the fields are
        // offered anything (HD-8). `controls` is 2 or 0: half a footer would put a
        // Submit on screen with no way back off it.
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
            // ANCHORED TO THE FOOT, so the spare room falls between the fields and the
            // controls and the two targets do not move as a maker scrolls (HD-8).
            while (left > controls) {
                say(std::string(), surface::role::kFill);
                --left;
            }
            const bool on_submit = c.cursor == submit_index(c.draft);
            const bool on_back = c.cursor == back_index(c.draft);
            // `[ ... ]` IS THIS TOOL'S EXISTING WORD FOR A PRESSABLE THING, and the
            // muted ground is the second signal after the brackets -- never the only
            // one, because a terminal has no ground to tint (HD-8, HD-9).
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
