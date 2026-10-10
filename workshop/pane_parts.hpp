// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_PARTS_HPP
#define ZENGINE_WORKSHOP_PANE_PARTS_HPP

// The parts a pane names, judged by their form: the rules Workshop admits a picture's names by,
// which a pane may ask of its own before it sends them, and the parts of a composition a row map
// records, named by the pane's own rule and listed in the order its press reads them.
// Reference: workshop/docs/workshop-panes.md#a-pane-names-its-parts.

#include "workshop/pane_canvas_vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "component/row_map.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHAT IS WRONG WITH A PART'S NAME, or nullptr: 1 to `kMaxPanePartNameLen` bytes of printable
/// ASCII, and more than spaces.
inline const char* pane_part_name_problem(std::string_view name) noexcept {
    if (name.empty()) return "a part's name cannot be empty";
    if (name.size() > kMaxPanePartNameLen) return "a part's name is too long";
    bool anything = false;
    for (const char c : name) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x20u || byte >= 0x7Fu) return "a part's name must be printable ASCII";
        anything = anything || byte != ' ';
    }
    return anything ? nullptr : "a part's name needs more than spaces in it";
}

namespace detail {

/// The names of a list of parts, judged: how many, each name's form, and none twice. A part
/// named "" is a place its pane names nothing, and shares its name with nothing.
template <class Part>
std::string part_names_problem(const std::vector<Part>& parts) {
    if (parts.size() > kMaxPaneParts) {
        return "lists " + std::to_string(parts.size()) + " parts, more than " +
               std::to_string(kMaxPaneParts);
    }
    std::set<std::string_view> seen;
    for (const Part& part : parts) {
        if (part.name.empty()) continue;
        if (const char* wrong = pane_part_name_problem(part.name)) return wrong;
        if (!seen.insert(part.name).second) return "names two parts `" + part.name + "`";
    }
    return std::string();
}

} // namespace detail

/// WHAT IS WRONG WITH THE PARTS OF `rows` ROWS IN A ROOM `columns` WIDE, or "": the names, and
/// each part a run of at least one column of a row said, inside the room.
inline std::string row_parts_problem(const std::vector<PaneRowPart>& parts, std::int64_t rows,
                                     std::int64_t columns) {
    if (std::string wrong = detail::part_names_problem(parts); !wrong.empty()) return wrong;
    for (const PaneRowPart& part : parts) {
        if (part.row < 0 || part.row >= rows) {
            return "names `" + part.name + "` on a row its content does not say";
        }
        if (part.column < 0 || part.columns <= 0 || part.columns > columns ||
            part.column > columns - part.columns) {
            return "names `" + part.name + "` outside the room granted";
        }
    }
    return std::string();
}

/// WHAT IS WRONG WITH A PICTURE'S PARTS, or "": the names, and each part a positive rectangle.
inline std::string canvas_parts_problem(const std::vector<PaneCanvasPart>& parts) {
    if (std::string wrong = detail::part_names_problem(parts); !wrong.empty()) return wrong;
    for (const PaneCanvasPart& part : parts) {
        if (part.w <= 0 || part.h <= 0) {
            return "names `" + part.name + "` on a rectangle with no extent";
        }
    }
    return std::string();
}

/// THE PARTS OF ONE COMPOSITION, gathered in the order its pane reads a press: every place is
/// kept, and a part named "", or whose name the judge would refuse or an earlier part took, stays
/// a place unnamed, since a press there still reaches it. Past `kMaxPaneParts` the earliest go --
/// none takes a press from a part after it -- so a picture's parts gathered here are never what
/// refuses it.
template <class Part>
class PartNames {
public:
    /// Keeps `part` under its name, or unnamed, saying which.
    bool add(Part part) {
        const bool named =
            pane_part_name_problem(part.name) == nullptr && names_.insert(part.name).second;
        if (!named) {
            part.name.clear();
        }
        parts_.push_back(std::move(part));
        if (parts_.size() > kMaxPaneParts) {
            parts_.pop_front();
        }
        return named;
    }
    std::vector<Part> take() {
        names_.clear();
        std::vector<Part> out(std::make_move_iterator(parts_.begin()),
                              std::make_move_iterator(parts_.end()));
        parts_.clear();
        return out;
    }

private:
    std::deque<Part> parts_;
    std::set<std::string> names_;
};

/// THE PARTS A ROW MAP RECORDED, named by `name_of(meaning)` -- a row entire as the room's
/// `columns`, a run of columns as itself -- listed as its press reads them
/// (`component::RowMap::press_order`) and gathered as `PartNames` gathers them. A pane naming its
/// parts from what each one means keeps their names wherever the composition puts them.
template <class Meaning, class NameOf>
std::vector<PaneRowPart> row_parts(const component::RowMap<Meaning>& map, std::int64_t columns,
                                   NameOf&& name_of) {
    PartNames<PaneRowPart> out;
    const auto& spans = map.spans();
    for (const std::size_t at : map.press_order()) {
        const auto& span = spans[at];
        const bool whole = span.last == component::RowMap<Meaning>::kWholeRow;
        (void)out.add(PaneRowPart{name_of(span.meaning), span.row, whole ? 0 : span.first,
                                  whole ? columns : span.last - span.first + 1});
    }
    return out.take();
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_PARTS_HPP
