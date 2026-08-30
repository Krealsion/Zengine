// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_POWERS_HPP
#define ZENGINE_INTROSPECTION_POWERS_HPP

// THE POWERS PANE, AS A MAKER USES IT (SOURCE-1) -- pure machinery over one reading.
//
//     PowersUi            everything the pane knows that is not a fact about the host
//     project_powers_ui   that state + a prose budget  ->  the rows a maker reads,
//                                                          AND what each place MEANS
//     target_at           a press in that room         ->  the one thing it means
//
// NOTHING HERE TOUCHES A BUS, for `loaded.hpp`'s reason exactly: the weave beside it
// owns WHEN to ask and WHOM to believe, and this owns what an answer MEANS and what
// a gesture DOES -- so both are provable over a value in a test rather than over a
// running system.
//
// ---- ONE CATALOG, TWO QUESTIONS -------------------------------------------------
//
//     Sources     what can answer with nothing supplied by me?
//     Operators   what can transform information I supply?
//
// THE MEMBERSHIP IS DERIVED AND IS NEVER AUTHORED TWICE. `PowerContribution::source`
// is `op::is_source` -- the definition's input schema being empty -- read off the
// same store `find` resolves through. Nothing here parses an identity, consults a
// naming convention, or reads a registration flag, because none exists and none may:
// a second statement of one classification is a second answer, and the second answer
// is the one that can lie. An identity spelled `source.anything` that takes an
// argument is an Operator here, and a case proves it.
//
// ---- SOURCE/OPERATOR AND COMPOSITE ARE INDEPENDENT ------------------------------
//
//     Source/Operator   the exterior contract of the POWER IDENTITY
//     Composite         the construction of the ACTIVE CONTRIBUTION
//
// All four combinations are legal and the pane must show all four. The composite
// badge means exactly one thing -- *this power's active contribution has known
// compositional structure* -- and it does NOT mean openable, editable,
// deconstructable, safe, preferred, or more powerful. There is no Flow here and no
// way to reach one.
//
// ---- BROWSING IS NOT EVALUATION, AND HERE IT IS STRUCTURAL ----------------------
//
// Describing, switching view, searching, filtering, moving the cursor, drawing the
// detail and repainting run zero evaluator bodies -- and not by discipline: this
// file cannot call one. It links no operator target, holds no `OperatorDef`, no
// callable and no catalog, and reads only the value `ResolvedPowers` carried.
// EVALUATION IS ONE EXPLICIT MAKER ACT and it leaves through a message
// (`workshop/sample_vocabulary.hpp`).
//
// ---- WHAT IT RETAINS, AND WHAT THAT RETENTION IS NOT ----------------------------
//
// `PowersUi::reading` is the last admitted `ResolvedPowers`, kept BETWEEN grants so
// that a search, a filter and a cursor can operate without asking the host again.
// It is a SNAPSHOT and it is the member most likely to be misread as a mirror, so:
// it is replaced WHOLE by the next reading, never diffed, dropped at every room
// grant (the SEL-0 discipline -- no rows on the screen means no row map to read a
// press against), and it is not evidence about the catalog NOW. The last row of the
// pane says `snapshot` for exactly this reason, in the words `resolved.hpp` already
// owns.
//
// ---- AND A SAMPLE IS HISTORY --------------------------------------------------
//
// `RetainedSample` is what a Source said WHEN THE MAKER ASKED. It survives view
// switches, filter changes, selection moves and the provider unloading, because
// none of those is evidence about what was said. It never claims to be current --
// there is no timestamp, no refresh, no watcher and no re-ask, and the row that
// carries it leads with `sampled when asked` so the tense survives a narrow pane's
// cut, which takes the tail.

#include "loaded.hpp"   // `fit`, `kElided`, the mark, the entry roles
#include "resolved.hpp" // `kHostResolution`, `kPowersSource`, `kHostItself`, `counted`

#include "component/text_box.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/arrangement_vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::introspection {

// ---- The two views, and the places a press can mean something ------------------

/// WHICH QUESTION THE ONE CATALOG IS BEING ASKED. Session-transient: it is not in
/// `IntrospectionState`, so it is not snapshotted, revived, persisted or in any
/// saved setup -- `selected_`'s precedent, for its reason.
namespace powers_view {
inline constexpr std::int64_t kSources = 0;
inline constexpr std::int64_t kOperators = 1;
} // namespace powers_view

/// WHAT ONE PLACE IN THIS PANE MEANS TO A MAKER -- and each place means exactly ONE
/// of these.
///
/// THE SINGLE-MEANING RULE IS THE POINT. A row that both selected and sampled would
/// make a maker's first press into a pane -- the press that also points the keyboard
/// at it -- a hidden double action, and there would be no gesture left for "select
/// without running anything". So selecting and sampling are two targets, and
/// `kNone` is what most of the pane is.
namespace powers_control {
inline constexpr std::int64_t kNone = -1;
inline constexpr std::int64_t kSources = 0;   ///< show the Sources view
inline constexpr std::int64_t kOperators = 1; ///< show the Operators view
inline constexpr std::int64_t kComposite = 2; ///< toggle the composite-only filter
inline constexpr std::int64_t kEntry = 3;     ///< select the identity this row names
inline constexpr std::int64_t kSample = 4;    ///< sample the selected Source
} // namespace powers_control

/// A RUN OF ONE ROW THAT CARRIES ONE MEANING -- the projection read backwards.
///
/// IT IS COLUMNS AND NOT JUST A ROW, because the chrome row carries three separate
/// controls side by side. `PanePressed` already names a column, so the inverse can
/// be exact; rounding a press to "whichever control is on that row" would let a
/// maker switch views by aiming at the search box.
///
/// FIRST AND LAST ARE INCLUSIVE, and both come from the same string assembly that
/// produced the text -- HD-3's rule, which this phase pays once more: the geometry
/// that drew a thing and the geometry that hits it must be one function.
struct PowersSpan {
    std::int64_t row = 0;
    std::int64_t first = 0;
    std::int64_t last = 0;
    std::int64_t control = powers_control::kNone;
    std::string identity; ///< for `kEntry`: the power this row named
};

/// WHAT A PRESS RESOLVED TO. An empty identity with `kEntry` is unreachable by
/// construction; every other control ignores the identity.
struct PowersTarget {
    std::int64_t control = powers_control::kNone;
    std::string identity;
};

// ---- What the pane retains ------------------------------------------------------

/// ONE SAMPLE, AS HISTORY.
///
/// `identity` IS THE SAMPLE'S OWN AND NOT THE SELECTION'S. A maker may sample one
/// Source and then move the cursor; the retained answer still belongs to the Source
/// it came from and says so, because relabelling it would attach one Source's answer
/// to another Source's name.
struct RetainedSample {
    bool present = false;
    std::string identity;
    bool ok = false;
    std::string reason;             ///< the refusal, in the words of whoever owns it
    std::vector<std::string> lines; ///< the rendered value, host-side
};

/// EVERYTHING THE POWERS PANE KNOWS THAT IS NOT A FACT ABOUT THE HOST.
///
/// ALL OF IT IS SESSION-TRANSIENT. Nothing here is persisted, snapshotted, revived,
/// or written to a setup, a desk or a user-state file: a revived incarnation holds
/// no room and is showing nothing, so a restored cursor would be a mark against a
/// projection that does not exist.
///
/// TWO SELECTIONS AND NOT ONE, BY IDENTITY AND NEVER BY INDEX. A row number is a
/// fact about a projection and stops meaning anything the moment a query changes; a
/// power's identity is a fact about the thing itself. So switching views, typing,
/// filtering and resizing all preserve both selections, and only a fresh reading
/// PROVING absence clears one (`revalidate`).
struct PowersUi {
    std::int64_t view = powers_view::kSources;

    /// The one editable field in this pane -- which is why typing needs no mode: a
    /// printable character has exactly one place it could go.
    component::TextBox query;

    /// Shared by both views, and combined with the query as logical AND. It reads
    /// the ACTIVE contribution, because that is the construction whose code runs.
    bool composite_only = false;

    std::string selected_source;
    std::string selected_operator;

    /// THE LAST ADMITTED READING. A snapshot between grants; see the header.
    workshop::ResolvedPowers reading;
    bool read = false; ///< has any reading been admitted into this pane at all

    RetainedSample sample;

    const std::string& selected() const noexcept {
        return view == powers_view::kSources ? selected_source : selected_operator;
    }
    void select(std::string identity) {
        (view == powers_view::kSources ? selected_source : selected_operator) =
            std::move(identity);
    }
};

// ---- Deriving the two views from one reading ------------------------------------

/// THE CONTRIBUTION WHOSE CODE ACTUALLY RUNS. `op::Catalog` holds a stack whose BACK
/// is what `find` resolves, and `PowerStack` carries that order unchanged -- so the
/// active contribution is the last one, and a stack is never empty (an identity with
/// no contribution is not in the catalog at all).
inline const workshop::PowerContribution* active_of(const workshop::PowerStack& p) noexcept {
    return p.contributions.empty() ? nullptr : &p.contributions.back();
}

/// IS THIS POWER A SOURCE? Asked of the ACTIVE contribution, and that is one rule
/// rather than two: `source` is uniform across a stack (ordinary collision refuses a
/// second contribution outright, and an overlay demands `same_identity` on both
/// ports, so every contribution of one power shares the input schema), and reading
/// the active one is also the rule the composite badge uses -- so the two can never
/// disagree about which contribution they were talking about.
inline bool is_source_power(const workshop::PowerStack& p) noexcept {
    const workshop::PowerContribution* active = active_of(p);
    return active != nullptr && active->source;
}

/// DOES THIS POWER'S ACTIVE CONTRIBUTION HAVE KNOWN COMPOSITE CONSTRUCTION? That is
/// the whole claim. It is not openability, editability or safety, and no surface in
/// this build can show the graph.
inline bool is_composite_power(const workshop::PowerStack& p) noexcept {
    const workshop::PowerContribution* active = active_of(p);
    return active != nullptr && active->composite;
}

/// Does this power belong in the view being shown?
inline bool in_view(const workshop::PowerStack& p, std::int64_t view) noexcept {
    return is_source_power(p) == (view == powers_view::kSources);
}

namespace detail {

/// ASCII CASE FOLDING, AND ONLY ASCII. A byte at or above 0x80 is left exactly as it
/// is, so a non-ASCII identity compares byte-for-byte rather than through some
/// locale this pane has no business having an opinion about.
inline unsigned char fold(char c) noexcept {
    const unsigned char b = static_cast<unsigned char>(c);
    return (b >= 'A' && b <= 'Z') ? static_cast<unsigned char>(b - 'A' + 'a') : b;
}

} // namespace detail

/// CASE-INSENSITIVE ASCII SUBSTRING, OVER THE IDENTITY, AND NOTHING ELSE.
///
/// AN EMPTY QUERY MATCHES EVERYTHING, which is what makes "no filter" and "a filter
/// that happens to accept all" the same state rather than two.
///
/// IT FILTERS AND IT NEVER RANKS. The catalog's order is the catalog's -- a name
/// ordered map -- and a view that reordered it would be a second opinion about a
/// fact whose owner is one ask away, and would then window entries by a rule that
/// owner never applied. There is no fuzzy match, no provider search, no schema
/// search and no history here, and each of those is absent because it has no
/// consumer rather than because it is hard.
inline bool matches_query(std::string_view identity, std::string_view query) noexcept {
    if (query.empty()) {
        return true;
    }
    if (query.size() > identity.size()) {
        return false;
    }
    const std::size_t last = identity.size() - query.size();
    for (std::size_t at = 0; at <= last; ++at) {
        std::size_t j = 0;
        while (j < query.size() && detail::fold(identity[at + j]) == detail::fold(query[j])) {
            ++j;
        }
        if (j == query.size()) {
            return true;
        }
    }
    return false;
}

/// EVERY POWER OF THE CURRENT VIEW, IN CATALOG ORDER, before the query and the
/// composite filter. It is the population a "nothing matches" sentence has to know
/// in order to tell a filter's emptiness from an absence.
inline std::vector<const workshop::PowerStack*> in_view_of(const PowersUi& ui) {
    std::vector<const workshop::PowerStack*> out;
    for (const workshop::PowerStack& p : ui.reading.powers) {
        if (in_view(p, ui.view)) {
            out.push_back(&p);
        }
    }
    return out;
}

/// THE LIST THE MAKER IS ACTUALLY NAVIGATING.
///
///     reading -> this view -> identity substring -> optional composite-only
///
/// DERIVED EVERY PROJECTION AND STORED NOWHERE. Two materialised lists would be two
/// owners of one truth, and the pane would then be able to show a Sources list that
/// disagreed with the reading it came from.
inline std::vector<const workshop::PowerStack*> filtered_of(const PowersUi& ui) {
    std::vector<const workshop::PowerStack*> out;
    const std::string& q = ui.query.text();
    for (const workshop::PowerStack& p : ui.reading.powers) {
        if (!in_view(p, ui.view)) {
            continue;
        }
        if (!matches_query(p.power, q)) {
            continue;
        }
        if (ui.composite_only && !is_composite_power(p)) {
            continue;
        }
        out.push_back(&p);
    }
    return out;
}

/// WHERE THE CURSOR IS IN THAT LIST, or -1 when the selected identity is not in it.
///
/// HIDDEN IS NOT ABSENT. A selection excluded by the query, by the composite filter
/// or by the other view being shown is HELD and merely unmarked; this answers only
/// "is there a row to put the mark on", and the mark returns with the row.
inline std::int64_t cursor_in(const std::vector<const workshop::PowerStack*>& list,
                              std::string_view identity) noexcept {
    if (identity.empty()) {
        return -1;
    }
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i]->power == identity) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

/// THE SELECTED POWER, IF THE CURRENT READING STILL HAS IT IN THIS VIEW.
///
/// IT ASKS THE POPULATION AND NOT THE FILTERED LIST, deliberately: the detail block
/// is about the maker's SELECTION, which a search does not revoke. A maker who
/// narrows the list can still read what they had selected, and can still sample it.
inline const workshop::PowerStack* selected_of(const PowersUi& ui) {
    const std::string& want = ui.selected();
    if (want.empty()) {
        return nullptr;
    }
    for (const workshop::PowerStack& p : ui.reading.powers) {
        if (p.power == want && in_view(p, ui.view)) {
            return &p;
        }
    }
    return nullptr;
}

/// THE IDENTITY AN EXPLICIT SAMPLE GESTURE WOULD SPEND, or empty when there is none.
///
/// SOURCES ONLY, AND THAT IS THE PANE'S HALF OF THE ANSWER. `op::sample` refuses a
/// parameterized Operator in its own words at the spend, which is the authority; the
/// pane simply offers no gesture for one, so Return in the Operators view is bound
/// to nothing and there is no operator-invocation surface to grow.
inline std::string sampleable(const PowersUi& ui) {
    const workshop::PowerStack* p = selected_of(ui);
    return (p != nullptr && is_source_power(*p)) ? p->power : std::string();
}

/// MOVE THE CURSOR ONE PLACE THROUGH THE VISIBLE LIST.
///
/// A HIDDEN SELECTION DOES NOT HAVE TO BE PROJECTED INTO THE LIST TO BE LEFT. When
/// the held identity is not in the filtered population, Up and Down begin at the
/// list's ordinary beginning rather than pretending the hidden entry has a place in
/// it -- which is the only answer that does not invent a position for something the
/// maker cannot see.
inline void move_cursor(PowersUi& ui, std::int64_t delta) {
    const std::vector<const workshop::PowerStack*> list = filtered_of(ui);
    if (list.empty()) {
        return;
    }
    const std::int64_t at = cursor_in(list, ui.selected());
    std::int64_t next = at < 0 ? 0 : at + delta;
    if (next < 0) {
        next = 0;
    }
    const std::int64_t last = static_cast<std::int64_t>(list.size()) - 1;
    if (next > last) {
        next = last;
    }
    ui.select(list[static_cast<std::size_t>(next)]->power);
}

/// A FRESH READING IS THE ONLY THING THAT MAY CLEAR A SELECTION (SEL-0's law, one
/// pane on).
///
/// IT ASKS THE POPULATION THIS ANSWER CARRIES, not the rows it will fit and not the
/// filtered list: presentation may HIDE, only the population may INVALIDATE. Losing
/// a maker's place because they typed three characters would be a fiction about the
/// system, and losing it because the power genuinely went away is the truth.
inline void revalidate(PowersUi& ui) {
    for (const std::int64_t view : {powers_view::kSources, powers_view::kOperators}) {
        std::string& held =
            view == powers_view::kSources ? ui.selected_source : ui.selected_operator;
        if (held.empty()) {
            continue;
        }
        bool still = false;
        for (const workshop::PowerStack& p : ui.reading.powers) {
            if (p.power == held && in_view(p, view)) {
                still = true;
                break;
            }
        }
        if (!still) {
            held.clear();
        }
    }
}

// ---- The window ------------------------------------------------------------------

/// WHICH RUN OF THE FILTERED LIST A BUDGET CAN SHOW WHILE KEEPING THE CURSOR VISIBLE,
/// and how much is hidden on each side.
///
/// THE THREE RULES ARE WORKSHOP'S OWN `list_window`'S, re-spelled here for `fit`'s
/// reason (a provider is a stranger to Workshop's private composition): a population
/// that fits is shown whole, the cursor is always inside the window, and every
/// omission is COUNTED and spends a row of the same budget.
///
/// ⚠ THIS IS THE SECOND PROVIDER-SIDE COPY of that arithmetic -- `composer::window_of`
/// is the first, and it made the same trade for the same reason. Two is a pressure
/// and not yet an extraction: a THIRD provider, or a shared presentation home for
/// `fit`/window/caret, is what would earn one, and this comment is the trigger to
/// watch for rather than a note that something is untidy.
///
/// TOTAL over every budget and every cursor, including ones no pane produces.
struct PowersWindow {
    std::int64_t first = 0;
    std::int64_t count = 0;
    std::int64_t before = 0;
    std::int64_t after = 0;
};

inline PowersWindow powers_window(std::int64_t population, std::int64_t cursor,
                                  std::int64_t rows) noexcept {
    PowersWindow w;
    if (population <= 0) {
        return w;
    }
    if (rows <= 0) {
        // The accounting stays total where nobody can spend it: the whole population
        // is hidden, and saying so keeps "shown plus hidden is the population" true
        // for exactly the case where the most is hidden.
        w.after = population;
        return w;
    }
    if (population <= rows) {
        w.count = population;
        return w;
    }
    if (rows == 1) {
        w.after = population; // the marker, and no room for an entry beside it
        return w;
    }
    w.count = rows - 1; // one row of the budget is the omission marker's
    std::int64_t focus = cursor < 0 ? 0 : cursor;
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

/// The omission marker, in one spelling, naming both sides when both are hidden.
inline std::string powers_omission(const PowersWindow& w, std::int64_t columns) {
    const std::string mark = "  " + std::string(kElided) + " ";
    if (w.before > 0 && w.after > 0) {
        return fit(mark + std::to_string(w.before) + " above, " + std::to_string(w.after) +
                       " below",
                   columns);
    }
    if (w.before > 0) {
        return fit(mark + std::to_string(w.before) + " more above", columns);
    }
    return fit(mark + std::to_string(w.after) + " more below", columns);
}

// ---- The words this pane owns ----------------------------------------------------

inline constexpr const char* kSourcesWord = "Sources";
inline constexpr const char* kOperatorsWord = "Operators";
inline constexpr const char* kCompositeWord = "Composite";
inline constexpr const char* kCompositeBadge = " (composite)";
inline constexpr const char* kFindLabel = "find:";
inline constexpr const char* kSampleControl = "[ Sample ]";

/// WHAT A RETAINED SAMPLE IS, LEADING WITH THE TENSE.
///
/// THE HONESTY IS FIRST BECAUSE `fit` CUTS THE TAIL. A narrow pane loses the end of
/// this row, and the end is the identity -- which a maker can recover from the list.
/// What must never be lost is the claim the row is making, and putting `when asked`
/// at the front is how a cut cannot reach it.
inline constexpr const char* kSampledWhenAsked = "sampled when asked";
inline constexpr const char* kSampleRefusedWord = "sample refused when asked";

/// The empty states, kept apart because they are different facts.
inline constexpr const char* kNoSourcesHere = "no sources resolve here";
inline constexpr const char* kNoOperatorsHere = "no operators resolve here";

/// `5 sources here -- all hidden by the current filter`. The COUNT is what makes it
/// a different sentence from an absence: a maker who filtered everything away is
/// told how much is behind the filter, so an empty pane never reads as an empty
/// system.
inline std::string all_hidden(std::int64_t population, std::int64_t view,
                              std::int64_t columns) {
    return fit(counted(population, view == powers_view::kSources ? "source" : "operator",
                       view == powers_view::kSources ? "sources" : "operators") +
                   " here -- all hidden by the current filter",
               columns);
}

// ---- The chrome row --------------------------------------------------------------

/// THE POSITION MARKER, against the list the maker is actually navigating.
///
/// `-` FOR NO CURSOR, because zero is a position and "nothing is selected" is not
/// one. The denominator is the FILTERED population for the same reason the numerator
/// is: they describe one list, and a numerator from one list over a denominator from
/// another is the kind of number that is never wrong on any single row and always
/// wrong as a sentence.
inline std::string position_marker(std::int64_t cursor, std::int64_t population) {
    return (cursor < 0 ? std::string("-") : std::to_string(cursor + 1)) + "/" +
           std::to_string(population);
}

/// WHICH CHROME SEGMENTS THIS WIDTH CAN CARRY, and how much of it the query gets.
///
/// THE DROP ORDER IS A PRIORITY AND IS MONOTONE: the composite control goes first,
/// then the search box, then the position. The view controls never go -- a pane that
/// could not say which of its two questions it is answering has stopped being this
/// pane -- and at a width too narrow even for them, `fit` marks the cut and the
/// spans below still describe exactly what is drawn.
///
/// ⚠ ONE FUNCTION ANSWERS FOR BOTH THE PAINTER AND THE CARET. `query_capacity` is
/// this, spent by the weave to reconcile the `TextBox`'s window before projecting;
/// HD-4 paid for learning that a second copy of a window's capacity is right until
/// the first line long enough to scroll.
struct ChromeFit {
    bool position = false;
    bool find = false;
    bool composite = false;
    std::int64_t query_room = 0; ///< columns for the query text AND its caret
};

namespace detail {

inline constexpr std::int64_t kGap = 2;      ///< between two chrome segments
inline constexpr std::int64_t kViewWidth = 19; ///< `[Sources] Operators` either way round
inline constexpr std::int64_t kCompositeWidth = 13; ///< `[x] Composite`
inline constexpr std::int64_t kFindMin = 6;  ///< `find:` and one column of query+caret

inline std::int64_t find_fixed() {
    return static_cast<std::int64_t>(std::char_traits<char>::length(kFindLabel));
}

} // namespace detail

inline ChromeFit fit_chrome(const std::string& position, std::int64_t columns) {
    ChromeFit out;
    const std::int64_t pos_w = static_cast<std::int64_t>(position.size());
    const std::int64_t find_min = detail::kGap + detail::kFindMin;
    const std::int64_t with_pos = detail::kGap + pos_w;
    const std::int64_t with_comp = detail::kGap + detail::kCompositeWidth;
    std::int64_t used = detail::kViewWidth;
    if (used + with_pos + find_min + with_comp <= columns) {
        out.position = out.find = out.composite = true;
        used += with_pos + with_comp;
    } else if (used + with_pos + find_min <= columns) {
        out.position = out.find = true;
        used += with_pos;
    } else if (used + with_pos <= columns) {
        out.position = true;
        used += with_pos;
    }
    if (out.find) {
        used += detail::kGap + detail::find_fixed();
        out.query_room = columns - used;
    }
    return out;
}

/// THE COLUMNS THE QUERY TEXT GETS -- the caret's own column subtracted, because a
/// caret sitting after the last character needs somewhere to sit.
inline std::int64_t query_capacity(const PowersUi& ui, std::int64_t columns) {
    const std::vector<const workshop::PowerStack*> list = filtered_of(ui);
    const std::string marker = position_marker(cursor_in(list, ui.selected()),
                                               static_cast<std::int64_t>(list.size()));
    const ChromeFit fit_of = fit_chrome(marker, columns);
    const std::int64_t room = fit_of.query_room - 1;
    return room > 0 ? room : 0;
}

// ---- The projection ---------------------------------------------------------------

/// WHAT THE PANE SHOWS, AND WHAT EACH PLACE IN IT MEANS.
struct PowersView {
    std::vector<surface::SurfaceTextRow> rows;
    std::vector<PowersSpan> spans;
    std::int64_t population = 0; ///< the filtered list's size -- what the marker counts
    std::int64_t cursor = -1;    ///< where in it the selection is, or -1
};

/// WHICH CONTROL A PRESS LANDED ON, or none.
///
/// TOTAL over every row and column, including ones no press can produce: a place
/// outside the view carries no meaning, exactly as a place inside it that carries
/// none does. A provider is handed a row off a wire and must not have to bound it
/// twice.
inline PowersTarget target_at(const PowersView& view, std::int64_t row, std::int64_t column) {
    for (const PowersSpan& s : view.spans) {
        if (s.row == row && column >= s.first && column <= s.last) {
            return PowersTarget{s.control, s.identity};
        }
    }
    return PowersTarget{};
}

namespace detail {

/// One row and the places in it that mean something, appended together. The closure
/// exists so that a row whose map entry was forgotten is not a thing this file can
/// produce: the rows and the spans are one projection in two pieces.
struct Sayer {
    PowersView& view;
    std::int64_t say(std::string text, std::int64_t role,
                     std::int64_t ground = surface::role::kNone) const {
        view.rows.push_back(surface::SurfaceTextRow{std::move(text), role, ground});
        return static_cast<std::int64_t>(view.rows.size()) - 1;
    }
    /// `solid` IS HOW MANY LEADING COLUMNS OF THE ROW ARE GENUINE TEXT, and a
    /// control that did not fit inside them is not a target. A press on the `...`
    /// `fit` left behind would otherwise operate a control the maker cannot see --
    /// the inverse disagreeing with the picture, which is the one thing a row map
    /// exists to prevent.
    void span(std::int64_t row, std::int64_t first, std::int64_t width, std::int64_t control,
              std::int64_t solid, std::string identity = std::string()) const {
        if (width <= 0 || first < 0 || first + width > solid) {
            return;
        }
        view.spans.push_back(
            PowersSpan{row, first, first + width - 1, control, std::move(identity)});
    }
};

/// HOW MUCH OF A ROW IS REAL. Everything, when the text fit; everything but the mark,
/// when `fit` had to cut.
inline std::int64_t solid_columns(const std::string& drawn, std::size_t wanted) {
    std::int64_t solid = static_cast<std::int64_t>(drawn.size());
    if (drawn.size() < wanted) {
        solid -= static_cast<std::int64_t>(std::char_traits<char>::length(kElided));
    }
    return solid < 0 ? 0 : solid;
}

/// ONE POWER, ON ONE LINE: the mark, the identity, and the composite badge.
///
/// THE BADGE IS RESERVED AT THE RIGHT AND THE IDENTITY IS FITTED INTO WHAT IS LEFT --
/// `picker_entry_text`'s rule (fit for the truth, reserve for the alignment). A plain
/// `fit` of the whole row would have cut the badge off first, which is the one part
/// of the row a maker cannot reconstruct from anything else on the screen.
///
/// AND THE MARK COSTS NO COLUMNS. `kUnselectedMark` is two spaces and `kSelectedMark`
/// is `> `, so selecting a row exchanges the indent for a mark and no budget moves.
inline std::string power_row_text(const workshop::PowerStack& p, bool chosen,
                                  std::int64_t columns) {
    const std::string mark = chosen ? kSelectedMark : kUnselectedMark;
    std::string badge = is_composite_power(p) ? kCompositeBadge : "";
    std::int64_t room = columns - static_cast<std::int64_t>(mark.size() + badge.size());
    if (room < 4) {
        // TOO NARROW FOR BOTH, and the identity is what the maker is navigating. The
        // composite fact is still on the detail block's active row, so nothing is
        // lost without another place to read it.
        badge.clear();
        room = columns - static_cast<std::int64_t>(mark.size());
    }
    return fit(mark + fit(p.power, room) + badge, columns);
}

/// THE ROWS OF THE SELECTED DETAIL, most-protected first.
///
///     yields <schema> v<N>        what a sample would claim to be. First because it
///                                 is the question `source` exists to make answerable
///                                 without running anything.
///     [ Sample ]                  the control -- Sources only. Above provenance
///                                 because a control a maker cannot reach is a
///                                 feature they do not have, while a provider name is
///                                 a fact a taller pane will show.
///     active / shadowed <who>     the contribution stack, active first, composite
///                                 marked where the definition says so.
///
/// THE BLOCK IS ALL-OR-NOTHING AT TWO ROWS. One row of it is a half answer -- a
/// yields line with no way to act on it, or a control with nothing said about what it
/// would produce -- so a pane too short for two shows none of it and spends the room
/// on the list instead.
struct Detail {
    std::vector<std::string> texts;
    std::vector<std::int64_t> roles;
    std::vector<bool> is_control;
};

inline Detail detail_rows(const workshop::PowerStack& p, std::int64_t columns) {
    Detail d;
    const workshop::PowerContribution* active = active_of(p);
    std::string yields = "  yields ";
    if (active == nullptr || active->output.name.empty()) {
        yields += "(not reported)";
    } else {
        yields += active->output.name + " v" + std::to_string(active->output.version);
    }
    d.texts.push_back(fit(yields, columns));
    d.roles.push_back(surface::role::kMuted);
    d.is_control.push_back(false);

    if (is_source_power(p)) {
        d.texts.push_back(fit(std::string("  ") + kSampleControl, columns));
        d.roles.push_back(surface::role::kAccent);
        d.is_control.push_back(true);
    }
    for (std::size_t i = p.contributions.size(); i > 0; --i) {
        const workshop::PowerContribution& c = p.contributions[i - 1];
        const bool live = i == p.contributions.size();
        std::string said = live ? "  active    " : "  shadowed  ";
        said += c.provider.empty() ? kHostItself : c.provider;
        if (c.composite) {
            said += kCompositeBadge;
        }
        d.texts.push_back(fit(said, columns));
        d.roles.push_back(live ? surface::role::kFill : surface::role::kMuted);
        d.is_control.push_back(false);
    }
    return d;
}

/// THE HEADER OF A RETAINED SAMPLE, WITH THE TENSE FIRST AND THE OMISSION COUNTED.
///
/// The identity is fitted into whatever the tense and the marker left, so a narrow
/// pane loses the end of a long identity and never the claim or the count.
inline std::string sample_header(const RetainedSample& s, std::size_t hidden,
                                 std::int64_t columns) {
    const std::string lead = std::string(s.ok ? kSampledWhenAsked : kSampleRefusedWord) + "  ";
    const std::string tail =
        hidden > 0 ? ("  " + std::string(kElided) + " " + std::to_string(hidden) + " more") : "";
    const std::int64_t room = columns - static_cast<std::int64_t>(lead.size() + tail.size());
    return fit(lead + fit(s.identity, room) + tail, columns);
}

/// EVERY LINE A RETAINED SAMPLE WOULD SPEND, header excluded.
inline const std::vector<std::string>& sample_body(const RetainedSample& s,
                                                   std::vector<std::string>& scratch) {
    if (s.ok) {
        return s.lines;
    }
    scratch.clear();
    if (!s.reason.empty()) {
        scratch.push_back(s.reason);
    }
    return scratch;
}

} // namespace detail

/// THE WHOLE POWERS PANE, spent against the room Workshop granted.
///
/// ---- THE PRIORITY ORDER, MOST-PROTECTED FIRST -----------------------------------
///
///     the chrome row     which view, where the cursor is, what is being searched
///                        for, and whether the composite filter is on. It is one row
///                        and it is never dropped: a pane that cannot say which of
///                        its two questions it is answering is not answering either.
///     the list           the entries, cursor-windowed, every omission counted. It
///                        keeps up to three rows before anything else takes any.
///     the selected detail    what a sample would yield, the control, the stack
///     the retained sample    what a Source said when it was asked
///     `kHostResolution`      whose resolution these rows describe
///     `kPowersSource`        where they came from and how old they are -- SLACK ONLY
///
/// ---- WHAT THE MEASURED DEFAULTS BUY ---------------------------------------------
///
/// The shipped graphical pane is FOUR prose rows and the shipped terminal pane is
/// EIGHT, so those are the two budgets every decision above was made against. At
/// four rows with a long list the pane is a chrome row and a windowed list, and that
/// is the honest answer rather than a defect: navigation is the thing a four-row pane
/// can do, every omission is counted, and a maker who wants the detail authors a
/// taller pane (WIND-2) and gets it with nothing here edited.
///
/// TOTAL over every budget, including ones no pane has. Zero rows or zero columns is
/// an empty projection; and being exactly inside the grant is this function's
/// obligation rather than a courtesy, because Workshop refuses an over-budget update
/// WHOLE and a provider that does not measure loses everything it said.
inline PowersView project_powers_ui(const PowersUi& ui, std::int64_t rows,
                                    std::int64_t columns) {
    PowersView view;
    const detail::Sayer say{view};
    if (rows <= 0 || columns <= 0) {
        return view;
    }

    const std::vector<const workshop::PowerStack*> list = filtered_of(ui);
    view.population = static_cast<std::int64_t>(list.size());
    view.cursor = cursor_in(list, ui.selected());

    // ---- the chrome row, composed to the width it was given ----------------------
    {
        const std::string marker = position_marker(view.cursor, view.population);
        const ChromeFit shape = fit_chrome(marker, columns);
        const bool sources = ui.view == powers_view::kSources;
        const std::string a =
            sources ? "[" + std::string(kSourcesWord) + "]" : std::string(kSourcesWord);
        const std::string b =
            sources ? std::string(kOperatorsWord) : "[" + std::string(kOperatorsWord) + "]";
        std::string text = a + " " + b;
        const std::int64_t operators_at = static_cast<std::int64_t>(a.size()) + 1;
        if (shape.position) {
            text += "  " + marker;
        }
        std::int64_t composite_at = -1;
        if (shape.composite) {
            composite_at = static_cast<std::int64_t>(text.size()) + detail::kGap;
            text += std::string("  [") + (ui.composite_only ? "x" : " ") + "] " + kCompositeWord;
        }
        if (shape.find) {
            const std::int64_t room = shape.query_room - 1;
            std::string shown = ui.query.visible(room > 0 ? room : 0);
            const std::size_t at = ui.query.caret_column();
            shown.insert(at <= shown.size() ? at : shown.size(), 1, surface::kCaretGlyph);
            text += std::string("  ") + kFindLabel + shown;
        }
        const std::string drawn = fit(text, columns);
        const std::int64_t solid = detail::solid_columns(drawn, text.size());
        const std::int64_t row = say.say(drawn, surface::role::kAccent);
        say.span(row, 0, static_cast<std::int64_t>(a.size()), powers_control::kSources, solid);
        say.span(row, operators_at, static_cast<std::int64_t>(b.size()),
                 powers_control::kOperators, solid);
        if (composite_at >= 0) {
            say.span(row, composite_at, detail::kCompositeWidth, powers_control::kComposite,
                     solid);
        }
    }

    // ---- how the remaining rows are shared ---------------------------------------
    //
    // THE LIST IS OFFERED UP TO THREE ROWS BEFORE ANYTHING ELSE TAKES ANY, and the
    // rest is offered in priority order; whatever nobody wanted returns to the list.
    // So a short list never starves the detail, and a long list never loses its
    // navigation to it.
    const std::int64_t left = rows - 1;
    const std::int64_t list_wants = view.population > 0 ? view.population : 1;
    // THE FLOOR IS THE SMALLEST OF THREE, and the third one matters: a list of ONE
    // entry does not reserve three rows just because three exist. Taking `min(left, 3)`
    // and stopping would starve the detail on a short pane over a short list, which is
    // exactly the arrangement the shipped host has.
    const std::int64_t wanted_floor = list_wants < 3 ? list_wants : 3;
    const std::int64_t floor_rows = left < wanted_floor ? left : wanted_floor;
    std::int64_t spare = left - floor_rows;

    const workshop::PowerStack* chosen = selected_of(ui);
    detail::Detail block;
    if (chosen != nullptr) {
        block = detail::detail_rows(*chosen, columns);
    }
    const std::int64_t detail_wants = static_cast<std::int64_t>(block.texts.size());
    const std::int64_t detail_rows_taken =
        (detail_wants >= 2 && spare >= 2) ? (spare < detail_wants ? spare : detail_wants) : 0;
    spare -= detail_rows_taken;

    std::vector<std::string> scratch;
    const std::vector<std::string>& body =
        ui.sample.present ? detail::sample_body(ui.sample, scratch) : scratch;
    const std::int64_t sample_wants =
        ui.sample.present ? 1 + static_cast<std::int64_t>(body.size()) : 0;
    const std::int64_t sample_rows_taken =
        (sample_wants >= 1 && spare >= 1) ? (spare < sample_wants ? spare : sample_wants) : 0;
    spare -= sample_rows_taken;

    const std::int64_t caveat = spare > 0 ? 1 : 0;
    spare -= caveat;
    // ...and the last two rows only out of GENUINE slack: the whole list must fit and
    // still leave a row over. A pane that had to window its own list spends that row
    // on the list instead, and keeps the caveat it already reserved.
    const std::int64_t census =
        caveat == 1 && spare > 0 && list_wants < floor_rows + spare ? 1 : 0;
    spare -= census;
    const std::int64_t source =
        census == 1 && spare > 0 && list_wants < floor_rows + spare ? 1 : 0;
    spare -= source;
    const std::int64_t list_budget = floor_rows + spare;

    // ---- the list ----------------------------------------------------------------
    if (view.population == 0) {
        if (list_budget > 0) {
            // TWO DIFFERENT FACTS, NEVER CONFUSED. A view with nothing in it and a
            // view whose entries the maker has filtered away are different states,
            // and the second one is counted so an empty pane cannot read as an empty
            // system.
            const std::int64_t here = static_cast<std::int64_t>(in_view_of(ui).size());
            const bool sources = ui.view == powers_view::kSources;
            say.say(here == 0 ? fit(sources ? kNoSourcesHere : kNoOperatorsHere, columns)
                              : all_hidden(here, ui.view, columns),
                    surface::role::kMuted);
        }
    } else if (list_budget > 0) {
        // AND THE MARKER IS A ROW LIKE ANY OTHER. `powers_window` stays total at a
        // zero-row budget -- it reports the whole population hidden -- but a caller
        // with no row to write that in must not write it anyway, which is how a
        // one-row pane came to publish two.
        const PowersWindow w = powers_window(view.population, view.cursor, list_budget);
        for (std::int64_t i = 0; i < w.count; ++i) {
            const workshop::PowerStack& p = *list[static_cast<std::size_t>(w.first + i)];
            const bool chosen_row = view.cursor == w.first + i;
            const std::int64_t row =
                say.say(detail::power_row_text(p, chosen_row, columns), entry_role(chosen_row),
                        entry_ground(chosen_row));
            // THE WHOLE ROW SELECTS, AND SELECTING IS ALL IT DOES. A row that also
            // sampled would make one press two acts. The cut inside a long identity
            // is still that entry's own row, so the span is the whole width.
            say.span(row, 0, columns, powers_control::kEntry, columns, p.power);
        }
        if (w.before > 0 || w.after > 0) {
            // A POPULATION FACT AND NOT A HIDDEN ENTRY. A maker who presses it has
            // pressed a sentence about a count, and the honest answer is that no
            // entry was selected -- so it carries no span at all.
            say.say(powers_omission(w, columns), surface::role::kMuted);
        }
    }

    // ---- the selected detail ------------------------------------------------------
    for (std::int64_t i = 0; i < detail_rows_taken; ++i) {
        const std::size_t at = static_cast<std::size_t>(i);
        const std::int64_t row = say.say(block.texts[at], block.roles[at]);
        if (block.is_control[at]) {
            const std::int64_t width =
                static_cast<std::int64_t>(std::char_traits<char>::length(kSampleControl));
            say.span(row, 2, width, powers_control::kSample,
                     detail::solid_columns(block.texts[at], static_cast<std::size_t>(2 + width)));
        }
    }

    // ---- the retained sample ------------------------------------------------------
    if (sample_rows_taken > 0) {
        const std::int64_t room = sample_rows_taken - 1;
        const std::int64_t have = static_cast<std::int64_t>(body.size());
        // An entry and its omission marker are ONE demand on the budget -- the
        // arithmetic INTR-0 was measured getting wrong, spent here on lines.
        std::int64_t shown = have < room ? have : room;
        if (shown < have && shown > 0) {
            --shown; // the marker's row comes out of this same budget
        }
        const std::size_t hidden = static_cast<std::size_t>(have - shown);
        say.say(detail::sample_header(ui.sample, hidden, columns),
                ui.sample.ok ? surface::role::kMuted : surface::role::kAlert);
        for (std::int64_t i = 0; i < shown; ++i) {
            say.say(fit("  " + body[static_cast<std::size_t>(i)], columns),
                    ui.sample.ok ? surface::role::kFill : surface::role::kAlert);
        }
        if (hidden > 0 && shown < room) {
            say.say(elision(hidden, columns), surface::role::kMuted);
        }
    }

    if (caveat > 0) {
        say.say(fit(kHostResolution, columns), surface::role::kMuted);
    }
    if (census > 0) {
        // THE CATALOG CENSUS, WHICH IS A DIFFERENT QUESTION FROM THE POSITION MARKER
        // and is why both exist. `17/143` counts the list the maker is NAVIGATING --
        // one view, after the query and the filter -- and this counts what the whole
        // reading contained. Neither can stand in for the other, and the four-row
        // graphical default has room for only the first, so this is the one that
        // waits for slack.
        //
        // THE SENTENCE IS THE ONE QR-4 REPAIRED, unchanged: the verb agrees with the
        // count and the noun agrees with the number, because a number whose grammar
        // disagrees with it spends a reader's attention on the grammar.
        const std::int64_t identities = static_cast<std::int64_t>(ui.reading.powers.size());
        say.say(fit(powers_said(identities) + (identities == 1 ? " resolves" : " resolve") +
                        " here -- from " +
                        providers_said(static_cast<std::int64_t>(ui.reading.providers.size())),
                    columns),
                surface::role::kMuted);
    }
    if (source > 0) {
        say.say(fit(kPowersSource, columns), surface::role::kMuted);
    }
    return view;
}

} // namespace zengine::introspection

#endif // ZENGINE_INTROSPECTION_POWERS_HPP
