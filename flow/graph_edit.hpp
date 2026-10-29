// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_GRAPH_EDIT_HPP
#define ZENGINE_FLOW_GRAPH_EDIT_HPP

#include "flow/project.hpp"
#include "flow/shape.hpp"
#include "operator/fold.hpp"
#include "operator/reference.hpp"
#include <algorithm>
#include <optional>
#include <limits>
#include <map>

namespace zengine::flow {
/// WHETHER A TRIGGER HAS AN ANSWER: one of its nodes is the value its output becomes. A delete of
/// the answer leaves none (`result_node` past the last node) until one is chosen.
inline bool answered(const maker::On &on) { return on.body.result_node < on.body.nodes.size(); }

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
  /// The ports a node takes and answers, whatever its form: a step's are its operator's; a
  /// fold's derive from its body (`op::fold_ports`), and before its body is chosen it has only
  /// its three counting ports and answers nothing.
  Ports node_ports(std::size_t t, std::size_t n, const Palette &palette) const {
    const auto &node = project.definition.on.at(t).body.nodes.at(n);
    if (!node.fold)
      return ports(palette, node.identity);
    const auto integer = loom::type_of(loom::Kind::Int);
    if (node.identity.empty()) {
      std::vector<loom::Field> counting;
      for (const char *name : {op::kFoldStart, op::kFoldLimit, op::kFoldStep})
        counting.push_back(loom::Field{name, integer, true});
      return {"", loom::make_schema("fold.in", 1, std::move(counting)),
              loom::make_schema("fold.out", 1, std::vector<loom::Field>{})};
    }
    const auto &body = ports(palette, node.identity);
    auto derived = op::fold_ports(node.identity, *body.inputs, *body.outputs, *node.fold);
    return {node.identity,
            loom::make_schema(node.identity + ".fold.in", 1, std::move(derived.inputs)),
            body.outputs};
  }
  void own(const loom::Schema &shape) const {
    if (!owns(shape))
      throw std::invalid_argument(shape.name() + " came from elsewhere: its sender says it as it is");
  }
  /// Accepted message `i` replaced by `next`, and each trigger it woke wakes on `next`.
  void replace_accepted(std::size_t i, std::shared_ptr<const loom::Schema> next) {
    auto &shape = project.definition.accepts.at(i);
    const auto previous = shape;
    shape = std::move(next);
    for (auto &on : project.definition.on)
      if (loom::same_identity(*on.message, *previous))
        on.message = shape;
    changed();
  }
  /// Each port of `on` reading `name` -- a kept field, or a field of its message where nothing kept
  /// is so named -- reads `to` instead.
  static void reads_of(maker::On &on, const std::string &name, const op::Binding &to) {
    for (auto &node : on.body.nodes)
      for (auto &b : node.arguments)
        if (b.from() == op::Binding::From::Input && b.input_name() == name)
          b = to;
  }
  /// Each trigger on `shape` reading its field `name`, where nothing kept is so named, reads `to`.
  void rewire_message_reads(const loom::Schema &shape, const std::string &name, const op::Binding &to) {
    if (project.definition.state->find(name))
      return;
    for (auto &on : project.definition.on)
      if (loom::same_identity(*on.message, shape))
        reads_of(on, name, to);
  }
  /// The state as `fields`, each value carried from the field `was` names it was, but `fresh`, which
  /// starts at its kind's default.
  template <class Was>
  void rekeep(std::vector<loom::Field> fields, Was was, const std::string &fresh = {}) {
    const auto schema = loom::make_schema(project.definition.state->name(),
                                          project.definition.state->version(), std::move(fields));
    loom::Value state(schema);
    for (const auto &f : schema->fields()) {
      const auto *c = f.name == fresh ? nullptr : project.state.get(was(f.name));
      if (c)
        state.set(f.name, *c);
      else if (f.required)
        state.set(f.name, maker::default_cell(f.type));
    }
    project.definition.state = schema;
    project.state = std::move(state);
  }
  /// Each say a kept field no longer feeds as it was declared goes: a source that names no kept
  /// field of the said field's kind is dropped, and an emit then missing a required field goes.
  void prune_emits() {
    const auto &state = *project.definition.state;
    for (auto &on : project.definition.on) {
      std::vector<maker::Emit> kept;
      for (auto &e : on.emits) {
        std::vector<maker::FieldSource> fed;
        for (const auto &source : e.fields) {
          bool fits = true;
          if (source.source) {
            const auto *field = state.find(*source.source);
            const auto *said = e.message->find(source.field);
            fits = field && said && op::detail::same_type(field->type, said->type);
          }
          if (fits)
            fed.push_back(source);
        }
        bool whole = true;
        for (const auto &f : e.message->fields()) {
          bool named = false;
          for (const auto &source : fed)
            named = named || source.field == f.name;
          whole = whole && (!f.required || named);
        }
        if (whole) {
          e.fields = std::move(fed);
          kept.push_back(std::move(e));
        }
      }
      on.emits = std::move(kept);
    }
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
    name = shape::qualified(project.definition.name, std::move(name));
    auto &shapes =
        emitted ? project.definition.emits : project.definition.accepts;
    shape::refuse_taken(shapes, name);
    shapes.push_back(shape::make(name, std::move(fields)));
    changed();
    return shapes.size() - 1;
  }
  /// Declare a shape exactly as it came -- its name, version and fields, so its identity is the
  /// one the value or description carried -- as accepted or, within the definition's namespace,
  /// emitted.
  std::size_t declare(std::shared_ptr<const loom::Schema> shape, bool emitted) {
    editable();
    auto &shapes = emitted ? project.definition.emits : project.definition.accepts;
    shape::refuse_taken(shapes, shape->name());
    const auto prefix = project.definition.name + ".";
    if (emitted && shape->name().rfind(prefix, 0) != 0)
      throw std::invalid_argument("an emitted message's name must begin `" + prefix + "`; `" +
                                  shape->name() + "` does not");
    shapes.push_back(std::move(shape));
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
    const auto previous = shape;
    shape = shape::with_field(*shape, name, std::move(type), required);
    for (auto &on : project.definition.on)
      if (loom::same_identity(*on.message, *previous))
        on.message = shape;
    changed();
  }
  /// A field of an emitted message, as `message_field` adds one to an accepted message. An emit
  /// already writing that message gains a source for it from the same-named state field; with no
  /// such field the edit is refused rather than leaving an emit that cannot be planned.
  void emitted_field(std::size_t index, const std::string &name, loom::TypeRef type,
                     bool required = true) {
    editable();
    auto &shape = project.definition.emits.at(index);
    const auto previous = shape;
    const auto next = shape::with_field(*shape, name, std::move(type), required);
    for (auto &on : project.definition.on)
      for (auto &e : on.emits)
        if (loom::same_identity(*e.message, *previous)) {
          if (required && !project.definition.state->find(name))
            throw std::invalid_argument(
                "the emit of " + previous->name() + " on " + on.message->name() +
                " would write no `" + name + "`; remove that emit or declare state." + name);
          e.message = next;
          if (required)
            e.fields.push_back({name, name, std::nullopt});
        }
    shape = next;
    changed();
  }
  /// Publish an emitted message after the active trigger's write, each field written from a
  /// state field or a constant as the definition holds it; a field named in neither comes from
  /// the state field of its own name. An emit on a trigger with no nodes is refused: a trigger
  /// kept empty is saved without its emits.
  void emit(std::size_t t, std::size_t emitted, std::vector<maker::FieldSource> fields) {
    editable();
    auto &on = project.definition.on.at(t);
    if (on.body.nodes.empty())
      throw std::invalid_argument("add the trigger's nodes before its emits: an empty trigger "
                                  "is kept without them");
    const auto &shape = project.definition.emits.at(emitted);
    for (const auto &f : shape->fields()) {
      const bool named = std::any_of(fields.begin(), fields.end(),
                                     [&](const auto &s) { return s.field == f.name; });
      if (!named && project.definition.state->find(f.name))
        fields.push_back({f.name, f.name, std::nullopt});
    }
    on.emits.push_back({shape, std::move(fields)});
    changed();
  }
  // ---- THE EDITS A RIGHT PRESS OFFERS ---------------------------------------------------------

  /// RENAME ACCEPTED MESSAGE `i`, declared in the definition's namespace: its fields and version
  /// stay, and the trigger it wakes follows it. A message from elsewhere keeps the name it came with,
  /// or it would no longer be the one its sender says.
  void rename_message(std::size_t i, const std::string &name) {
    editable();
    auto &shape = project.definition.accepts.at(i);
    own(*shape);
    const auto next_name = shape::qualified(project.definition.name, name);
    if (next_name == shape->name())
      return;
    shape::refuse_taken(project.definition.accepts, next_name);
    shape::refuse_taken(project.definition.emits, next_name);
    replace_accepted(i, loom::make_schema(next_name, shape->version(), shape->fields()));
  }
  /// RENAME, RETYPE OR REMOVE A FIELD of accepted message `i`, declared here. A port reading it
  /// reads the new name; retyped, a port of another kind is a lack to repair; removed, it is open.
  void rename_message_field(std::size_t i, const std::string &from, const std::string &to) {
    editable();
    const auto shape = project.definition.accepts.at(i);
    own(*shape);
    if (!shape->find(from))
      throw std::invalid_argument(shape->name() + " has no field " + from);
    if (to.empty() || (to != from && shape->find(to)))
      throw std::invalid_argument("message field name is empty or already used");
    auto fields = shape->fields();
    for (auto &f : fields)
      if (f.name == from)
        f.name = to;
    rewire_message_reads(*shape, from, op::Binding::input(to));
    replace_accepted(i, loom::make_schema(shape->name(), shape->version(), std::move(fields)));
  }
  void retype_message_field(std::size_t i, const std::string &name, loom::TypeRef type) {
    editable();
    const auto shape = project.definition.accepts.at(i);
    own(*shape);
    if (!shape->find(name))
      throw std::invalid_argument(shape->name() + " has no field " + name);
    auto fields = shape->fields();
    for (auto &f : fields)
      if (f.name == name)
        f.type = std::move(type);
    replace_accepted(i, loom::make_schema(shape->name(), shape->version(), std::move(fields)));
  }
  void remove_message_field(std::size_t i, const std::string &name) {
    editable();
    const auto shape = project.definition.accepts.at(i);
    own(*shape);
    if (!shape->find(name))
      throw std::invalid_argument(shape->name() + " has no field " + name);
    auto fields = shape->fields();
    fields.erase(std::remove_if(fields.begin(), fields.end(), [&](const auto &f) { return f.name == name; }),
                 fields.end());
    rewire_message_reads(*shape, name, op::Binding::input(""));
    replace_accepted(i, loom::make_schema(shape->name(), shape->version(), std::move(fields)));
  }
  /// TAKE TRIGGER `t` OUT OF THE RULE: its steps and places go with it; the message stays declared.
  void remove_trigger(std::size_t t) {
    editable();
    auto &ons = project.definition.on;
    (void)ons.at(t);
    ons.erase(ons.begin() + static_cast<std::ptrdiff_t>(t));
    const auto at = static_cast<std::int64_t>(t);
    places.erase(std::remove_if(places.begin(), places.end(), [&](const auto &p) { return p.trigger == at; }),
                 places.end());
    for (auto &p : places)
      if (p.trigger > at)
        --p.trigger;
    changed();
  }
  /// RENAME A KEPT FIELD: its value, the ports that read it, what a trigger writes into it and what
  /// a say carries from it follow the new name.
  void rename_kept(const std::string &from, const std::string &to) {
    editable();
    const auto &state = *project.definition.state;
    if (!state.find(from))
      throw std::invalid_argument("the rule keeps no " + from);
    if (to.empty() || (to != from && state.find(to)))
      throw std::invalid_argument("state field name is empty or already used");
    if (to == from)
      return;
    auto fields = state.fields();
    for (auto &f : fields)
      if (f.name == from)
        f.name = to;
    rekeep(std::move(fields), [&](const std::string &name) { return name == to ? from : name; });
    for (auto &on : project.definition.on) {
      reads_of(on, from, op::Binding::input(to));
      if (on.output == from)
        on.output = to;
      for (auto &e : on.emits)
        for (auto &s : e.fields)
          if (s.source && *s.source == from)
            s.source = to;
    }
    changed();
  }
  /// A KEPT FIELD OF ANOTHER KIND: its value starts again at that kind's default, and a say that
  /// carried it as the old kind no longer does.
  void retype_kept(const std::string &name, loom::TypeRef type) {
    editable();
    if (!project.definition.state->find(name))
      throw std::invalid_argument("the rule keeps no " + name);
    auto fields = project.definition.state->fields();
    for (auto &f : fields)
      if (f.name == name)
        f.type = std::move(type);
    rekeep(std::move(fields), [](const std::string &n) { return n; }, name);
    prune_emits();
    changed();
  }
  /// REMOVE A KEPT FIELD: the ports that read it are left open and a say that carried it goes.
  /// The field a trigger writes is refused: that trigger writes another first.
  void remove_kept(const std::string &name) {
    editable();
    if (!project.definition.state->find(name))
      throw std::invalid_argument("the rule keeps no " + name);
    for (const auto &on : project.definition.on)
      if (on.output == name)
        throw std::invalid_argument(on.message->name() + " writes " + name +
                                    ": choose another field for it to write first");
    auto fields = project.definition.state->fields();
    fields.erase(std::remove_if(fields.begin(), fields.end(), [&](const auto &f) { return f.name == name; }),
                 fields.end());
    rekeep(std::move(fields), [](const std::string &n) { return n; });
    for (auto &on : project.definition.on)
      reads_of(on, name, op::Binding::input(""));
    prune_emits();
    changed();
  }
  /// A KEPT FIELD'S VALUE, as the rule starts with it.
  void start_kept(const std::string &name, loom::Cell cell) {
    editable();
    if (!project.definition.state->find(name))
      throw std::invalid_argument("the rule keeps no " + name);
    auto state = project.state;
    state.set(name, std::move(cell));
    project.state = std::move(state);
    changed();
  }
  /// PORT `argument` OF NODE `n` LEFT OPEN.
  void unbind(std::size_t t, std::size_t n, std::size_t argument) {
    editable();
    project.definition.on.at(t).body.nodes.at(n).arguments.at(argument) = op::Binding::input("");
    changed();
  }
  /// EVERY WIRE TO AND FROM NODE `n` TAKEN AWAY: its ports open, the ports that read it open, and
  /// where it was the answer the trigger has none.
  void disconnect(std::size_t t, std::size_t n) {
    editable();
    auto &body = project.definition.on.at(t).body;
    for (auto &b : body.nodes.at(n).arguments)
      b = op::Binding::input("");
    for (auto &node : body.nodes)
      for (auto &b : node.arguments)
        if (b.from() == op::Binding::From::Node && b.node_index() == n)
          b = op::Binding::input("");
    if (body.result_node == n)
      body.result_node = body.nodes.size();
    changed();
  }
  /// THE WIRE INTO WHAT TRIGGER `t` WRITES TAKEN AWAY: it has no answer until one is chosen.
  void unanswer(std::size_t t) {
    editable();
    auto &body = project.definition.on.at(t).body;
    body.result_node = body.nodes.size();
    changed();
  }
  /// TRIGGER `t` WRITES ANOTHER KEPT FIELD.
  void write_into(std::size_t t, const std::string &name) {
    editable();
    if (!project.definition.state->find(name))
      throw std::invalid_argument("the rule keeps no " + name);
    project.definition.on.at(t).output = name;
    changed();
  }
  /// WHAT EMIT `e` OF TRIGGER `t` CARRIES: these kept fields, each under its own name. The message
  /// it says changes with it, wherever it is said.
  void say_with(std::size_t t, std::size_t e, const std::vector<std::string> &names) {
    editable();
    const auto previous = project.definition.on.at(t).emits.at(e).message;
    std::vector<loom::Field> fields;
    for (const auto &name : names) {
      const auto *kept = project.definition.state->find(name);
      if (!kept)
        throw std::invalid_argument("the rule keeps no `" + name + "` to say");
      fields.push_back({name, kept->type, true});
    }
    const auto next = shape::make(previous->name(), std::move(fields));
    for (auto &shape : project.definition.emits)
      if (loom::same_identity(*shape, *previous))
        shape = next;
    for (auto &on : project.definition.on)
      for (auto &emit : on.emits)
        if (loom::same_identity(*emit.message, *previous)) {
          emit.message = next;
          emit.fields.clear();
          for (const auto &name : names)
            emit.fields.push_back({name, name, std::nullopt});
        }
    changed();
  }

  void remove_emit(std::size_t t, std::size_t which) {
    editable();
    auto &emits = project.definition.on.at(t).emits;
    (void)emits.at(which);
    emits.erase(emits.begin() + static_cast<std::ptrdiff_t>(which));
    changed();
  }
  std::size_t add(std::size_t trigger_index, const Ports &signature,
                  std::optional<std::size_t> before = std::nullopt) {
    if (!signature.inputs || !signature.outputs ||
        signature.outputs->fields().size() != 1)
      throw std::invalid_argument("a graph node needs one output port");
    op::Node node;
    node.identity = signature.identity;
    node.authored_in = signature.inputs->content_id();
    node.authored_out = signature.outputs->content_id();
    // An empty input name is an explicit unwired port, never a fabricated zero.
    for (std::size_t i = 0; i < signature.inputs->fields().size(); ++i)
      node.arguments.push_back(op::Binding::input(""));
    return place_node(trigger_index, std::move(node), before);
  }
  /// Add a step for an operator reference, at the end or before node `before`. A reference
  /// whose operator the host's ports no longer describe at the signature it was found at is
  /// refused, never re-bound to whatever now bears the name.
  std::size_t add(std::size_t trigger_index, const op::OperatorRef &ref,
                  const Palette &palette,
                  std::optional<std::size_t> before = std::nullopt) {
    const auto found =
        std::find_if(palette.begin(), palette.end(),
                     [&](const auto &p) { return p.identity == ref.identity; });
    if (found == palette.end())
      throw std::invalid_argument(op::unsupplied_reason(ref.identity));
    if (found->inputs->content_id() != ref.authored_in ||
        found->outputs->content_id() != ref.authored_out)
      throw std::invalid_argument(op::reshaped_reason(ref.identity));
    return add(trigger_index, *found, before);
  }
  /// Place the evaluator's fold, its body not yet chosen: start, limit and step, the step bound
  /// to 1 -- the default a maker sees and changes like any constant.
  std::size_t add_fold(std::size_t trigger_index,
                       std::optional<std::size_t> before = std::nullopt) {
    op::Node node;
    node.fold = op::Fold{};
    node.arguments = {op::Binding::input(""), op::Binding::input(""),
                      op::Binding::constant(loom::Cell::integer(1))};
    return place_node(trigger_index, std::move(node), before);
  }
  /// Choose a fold's body: the reference it spends and the two body ports it threads. Start,
  /// limit and step keep their bindings, and so does the initial value while its type is still
  /// the body's answer; the body's other ports start unwired.
  void fold_body(std::size_t t, std::size_t n, const op::OperatorRef &ref,
                 const std::string &count, const std::string &accumulator,
                 const Palette &palette) {
    editable();
    auto &node = project.definition.on.at(t).body.nodes.at(n);
    if (!node.fold)
      throw std::invalid_argument("node " + std::to_string(n) + " is not a fold");
    const auto found =
        std::find_if(palette.begin(), palette.end(),
                     [&](const auto &p) { return p.identity == ref.identity; });
    if (found == palette.end())
      throw std::invalid_argument(op::unsupplied_reason(ref.identity));
    if (found->inputs->content_id() != ref.authored_in ||
        found->outputs->content_id() != ref.authored_out)
      throw std::invalid_argument(op::reshaped_reason(ref.identity));
    const op::Fold form{count, accumulator};
    const auto derived =
        op::fold_ports(ref.identity, *found->inputs, *found->outputs, form);
    bool used = false;
    for (const auto &other : project.definition.on.at(t).body.nodes)
      for (const auto &b : other.arguments)
        used = used || (b.from() == op::Binding::From::Node && b.node_index() == n);
    if (used && !node.identity.empty() &&
        !op::detail::same_type(derived.answer.type, node_answer_type(t, n, palette)))
      throw std::invalid_argument(
          "another node uses this fold's answer; reconnect it before changing its type");
    std::vector<op::Binding> arguments(derived.inputs.size(), op::Binding::input(""));
    for (std::size_t k = 0; k < 3 && k < node.arguments.size(); ++k)
      arguments[k] = node.arguments[k];
    if (node.arguments.size() > 3) {
      try {
        if (op::detail::same_type(source_type(t, node.arguments[3], palette),
                                  derived.inputs[3].type))
          arguments[3] = node.arguments[3];
      } catch (const std::exception &) {
      }
    }
    node.identity = ref.identity;
    node.authored_in = ref.authored_in;
    node.authored_out = ref.authored_out;
    node.fold = form;
    node.arguments = std::move(arguments);
    changed();
  }
  /// Put a node at the end of a trigger's body, or before node `before`: every binding that
  /// names a node from there on, the result and each layout place move up one, so the graph still
  /// means what it meant, the new node running just before the one it was placed ahead of.
  std::size_t place_node(std::size_t trigger_index, op::Node node,
                         std::optional<std::size_t> before) {
    editable();
    if (trigger_index >= project.definition.on.size())
      throw std::invalid_argument("add a trigger before adding a node");
    if (next_id == std::numeric_limits<std::int64_t>::max())
      throw std::invalid_argument("node identity exhausted");
    auto &body = project.definition.on.at(trigger_index).body;
    const auto at = before.value_or(body.nodes.size());
    if (at > body.nodes.size())
      throw std::invalid_argument("there is no node " + std::to_string(at) +
                                  " to place a node before");
    const bool had_nodes = !body.nodes.empty();
    body.nodes.insert(body.nodes.begin() + static_cast<std::ptrdiff_t>(at), std::move(node));
    for (std::size_t i = at + 1; i < body.nodes.size(); ++i)
      for (auto &b : body.nodes[i].arguments)
        if (b.from() == op::Binding::From::Node && b.node_index() >= at)
          b = op::Binding::node(b.node_index() + 1);
    if (!had_nodes)
      body.result_node = 0;
    else if (body.result_node >= at)
      ++body.result_node;
    const auto t = static_cast<std::int64_t>(trigger_index);
    for (auto &p : places)
      if (p.trigger == t && p.node >= static_cast<std::int64_t>(at))
        ++p.node;
    const auto count = static_cast<std::int64_t>(body.nodes.size() - 1);
    places.push_back({next_id++, t, static_cast<std::int64_t>(at),
                      1056 + (count % 3) * 1344, 240 + (count / 3) * 720});
    changed();
    return at;
  }
  /// What node `n` answers: a step's operator's one output, a fold's body's.
  loom::TypeRef node_answer_type(std::size_t t, std::size_t n,
                                 const Palette &palette) const {
    const auto p = node_ports(t, n, palette);
    if (p.outputs->fields().size() != 1)
      throw std::invalid_argument(
          p.identity.empty() ? "the fold answers nothing until its body is chosen"
                             : "source has no single output");
    return p.outputs->fields().front().type;
  }
  loom::TypeRef source_type(std::size_t t, const op::Binding &binding,
                            const Palette &palette) const {
    const auto &on = project.definition.on.at(t);
    if (binding.from() == op::Binding::From::Constant)
      return loom::type_of(binding.constant_cell().kind());
    if (binding.from() == op::Binding::From::Node)
      return node_answer_type(t, binding.node_index(), palette);
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
    if (!node.identity.empty()) {
      const auto &operator_ports = ports(palette, node.identity);
      if (operator_ports.inputs->content_id() != node.authored_in ||
          operator_ports.outputs->content_id() != node.authored_out)
        throw std::invalid_argument(
            "operator signature changed; add a node against its current ports");
    }
    const auto signature = node_ports(t, n, palette);
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
    // A DELETE TAKES EVERY CONNECTION WITH IT: each port that read the node is left open, and
    // where it was the answer the trigger has none (`answered` is false) until one is chosen.
    const bool answer = body.result_node == n;
    for (auto &node : body.nodes)
      for (auto &b : node.arguments)
        if (b.from() == op::Binding::From::Node && b.node_index() == n)
          b = op::Binding::input("");
    body.nodes.erase(body.nodes.begin() + static_cast<std::ptrdiff_t>(n));
    for (auto &node : body.nodes)
      for (auto &b : node.arguments)
        if (b.from() == op::Binding::From::Node && b.node_index() > n)
          b = op::Binding::node(b.node_index() - 1);
    if (answer || body.result_node > body.nodes.size())
      body.result_node = body.nodes.size();
    else if (body.result_node > n)
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
  /// WHETHER `shape` WAS DECLARED IN THIS DEFINITION'S NAMESPACE, as only such a message is
  /// renamed or reshaped here.
  bool owns(const loom::Schema &shape) const {
    return shape.name().rfind(project.definition.name + ".", 0) == 0;
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
  /// ONE THING THE DRAFT LACKS BEFORE IT RUNS: what kind of lack it is, the trigger it is in, the
  /// node and its port where it names them, and why, in this library's words.
  struct Problem {
    enum class Lack { Steps, Body, Ports, Order, Type, Answer, Other };
    Lack lack = Lack::Other;
    std::size_t trigger = 0;
    std::optional<std::size_t> node, port;
    std::string port_name, why;
  };
  std::vector<Problem> problem_list(const Palette &palette) const {
    using Lack = Problem::Lack;
    std::vector<Problem> out;
    // A refusal by a kind, its words in this library's; any other is said as it was thrown.
    struct Lacking : std::invalid_argument {
      Lack lack;
      Lacking(Lack l, const std::string &why) : std::invalid_argument(why), lack(l) {}
    };
    const auto lack_of = [](const std::exception &e) {
      const auto *said = dynamic_cast<const Lacking *>(&e);
      return said != nullptr ? said->lack : Lack::Other;
    };
    for (std::size_t t = 0; t < project.definition.on.size(); ++t) {
      const auto &on = project.definition.on[t];
      if (on.body.nodes.empty()) {
        out.push_back({Lack::Steps, t, std::nullopt, std::nullopt, {}, "add a result node"});
        continue;
      }
      if (!answered(on))
        out.push_back({Lack::Answer, t, std::nullopt, std::nullopt, {}, "choose a result node"});
      for (std::size_t n = 0; n < on.body.nodes.size(); ++n) {
        const auto &node = on.body.nodes[n];
        try {
          if (node.fold && node.identity.empty())
            throw Lacking(Lack::Body, "choose the fold's body");
          const auto &operator_ports = ports(palette, node.identity);
          if (node.authored_in != operator_ports.inputs->content_id() ||
              node.authored_out != operator_ports.outputs->content_id())
            throw Lacking(Lack::Ports, "operator signature changed");
          const auto p = node_ports(t, n, palette);
          if (!p.inputs || !p.outputs || p.outputs->fields().size() != 1)
            throw Lacking(Lack::Ports, "operator has no single output");
          if (!project.definition.state->find(on.output))
            throw std::invalid_argument("unknown state output");
          if (node.arguments.size() != p.inputs->fields().size())
            throw Lacking(Lack::Ports, "port count changed");
          for (std::size_t a = 0; a < node.arguments.size(); ++a) {
            const auto &b = node.arguments[a];
            try {
              if (b.from() == op::Binding::From::Node && b.node_index() >= n)
                throw Lacking(Lack::Order, "forward or cyclic connection");
              if (!op::detail::same_type(source_type(t, b, palette),
                                         p.inputs->fields()[a].type))
                throw Lacking(Lack::Type, "incompatible type");
            } catch (const std::exception &e) {
              out.push_back({lack_of(e), t, n, a, p.inputs->fields()[a].name, e.what()});
            }
          }
          if (n == on.body.result_node &&
              !op::detail::same_type(
                  p.outputs->fields()[0].type,
                  project.definition.state->find(on.output)->type))
            throw Lacking(Lack::Answer, "result type does not match state." + on.output);
        } catch (const std::exception &e) {
          out.push_back({lack_of(e), t, n, std::nullopt, {}, e.what()});
        }
      }
    }
    return out;
  }
  /// Each problem as one line: its trigger's message, its node and port where it names them, why.
  std::vector<std::string> problems(const Palette &palette) const {
    std::vector<std::string> out;
    for (const auto &p : problem_list(palette)) {
      auto line = project.definition.on.at(p.trigger).message->name();
      if (p.node) line += " / node " + std::to_string(*p.node);
      if (p.port) line += " / " + p.port_name;
      out.push_back(line + ": " + p.why);
    }
    return out;
  }
};
} // namespace zengine::flow
#endif
