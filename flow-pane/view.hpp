// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_VIEW_HPP
#define ZENGINE_FLOW_PANE_VIEW_HPP
#include "flow-pane/find.hpp"
#include "flow-pane/model.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/pane_parts.hpp"
#include "surface/pointing.hpp"
#include <limits>

namespace zengine::flow_pane {
namespace ws = zengine::workshop;
namespace ink = zengine::surface::role;
/// Flow's authored grid: `unit` to one character of the medium's text, the unit its saved node
/// places and its pan are in. Presentation scales it to the room's pixels (`GridProjection`).
inline constexpr std::int64_t unit = 48;
struct Hit {
  std::int64_t x = 0, y = 0, w = 0, h = 0;
  std::string action;
  std::vector<std::string> args;
  std::int64_t subject = 0;
  bool contains(std::int64_t px, std::int64_t py, std::int64_t grain) const {
    return zengine::surface::px_span_contains(x, w, px, grain) &&
           zengine::surface::px_span_contains(y, h, py, grain);
  }
};
struct Picture {
  ws::PaneCanvasContent content;
  std::vector<Hit> hits;
  std::vector<ws::CanvasTextBox> text_clips;
  std::int64_t revision = 0, width = 0, height = 0, grain = 1;
  /// Where the graph's authored origin sits in this picture, in authored grid units: a drop lands
  /// a node where it was released. Unset off the graph page.
  std::optional<std::int64_t> graph_top;
  std::int64_t pan_x = 0, pan_y = 0, zoom = 100;
  /// The selected node's box, top and bottom, and the graph viewport's, in authored grid units:
  /// what `reveal_selected` reads to bring a node into view.
  std::optional<std::pair<std::int64_t, std::int64_t>> node_extent, graph_extent;
  std::int64_t view_upper = 0, view_lower = 0;
  const Hit *hit(std::int64_t x, std::int64_t y) const {
    for (auto at = hits.rbegin(); at != hits.rend(); ++at)
      if (at->contains(x, y, grain))
        return &*at;
    return nullptr;
  }
};
inline std::string clean(std::string text) {
  for (auto &c : text)
    if (static_cast<unsigned char>(c) < 32 ||
        static_cast<unsigned char>(c) > 126)
      c = '?';
  return text;
}
// Workspace coordinates use an authored square grid. Only presentation spends
// the medium's independent horizontal advance and padded text-row height.
struct GridProjection {
  std::int64_t advance = unit, row = unit, padding = 0;
  explicit GridProjection(const ws::PaneCanvasRoom &room) {
    const auto metrics = ws::canvas_text_metrics(room);
    advance = metrics.advance;
    row = zengine::surface::add_cells(metrics.line, 2 * metrics.inset);
    const auto pad = 2 * metrics.inset * unit;
    padding = pad / advance + (pad % advance != 0 ? 1 : 0);
  }
  static std::int64_t scaled(std::int64_t value, std::int64_t factor,
                             std::int64_t divisor) {
    // Saturation only affects coordinates already far outside an ordinary room.
    // A hostile metric must not turn clipping into signed multiplication overflow.
    if (value >= 0) return zengine::surface::mul_px(value, factor) / divisor;
    const auto magnitude = value == (std::numeric_limits<std::int64_t>::min)()
        ? (std::numeric_limits<std::int64_t>::max)() : -value;
    return -zengine::surface::mul_px(magnitude, factor) / divisor;
  }
  std::int64_t x(std::int64_t value) const { return scaled(value, advance, unit); }
  std::int64_t y(std::int64_t value) const { return scaled(value, row, unit); }
  std::int64_t grid_x(std::int64_t value) const { return scaled(value, unit, advance); }
  std::int64_t grid_y(std::int64_t value) const { return scaled(value, unit, row); }
  template <class Box> void map(Box &box) const {
    const auto right = x(zengine::surface::add_cells(box.x, box.w));
    const auto bottom = y(zengine::surface::add_cells(box.y, box.h));
    box.x = x(box.x); box.y = y(box.y);
    box.w = zengine::surface::sub_px(right, box.x);
    box.h = zengine::surface::sub_px(bottom, box.y);
  }
};

inline Picture project_picture(Picture view, const ws::PaneCanvasRoom &room) {
  const GridProjection grid(room);
  auto clip = [&](auto &r) {
    grid.map(r);
    const auto left = std::max(std::int64_t{0}, r.x);
    const auto top = std::max(std::int64_t{0}, r.y);
    const auto right = std::min(room.width, zengine::surface::add_cells(r.x, r.w));
    const auto bottom = std::min(room.height, zengine::surface::add_cells(r.y, r.h));
    r.x = left; r.y = top;
    r.w = std::max(std::int64_t{0}, right - left);
    r.h = std::max(std::int64_t{0}, bottom - top);
  };
  for (auto &r : view.content.rects) clip(r);
  // The authored ground also covers any fractional grid remainder at the edge.
  if (!view.content.rects.empty())
    view.content.rects.front() = {0, 0, room.width, room.height, ink::kGround};
  std::erase_if(view.content.rects, [](const auto &r) { return r.w <= 0 || r.h <= 0; });
  for (auto &h : view.hits) clip(h);
  std::erase_if(view.hits, [](const auto &h) { return h.w <= 0 || h.h <= 0; });
  for (std::size_t i = 0; i < view.content.texts.size(); ++i) {
    auto &text = view.content.texts[i];
    auto bounds = view.text_clips.at(i);
    clip(bounds);
    text.x = grid.x(text.x); text.y = grid.y(text.y);
    const auto original = text.text.size();
    auto layout = ws::clip_canvas_text(text, bounds, room);
    if (!layout.visible()) { text.text.clear(); text.caret_col = -1; continue; }
    const bool left_cut = layout.first_column > 0;
    const bool right_cut = layout.first_column + layout.text.text.size() < original;
    text = std::move(layout.text);
    const auto mark = std::min(std::size_t{3}, text.text.size());
    if (left_cut) text.text.replace(0, mark, mark, '.');
    if (right_cut) text.text.replace(text.text.size() - mark, mark, mark, '.');
  }
  std::erase_if(view.content.texts, [](const auto &text) { return text.text.empty() && text.caret_col < 0; });
  view.text_clips.clear();
  view.width = room.width; view.height = room.height;
  return view;
}
inline Picture finish_picture(Picture view, const ws::PaneCanvasRoom &room) {
  view = project_picture(std::move(view), room);
  std::size_t bytes = 0;
  for (const auto &text : view.content.texts) bytes += text.text.size();
  if (view.content.rects.size() <= ws::kPaneCanvasMaxRects &&
      view.content.texts.size() <= ws::kPaneCanvasMaxTexts &&
      bytes <= ws::kPaneCanvasMaxTextBytes) return view;

  // A partial graph would look like a different graph. Show a bounded recovery
  // picture instead, keeping save/export and view controls available.
  const GridProjection grid(room);
  view.width = grid.grid_x(room.width); view.height = grid.grid_y(room.height);
  view.content.rects = {{0, 0, view.width, view.height, ink::kGround}};
  view.content.texts.clear(); view.hits.clear();
  auto text = [&](std::int64_t x, std::int64_t y, std::string value,
                  std::int64_t role = ink::kAlert) {
    view.content.texts.push_back({x, y, std::move(value), role});
    view.text_clips.push_back({0, 0, view.width, view.height});
  };
  text(0, 0, "View limit reached: pan/zoom or choose a smaller graph.");
  text(0, unit, "Draft intact. Save or export it before changing the graph.");
  std::int64_t x = 0, y = 3 * unit;
  auto button = [&](const std::string &title, const std::string &action) {
    const auto w = static_cast<std::int64_t>(title.size() + 2) * unit + grid.padding;
    if (x + w > view.width) { x = 0; y += unit; }
    if (w > view.width || y + unit > view.height) return;
    text(x, y, "[" + title + "]", ink::kAccent);
    view.hits.push_back({x, y, w, unit, action, {}, 0});
    x += w + unit;
  };
  button("Save", "ask-save"); button("Export", "ask-export");
  button("Graph", "page-graph"); button("State", "page-state");
  button("Messages", "page-messages"); button("Events", "page-events");
  button("Reset view", "fit"); button("Zoom in", "zoom-in");
  button("Zoom out", "zoom-out");
  return project_picture(std::move(view), room);
}
inline Picture picture(const Model &model, const ws::PaneCanvasRoom &canvas_room,
                       std::int64_t sequence) {
  const GridProjection grid(canvas_room);
  auto room = canvas_room;
  room.width = grid.grid_x(canvas_room.width);
  room.height = grid.grid_y(canvas_room.height);
  Picture view;
  view.content.pane = "flow";
  view.content.grant = room.grant;
  view.content.picture = sequence;
  view.revision = model.workspace.graph.project.definition.revision;
  view.width = room.width;
  view.height = room.height;
  view.grain = std::max(std::int64_t{1}, room.grain);
  auto rect = [&](std::int64_t x, std::int64_t y, std::int64_t w,
                  std::int64_t h, std::int64_t role = ink::kFill) {
    if (w > 0 && h > 0)
      view.content.rects.push_back({x, y, w, h, role});
  };
  const auto grain = view.grain;
  const auto stroke_x = std::max(std::int64_t{4}, zengine::surface::add_cells(
      grid.grid_x(grain), grid.x(grid.grid_x(grain)) < grain ? 1 : 0));
  const auto stroke_y = std::max(std::int64_t{4}, zengine::surface::add_cells(
      grid.grid_y(grain), grid.y(grid.grid_y(grain)) < grain ? 1 : 0));
  auto stroke = [&](std::int64_t x, std::int64_t y, std::int64_t w,
                    std::int64_t h, std::int64_t role) {
    // Strokes survive device quantization; boxes and hit regions keep their
    // measured extents. Clip these visible strokes through the graph viewport.
    rect(x, y, std::max(w, stroke_x), std::max(h, stroke_y), role);
  };
  ws::CanvasTextBox text_clip{0, 0, room.width, room.height};
  auto label = [&](std::int64_t x, std::int64_t y, std::string text,
                   std::int64_t role = ink::kFill) {
    view.content.texts.push_back({x, y, clean(std::move(text)), role});
    view.text_clips.push_back(text_clip);
  };
  auto button = [&](std::int64_t x, std::int64_t y, std::string title,
                    std::string action,
                    std::vector<std::string> args = std::vector<std::string>{},
                    std::int64_t subject = 0) {
    const auto w = static_cast<std::int64_t>(title.size() + 2) * unit + grid.padding;
    label(x, y, "[" + title + "]", ink::kAccent);
    view.hits.push_back(
        {x, y, w, unit, std::move(action), std::move(args), subject});
    return w + unit / 2;
  };
  // The graph owns its ground; selection remains in Workshop's border/title.
  rect(0, 0, room.width, room.height, ink::kGround);
  if (room.width < 12 * unit || room.height < 7 * unit) {
    label(0, 0, "Flow needs more room");
    label(0, unit, "Resize this pane");
    return finish_picture(std::move(view), canvas_room);
  }
  std::int64_t bx = 0, by = 0;
  auto bar = [&](std::string title, std::string action) {
    const auto needed = static_cast<std::int64_t>(title.size() + 3) * unit + grid.padding;
    if (bx + needed > room.width) {
      bx = 0;
      by += unit;
    }
    bx += button(bx, by, std::move(title), std::move(action));
  };
  bar("New", "ask-new");
  bar("Open", "ask-open");
  bar("Import", "ask-import");
  bar("Export", "ask-export");
  bar(model.dirty ? "Save*" : "Save", "ask-save");
  bar(model.running ? "Running" : "Run", "run");
  bar("Apply", "apply");
  bar("Stop", "stop");
  bar("Inspect", "inspect");
  bar("Catalog", "catalog");
  bar("Discard", "ask-discard");
  bx = 0;
  by += unit;
  bar("Graph", "page-graph");
  bar("State", "page-state");
  bar("Messages", "page-messages");
  bar("Library", "page-library");
  bar("Events", "page-events");
  const auto top = by + 2 * unit;
  const auto bottom = room.height - 2 * unit;
  label(0, room.height - unit, model.notice, ink::kAccent);
  label(0, room.height - 2 * unit,
        model.workspace.graph.project.definition.name + " / draft r" +
            std::to_string(view.revision) +
            (model.running ? " / interpreted live" : " / stopped"));
  text_clip = {0, top, room.width, std::max(std::int64_t{0}, bottom - top)};
  const auto body_hits = view.hits.size();
  auto finish_body = [&] {
    // Body rows stop before the footer. Omit a partial row's action with its
    // text, so shrinking cannot leave an invisible action over the status.
    for (auto i = body_hits; i < view.hits.size(); ++i) {
      auto &hit = view.hits[i];
      if (hit.y < top || hit.y >= bottom || hit.h > bottom - hit.y) hit.h = 0;
    }
    return finish_picture(std::move(view), canvas_room);
  };
  if (model.dialog) {
    const auto &dialog = *model.dialog;
    label(0, top, dialog.title, ink::kAccent);
    std::int64_t y = top + 2 * unit;
    for (std::size_t i = 0; i < dialog.entries.size(); ++i) {
      const auto &e = dialog.entries[i];
      const auto prefix = (i == dialog.selected ? "> " : "  ") + e.label + ": ";
      auto text = e.text;
      const auto columns = std::max(
          std::int64_t{1},
          room.width / unit - static_cast<std::int64_t>(prefix.size()) - 1);
      text.keep_caret_visible(columns);
      // A selected entry's caret after its text stands on a blank of its own.
      const bool after = i == dialog.selected &&
          text.caret_column() >= text.visible(columns).size();
      label(0, y, prefix + text.visible(columns) + (after ? " " : ""));
      if (i == dialog.selected) {
        auto &run = view.content.texts.back();
        const auto prefix_columns = static_cast<std::int64_t>(prefix.size());
        run.caret_col = prefix_columns + static_cast<std::int64_t>(text.caret_column());
        const auto selection = text.visible_selection(columns);
        if (selection.present()) {
          run.sel_begin_col = prefix_columns + selection.begin;
          run.sel_end_col = prefix_columns + selection.end;
        }
      }
      view.hits.push_back(
          {0, y, room.width, unit, "dialog-field", {std::to_string(i)}, 0});
      y += 2 * unit;
    }
    button(0, y, "Confirm", "dialog-confirm");
    button(12 * unit, y, "Cancel", "dialog-cancel");
    label(0, y + 2 * unit, "Tab changes field; Enter confirms; Escape cancels");
    return finish_body();
  }
  if (model.dropped) {
    // A DROPPED VALUE'S PAGE: what came, and what it can become here; nothing is done until the
    // maker chooses, and Escape or Cancel puts it down.
    const auto summary = summarize_drop(model.dropped->value);
    label(0, top, summary.title, ink::kAccent);
    std::int64_t y = top + unit;
    for (const auto &row : summary.rows) {
      if (y >= top + 6 * unit) break;
      label(unit, y, row, ink::kMuted);
      y += unit;
    }
    if (!summary.why.empty()) {
      label(unit, y, "Not declarable: " + summary.why, ink::kAlert);
      y += unit;
    }
    y += unit;
    for (const auto &offer : drop_offers(model)) {
      if (y >= bottom) break;
      button(0, y, offer.title, offer.action, offer.args);
      y += unit;
    }
    return finish_body();
  }
  const auto &graph = model.workspace.graph;
  const auto &def = graph.project.definition;
  if (model.page == Page::Graph) {
    view.graph_top = top;
    view.pan_x = model.workspace.pan_x;
    view.pan_y = model.workspace.pan_y;
    view.zoom = model.workspace.zoom;
    const auto sidebar_labels = view.content.texts.size(), sidebar_hits = view.hits.size();
    std::int64_t y = top;
    button(0, y, "Add trigger", "ask-trigger");
    if (!def.on.empty() && !def.emits.empty())
      button(14 * unit, y, "Emit", "ask-emit");
    y += unit;
    for (std::size_t t = 0; t < def.on.size(); ++t) {
      if (y >= bottom)
        break;
      const auto &on = def.on[t];
      button(0, y, (t == model.trigger() ? "> " : "") + on.message->name(),
             "select-trigger", {std::to_string(t)});
      y += unit;
      // WHAT THE ACTIVE TRIGGER PUBLISHES after its write, each removable.
      if (t != model.trigger())
        continue;
      for (std::size_t e = 0; e < on.emits.size() && y < bottom; ++e) {
        label(unit, y, "emits " + on.emits[e].message->name(), ink::kMuted);
        button(18 * unit, y, "x", "emit-remove", {std::to_string(e)});
        y += unit;
      }
    }
    if (def.on.empty()) {
      label(0, y + unit, "Add state and a message, then a trigger.");
      return finish_body();
    }
    const auto &on = def.on.at(model.trigger());
    y += unit;
    // THE SEARCH LINE, the one field outside a dialog: typing reaches it with no gesture.
    {
      const std::string prefix = "find: ";
      auto text = model.search;
      const auto columns = std::max(std::int64_t{1},
                                    21 - static_cast<std::int64_t>(prefix.size()));
      text.keep_caret_visible(columns);
      // A caret after the query stands on a blank of its own.
      const bool after = text.caret_column() >= text.visible(columns).size();
      label(0, y, prefix + text.visible(columns) + (after ? " " : ""));
      auto &run = view.content.texts.back();
      const auto prefix_columns = static_cast<std::int64_t>(prefix.size());
      run.caret_col = prefix_columns + static_cast<std::int64_t>(text.caret_column());
      const auto selection = text.visible_selection(columns);
      if (selection.present()) {
        run.sel_begin_col = prefix_columns + selection.begin;
        run.sel_end_col = prefix_columns + selection.end;
      }
      y += unit;
    }
    const auto chosen_port = selected_port(model);
    const auto slot = body_slot_node(model);
    if (slot) {
      // A FOLD'S BODY SLOT lists what a fold could spend: one answer, an Int port for the count
      // and another of the answer's type for the accumulator. The preview says which port is which.
      label(0, y, "For %" + std::to_string(*slot) + " fold's body", ink::kAccent);
      y += unit;
      label(0, y, "(acc: T, count: Int) -> T", ink::kMuted);
      y += unit;
    } else if (chosen_port) {
      // A SELECTED PORT lists what could fill it, in the order a maker reaches for it: what is
      // in scope, a typed constant, then the door's Sources and operators that yield its type.
      const auto n = std::to_string(chosen_port->node), a = std::to_string(chosen_port->port);
      label(0, y, "For %" + n + " " + chosen_port->field.name + " : " + chosen_port->type, ink::kAccent);
      y += unit;
      label(0, y, "In scope", ink::kAccent);
      y += unit;
      const auto scope = in_scope(model, *chosen_port);
      for (const auto &candidate : scope) {
        if (y >= bottom)
          break;
        label(0, y, "  " + candidate.label);
        view.hits.push_back({0, y, 21 * unit, unit, "scope", {n, a, candidate.bind}, 0});
        y += unit;
      }
      if (scope.empty() && y < bottom) {
        label(0, y, "  none of this type", ink::kMuted);
        y += unit;
      }
      if (takes_constant(*chosen_port) && y < bottom) {
        label(0, y, "Constant", ink::kAccent);
        y += unit;
        button(0, y, std::string(loom::name_of(chosen_port->field.type.kind)) + " constant",
               "constant", {n, a});
        y += unit;
      }
    } else {
      label(0, y, "In scope", ink::kAccent);
      y += unit;
      auto input_row = [&](const loom::Field &f, bool state) {
        if (y >= bottom)
          return;
        label(0, y,
              (state ? "state." : "input.") + f.name + " : " +
                  loom::name_of(f.type.kind));
        view.hits.push_back({0, y, 21 * unit, unit, "source-field", {f.name}, 0});
        y += unit;
      };
      for (const auto &f : def.state->fields())
        input_row(f, true);
      for (const auto &f : on.message->fields())
        input_row(f, false);
    }
    y += unit;
    // THE DOOR'S ROWS, grouped by classification and in the order it gave them: every power
    // its contributor offers. A row previews; the preview's Add is today's add-node.
    if (!model.discovered_read) {
      label(0, y, "(asking the door)", ink::kMuted);
    } else if (!model.discovered.ok) {
      label(0, y, model.discovered.reason, ink::kAlert);
    } else {
      struct RailRow {
        std::string text, identity, kind;
        std::int64_t role = ink::kFill;
      };
      std::vector<RailRow> rows;
      for (const auto &[kind, heading] : kGroups) {
        bool headed = false;
        for (const auto &row : model.discovered.rows) {
          if (row.kind != kind)
            continue;
          if (!headed)
            rows.push_back({heading, "", "", ink::kAccent});
          headed = true;
          rows.push_back({"  " + row.identity +
                              (row.construction == ws::kCompositeConstruction ? " (composite)" : ""),
                          row.identity, row.kind,
                          is_previewed(model, row) ? ink::kAccent : ink::kFill});
        }
      }
      if (rows.empty())
        rows.push_back({"none offered match", "", "", ink::kMuted});
      const auto listed = static_cast<std::int64_t>(model.discovered.rows.size());
      if (model.discovered.total > listed)
        rows.push_back({"+ " + std::to_string(model.discovered.total - listed) +
                            " more match -- narrow the search",
                        "", "", ink::kMuted});
      for (std::size_t i = model.first_row; i < rows.size() && y < bottom; ++i) {
        label(0, y, rows[i].text, rows[i].role);
        if (!rows[i].identity.empty())
          view.hits.push_back({0, y, 21 * unit, unit, "found", {rows[i].kind, rows[i].identity}, 0});
        y += unit;
      }
    }
    // The catalog/source rail cannot paint or claim presses inside the graph.
    for (auto i = sidebar_labels; i < view.content.texts.size(); ++i)
      view.text_clips[i] = {0, top, 22 * unit, std::max(std::int64_t{0}, bottom - top)};
    for (auto i = sidebar_hits; i < view.hits.size(); ++i)
      view.hits[i].w = std::min(view.hits[i].w, std::max(std::int64_t{0}, 22 * unit - view.hits[i].x));
    const auto scale = [&](std::int64_t v) {
      return v * model.workspace.zoom / 100;
    };
    auto position =
        [&](std::size_t n) -> std::pair<std::int64_t, std::int64_t> {
      for (const auto &p : graph.places)
        if (p.trigger == model.workspace.active_trigger &&
            p.node == static_cast<std::int64_t>(n))
          return {scale(p.x) + model.workspace.pan_x,
                  top + scale(p.y) + model.workspace.pan_y};
      return {0, top};
    };
    const auto node_width = scale(24 * unit);
    const auto rect_begin = view.content.rects.size(),
               label_begin = view.content.texts.size(),
               hit_begin = view.hits.size();
    // A fold carries one row more than a step: its body slot, under its title.
    const auto extra = [&](std::size_t n) -> std::int64_t {
      return on.body.nodes[n].fold ? 1 : 0;
    };
    // Each node's words stay inside its box: a long title is shortened and marked, never drawn
    // across the box's edge or another node.
    std::vector<std::pair<std::size_t, ws::CanvasTextBox>> node_clips;
    for (std::size_t n = 0; n < on.body.nodes.size(); ++n) {
      const auto [x, ny] = position(n);
      const auto &node = on.body.nodes[n];
      const auto h =
          (static_cast<std::int64_t>(node.arguments.size() + 3) + extra(n)) * unit;
      const auto node_labels = view.content.texts.size();
      if (model.node && *model.node == n)
        view.node_extent = std::pair{ny, ny + h + stroke_y};
      view.graph_extent = std::pair{view.graph_extent ? std::min(view.graph_extent->first, ny) : ny,
                                    view.graph_extent ? std::max(view.graph_extent->second, ny + h + stroke_y)
                                                      : ny + h + stroke_y};
      const auto role =
          model.node && *model.node == n ? ink::kAccent : ink::kFill;
      stroke(x, ny, node_width, 4, role);
      stroke(x, ny + h, node_width, 4, role);
      stroke(x, ny, 4, h, role);
      stroke(x + node_width, ny, 4, h, role);
      label(x + unit / 2, ny, "%" + std::to_string(n) + " " + node_title(node), role);
      std::int64_t id = 0;
      for (const auto &p : graph.places)
        if (p.trigger == model.workspace.active_trigger &&
            p.node == static_cast<std::int64_t>(n))
          id = p.id;
      view.hits.push_back(
          {x, ny, node_width, unit, "node", {std::to_string(n)}, id});
      if (node.fold) {
        const bool open = slot && *slot == n;
        label(x + unit / 2, ny + unit, body_slot_text(node), open ? ink::kAccent : ink::kFill);
        view.hits.push_back(
            {x, ny + unit, node_width, unit, "body-slot", {std::to_string(n)}, id});
      }
      std::vector<std::string> port_names;
      try {
        const auto node_ports = graph.node_ports(model.trigger(), n, model.palette);
        for (const auto &f : node_ports.inputs->fields())
          port_names.push_back(f.name);
      } catch (const std::exception &) {
      }
      for (std::size_t a = 0; a < node.arguments.size(); ++a) {
        const std::string port = a < port_names.size() ? port_names[a] : std::to_string(a);
        const auto &binding = node.arguments[a];
        const auto py = ny + (static_cast<std::int64_t>(a + 1) + extra(n)) * unit;
        const bool marked = chosen_port && chosen_port->node == n && chosen_port->port == a;
        label(x + unit / 2, py,
              "o " + port + " = " +
                  (binding.from() == zengine::op::Binding::From::Input &&
                           binding.input_name().empty()
                       ? "[unwired]"
                       : flow::binding_text(binding)),
              marked ? ink::kAccent : ink::kFill);
        view.hits.push_back({x,
                             py,
                             node_width,
                             unit,
                             "port",
                             {std::to_string(n), std::to_string(a)},
                             id});
        if (binding.from() == zengine::op::Binding::From::Node) {
          const auto [sx, sy] = position(binding.node_index());
          const auto sh =
              (static_cast<std::int64_t>(
                   on.body.nodes[binding.node_index()].arguments.size() + 2) +
               extra(binding.node_index())) *
              unit;
          const auto ax = sx + node_width, ay = sy + sh + unit / 2, tx = x,
                     ty = py + unit / 2, mx = (ax + tx) / 2;
          stroke(std::min(ax, mx), ay,
               std::max<std::int64_t>(4, std::abs(mx - ax)), 4, ink::kAccent);
          stroke(mx, std::min(ay, ty), 4,
               std::max<std::int64_t>(4, std::abs(ty - ay)), ink::kAccent);
          stroke(std::min(mx, tx), ty,
               std::max<std::int64_t>(4, std::abs(tx - mx)), 4, ink::kAccent);
        }
      }
      const auto oy =
          ny + (static_cast<std::int64_t>(node.arguments.size() + 2) + extra(n)) * unit;
      button(x + unit / 2, oy,
             on.body.result_node == n ? "-> state." + on.output : "output",
             "source-node", {std::to_string(n)}, id);
      for (auto i = node_labels; i < view.content.texts.size(); ++i)
        // A unit of slack before the box: a node dragged off the device grain rounds its clip's
        // edges up, which must not take the first glyph or the first row of the node's words.
        node_clips.push_back({i, ws::CanvasTextBox{x - unit, ny - unit, node_width + unit, h + unit}});
    }
    // The graph owns a viewport within the pane. Its content and hit regions
    // are clipped by the same rectangle, so panning cannot cover controls. A
    // preview takes the three rows beneath it.
    const auto *shown = previewed(model);
    const std::int64_t left = 22 * unit, right = room.width, upper = top + unit,
                       lower = bottom - (shown ? 5 : 2) * unit;
    auto clip = [&](auto &r) {
      const auto x0 = std::max(r.x, left), y0 = std::max(r.y, upper);
      const auto x1 = std::min(r.x + r.w, right),
                 y1 = std::min(r.y + r.h, lower);
      r.x = x0;
      r.y = y0;
      r.w = std::max<std::int64_t>(0, x1 - x0);
      r.h = std::max<std::int64_t>(0, y1 - y0);
    };
    for (std::size_t i = rect_begin; i < view.content.rects.size(); ++i)
      clip(view.content.rects[i]);
    view.content.rects.erase(
        std::remove_if(view.content.rects.begin() +
                           static_cast<std::ptrdiff_t>(rect_begin),
                       view.content.rects.end(),
                       [](const auto &r) { return r.w == 0 || r.h == 0; }),
        view.content.rects.end());
    for (std::size_t i = hit_begin; i < view.hits.size(); ++i)
      clip(view.hits[i]);
    for (std::size_t i = label_begin; i < view.content.texts.size(); ++i) {
      view.text_clips[i] = {left, upper, std::max(std::int64_t{0}, right - left),
                            std::max(std::int64_t{0}, lower - upper)};
    }
    for (const auto &[i, box] : node_clips) {
      auto &c = view.text_clips[i];
      const auto x0 = std::max(c.x, box.x), y0 = std::max(c.y, box.y);
      const auto x1 = std::min(c.x + c.w, box.x + box.w), y1 = std::min(c.y + c.h, box.y + box.h);
      c = {x0, y0, std::max<std::int64_t>(0, x1 - x0), std::max<std::int64_t>(0, y1 - y0)};
    }
    view.view_upper = upper;
    view.view_lower = lower;
    if (model.node) {
      const auto n = std::to_string(*model.node);
      button(23 * unit, top, "Use as result", "result", {n});
      button(42 * unit, top, "Delete node", "remove", {n});
    }
    if (shown) {
      // THE PREVIEW: the door's row expanded -- what it is, whose it is, its signature and what
      // its contributor says it is for. Reading it evaluates nothing.
      const auto band = static_cast<std::size_t>(view.content.texts.size());
      const auto y0 = bottom - 4 * unit;
      label(23 * unit, y0, shown->identity + " -- " + classification_of(*shown), ink::kAccent);
      label(23 * unit, y0 + unit, shown->signature);
      label(23 * unit, y0 + 2 * unit,
            shown->about.empty() ? std::string("(its contributor says nothing of what it is for)")
                                 : shown->about,
            shown->about.empty() ? ink::kMuted : ink::kFill);
      // The clip begins a row above the band: a clip's top rounds up to the device grain, so a
      // line set exactly on it could round out of it and vanish.
      for (auto i = band; i < view.content.texts.size(); ++i)
        view.text_clips[i] = {left, lower, std::max(std::int64_t{0}, right - left - 8 * unit),
                              bottom - lower};
      // WHAT ADD DOES with the row previewed: a form is placed, a body is chosen for the open
      // slot, and an operator is added -- at the end, before the selected node, or before the node
      // whose port is being filled and into that port.
      std::int64_t bx2 = 23 * unit;
      const auto offer = [&](const std::string &title, const std::string &action,
                             std::vector<std::string> args) {
        const auto w = static_cast<std::int64_t>(title.size() + 2) * unit + grid.padding;
        // The row beneath the preview, left of the view's own controls.
        if (bx2 + w > room.width - 23 * unit) return;
        bx2 += button(bx2, bottom - unit, title, action, std::move(args));
      };
      if (shown->kind == ws::kFormKind) {
        button(room.width - 7 * unit, y0, "Add", "add-fold", {});
        if (model.node)
          offer("Add before %" + std::to_string(*model.node), "add-fold",
                {std::to_string(*model.node)});
      } else if (slot) {
        // A reference dropped on this slot is the one the body is chosen from: a stale one is
        // refused when chosen, never replaced by the ports the door holds now.
        auto in = static_cast<std::int64_t>(shown->inputs.content_id);
        auto out = static_cast<std::int64_t>(shown->outputs.content_id);
        if (model.slot_reference && model.slot_reference->slot == *model.body_slot &&
            model.slot_reference->ref.identity == shown->identity) {
          in = static_cast<std::int64_t>(model.slot_reference->ref.authored_in);
          out = static_cast<std::int64_t>(model.slot_reference->ref.authored_out);
        }
        for (const auto &choice : fold_choices(model, *shown))
          offer("count " + choice.count + ", acc " + choice.accumulator, "fold-body",
                {std::to_string(*slot), shown->identity, std::to_string(in),
                 std::to_string(out), choice.count, choice.accumulator});
      } else {
        button(room.width - 7 * unit, y0, "Add", "add-found", {shown->kind, shown->identity});
        if (chosen_port)
          offer("Add into %" + std::to_string(chosen_port->node) + " " +
                    chosen_port->field.name,
                "add-into",
                {shown->kind, shown->identity, std::to_string(chosen_port->node),
                 std::to_string(chosen_port->port)});
        else if (model.node)
          offer("Add before %" + std::to_string(*model.node), "add-found",
                {shown->kind, shown->identity, std::to_string(*model.node)});
      }
    }
    button(room.width - 22 * unit, bottom - unit, "Reset view", "fit");
    button(room.width - 9 * unit, bottom - unit, "-", "zoom-out");
    button(room.width - 4 * unit, bottom - unit, "+", "zoom-in");
    return finish_body();
  }
  if (model.page == Page::Events) {
    // A long event -- a refusal in its owner's words -- continues on the rows beneath it,
    // indented, rather than ending where the room does.
    const auto columns = std::max<std::size_t>(
        24, static_cast<std::size_t>(std::max<std::int64_t>(0, room.width / unit - 1)));
    std::int64_t y = top;
    const auto lines = [&](const std::string &text, std::int64_t role) {
      std::string rest = text;
      while (y < bottom) {
        if (rest.size() <= columns) {
          label(0, y, rest, role);
          y += unit;
          return;
        }
        if (y + unit >= bottom) { // the last row the room has: shortened, and marked so
          label(0, y, rest.substr(0, columns - 3) + "...", role);
          y += unit;
          return;
        }
        auto cut = rest.rfind(' ', columns);
        if (cut == std::string::npos || cut < columns / 2)
          cut = columns;
        label(0, y, rest.substr(0, cut), role);
        y += unit;
        rest = "    " + rest.substr(cut < rest.size() && rest[cut] == ' ' ? cut + 1 : cut);
      }
    };
    for (std::size_t i = model.first_row; i < model.events.size() && y < bottom; ++i)
      lines(model.events[i], ink::kFill);
    for (const auto &error : graph.problems(model.palette)) {
      if (y >= bottom)
        break;
      lines(error, ink::kAccent);
    }
    if (model.events.empty())
      label(0, y, "Run and send a message. Observed results appear here.");
    return finish_body();
  }
  std::int64_t y = top;
  if (model.page == Page::State) {
    button(0, y, "Add state field", "ask-state-field");
    button(20 * unit, y, "Keep state", "keep-state");
    y += 2 * unit;
  }
  if (model.page == Page::Messages) {
    button(0, y, "New message", "ask-message");
    button(16 * unit, y, "Add field", "ask-message-field");
    y += unit;
    for (std::size_t i = 0; i < def.accepts.size() && y < bottom; ++i) {
      button(0, y, def.accepts[i]->name(), "message-open", {std::to_string(i)});
      y += unit;
    }
    y += unit;
    // THE MESSAGES THIS DEFINITION MAY PUBLISH, inside its namespace; a trigger's Emit sends one.
    label(0, y, "Emitted", ink::kAccent);
    button(10 * unit, y, "New emitted", "ask-emitted-message");
    button(25 * unit, y, "Add emitted field", "ask-emitted-field");
    y += unit;
    for (std::size_t i = 0; i < def.emits.size() && y < bottom; ++i) {
      std::string fields;
      for (const auto &f : def.emits[i]->fields())
        fields += (fields.empty() ? "" : ", ") + f.name + ": " + loom::name_of(f.type.kind);
      label(unit, y, std::to_string(i) + " " + def.emits[i]->name() + " {" + fields + "}");
      view.hits.push_back({0, y, room.width, unit, "emitted-row", {std::to_string(i)}, 0});
      y += unit;
    }
    y += unit;
  }
  if (model.page == Page::Library) {
    button(0, y, "Import", "ask-library-open");
    button(12 * unit, y, "Export", "ask-library-save");
    y += unit;
    for (const auto &p : model.workspace.library.entries()) {
      if (y >= bottom)
        break;
      button(0, y, p.title + (p.draft.ready() ? "" : " (unfinished)"),
             "preset-open", {p.title});
      y += unit;
    }
    y += unit;
    label(0, y, "Retained forms", ink::kAccent);
    y += unit;
    for (std::size_t i = 0; i < model.workspace.forms.size() && y < bottom;
         ++i) {
      const auto &saved = model.workspace.forms[i];
      button(0, y,
             saved.title + " #" +
                 std::to_string(saved.draft.schema()->content_id()),
             "draft-open", {std::to_string(i)});
      y += unit;
    }
    y += unit;
  }
  if (model.form) {
    label(0, y, model.form_title, ink::kAccent);
    y += unit;
    button(0, y, "Save example", "ask-preset");
    if (!model.form_state)
      button(18 * unit, y, "Send", "send");
    y += unit;
    const auto rows = model.form->rows();
    for (std::size_t i = model.first_row; i < rows.size() && y < bottom; ++i) {
      const auto &row = rows[i];
      label(0, y,
            row.label + " : " + loom::name_of(row.type.kind) + " = " +
                row.summary);
      view.hits.push_back({0,
                           y,
                           std::max<std::int64_t>(unit, room.width - 8 * unit),
                           unit,
                           "value-row",
                           {std::to_string(i)},
                           0});
      button(room.width - 8 * unit, y, "unset", "unset", {std::to_string(i)});
      y += unit;
    }
    if (!model.form->ready() && y < bottom)
      label(0, y, "Unfinished draft: save it now, complete before sending.",
            ink::kAccent);
  }
  return finish_body();
}
/// Pan a graph that runs past the bottom of its viewport up, as far as its topmost node allows, so
/// a workspace opened in a short pane shows its nodes whole where the room can hold them.
inline void reveal_graph(Model &model, const ws::PaneCanvasRoom &room) {
  if (room.grant <= 0 || room.width <= 0 || room.height <= 0)
    return;
  const auto drawn = picture(model, room, 0);
  if (!drawn.graph_extent)
    return;
  const auto [top, bottom] = *drawn.graph_extent;
  if (bottom <= drawn.view_lower)
    return;
  const auto dy = std::max(drawn.view_lower - bottom, drawn.view_upper - top);
  if (dy >= 0)
    return;
  model.workspace.pan_y =
      std::clamp(model.workspace.pan_y + dy, std::int64_t{-10000000}, std::int64_t{10000000});
}

/// Pan the graph so the selected node's whole box -- its title, body slot, ports and answer --
/// sits inside the graph viewport, above a preview band, as this room draws it. A node just added
/// is then seen whole; one already in view does not move.
inline void reveal_selected(Model &model, const ws::PaneCanvasRoom &room) {
  if (!model.node || room.grant <= 0 || room.width <= 0 || room.height <= 0)
    return;
  const auto drawn = picture(model, room, 0);
  if (!drawn.node_extent)
    return;
  const auto [top, bottom] = *drawn.node_extent;
  std::int64_t dy = 0;
  if (bottom > drawn.view_lower)
    dy = drawn.view_lower - bottom;
  if (top + dy < drawn.view_upper)
    dy = drawn.view_upper - top;
  if (dy == 0)
    return;
  model.workspace.pan_y =
      std::clamp(model.workspace.pan_y + dy, std::int64_t{-10000000}, std::int64_t{10000000});
  model.touched();
}
/// WHAT FLOW CALLS A PLACE IT ANSWERS A PRESS AT: a node by its place, `node:<id>`, and its
/// argument `port:<id>.<n>` (`#<index>` where the node has no place yet); a control by what it
/// does, `control:<action>`; any other row by what it does and what it stands for,
/// `<action>:<args>`.
inline std::string part_name(const Hit &hit) {
  const auto node = [&]() {
    return hit.subject != 0 ? std::to_string(hit.subject)
                            : "#" + (hit.args.empty() ? std::string() : hit.args[0]);
  };
  if (hit.action == "node")
    return "node:" + node();
  if (hit.action == "port" && hit.args.size() == 2)
    return "port:" + node() + "." + hit.args[1];
  if (hit.args.empty())
    return "control:" + hit.action;
  std::string name = hit.action;
  for (const auto &arg : hit.args)
    name += ":" + arg;
  return name;
}

/// FLOW'S PICTURE AS IT IS SAID, each place a press means something named (`part_name`).
inline ws::v4::PaneCanvasContent named(const Picture &p) {
  ws::PartNames<ws::PaneCanvasPart> parts;
  for (const Hit &hit : p.hits)
    (void)parts.add(ws::PaneCanvasPart{part_name(hit), hit.x, hit.y, hit.w, hit.h});
  const auto &c = p.content;
  return ws::v4::PaneCanvasContent{c.pane,   c.grant,  c.picture,   c.rects,
                                   c.labels, c.texts, parts.take()};
}
} // namespace zengine::flow_pane
#endif
