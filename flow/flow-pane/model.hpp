// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_MODEL_HPP
#define ZENGINE_FLOW_PANE_MODEL_HPP
#include "flow/workspace.hpp"
#include "flow/author.hpp"
#include "flow/flow-pane/observed.hpp"
#include "flow/generate.hpp"
#include "flow/graph.hpp"
#include "operator/reference.hpp"
#include "component/text_box.hpp"
#include "workshop/powers_vocabulary.hpp"
#include <optional>

namespace zengine::flow_pane {
namespace flow = zengine::flow;
namespace md = zengine::message_draft;
enum class Page { Graph, State, Messages, Library, Events };
enum class Effect { None, Run, Apply, Stop, Send, Catalog, Inspect };
struct Action { Effect effect = Effect::None; loom::Bytes payload; };
struct Entry { std::string label; zengine::component::TextBox text; };
struct Dialog { std::string title, action; std::vector<Entry> entries; std::size_t selected = 0; };
/// An input port a maker selected to fill: its node by the place's stable id, so removing
/// another node never moves the choice onto a different one.
struct PortChoice { std::int64_t place = 0; std::size_t port = 0; };
/// A value a person carried here and dropped, waiting for them to say what it should become; the
/// port it landed on, if any, by its node's stable place. Another workspace puts it down, since
/// that graph numbers its places afresh.
struct Dropped { loom::Value value; std::optional<PortChoice> port; };
/// An operator reference dropped on a fold's body slot, by that slot's place: the reference the
/// body is chosen from, whatever the door finds under its identity now.
struct SlotReference { std::int64_t slot = 0; zengine::op::OperatorRef ref; };
/// One end of a wire a hand holds: an output -- a node's answer or a field in scope -- or an input
/// port, by its node's stable place.
struct WireEnd { std::optional<zengine::op::Binding> source; std::optional<PortChoice> port; };
/// A wire dragged from where it was pressed to the hand, in the area's pixels, and the port it
/// would bind to there, lit.
struct WireDrawn {
    std::int64_t from_x = 0, from_y = 0, to_x = 0, to_y = 0;
    std::optional<PortChoice> lit;
};
/// THE SEARCH A WIRE LET GO ON EMPTY GRAPH ROOM OPENED: the end it wires, where the list stands in
/// the area's pixels, where a node made from it is placed in the graph, the draft's revision it was
/// opened against, the row the keys are on, and whether the door has answered its question.
struct WireSearch {
    WireEnd from;
    std::int64_t x = 0, y = 0, place_x = 0, place_y = 0, revision = 0;
    std::size_t selected = 0;
    bool answered = false;
};

class Model {
public:
    flow::Workspace workspace;
    flow::Palette palette;
    Page page = Page::Graph;
    std::optional<std::size_t> node;
    std::size_t message = 0, first_row = 0;
    std::optional<md::Draft> form;
    std::string form_title, form_key, path, notice = "Create state, an input message and a trigger; then connect operators.";
    bool dirty = false, running = false, form_state = false;
    bool state_edited = false;
    std::uint64_t state_epoch = 0;
    std::optional<Dialog> dialog;
    std::vector<std::string> events;
    std::optional<zengine::op::Binding> connecting;
    // Finding what to compose, none of it saved: the search line, the port being filled, the
    // discovery door's last answer (replaced whole, never a copy of the catalog) and the row
    // previewed, by its kind and identity.
    zengine::component::TextBox search;
    std::optional<PortChoice> filling;
    /// A fold whose body slot is open, by its node's stable place id: the door is asked for what
    /// a fold could spend, and choosing a row chooses its body.
    std::optional<std::int64_t> body_slot;
    std::optional<SlotReference> slot_reference;
    std::optional<Dropped> dropped;
    /// A wire being dragged, and the search one let go on empty room opened; neither is saved.
    std::optional<WireDrawn> wire_drawn;
    std::optional<WireSearch> wire_search;
    zengine::workshop::PowersFound discovered;
    bool discovered_read = false;
    std::string preview, preview_kind;
    /// Run was refused: the Next line says what it needs first, until an edit is taken.
    bool nudge = false;
    /// The rule ran and has been stopped since: the Next line says it is put away.
    bool ran = false;
    /// The definition the Flow host runs, as its last answer said; none while nothing runs.
    std::optional<zengine::maker::Definition> running_definition;
    /// The state the running rule was run with, and what its journal says it did since.
    std::optional<loom::Value> started;
    Observed observed;
    /// What reached the rule in the runs before this one, oldest first: a restart starts a journal
    /// afresh, and the Events go on from these.
    std::vector<Arrival> earlier;
    std::size_t trigger() const { return static_cast<std::size_t>(workspace.active_trigger); }
    /// Another rule in the draft's place: what the rule before it did is not this one's to show.
    void forget_runs() {
        ran = false;
        started.reset();
        observed = {};
        earlier.clear();
        events.clear();
    }
    void touched() { dirty = true; }
    void edited_state() {
        if (state_epoch == std::numeric_limits<std::uint64_t>::max()) throw std::invalid_argument("state edit epoch exhausted");
        ++state_epoch; state_edited = true; touched();
    }
    void ask(std::string title, std::string action, const std::vector<std::pair<std::string,std::string>>& fields) {
        Dialog next{std::move(title), std::move(action), {}, 0};
        for (const auto& [label, value] : fields) { Entry e; e.label = label; e.text.set(value, value.size()); next.entries.push_back(std::move(e)); }
        dialog = std::move(next);
    }
    void retain_form() {
        if (!form) return;
        if (form_key.empty()) throw std::invalid_argument("active form has no retention key");
        auto found = std::find_if(workspace.forms.begin(), workspace.forms.end(), [&](const auto& saved) { return saved.key == form_key; });
        if (found == workspace.forms.end()) workspace.forms.push_back({form_key, form_title, form_state, *form});
        else {
            if (!loom::same_identity(*found->draft.schema(), *form->schema()))
                throw std::invalid_argument("retained form schema changed; open a distinct draft");
            *found = {form_key, form_title, form_state, *form};
        }
        workspace.active_form = form_key;
    }
    void restore_form() {
        form.reset(); form_key.clear(); form_title.clear(); form_state = false; message = 0;
        if (workspace.active_form.empty()) return;
        const auto found = std::find_if(workspace.forms.begin(), workspace.forms.end(), [&](const auto& saved) {
            return saved.key == workspace.active_form;
        });
        if (found == workspace.forms.end()) throw std::invalid_argument("workspace has no retained active form");
        form = found->draft; form_key = found->key; form_title = found->title; form_state = found->state;
        if (!form_state) {
            const auto& accepted = workspace.graph.project.definition.accepts;
            const auto matching = std::find_if(accepted.begin(), accepted.end(), [&](const auto& schema) {
                return loom::same_identity(*schema, *form->schema());
            });
            if (matching != accepted.end()) message = static_cast<std::size_t>(matching - accepted.begin());
        }
    }
    static std::string key_for(const std::string& scope, const loom::Schema& schema) {
        return scope + "/" + std::to_string(schema.version()) + ":" + std::to_string(schema.content_id()) + ":" + schema.name();
    }
    void open_form(const std::string& scope, const std::shared_ptr<const loom::Schema>& schema,
                   const std::string& title, bool state, const loom::Value* initial = nullptr) {
        retain_form();
        const auto key = key_for(scope, *schema);
        const auto found = std::find_if(workspace.forms.begin(), workspace.forms.end(), [&](const auto& saved) { return saved.key == key; });
        if (found != workspace.forms.end()) form = found->draft;
        else {
            const auto old = std::find_if(workspace.forms.begin(), workspace.forms.end(), [&](const auto& saved) {
                return saved.state == state && saved.draft.schema()->name() == schema->name() &&
                    saved.draft.schema()->version() == schema->version() && !loom::same_identity(*saved.draft.schema(), *schema);
            });
            if (old != workspace.forms.end()) notice = "Schema changed; the previous draft remains in Retained drafts. Fill the new shape explicitly.";
            if (initial) form.emplace(*initial); else form.emplace(schema);
        }
        form_key = key; form_title = title; form_state = state;
        retain_form();
    }
    void state_form() {
        open_form("state", workspace.graph.project.definition.state, "Initial / saved state", true, &workspace.graph.project.state);
        page = Page::State; first_row = 0;
    }
    void message_form(std::size_t which) {
        const auto schema = workspace.graph.project.definition.accepts.at(which);
        open_form("message", schema, schema->name(), false);
        message = which; page = Page::Messages; first_row = 0;
    }
    loom::TypeRef type(const std::string& spelling) const {
        return flow::shape::type_named(spelling, workspace.library);
    }
    void keep_form() {
        if (!form) throw std::invalid_argument("open a value first");
        const auto admitted = form->admit();
        if (!admitted) throw std::invalid_argument(admitted.first_error().message());
        if (form_state) {
            if (!loom::same_identity(*form->schema(), *workspace.graph.project.definition.state))
                throw std::invalid_argument("state schema changed; reopen its form");
            workspace.graph.project.state = admitted.value(); edited_state();
        }
    }
    Action command(const std::string& action, const std::vector<std::string>& args = {}) {
        Model candidate = *this;
        auto result = candidate.perform(action, args);
        // Every committed authoring state must fit the same envelope used by reload.
        (void)flow::workspace_bytes(candidate.workspace);
        // An edit taken answers the refused Run: the Next line reads the draft afresh.
        candidate.nudge = false;
        *this = std::move(candidate);
        return result;
    }
    /// Declare a shape exactly as it came, as one edit: refused, the model is as it was.
    void declare(std::shared_ptr<const loom::Schema> shape, bool emitted) {
        Model candidate = *this;
        candidate.workspace.graph.declare(std::move(shape), emitted);
        (void)flow::workspace_bytes(candidate.workspace);
        candidate.touched();
        *this = std::move(candidate);
    }
    /// An operator reference from its three spelled parts; a content id is Loom's Int, read back
    /// to the 64 bits it is.
    static zengine::op::OperatorRef reference(const std::string& identity, const std::string& in,
                                              const std::string& out) {
        if (identity.empty()) throw std::invalid_argument("an operator reference names an identity");
        return {identity, static_cast<loom::ContentId>(flow::integer(in)),
                static_cast<loom::ContentId>(flow::integer(out))};
    }
    /// THE VERBS THAT WRITE A FILE, each with the word a notice names it by; empty for every other
    /// verb. A hand reaches one only through its dialog's confirm, which asks Workshop for the
    /// confirming actor's class `write` before `command` runs it. `command` judges no actor.
    static std::string writing(const std::string& action) {
        if (action == "save") return "Saving";
        if (action == "generate") return "Generating";
        if (action == "export-project") return "Exporting";
        if (action == "library-save") return "Saving the library";
        return {};
    }
    static bool writes_file(const std::string& action) { return !writing(action).empty(); }
    /// What an edit that wrote a file says it did, and nothing for one that wrote none.
    static std::string wrote(const std::string& action, const std::vector<std::string>& args) {
        if (args.empty()) return {};
        if (action == "export-project") return "Exported " + args[0];
        if (action == "generate") return "Generated into " + args[0];
        if (action == "library-save") return "Saved the library to " + args[0];
        return {};
    }
    /// A wire's source as `bind` spells one: `$field` or `%node`.
    static zengine::op::Binding wire_source(const std::string& text) {
        if (text.size() > 1 && text[0] == '$') return zengine::op::Binding::input(text.substr(1));
        if (text.size() > 1 && text[0] == '%')
            return zengine::op::Binding::node(flow::index_of(text.substr(1)));
        throw std::invalid_argument("a wire's source is $field or %node");
    }
private:
    /// A port's binding as `bind` spells one: `$field`, `%node`, or a constant of the port's kind.
    zengine::op::Binding binding_of(std::size_t n, std::size_t port, const std::string& text) const {
        if (!text.empty() && text[0] == '$') return zengine::op::Binding::input(text.substr(1));
        if (!text.empty() && text[0] == '%') return zengine::op::Binding::node(flow::index_of(text.substr(1)));
        const auto& on = workspace.graph.project.definition.on.at(trigger());
        (void)on.body.nodes.at(n);
        const auto ports = workspace.graph.node_ports(trigger(), n, palette);
        const auto kind = ports.inputs->fields().at(port).type.kind;
        if (kind != loom::Kind::Int && kind != loom::Kind::Bool) throw std::invalid_argument("operator constants currently support Int and Bool; wire another kind from a field");
        return zengine::op::Binding::constant(flow::scalar(kind, text));
    }
    /// Set node `n`'s place in the graph, within the workspace's bounds.
    void place_at(std::size_t n, const std::string& xs, const std::string& ys) {
        auto& p = workspace.graph.place(trigger(), n);
        const auto x = flow::integer(xs), y = flow::integer(ys);
        if (x < -10000000 || x > 10000000 || y < -10000000 || y > 10000000)
            throw std::invalid_argument("node position is outside workspace bounds");
        p.x = x; p.y = y;
    }
    Action perform(const std::string& action, const std::vector<std::string>& args) {
        const auto previous_notice = notice;
        auto need = [&](std::size_t size) { if (args.size() != size) throw std::invalid_argument(action + " expects " + std::to_string(size) + " arguments"); };
        if (action == "describe") {
            notice = "new(name,discard), state-field(name,type,required), keep(name,type), message(name), message-field(index,name,type,required), trigger(message,output), add-node(identity,authored_in,authored_out[,before]), add-node-into(identity,authored_in,authored_out,node,port[,x,y]), add-node-from(identity,authored_in,authored_out,$field|%node,port[,x,y]), add-fold([before]), add-fold-from($field|%node,port[,x,y]), fold-body(node,identity,authored_in,authored_out,count,accumulator), emitted-message(name), emitted-field(index,name,type,required), emit(index,field=$state|field=constant...), emit-remove(index), generate(directory), bind(node,port,$field|%node|constant), bind-all(node,binding...), say(name,fields), result(node), move(node,x,y), remove(node), save(path), open(path,discard), export-project(path), import-project(path,discard), preset(name), drafts, draft-open(index), run, apply, send, inspect, stop";
        } else if (action == "new") {
            need(2); if (dirty && args[1] != "discard") throw std::invalid_argument("save the draft or explicitly choose discard");
            if (running) throw std::invalid_argument("stop this project's session before creating another");
            workspace = flow::Workspace{}; workspace.graph = flow::GraphDraft(args[0]); forget_runs();
            node.reset(); form.reset(); form_key.clear(); form_title.clear(); form_state = false; state_edited = false; connecting.reset(); filling.reset(); body_slot.reset(); slot_reference.reset(); dropped.reset(); page = Page::Graph; path.clear(); touched();
        } else if (action == "state-field") {
            need(3); if (args[2] != "required" && args[2] != "optional") throw std::invalid_argument("presence must be required or optional");
            workspace.graph.state_field(args[0], type(args[1]), args[2] == "required"); edited_state(); state_form();
        } else if (action == "keep") {
            // A FIELD THE RULE KEEPS BETWEEN MESSAGES, and, where a message is declared and no
            // trigger yet reacts to one, the trigger that message wakes, writing this field.
            need(2);
            workspace.graph.state_field(args[0], type(args[1]), true);
            auto& def = workspace.graph.project.definition;
            if (def.on.empty() && !def.accepts.empty())
                workspace.active_trigger = static_cast<std::int64_t>(workspace.graph.trigger(0, args[0]));
            edited_state(); page = Page::Graph;
        } else if (action == "bind-all") {
            // A STEP'S PORTS WIRED AT ONE PRESS: the node, then a binding a port, an empty one leaving
            // that port as it is; one edit, refused whole.
            if (args.empty()) throw std::invalid_argument("bind-all expects a node, then a binding a port");
            const auto n = flow::index_of(args[0]);
            for (std::size_t p = 1; p < args.size(); ++p)
                if (!args[p].empty()) workspace.graph.bind(trigger(), n, p - 1, binding_of(n, p - 1, args[p]), palette);
            touched();
        } else if (action == "say") {
            // A MESSAGE THE ACTIVE TRIGGER SAYS AFTER ITS WRITE: declared inside the definition's
            // name with the kept fields it carries, unless declared already, then said -- one edit.
            need(2);
            auto& def = workspace.graph.project.definition;
            if (def.on.empty()) throw std::invalid_argument("a rule says a message after its change: give it a message and what it keeps first");
            const auto name = flow::shape::qualified(def.name, args[0]);
            std::optional<std::size_t> index;
            for (std::size_t i = 0; i < def.emits.size(); ++i)
                if (def.emits[i]->name() == name) index = i;
            if (!index) {
                index = workspace.graph.message(args[0], {}, true);
                for (const auto& word : flow::words(args[1])) {
                    const auto* kept = workspace.graph.project.definition.state->find(word);
                    if (!kept) throw std::invalid_argument("the rule keeps no `" + word + "` to say");
                    workspace.graph.emitted_field(*index, word, kept->type, true);
                }
            }
            workspace.graph.emit(trigger(), *index, {}); page = Page::Graph; touched();
        } else if (action == "message") {
            need(1); message = workspace.graph.message(args[0], {}); touched(); message_form(message);
        } else if (action == "message-field") {
            need(4); if (args[3] != "required" && args[3] != "optional") throw std::invalid_argument("presence must be required or optional");
            message = flow::index_of(args[0]); workspace.graph.message_field(message, args[1], type(args[2]), args[3] == "required"); touched(); message_form(message);
        } else if (action == "trigger") {
            need(2); workspace.active_trigger = static_cast<std::int64_t>(workspace.graph.trigger(flow::index_of(args[0]), args[1])); page = Page::Graph; node.reset(); filling.reset(); body_slot.reset(); slot_reference.reset(); touched();
        } else if (action == "select-trigger") {
            need(1); const auto at = flow::index_of(args[0]); (void)workspace.graph.project.definition.on.at(at); workspace.active_trigger = static_cast<std::int64_t>(at); node.reset(); connecting.reset(); filling.reset(); body_slot.reset(); slot_reference.reset(); first_row = 0;
        } else if (action == "add-node") {
            // An operator reference -- the identity and the two content ids it was found at -- and
            // where: at the end, or before a node, renumbering what follows.
            if (args.size() != 3 && args.size() != 4)
                throw std::invalid_argument("add-node expects an identity, the two content ids it was found at, and optionally the node to place it before");
            std::optional<std::size_t> before;
            if (args.size() == 4) before = flow::index_of(args[3]);
            node = workspace.graph.add(trigger(), reference(args[0], args[1], args[2]), palette, before);
            page = Page::Graph; touched();
        } else if (action == "add-node-into") {
            // Placed before the node whose port it fills, and wired into that port, in one edit;
            // given a place, it stands there in the graph.
            if (args.size() != 5 && args.size() != 7)
                throw std::invalid_argument(
                    "add-node-into expects an identity, the two content ids it was found at, the "
                    "node and its port, and optionally the place x and y");
            const auto into = flow::index_of(args[3]), port = flow::index_of(args[4]);
            const auto added = workspace.graph.add(trigger(), reference(args[0], args[1], args[2]), palette, into);
            workspace.graph.bind(trigger(), into + 1, port, zengine::op::Binding::node(added), palette);
            if (args.size() == 7) place_at(added, args[5], args[6]);
            node = added; filling.reset(); page = Page::Graph; touched();
        } else if (action == "add-node-from" || action == "add-fold-from") {
            // A power, or the fold, placed after what it reads -- after node %n, or at the
            // trigger's start for a field -- one of its ports wired from it, in one edit; given a
            // place, it stands there in the graph.
            const bool fold = action == "add-fold-from";
            const std::size_t given = fold ? 2 : 5;
            if (args.size() != given && args.size() != given + 2)
                throw std::invalid_argument(
                    action + " expects " +
                    (fold ? std::string()
                          : std::string("an identity, the two content ids it was found at, ")) +
                    "the source ($field or %node) and the port, and optionally the place x and y");
            const auto source = wire_source(args[given - 2]);
            const auto port = flow::index_of(args[given - 1]);
            std::size_t after = 0;
            if (source.from() == zengine::op::Binding::From::Node) {
                const auto& nodes = workspace.graph.project.definition.on.at(trigger()).body.nodes;
                if (source.node_index() >= nodes.size())
                    throw std::invalid_argument("there is no node " +
                                                std::to_string(source.node_index()) +
                                                " to wire from");
                after = source.node_index() + 1;
            }
            const auto added =
                fold ? workspace.graph.add_fold(trigger(), after)
                     : workspace.graph.add(trigger(), reference(args[0], args[1], args[2]), palette,
                                           after);
            workspace.graph.bind(trigger(), added, port, source, palette);
            if (args.size() == given + 2) place_at(added, args[given], args[given + 1]);
            node = added; connecting.reset(); filling.reset(); page = Page::Graph; touched();
        } else if (action == "emitted-message") {
            need(1); (void)workspace.graph.message(args[0], {}, true); page = Page::Messages; touched();
        } else if (action == "emitted-field") {
            need(4); if (args[3] != "required" && args[3] != "optional") throw std::invalid_argument("presence must be required or optional");
            workspace.graph.emitted_field(flow::index_of(args[0]), args[1], type(args[2]), args[3] == "required"); touched();
        } else if (action == "emit") {
            // The emitted message, then each field's write: `field=$state` or `field=constant`,
            // one argument each or several in one, as a dialog confirms them.
            if (args.empty()) throw std::invalid_argument("emit expects the emitted message's index, then its fields");
            const auto which = flow::index_of(args[0]);
            const auto& shape = workspace.graph.project.definition.emits.at(which);
            std::vector<zengine::maker::FieldSource> fields;
            for (std::size_t i = 1; i < args.size(); ++i)
                for (const auto& word : flow::words(args[i])) {
                    const auto [field, value] = flow::split(word, '=');
                    const auto* target = shape->find(field);
                    if (!target) throw std::invalid_argument(shape->name() + " has no field " + field);
                    if (!value.empty() && value[0] == '$') fields.push_back({field, value.substr(1), std::nullopt});
                    else fields.push_back({field, std::nullopt, flow::scalar(target->type.kind, value)});
                }
            workspace.graph.emit(trigger(), which, std::move(fields)); touched();
        } else if (action == "emit-remove") {
            need(1); workspace.graph.remove_emit(trigger(), flow::index_of(args[0])); touched();
        } else if (action == "generate") {
            // Native C++ for the definition, or the refusal in words; nothing is written then.
            need(1); const auto errors = workspace.graph.problems(palette);
            if (!errors.empty()) throw std::invalid_argument(errors.front());
            notice = "Generated " + flow::write_generated(workspace.graph.project.definition, args[0]);
            return {};
        } else if (action == "add-fold") {
            if (args.size() > 1) throw std::invalid_argument("add-fold expects at most the node to place it before");
            std::optional<std::size_t> before;
            if (args.size() == 1) before = flow::index_of(args[0]);
            node = workspace.graph.add_fold(trigger(), before); page = Page::Graph; touched();
        } else if (action == "fold-body") {
            need(6); const auto n = flow::index_of(args[0]);
            workspace.graph.fold_body(trigger(), n, reference(args[1], args[2], args[3]), args[4], args[5], palette);
            node = n; body_slot.reset(); slot_reference.reset(); touched();
        } else if (action == "bind") {
            need(3); const auto n = flow::index_of(args[0]), port = flow::index_of(args[1]);
            workspace.graph.bind(trigger(), n, port, binding_of(n, port, args[2]), palette); touched();
        } else if (action == "result") {
            need(1); workspace.graph.result(trigger(), flow::index_of(args[0]), palette); touched();
        } else if (action == "remove") {
            need(1); workspace.graph.remove(trigger(), flow::index_of(args[0])); node.reset(); connecting.reset(); touched();
        } else if (action == "move") {
            need(3); place_at(flow::index_of(args[0]), args[1], args[2]); touched();
        } else if (action == "export-project") {
            need(1); const auto errors = workspace.graph.problems(palette);
            if (!errors.empty()) throw std::invalid_argument(errors.front());
            flow::save_project(args[0], workspace.graph.project);
        } else if (action == "import-project") {
            need(2); if (dirty && args[1] != "discard") throw std::invalid_argument("save the draft or explicitly choose discard");
            if (running) throw std::invalid_argument("stop the session before importing another project");
            auto project = flow::open_project(args[0]);
            workspace = flow::Workspace{}; workspace.graph = flow::GraphDraft(std::move(project)); forget_runs();
            form.reset(); form_key.clear(); form_title.clear(); form_state = false;
            node.reset(); connecting.reset(); filling.reset(); body_slot.reset(); slot_reference.reset(); dropped.reset(); path.clear(); page = Page::Graph; state_edited = false; touched();
        } else if (action == "save") {
            need(1); retain_form(); flow::save_workspace(args[0], workspace); path = args[0]; dirty = false; notice = "Saved " + path; return {};
        } else if (action == "open") {
            need(2); if (dirty && args[1] != "discard") throw std::invalid_argument("save the draft or explicitly choose discard");
            if (running) throw std::invalid_argument("stop this project's session before opening another");
            auto opened = flow::open_workspace(args[0]); workspace = std::move(opened); path = args[0]; dirty = false; forget_runs();
            state_edited = false; node.reset(); restore_form(); connecting.reset(); filling.reset(); body_slot.reset(); slot_reference.reset(); dropped.reset(); page = Page::Graph;
        } else if (action == "message-open") { need(1); message_form(flow::index_of(args[0]));
        } else if (action == "state-open") { need(0); state_form();
        } else if (action == "value") {
            need(2); if (!form) throw std::invalid_argument("open a message or state form");
            const auto rows = form->rows(); form->set_text(rows.at(flow::index_of(args[0])).path, args[1]);
            retain_form(); touched();
        } else if (action == "unset") {
            need(1); if (!form) throw std::invalid_argument("open a form"); const auto rows = form->rows(); form->unset(rows.at(flow::index_of(args[0])).path);
            retain_form(); touched();
        } else if (action == "container") {
            need(1); if (!form) throw std::invalid_argument("open a form"); const auto row = form->rows().at(flow::index_of(args[0]));
            if (row.type.kind == loom::Kind::Message) {
                if (!row.present) form->create_message(row.path);
                else notice = "Message already present; edit its fields below, or unset it explicitly.";
            }
            else if (row.type.kind == loom::Kind::List) {
                if (!row.present) form->create_list(row.path);
                else if (row.type.element->kind == loom::Kind::Message)
                    form->append(row.path, loom::Cell::message(loom::Value(row.type.element->message)));
                else form->append(row.path, zengine::maker::default_cell(*row.type.element));
            } else throw std::invalid_argument("selected field is not a container");
            retain_form(); touched();
        } else if (action == "list-erase") {
            need(2); if (!form) throw std::invalid_argument("open a form"); const auto row = form->rows().at(flow::index_of(args[0]));
            form->erase(row.path, flow::index_of(args[1])); retain_form(); touched();
        } else if (action == "keep-state") { need(0); keep_form();
        } else if (action == "preset") {
            need(1); if (!form) throw std::invalid_argument("open a value first"); workspace.library.put(args[0], *form); touched();
        } else if (action == "preset-open") {
            need(1); const auto* value = workspace.library.find(args[0]); if (!value) throw std::invalid_argument("unknown saved draft");
            const auto schema = value->schema();
            open_form("preset:" + std::to_string(args[0].size()) + ":" + args[0], schema, args[0], false, &value->value());
            page = Page::Library; first_row = 0;
        } else if (action == "drafts") {
            need(0); notice = "Retained drafts:";
            for (std::size_t i = 0; i < workspace.forms.size(); ++i) {
                const auto& saved = workspace.forms[i];
                notice += " " + std::to_string(i) + "=" + saved.title + " (" + saved.draft.schema()->name() +
                    " v" + std::to_string(saved.draft.schema()->version()) + ", shape " + std::to_string(saved.draft.schema()->content_id()) + ")";
            }
        } else if (action == "draft-open") {
            need(1); retain_form(); const auto saved = workspace.forms.at(flow::index_of(args[0]));
            workspace.active_form = saved.key; restore_form(); page = saved.state ? Page::State : Page::Messages; first_row = 0;
        } else if (action == "preset-delete") { need(1); workspace.library.erase(args[0]); touched();
        } else if (action == "library-save") { need(1); md::save_library(args[0], workspace.library);
        } else if (action == "library-open") { need(1); auto loaded = md::open_library(args[0]); auto combined = workspace.library; for (const auto& p : loaded.entries()) combined.put(p.title, p.draft); workspace.library = std::move(combined); touched();
        } else if (action == "run" || action == "apply") {
            need(0); const auto errors = workspace.graph.problems(palette); if (!errors.empty()) throw std::invalid_argument(errors.front());
            if (action == "run") return {Effect::Run, flow::byte_vector(flow::project_bytes(workspace.graph.project))};
            return {Effect::Apply, flow::byte_vector(zengine::maker::definition_bytes(workspace.graph.project.definition))};
        } else if (action == "send") {
            need(0); if (!form || form_state) throw std::invalid_argument("open a message or reusable value to send");
            const auto value = form->admit(); if (!value) throw std::invalid_argument(value.first_error().message());
            return {Effect::Send, flow::byte_vector(loom::serialize(value.value()))};
        } else if (action == "stop") { need(0); return {Effect::Stop, {}};
        } else if (action == "inspect") { need(0); return {Effect::Inspect, {}};
        } else if (action == "catalog") { need(0); return {Effect::Catalog, {}};
        } else throw std::invalid_argument("unknown Flow edit: " + action);
        // AN EDIT THAT DID ITS WORK SAYS NOTHING, the pane showing what it did; a file it wrote
        // is named.
        if (notice == previous_notice) notice = wrote(action, args);
        return {};
    }
};
} // namespace zengine::flow_pane
#endif
