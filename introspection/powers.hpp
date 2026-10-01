// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_POWERS_HPP
#define ZENGINE_INTROSPECTION_POWERS_HPP

// The Powers pane as a weaver uses it, pure machinery over the discovery door's answers:
// `PowersUi` (all the pane knows that is not a fact about the host), `powers_question` (the search
// the view, the query and the filter make), `project_powers_ui` (that state and a budget become
// rows and what each place means) and `target_at` (a press becomes its one meaning). No bus here:
// the weave owns when to ask and whom to believe. Sources and Operators are the door's `kind`,
// derived from the catalog and never authored; composite is independent of both.
// Pane law: agents/panes.md

// Browsing cannot evaluate: this links no operator target and reads only the door's rows. Which
// rows match a query is the door's to say, the same rows it says to Flow and the Terminal; the
// pane keeps the last answer between asks, replaced whole and dropped at every grant. A retained
// sample is history: it never claims to be current, and its row leads with the tense.

#include "loaded.hpp"   // `fit`, `kElided`, the mark, the entry roles
#include "resolved.hpp" // `kHostResolution`, `kPowersSource`, `kHostItself`, `counted`

#include "component/text_box.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/powers_vocabulary.hpp"

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

/// What one place in this pane means to a weaver, and each place means exactly one: a row that
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
/// keep both, and only the door saying an identity is gone clears one.
struct PowersUi {
    std::int64_t view = powers_view::kSources;

    /// The one editable field in this pane -- which is why typing needs no mode: a
    /// printable character has exactly one place it could go.
    component::TextBox query;

    /// Shared by both views, and asked of the door with the query as logical AND. It reads
    /// the ACTIVE contribution, because that is the construction whose code runs.
    bool composite_only = false;

    std::string selected_source;
    std::string selected_operator;

    /// THE LAST ADMITTED ANSWER to this pane's search: one page of the door's rows, replaced
    /// whole by the next answer, dropped at every grant, never evidence about the catalog now.
    workshop::PowersFound reading;
    bool read = false; ///< has any answer been admitted into this pane at all

    /// What the door last said of one identity: its row and its whole contribution stack. Shown
    /// only while it is about the identity selected now.
    workshop::PowerDescribed described;

    RetainedSample sample;

    const std::string& selected() const noexcept {
        return view == powers_view::kSources ? selected_source : selected_operator;
    }
    void select(std::string identity) {
        (view == powers_view::kSources ? selected_source : selected_operator) =
            std::move(identity);
    }
};

// ---- The question, and what its answer shows -------------------------------------

/// THE SEARCH THIS PANE ASKS THE DOOR: the view as a kind (a conversion is an operator, so the
/// Operators view finds both), the query as text, the filter as a construction, and the most
/// rows an answer carries. Matching is the door's; nothing here re-matches what it said.
inline workshop::FindPowers powers_question(const PowersUi& ui) {
    workshop::FindPowers asked;
    asked.kind = ui.view == powers_view::kSources ? workshop::kSourceKind
                                                  : workshop::kOperatorKind;
    if (!ui.query.text().empty()) {
        asked.text = ui.query.text();
    }
    if (ui.composite_only) {
        asked.construction = workshop::kCompositeConstruction;
    }
    asked.limit = workshop::kMaxPowerRows;
    return asked;
}

/// Is this row a Source? The door's `kind`, read off the contribution in force; nothing here
/// classifies anything.
inline bool is_source_power(const workshop::PowerRow& row) noexcept {
    return row.kind == workshop::kSourceKind;
}

/// DOES THIS POWER'S ACTIVE CONTRIBUTION HAVE KNOWN COMPOSITE CONSTRUCTION? That is
/// the whole claim. It is not openability, editability or safety, and no surface in
/// this build can show the graph.
inline bool is_composite_power(const workshop::PowerRow& row) noexcept {
    return row.construction == workshop::kCompositeConstruction;
}

/// Does this row belong in the view being shown?
inline bool in_view(const workshop::PowerRow& row, std::int64_t view) noexcept {
    return is_source_power(row) == (view == powers_view::kSources);
}

/// The list the weaver navigates: the answer's rows that belong to this view and filter, in the
/// order the door gave them. The door asked those same questions, so this keeps a row only while
/// an answer to an earlier search is still on screen; the query is never re-matched here.
inline std::vector<const workshop::PowerRow*> filtered_of(const PowersUi& ui) {
    std::vector<const workshop::PowerRow*> out;
    for (const workshop::PowerRow& row : ui.reading.rows) {
        if (in_view(row, ui.view) && (!ui.composite_only || is_composite_power(row))) {
            out.push_back(&row);
        }
    }
    return out;
}

/// Where the cursor is in that list, or -1. Hidden is not absent: a selection the query, the
/// filter or the other view excludes is held, merely unmarked.
inline std::int64_t cursor_in(const std::vector<const workshop::PowerRow*>& list,
                              std::string_view identity) noexcept {
    if (identity.empty()) {
        return -1;
    }
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i]->identity == identity) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

/// The selected power as the door last described it, if that answer is about it and it belongs
/// to this view -- the whole of what the detail shows, the stack included.
inline const workshop::PowerDescribed* described_of(const PowersUi& ui) {
    const std::string& want = ui.selected();
    if (want.empty() || !ui.described.ok || ui.described.identity != want ||
        !in_view(ui.described.row, ui.view)) {
        return nullptr;
    }
    return &ui.described;
}

/// The selected power's row: the described one, or else the answer's own row for it.
inline const workshop::PowerRow* selected_of(const PowersUi& ui) {
    if (const workshop::PowerDescribed* d = described_of(ui); d != nullptr) {
        return &d->row;
    }
    const std::string& want = ui.selected();
    if (want.empty()) {
        return nullptr;
    }
    for (const workshop::PowerRow& row : ui.reading.rows) {
        if (row.identity == want && in_view(row, ui.view)) {
            return &row;
        }
    }
    return nullptr;
}

/// The identity a sample gesture would spend, or empty: Sources only. `op::sample` refuses an
/// Operator at the spend; the pane simply offers no gesture for one.
inline std::string sampleable(const PowersUi& ui) {
    const workshop::PowerRow* row = selected_of(ui);
    return (row != nullptr && is_source_power(*row)) ? row->identity : std::string();
}

/// Move the cursor one place through the visible list. A hidden selection starts from the list's
/// beginning, rather than inventing a place for what the weaver cannot see.
inline void move_cursor(PowersUi& ui, std::int64_t delta) {
    const std::vector<const workshop::PowerRow*> list = filtered_of(ui);
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
    ui.select(list[static_cast<std::size_t>(next)]->identity);
}

/// What the door said of one identity, taken in: kept for the detail, and the one fact that may
/// clear a selection -- the identity is gone, or it no longer belongs to the view holding it.
/// Presentation may hide a selection; only the door's answer about that identity invalidates it.
inline void take_described(PowersUi& ui, workshop::PowerDescribed said) {
    for (const std::int64_t view : {powers_view::kSources, powers_view::kOperators}) {
        std::string& held =
            view == powers_view::kSources ? ui.selected_source : ui.selected_operator;
        if (!held.empty() && held == said.identity && (!said.ok || !in_view(said.row, view))) {
            held.clear();
        }
    }
    ui.described = std::move(said);
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

/// What a power its contributor offers to no one else's composition says, beside its identity
/// and in its detail: listed, and said to be another participant's own.
inline constexpr const char* kNotOfferedBadge = " (not offered)";
inline constexpr const char* kNotOfferedLine = "  not offered for reuse: another participant's own";

/// What a retained sample is, leading with the tense: `fit` cuts the tail, so a narrow pane loses
/// the identity (recoverable from the list) and never the claim.
inline constexpr const char* kSampledWhenAsked = "sampled when asked";
inline constexpr const char* kSampleRefusedWord = "sample refused when asked";

/// The empty states, kept apart because they are different facts.
inline constexpr const char* kNoSourcesHere = "no sources resolve here";
inline constexpr const char* kNoOperatorsHere = "no operators resolve here";
inline constexpr const char* kNoneMatch = "none here match the current search";

/// `+ 12 more match -- narrow the search`: the rows past one answer's page are counted, never
/// silently absent.
inline std::string beyond_page(std::int64_t more, std::int64_t columns) {
    return fit("  + " + std::to_string(more) + " more match -- narrow the search", columns);
}

/// A row carries printable ASCII and a contributor's words may carry any byte, so another byte
/// is drawn as `?`; the words themselves are not rewritten anywhere.
inline std::string printable(std::string text) {
    for (char& c : text) {
        const unsigned char b = static_cast<unsigned char>(c);
        if (b < 0x20u || b >= 0x7Fu) {
            c = '?';
        }
    }
    return text;
}

// ---- The chrome row --------------------------------------------------------------

/// The position marker, against the list the weaver navigates: `-` for no cursor (zero is a
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
    const std::vector<const workshop::PowerRow*> list = filtered_of(ui);
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
    /// not a target, or a press could operate a control the weaver cannot see.
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

/// One power on one line: the mark, the identity and its badges. The badges are reserved at the
/// right and the identity fitted into the rest, since a plain `fit` would cut them, and nothing
/// else in the list restates them. The mark takes the indent's place, costing nothing.
inline std::string power_row_text(const workshop::PowerRow& row, bool chosen,
                                  std::int64_t columns) {
    const std::string mark = chosen ? kSelectedMark : kUnselectedMark;
    std::string badge = std::string(is_composite_power(row) ? kCompositeBadge : "") +
                        (row.offered ? "" : kNotOfferedBadge);
    std::int64_t room = columns - static_cast<std::int64_t>(mark.size() + badge.size());
    if (room < 4) {
        // Too narrow for both: the identity is what the weaver navigates, and the detail block
        // still says what the badges said.
        badge.clear();
        room = columns - static_cast<std::int64_t>(mark.size());
    }
    return fit(mark + fit(printable(row.identity), room) + badge, columns);
}

/// The selected detail rows, most-protected first: `yields <schema> v<N>` (what a sample would
/// claim, answerable without running anything), `[ Sample ]` for a Source (a control a weaver
/// cannot reach is a missing feature), what it is for in its contributor's words, whether it is
/// offered, its signature, then the contribution stack, active first. All or nothing at two rows:
/// one row alone is half an answer.
struct Detail {
    std::vector<std::string> texts;
    std::vector<std::int64_t> roles;
    std::vector<bool> is_control;
};

inline Detail detail_rows(const workshop::PowerRow& row, const workshop::PowerDescribed* described,
                          std::int64_t columns) {
    Detail d;
    const auto line = [&d, columns](std::string text, std::int64_t role, bool control = false) {
        d.texts.push_back(fit(std::move(text), columns));
        d.roles.push_back(role);
        d.is_control.push_back(control);
    };
    line(row.outputs.name.empty()
             ? std::string("  yields (not reported)")
             : "  yields " + printable(row.outputs.name) + " v" +
                   std::to_string(row.outputs.version),
         surface::role::kMuted);
    if (is_source_power(row)) {
        line(std::string("  ") + kSampleControl, surface::role::kAccent, true);
    }
    if (!row.about.empty()) {
        line("  " + printable(row.about), surface::role::kFill);
    }
    if (!row.offered) {
        line(kNotOfferedLine, surface::role::kMuted);
    }
    line("  " + printable(row.signature), surface::role::kMuted);
    if (described == nullptr) {
        // The answer about this identity has not come: what is in force, from its row alone.
        line("  active    " + (row.provider.empty() ? std::string(kHostItself)
                                                    : printable(row.provider)) +
                 (is_composite_power(row) ? kCompositeBadge : ""),
             surface::role::kFill);
        return d;
    }
    for (std::size_t i = described->stack.size(); i > 0; --i) {
        const workshop::PowerLayer& c = described->stack[i - 1];
        const bool live = i == described->stack.size();
        std::string said = live ? "  active    " : "  shadowed  ";
        said += c.provider.empty() ? kHostItself : printable(c.provider);
        if (c.construction == workshop::kCompositeConstruction) {
            said += kCompositeBadge;
        }
        if (!live && !c.about.empty()) {
            said += " -- " + printable(c.about);
        }
        line(std::move(said), live ? surface::role::kFill : surface::role::kMuted);
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
    return fit(lead + fit(printable(s.identity), room) + tail, columns);
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

    const std::vector<const workshop::PowerRow*> list = filtered_of(ui);
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
    // nobody wanted returns to the list. Rows past the answer's page are one more line of it.
    const std::int64_t beyond =
        ui.reading.total > static_cast<std::int64_t>(ui.reading.rows.size())
            ? ui.reading.total - static_cast<std::int64_t>(ui.reading.rows.size())
            : 0;
    const std::int64_t left = rows - 1;
    const std::int64_t list_wants =
        (view.population > 0 ? view.population : 1) + (beyond > 0 ? 1 : 0);
    // The floor is the smallest of three: a one-entry list does not reserve three rows.
    const std::int64_t wanted_floor = list_wants < 3 ? list_wants : 3;
    const std::int64_t floor_rows = left < wanted_floor ? left : wanted_floor;
    std::int64_t spare = left - floor_rows;

    const workshop::PowerRow* chosen = selected_of(ui);
    detail::Detail block;
    if (chosen != nullptr) {
        block = detail::detail_rows(*chosen, described_of(ui), columns);
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
    std::int64_t list_budget = floor_rows + spare;

    // ---- the list ----------------------------------------------------------------
    if (view.population == 0) {
        if (list_budget > 0 && !ui.reading.ok && !ui.reading.reason.empty()) {
            // A search the door refused is not a search nothing matched: its sentence is the list.
            say.say(fit(printable(ui.reading.reason), columns), surface::role::kAlert);
            --list_budget;
        } else if (list_budget > 0) {
            // An empty catalog view and a search nothing matched are different facts.
            const bool searching = !ui.query.text().empty() || ui.composite_only;
            const bool sources = ui.view == powers_view::kSources;
            say.say(fit(searching ? kNoneMatch : (sources ? kNoSourcesHere : kNoOperatorsHere),
                        columns),
                    surface::role::kMuted);
            --list_budget;
        }
    } else if (list_budget > 0) {
        // The page's own count is a row, taken from the list's budget before the window.
        const std::int64_t entries = beyond > 0 && list_budget > 1 ? list_budget - 1 : list_budget;
        // The marker is a row like any other: with no row for it, nothing is written.
        const PowersWindow w = powers_window(view.population, view.cursor, entries);
        for (std::int64_t i = 0; i < w.count; ++i) {
            const workshop::PowerRow& p = *list[static_cast<std::size_t>(w.first + i)];
            const bool chosen_row = view.cursor == w.first + i;
            const std::int64_t row =
                say.say(detail::power_row_text(p, chosen_row, columns), entry_role(chosen_row),
                        entry_ground(chosen_row));
            // The whole row selects, and selecting is all it does.
            say.span(row, 0, columns, powers_control::kEntry, columns, p.identity);
        }
        if (w.before > 0 || w.after > 0) {
            // A count, not an entry: it carries no span.
            say.say(powers_omission(w, columns), surface::role::kMuted);
        }
        list_budget -= entries;
    }
    if (beyond > 0 && list_budget > 0) {
        say.say(beyond_page(beyond, columns), surface::role::kMuted);
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
        // list being navigated, this the whole catalog the door read. The verb agrees.
        const std::int64_t identities = ui.reading.powers;
        say.say(fit(powers_said(identities) + (identities == 1 ? " resolves" : " resolve") +
                        " here -- from " + providers_said(ui.reading.providers),
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
