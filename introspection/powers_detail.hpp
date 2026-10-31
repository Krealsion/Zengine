// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_POWERS_DETAIL_HPP
#define ZENGINE_INTROSPECTION_POWERS_DETAIL_HPP

// The Powers pane's words and the geometry of its rows, pure text over the discovery door's rows:
// which chrome segments a width carries, which run of the list a budget shows and what it says it
// hid, a power's line, the selected detail with its contribution stack, and a retained sample's
// header and body. `project_powers_ui` (powers.hpp) spends them; none of it reads the pane's state.
// Pane law: agents/panes.md

#include "loaded.hpp"   // `fit`, `kElided`, the marks
#include "resolved.hpp" // `kHostItself`

#include "workshop/powers_vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::introspection {

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

// ---- The window ------------------------------------------------------------------

/// Which run of the filtered list a budget shows with the cursor visible, and how much each side
/// hides: Workshop's `list_window` rules restated, since a provider is a stranger to Workshop's
/// composition -- a fitting population whole, the cursor inside, every omission counted in the
/// budget. The second provider-side copy, after `composer::window_of`: one provider-side window
/// rule is the seam they share. Total over every budget and cursor.
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

/// The widest a layout may keep a control's column, and the fewest and most spaces between two
/// segments: a control's column is never narrower than its own label.
inline constexpr std::int64_t kMostControlWidth = 40;
inline constexpr std::int64_t kFewestGap = 1;
inline constexpr std::int64_t kMostGap = 4;

inline std::int64_t find_fixed() {
    return static_cast<std::int64_t>(std::char_traits<char>::length(kFindLabel));
}

} // namespace detail

/// THE CHROME ROW'S WIDTHS AS A LAYOUT KEEPS THEM: the columns the view control and the composite
/// control are drawn in, each padded past its label, and the spaces between two segments.
struct ChromeWidths {
    std::int64_t view = detail::kViewWidth;
    std::int64_t composite = detail::kCompositeWidth;
    std::int64_t gap = detail::kGap;
    friend bool operator==(const ChromeWidths&, const ChromeWidths&) = default;
};

inline ChromeFit fit_chrome(const std::string& position, std::int64_t columns,
                            const ChromeWidths& widths = {}) {
    ChromeFit out;
    const std::int64_t pos_w = static_cast<std::int64_t>(position.size());
    const std::int64_t find_min = widths.gap + detail::kFindMin;
    const std::int64_t with_pos = widths.gap + pos_w;
    const std::int64_t with_comp = widths.gap + widths.composite;
    std::int64_t used = widths.view;
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
        used += widths.gap + detail::find_fixed();
        out.query_room = columns - used;
    }
    return out;
}

namespace detail {

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
         graphics::role::kMuted);
    if (is_source_power(row)) {
        line(std::string("  ") + kSampleControl, graphics::role::kAccent, true);
    }
    if (!row.about.empty()) {
        line("  " + printable(row.about), graphics::role::kFill);
    }
    if (!row.offered) {
        line(kNotOfferedLine, graphics::role::kMuted);
    }
    line("  " + printable(row.signature), graphics::role::kMuted);
    if (described == nullptr) {
        // The answer about this identity has not come: what is in force, from its row alone.
        line("  active    " + (row.provider.empty() ? std::string(kHostItself)
                                                    : printable(row.provider)) +
                 (is_composite_power(row) ? kCompositeBadge : ""),
             graphics::role::kFill);
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
        line(std::move(said), live ? graphics::role::kFill : graphics::role::kMuted);
    }
    return d;
}

/// A retained sample's header, the tense first and the omission counted; a narrow pane loses the
/// end of a long identity, never the claim or the count.
inline std::string sample_header(bool ok, const std::string& identity, std::size_t hidden,
                                 std::int64_t columns) {
    const std::string lead = std::string(ok ? kSampledWhenAsked : kSampleRefusedWord) + "  ";
    const std::string tail =
        hidden > 0 ? ("  " + std::string(kElided) + " " + std::to_string(hidden) + " more") : "";
    const std::int64_t room = columns - static_cast<std::int64_t>(lead.size() + tail.size());
    return fit(lead + fit(printable(identity), room) + tail, columns);
}

/// EVERY LINE A RETAINED SAMPLE WOULD SPEND, header excluded: the value's lines when it was
/// answered, else its refusal's sentence.
inline const std::vector<std::string>& sample_body(bool ok, const std::vector<std::string>& lines,
                                                   const std::string& reason,
                                                   std::vector<std::string>& scratch) {
    if (ok) {
        return lines;
    }
    scratch.clear();
    if (!reason.empty()) {
        scratch.push_back(reason);
    }
    return scratch;
}

} // namespace detail

} // namespace zengine::introspection

#endif // ZENGINE_INTROSPECTION_POWERS_DETAIL_HPP
