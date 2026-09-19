// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_VIEW_HPP
#define ZENGINE_FLOW_PANE_VIEW_HPP
#include "flow-pane/model.hpp"
#include "workshop/pane_canvas_vocabulary.hpp"
#include "surface/pointing.hpp"

namespace zengine::flow_pane {
namespace ws = zengine::workshop;
namespace ink = zengine::surface::role;
inline constexpr std::int64_t unit = ws::kPaneCanvasUnit;
struct Hit {
  std::int64_t x = 0, y = 0, w = 0, h = 0;
  std::string action;
  std::vector<std::string> args;
  std::int64_t subject = 0;
  bool contains(std::int64_t px, std::int64_t py, std::int64_t grain) const {
    return zengine::surface::sub_span_contains(x, w, px, grain) &&
           zengine::surface::sub_span_contains(y, h, py, grain);
  }
};
struct Picture {
  ws::PaneCanvasContent content;
  std::vector<Hit> hits;
  std::int64_t revision = 0, width = 0, height = 0, grain = 1;
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
// Whole glyph clipping also bounds the wire image. A shortened visible label
// carries an ellipsis; clipping never changes the underlying draft or command.
inline void fit_label(ws::PaneCanvasLabel &label, std::int64_t width,
                      std::int64_t height) {
  if (label.y < 0 || label.y > height - unit) {
    label.text.clear();
    return;
  }
  bool left_cut = false;
  while (!label.text.empty() && label.x < 0) {
    const auto skip = std::min(label.text.size(),
        static_cast<std::size_t>((-label.x + unit - 1) / unit));
    label.text.erase(0, skip);
    label.x += static_cast<std::int64_t>(skip) * unit;
    left_cut = true;
  }
  const auto columns = static_cast<std::size_t>(
      std::max(std::int64_t{0}, (width - label.x) / unit));
  const auto limit = std::min(columns, ws::kPaneCanvasMaxLabelBytes);
  const bool right_cut = label.text.size() > limit;
  if (right_cut) label.text.resize(limit);
  const auto mark = std::min(std::size_t{3}, label.text.size());
  if (left_cut) label.text.replace(0, mark, mark, '.');
  if (right_cut) label.text.replace(label.text.size() - mark, mark, mark, '.');
}

inline Picture finish_picture(Picture view) {
  auto clip = [&](auto &r) {
    const auto left = std::max(std::int64_t{0}, r.x);
    const auto top = std::max(std::int64_t{0}, r.y);
    const auto right = std::min(view.width, zengine::surface::add_cells(r.x, r.w));
    const auto bottom = std::min(view.height, zengine::surface::add_cells(r.y, r.h));
    r.x = left; r.y = top;
    r.w = std::max(std::int64_t{0}, right - left);
    r.h = std::max(std::int64_t{0}, bottom - top);
  };
  for (auto &r : view.content.rects) clip(r);
  std::erase_if(view.content.rects, [](const auto &r) { return r.w <= 0 || r.h <= 0; });
  for (auto &h : view.hits) clip(h);
  std::erase_if(view.hits, [](const auto &h) { return h.w <= 0 || h.h <= 0; });
  std::size_t bytes = 0;
  for (auto &label : view.content.labels) {
    fit_label(label, view.width, view.height);
    bytes += label.text.size();
  }
  std::erase_if(view.content.labels, [](const auto &l) { return l.text.empty(); });
  if (view.content.rects.size() <= ws::kPaneCanvasMaxRects &&
      view.content.labels.size() <= ws::kPaneCanvasMaxLabels &&
      bytes <= ws::kPaneCanvasMaxTextBytes) return view;

  // A partial graph would look like a different graph. Show a bounded recovery
  // picture instead, keeping save/export and view controls available.
  view.content.rects.clear(); view.content.labels.clear(); view.hits.clear();
  auto text = [&](std::int64_t y, std::string value) {
    ws::PaneCanvasLabel label{0, y, std::move(value), ink::kAlert};
    fit_label(label, view.width, view.height);
    if (!label.text.empty()) view.content.labels.push_back(std::move(label));
  };
  text(0, "View limit reached: pan/zoom or choose a smaller graph.");
  text(unit, "Draft intact. Save or export it before changing the graph.");
  std::int64_t x = 0, y = 3 * unit;
  auto button = [&](const std::string &title, const std::string &action) {
    const auto w = static_cast<std::int64_t>(title.size() + 2) * unit;
    if (x + w > view.width) { x = 0; y += unit; }
    if (w > view.width || y + unit > view.height) return;
    view.content.labels.push_back({x, y, "[" + title + "]", ink::kAccent});
    view.hits.push_back({x, y, w, unit, action, {}, 0});
    x += w + unit;
  };
  button("Save", "ask-save"); button("Export", "ask-export");
  button("Graph", "page-graph"); button("State", "page-state");
  button("Messages", "page-messages"); button("Events", "page-events");
  button("Reset view", "fit"); button("Zoom in", "zoom-in");
  button("Zoom out", "zoom-out");
  return view;
}
inline Picture picture(const Model &model, const ws::PaneCanvasRoom &room,
                       std::int64_t sequence) {
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
  auto label = [&](std::int64_t x, std::int64_t y, std::string text,
                   std::int64_t role = ink::kFill) {
    view.content.labels.push_back({x, y, clean(std::move(text)), role});
  };
  auto button = [&](std::int64_t x, std::int64_t y, std::string title,
                    std::string action,
                    std::vector<std::string> args = std::vector<std::string>{},
                    std::int64_t subject = 0) {
    const auto w = static_cast<std::int64_t>(title.size() + 2) * unit;
    label(x, y, "[" + title + "]", ink::kAccent);
    view.hits.push_back(
        {x, y, w, unit, std::move(action), std::move(args), subject});
    return w + unit / 2;
  };
  // The graph owns its ground; selection remains in Workshop's border/title.
  rect(0, 0, room.width, room.height, ink::kMuted);
  if (room.width < 12 * unit || room.height < 7 * unit) {
    label(0, 0, "Flow needs more room");
    label(0, unit, "Resize this pane");
    return finish_picture(std::move(view));
  }
  std::int64_t bx = 0, by = 0;
  auto bar = [&](std::string title, std::string action) {
    const auto needed = static_cast<std::int64_t>(title.size() + 3) * unit;
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
      label(0, y, prefix + text.visible(columns));
      if (i == dialog.selected) {
        const auto selection = text.visible_selection(columns);
        if (selection.present())
          rect((static_cast<std::int64_t>(prefix.size()) + selection.begin) *
                   unit,
               y + unit + 8, (selection.end - selection.begin) * unit, 8,
               ink::kAccent);
        rect((static_cast<std::int64_t>(prefix.size()) +
              static_cast<std::int64_t>(text.caret_column())) *
                 unit,
             y + unit, unit, 4, ink::kAccent);
      }
      view.hits.push_back(
          {0, y, room.width, unit, "dialog-field", {std::to_string(i)}, 0});
      y += 2 * unit;
    }
    button(0, y, "Confirm", "dialog-confirm");
    button(12 * unit, y, "Cancel", "dialog-cancel");
    label(0, y + 2 * unit, "Tab changes field; Enter confirms; Escape cancels");
    return finish_picture(std::move(view));
  }
  const auto &graph = model.workspace.graph;
  const auto &def = graph.project.definition;
  if (model.page == Page::Graph) {
    const auto sidebar_labels = view.content.labels.size(), sidebar_hits = view.hits.size();
    std::int64_t y = top;
    button(0, y, "Add trigger", "ask-trigger");
    y += unit;
    for (std::size_t t = 0; t < def.on.size(); ++t) {
      if (y >= bottom)
        break;
      const auto &on = def.on[t];
      button(0, y, (t == model.trigger() ? "> " : "") + on.message->name(),
             "select-trigger", {std::to_string(t)});
      y += unit;
    }
    if (def.on.empty()) {
      label(0, y + unit, "Add state and a message, then a trigger.");
      return finish_picture(std::move(view));
    }
    const auto &on = def.on.at(model.trigger());
    y += unit;
    label(0, y, "Sources (click to wire)", ink::kAccent);
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
    y += unit;
    label(0, y, "Operators (click to add)", ink::kAccent);
    y += unit;
    for (std::size_t p = model.first_row;
         p < model.palette.size() && y < bottom; ++p) {
      const auto &ports = model.palette[p];
      button(0, y, ports.identity, "add-node", {ports.identity});
      y += unit;
    }
    // The catalog/source rail cannot paint or claim presses inside the graph.
    for (auto i = sidebar_labels; i < view.content.labels.size(); ++i)
      fit_label(view.content.labels[i], 22 * unit, room.height);
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
               label_begin = view.content.labels.size(),
               hit_begin = view.hits.size();
    for (std::size_t n = 0; n < on.body.nodes.size(); ++n) {
      const auto [x, ny] = position(n);
      const auto &node = on.body.nodes[n];
      const auto h =
          static_cast<std::int64_t>(node.arguments.size() + 3) * unit;
      const auto role =
          model.node && *model.node == n ? ink::kAccent : ink::kFill;
      rect(x, ny, node_width, 4, role);
      rect(x, ny + h, node_width, 4, role);
      rect(x, ny, 4, h, role);
      rect(x + node_width, ny, 4, h, role);
      label(x + unit / 2, ny + unit / 4,
            "%" + std::to_string(n) + " " + node.identity, role);
      std::int64_t id = 0;
      for (const auto &p : graph.places)
        if (p.trigger == model.workspace.active_trigger &&
            p.node == static_cast<std::int64_t>(n))
          id = p.id;
      view.hits.push_back(
          {x, ny, node_width, unit, "node", {std::to_string(n)}, id});
      for (std::size_t a = 0; a < node.arguments.size(); ++a) {
        std::string port = std::to_string(a);
        try {
          port = graph.ports(model.palette, node.identity)
                     .inputs->fields()
                     .at(a)
                     .name;
        } catch (const std::exception &) {
        }
        const auto &binding = node.arguments[a];
        const auto py = ny + static_cast<std::int64_t>(a + 1) * unit;
        label(x + unit / 2, py,
              "o " + port + " = " +
                  (binding.from() == zengine::op::Binding::From::Input &&
                           binding.input_name().empty()
                       ? "[unwired]"
                       : flow::binding_text(binding)));
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
              static_cast<std::int64_t>(
                  on.body.nodes[binding.node_index()].arguments.size() + 2) *
              unit;
          const auto ax = sx + node_width, ay = sy + sh + unit / 2, tx = x,
                     ty = py + unit / 2, mx = (ax + tx) / 2;
          rect(std::min(ax, mx), ay,
               std::max<std::int64_t>(4, std::abs(mx - ax)), 4, ink::kAccent);
          rect(mx, std::min(ay, ty), 4,
               std::max<std::int64_t>(4, std::abs(ty - ay)), ink::kAccent);
          rect(std::min(mx, tx), ty,
               std::max<std::int64_t>(4, std::abs(tx - mx)), 4, ink::kAccent);
        }
      }
      const auto oy =
          ny + static_cast<std::int64_t>(node.arguments.size() + 2) * unit;
      button(x + unit / 2, oy,
             on.body.result_node == n ? "-> state." + on.output : "output",
             "source-node", {std::to_string(n)}, id);
    }
    // The graph owns a viewport within the pane. Its content and hit regions
    // are clipped by the same rectangle, so panning cannot cover controls.
    const std::int64_t left = 22 * unit, right = room.width, upper = top + unit,
                       lower = bottom - 2 * unit;
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
    for (std::size_t i = label_begin; i < view.content.labels.size(); ++i) {
      auto &l = view.content.labels[i];
      l.x -= left; l.y -= upper;
      fit_label(l, right - left, lower - upper);
      l.x += left; l.y += upper;
    }
    if (model.node) {
      const auto n = std::to_string(*model.node);
      button(23 * unit, top, "Use as result", "result", {n});
      button(42 * unit, top, "Delete node", "remove", {n});
    }
    button(room.width - 22 * unit, bottom - unit, "Reset view", "fit");
    button(room.width - 9 * unit, bottom - unit, "-", "zoom-out");
    button(room.width - 4 * unit, bottom - unit, "+", "zoom-in");
    return finish_picture(std::move(view));
  }
  if (model.page == Page::Events) {
    std::int64_t y = top;
    for (std::size_t i = model.first_row; i < model.events.size() && y < bottom;
         ++i) {
      label(0, y, model.events[i]);
      y += unit;
    }
    for (const auto &error : graph.problems(model.palette)) {
      if (y >= bottom)
        break;
      label(0, y, error, ink::kAccent);
      y += unit;
    }
    if (model.events.empty())
      label(0, y, "Run and send a message. Observed results appear here.");
    return finish_picture(std::move(view));
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
  return finish_picture(std::move(view));
}
} // namespace zengine::flow_pane
#endif
