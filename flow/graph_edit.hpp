// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_GRAPH_EDIT_HPP
#define ZENGINE_FLOW_GRAPH_EDIT_HPP

#include "flow/project.hpp"
#include <algorithm>
#include <limits>
#include <map>

namespace zengine::flow {

// Descriptions are observations, never callable implementations. Runtime
// admission and spend resolve the host catalog again after an author finishes a
// draft.
struct Ports {
  std::string identity;
  std::shared_ptr<const loom::Schema> inputs, outputs;
};
using Palette = std::vector<Ports>;
struct NodePlace {
  std::int64_t id = 0, trigger = 0, node = 0, x = 0, y = 0;
  ZEN_SHAPE(NodePlace, 1, ZEN_FIELD(id), ZEN_FIELD(trigger), ZEN_FIELD(node),
            ZEN_FIELD(x), ZEN_FIELD(y));
};

class GraphDraft {
public:
  explicit GraphDraft(std::string name = "my_flow")
      : project{blank(name), loom::Value(blank(name).state)} {}
  explicit GraphDraft(Project value) : project(std::move(value)) { arrange(); }
  Project project;
  std::vector<NodePlace> places;
  std::int64_t next_id = 1;

  static maker::Definition blank(const std::string &name) {
    if (name.empty() || name.size() > 120)
      throw std::invalid_argument("use a project name of 1 to 120 bytes");
    maker::Definition d;
    d.name = name;
    d.state = loom::make_schema(name + ".State", 1, {});
    return d;
  }
  const Ports &ports(const Palette &palette, const std::string &name) const {
    const auto found =
        std::find_if(palette.begin(), palette.end(),
                     [&](const auto &p) { return p.identity == name; });
    if (found == palette.end())
      throw std::invalid_argument("operator is unavailable: " + name);
    return *found;
  }
  void editable() const {
    if (project.definition.revision == std::numeric_limits<std::int64_t>::max())
      throw std::invalid_argument("definition revision exhausted");
  }
  void changed() {
    if (project.definition.revision == std::numeric_limits<std::int64_t>::max())
      throw std::invalid_argument("definition revision exhausted");
    ++project.definition.revision;
  }
  void state_field(const std::string &name, loom::TypeRef type,
                   bool required = true) {
    editable();
    if (name.empty() || project.definition.state->find(name))
      throw std::invalid_argument("state field name is empty or already used");
    auto fields = project.definition.state->fields();
    fields.push_back({name, std::move(type), required});
    const auto schema = loom::make_schema(project.definition.state->name(),
                                          project.definition.state->version(),
                                          std::move(fields));
    loom::Value state(schema);
    for (const auto &field : project.definition.state->fields())
      if (const auto *c = project.state.get(field.name))
        state.set(field.name, *c);
    if (required)
      state.set(name, maker::default_cell(schema->find(name)->type));
    project.definition.state = schema;
    project.state = std::move(state);
    changed();
  }
  std::size_t message(std::string name, std::vector<loom::Field> fields,
                      bool emitted = false) {
    editable();
    if (name.find('.') == std::string::npos)
      name = project.definition.name + "." + name;
    auto &shapes =
        emitted ? project.definition.emits : project.definition.accepts;
    for (const auto &shape : shapes)
      if (shape->name() == name)
        throw std::invalid_argument("message already declared");
    shapes.push_back(loom::make_schema(name, 1, std::move(fields)));
    changed();
    return shapes.size() - 1;
  }
  std::size_t trigger(std::size_t accepted, const std::string &field) {
    editable();
    const auto &shape = project.definition.accepts.at(accepted);
    if (!project.definition.state->find(field))
      throw std::invalid_argument("select a declared state output field");
    for (const auto &on : project.definition.on)
      if (loom::same_identity(*on.message, *shape))
        throw std::invalid_argument("message already has a trigger");
    project.definition.on.push_back({shape, {}, field, {}});
    changed();
    return project.definition.on.size() - 1;
  }
  void message_field(std::size_t message_index, const std::string &name,
                     loom::TypeRef type, bool required = true) {
    editable();
    auto &shape = project.definition.accepts.at(message_index);
    if (name.empty() || shape->find(name))
      throw std::invalid_argument(
          "message field name is empty or already used");
    auto fields = shape->fields();
    fields.push_back({name, std::move(type), required});
    const auto previous = shape;
    shape =
        loom::make_schema(shape->name(), shape->version(), std::move(fields));
    for (auto &on : project.definition.on)
      if (loom::same_identity(*on.message, *previous))
        on.message = shape;
    changed();
  }
  std::size_t add(std::size_t trigger_index, const Ports &signature) {
    editable();
    if (next_id == std::numeric_limits<std::int64_t>::max())
      throw std::invalid_argument("node identity exhausted");
    if (!signature.inputs || !signature.outputs ||
        signature.outputs->fields().size() != 1)
      throw std::invalid_argument("a graph node needs one output port");
    auto &body = project.definition.on.at(trigger_index).body;
    op::Node node;
    node.identity = signature.identity;
    node.authored_in = signature.inputs->content_id();
    node.authored_out = signature.outputs->content_id();
    // An empty input name is an explicit unwired port, never a fabricated zero.
    for (std::size_t i = 0; i < signature.inputs->fields().size(); ++i)
      node.arguments.push_back(op::Binding::input(""));
    body.nodes.push_back(std::move(node));
    const auto index = body.nodes.size() - 1;
    places.push_back({next_id++, static_cast<std::int64_t>(trigger_index),
                      static_cast<std::int64_t>(index),
                      1056 + static_cast<std::int64_t>(index % 3) * 1344,
                      240 + static_cast<std::int64_t>(index / 3) * 720});
    changed();
    return index;
  }
  loom::TypeRef source_type(std::size_t t, const op::Binding &binding,
                            const Palette &palette) const {
    const auto &on = project.definition.on.at(t);
    if (binding.from() == op::Binding::From::Constant)
      return loom::type_of(binding.constant_cell().kind());
    if (binding.from() == op::Binding::From::Node) {
      const auto &p =
          ports(palette, on.body.nodes.at(binding.node_index()).identity);
      if (p.outputs->fields().size() != 1)
        throw std::invalid_argument("source has no single output");
      return p.outputs->fields().front().type;
    }
    const auto *f = project.definition.state->find(binding.input_name());
    if (!f)
      f = on.message->find(binding.input_name());
    if (!f)
      throw std::invalid_argument("unknown input field: " +
                                  binding.input_name());
    if (!f->required)
      throw std::invalid_argument("optional fields cannot be wired without an "
                                  "authored presence policy");
    return f->type;
  }
  void bind(std::size_t t, std::size_t n, std::size_t argument,
            op::Binding value, const Palette &palette) {
    editable();
    auto &node = project.definition.on.at(t).body.nodes.at(n);
    const auto &signature = ports(palette, node.identity);
    if (signature.inputs->content_id() != node.authored_in ||
        signature.outputs->content_id() != node.authored_out)
      throw std::invalid_argument(
          "operator signature changed; add a node against its current ports");
    if (value.from() == op::Binding::From::Node && value.node_index() >= n)
      throw std::invalid_argument("connect an earlier node: the graph executes "
                                  "in order and cannot cycle");
    if (!op::detail::same_type(source_type(t, value, palette),
                               signature.inputs->fields().at(argument).type))
      throw std::invalid_argument("port '" +
                                  signature.inputs->fields().at(argument).name +
                                  "' has an incompatible type");
    node.arguments.at(argument) = std::move(value);
    changed();
  }
  void result(std::size_t t, std::size_t n, const Palette &palette) {
    editable();
    auto &on = project.definition.on.at(t);
    if (!op::detail::same_type(source_type(t, op::Binding::node(n), palette),
                               project.definition.state->find(on.output)->type))
      throw std::invalid_argument("node output does not match state." +
                                  on.output);
    on.body.result_node = n;
    changed();
  }
  void remove(std::size_t t, std::size_t n) {
    editable();
    auto &body = project.definition.on.at(t).body;
    (void)body.nodes.at(n);
    if (body.nodes.size() > 1 && body.result_node == n)
      throw std::invalid_argument(
          "choose another result before deleting this node");
    for (const auto &node : body.nodes)
      for (const auto &b : node.arguments)
        if (b.from() == op::Binding::From::Node && b.node_index() == n)
          throw std::invalid_argument(
              "another node uses this output; reconnect it before deleting");
    body.nodes.erase(body.nodes.begin() + static_cast<std::ptrdiff_t>(n));
    for (auto &node : body.nodes)
      for (auto &b : node.arguments)
        if (b.from() == op::Binding::From::Node && b.node_index() > n)
          b = op::Binding::node(b.node_index() - 1);
    if (body.result_node >= n && body.result_node != 0)
      --body.result_node;
    places.erase(std::remove_if(places.begin(), places.end(),
                                [&](const auto &p) {
                                  return p.trigger ==
                                             static_cast<std::int64_t>(t) &&
                                         p.node == static_cast<std::int64_t>(n);
                                }),
                 places.end());
    for (auto &p : places)
      if (p.trigger == static_cast<std::int64_t>(t) &&
          p.node > static_cast<std::int64_t>(n))
        --p.node;
    changed();
  }
  NodePlace &place(std::size_t t, std::size_t n) {
    const auto found =
        std::find_if(places.begin(), places.end(), [&](const auto &p) {
          return p.trigger == static_cast<std::int64_t>(t) &&
                 p.node == static_cast<std::int64_t>(n);
        });
    if (found == places.end())
      throw std::invalid_argument("node has no layout position");
    return *found;
  }
  void arrange() {
    places.clear();
    next_id = 1;
    for (std::size_t t = 0; t < project.definition.on.size(); ++t)
      for (std::size_t n = 0; n < project.definition.on[t].body.nodes.size();
           ++n)
        places.push_back({next_id++, static_cast<std::int64_t>(t),
                          static_cast<std::int64_t>(n),
                          1056 + static_cast<std::int64_t>(n % 3) * 1344,
                          240 + static_cast<std::int64_t>(n / 3) * 720});
  }
  std::vector<std::string> problems(const Palette &palette) const {
    std::vector<std::string> out;
    for (std::size_t t = 0; t < project.definition.on.size(); ++t) {
      const auto &on = project.definition.on[t];
      if (on.body.nodes.empty()) {
        out.push_back(on.message->name() + ": add a result node");
        continue;
      }
      for (std::size_t n = 0; n < on.body.nodes.size(); ++n) {
        const auto &node = on.body.nodes[n];
        try {
          const auto &p = ports(palette, node.identity);
          if (!p.inputs || !p.outputs || p.outputs->fields().size() != 1)
            throw std::invalid_argument("operator has no single output");
          if (!project.definition.state->find(on.output))
            throw std::invalid_argument("unknown state output");
          if (node.authored_in != p.inputs->content_id() ||
              node.authored_out != p.outputs->content_id())
            throw std::invalid_argument("operator signature changed");
          if (node.arguments.size() != p.inputs->fields().size())
            throw std::invalid_argument("port count changed");
          for (std::size_t a = 0; a < node.arguments.size(); ++a) {
            const auto &b = node.arguments[a];
            try {
              if (b.from() == op::Binding::From::Node && b.node_index() >= n)
                throw std::invalid_argument("forward or cyclic connection");
              if (!op::detail::same_type(source_type(t, b, palette),
                                         p.inputs->fields()[a].type))
                throw std::invalid_argument("incompatible type");
            } catch (const std::exception &e) {
              out.push_back(on.message->name() + " / node " +
                            std::to_string(n) + " / " +
                            p.inputs->fields()[a].name + ": " + e.what());
            }
          }
          if (n == on.body.result_node &&
              !op::detail::same_type(
                  p.outputs->fields()[0].type,
                  project.definition.state->find(on.output)->type))
            throw std::invalid_argument("result type does not match state." +
                                        on.output);
        } catch (const std::exception &e) {
          out.push_back(on.message->name() + " / node " + std::to_string(n) +
                        ": " + e.what());
        }
      }
    }
    return out;
  }
};
} // namespace zengine::flow
#endif
