// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_POWERS_HPP
#define ZENGINE_INTROSPECTION_POWERS_HPP

// The Powers pane as a maker uses it, pure machinery over one reading: `PowersUi` (all the pane
// knows that is not a fact about the host), `project_powers_ui` (that state and a budget become
// rows and what each place means) and `target_at` (a press becomes its one meaning). No bus
// here: the weave owns when to ask and whom to believe. Sources and Operators are derived from
// the catalog (`op::is_source`), never authored; composite is independent of both.
// Pane law: agents/panes.md

// Browsing cannot evaluate: this links no operator target and reads only `ResolvedPowers`.
// `PowersUi::reading` is a snapshot kept between grants for search, filter and cursor -- replaced
// whole, dropped at every grant, never evidence about the catalog now. A retained sample is
// history: it never claims to be current, and its row leads with the tense.

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

/// Which question the one catalog is being asked. Transient: never snapshotted, revived,
/// persisted or saved.
namespace powers_view {
inline constexpr std::int64_t kSources = 0;
inline constexpr std::int64_t kOperators = 1;
} // namespace powers_view

/// What one place in this pane means to a maker, and each place means exactly one: a row that
/// both selected and sampled would make a cold pane's first press a hidden double act.
namespace powers_control {
inline constexpr std::int64_t kNone = -1;
inline constexpr std::int64_t kSources = 0;   ///< show the Sources view
inline constexpr std::int64_t kOperators = 1; ///< show the Operators view
inline constexpr std::int64_t kComposite = 2; ///< toggle the composite-only filter
inline constexpr std::int64_t kEntry = 3;     ///< select the identity this row names
inline constexpr std::int64_t kSample = 4;    ///< sample the selected Source
} // namespace powers_control

/// A run of one row carrying one meaning -- the projection read backwards, in columns because
/// the chrome row holds three controls side by side. First and last are inclusive and come from
/// the assembly that drew the text: the geometry that draws and the one that hits are one.
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

/// One sample, as history. `identity` is the sample's own, not the selection's: moving the
/// cursor must not relabel one Source's answer with another's name.
struct RetainedSample {
    bool present = false;
    std::string identity;
    bool ok = false;
    std::string reason;             ///< the refusal, in the words of whoever owns it
    std::vector<std::string> lines; ///< the rendered value, host-side
};

/// Everything the Powers pane knows that is not a fact about the host, all of it transient. Two
/// selections, by identity and never by index: view switches, typing, filtering and resizing
/// keep both, and only a fresh reading proving absence clears one (`revalidate`).
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

/// Is this power a Source? Asked of the active contribution: `source` is uniform across a stack
/// (collision refuses a second contribution, an overlay demands the same ports), and the
/// composite badge reads the active one too, so the two never talk about different ones.
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

/// Case-insensitive ASCII substring over the identity, and nothing else. An empty query matches
/// everything. It filters and never ranks: the catalog's order is its owner's.
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

/// The list the maker navigates -- the reading, this view, the query, the composite filter --
/// derived every projection and stored nowhere, so it cannot disagree with its reading.
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

/// Where the cursor is in that list, or -1. Hidden is not absent: a selection the query, the
/// filter or the other view excludes is held, merely unmarked.
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

/// The selected power, if the reading still has it in this view -- asked of the population, not
/// the filtered list, since a search does not revoke a selection.
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

/// The identity a sample gesture would spend, or empty: Sources only. `op::sample` refuses an
/// Operator at the spend; the pane simply offers no gesture for one.
inline std::string sampleable(const PowersUi& ui) {
    const workshop::PowerStack* p = selected_of(ui);
    return (p != nullptr && is_source_power(*p)) ? p->power : std::string();
}

/// Move the cursor one place through the visible list. A hidden selection starts from the list's
/// beginning, rather than inventing a place for what the maker cannot see.
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

/// Only a fresh reading may clear a selection, asked of the population it carries: presentation
/// may hide, and only the population may invalidate.
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

/// Which run of the filtered list a budget shows with the cursor visible, and how much each side
/// hides: Workshop's `list_window` rules restated, since a provider is a stranger to Workshop's
/// composition -- a fitting population whole, the cursor inside, every omission counted in the
/// budget. The second provider-side copy, after `composer::window_of`; a third would earn an
/// extraction. Total over every budget and cursor.
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

/// What a retained sample is, leading with the tense: `fit` cuts the tail, so a narrow pane loses
/// the identity (recoverable from the list) and never the claim.
inline constexpr const char* kSampledWhenAsked = "sampled when asked";
inline constexpr const char* kSampleRefusedWord = "sample refused when asked";

/// The empty states, kept apart because they are different facts.
inline constexpr const char* kNoSourcesHere = "no sources resolve here";
inline constexpr const char* kNoOperatorsHere = "no operators resolve here";

/// `5 sources here -- all hidden by the current filter`: the count keeps an empty pane from
/// reading as an empty system.
inline std::string all_hidden(std::int64_t population, std::int64_t view,
                              std::int64_t columns) {
    return fit(counted(population, view == powers_view::kSources ? "source" : "operator",
                       view == powers_view::kSources ? "sources" : "operators") +
                   " here -- all hidden by the current filter",
               columns);
}

// ---- The chrome row --------------------------------------------------------------

/// The position marker, against the list the maker navigates: `-` for no cursor (zero is a
/// position), numerator and denominator from the same filtered list.
inline std::string position_marker(std::int64_t cursor, std::int64_t population) {
    return (cursor < 0 ? std::string("-") : std::to_string(cursor + 1)) + "/" +
           std::to_string(population);
}

/// Which chrome segments this width carries, and the query's share. The drop order is monotone:
/// the composite control, then the search box, then the position; the view controls never go.
/// One function answers the painter and the caret (`query_capacity`), since a second copy of a
/// window's capacity is right only until a line long enough to scroll.
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

/// Which control a press landed on, or none; total over every row and column, since a provider
/// is handed a row off a wire and must not bound it twice.
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
    /// `solid` is how many leading columns are genuine text: a control cut into `fit`'s `...` is
    /// not a target, or a press could operate a control the maker cannot see.
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

/// One power on one line: the mark, the identity and the composite badge. The badge is reserved
/// at the right and the identity fitted into the rest, since a plain `fit` would cut the badge,
/// which nothing else on screen restates. The mark takes the indent's place, costing nothing.
inline std::string power_row_text(const workshop::PowerStack& p, bool chosen,
                                  std::int64_t columns) {
    const std::string mark = chosen ? kSelectedMark : kUnselectedMark;
    std::string badge = is_composite_power(p) ? kCompositeBadge : "";
    std::int64_t room = columns - static_cast<std::int64_t>(mark.size() + badge.size());
    if (room < 4) {
        // Too narrow for both: the identity is what the maker navigates, and the detail block
        // still says composite.
        badge.clear();
        room = columns - static_cast<std::int64_t>(mark.size());
    }
    return fit(mark + fit(p.power, room) + badge, columns);
}

/// The selected detail rows, most-protected first: `yields <schema> v<N>` (what a sample would
/// claim, answerable without running anything), `[ Sample ]` for a Source (a control a maker
/// cannot reach is a missing feature), then the contribution stack, active first. All or
/// nothing at two rows: one row alone is half an answer.
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

/// A retained sample's header, the tense first and the omission counted; a narrow pane loses the
/// end of a long identity, never the claim or the count.
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

/// The whole Powers pane, spent against the room Workshop granted. Most-protected first: the
/// chrome row (never dropped), the list (up to three rows before anything else takes any), the
/// selected detail, the retained sample, `kHostResolution`, then `kPowersSource` from slack
/// only -- measured against the shipped four-row graphical and eight-row terminal defaults.
/// Staying inside the grant is an obligation: Workshop refuses an over-budget update whole.
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
    // The list is offered up to three rows first, the rest in priority order, and whatever
    // nobody wanted returns to the list.
    const std::int64_t left = rows - 1;
    const std::int64_t list_wants = view.population > 0 ? view.population : 1;
    // The floor is the smallest of three: a one-entry list does not reserve three rows.
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
            // An empty view and a filtered-away one are different facts; the second is counted.
            const std::int64_t here = static_cast<std::int64_t>(in_view_of(ui).size());
            const bool sources = ui.view == powers_view::kSources;
            say.say(here == 0 ? fit(sources ? kNoSourcesHere : kNoOperatorsHere, columns)
                              : all_hidden(here, ui.view, columns),
                    surface::role::kMuted);
        }
    } else if (list_budget > 0) {
        // The marker is a row like any other: with no row for it, nothing is written.
        const PowersWindow w = powers_window(view.population, view.cursor, list_budget);
        for (std::int64_t i = 0; i < w.count; ++i) {
            const workshop::PowerStack& p = *list[static_cast<std::size_t>(w.first + i)];
            const bool chosen_row = view.cursor == w.first + i;
            const std::int64_t row =
                say.say(detail::power_row_text(p, chosen_row, columns), entry_role(chosen_row),
                        entry_ground(chosen_row));
            // The whole row selects, and selecting is all it does.
            say.span(row, 0, columns, powers_control::kEntry, columns, p.power);
        }
        if (w.before > 0 || w.after > 0) {
            // A count, not an entry: it carries no span.
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
        // An entry and its omission marker are one demand on the budget.
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
        // The catalog census, a different question from the position marker: that counts the
        // list being navigated, this the whole reading. The verb agrees with the count.
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
