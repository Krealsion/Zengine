// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_MODEL_HPP
#define ZENGINE_FLOW_PANE_MODEL_HPP
#include "flow/workspace.hpp"
#include "flow/author.hpp"
#include "flow/graph.hpp"
#include "component/text_box.hpp"
#include <optional>

namespace zengine::flow_pane {
namespace flow = zengine::flow;
namespace md = zengine::message_draft;
enum class Page { Graph, State, Messages, Library, Events };
enum class Effect { None, Run, Apply, Stop, Send, Catalog, Inspect };
struct Action { Effect effect = Effect::None; loom::Bytes payload; };
struct Entry { std::string label; zengine::component::TextBox text; };
struct Dialog { std::string title, action; std::vector<Entry> entries; std::size_t selected = 0; };

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
    std::size_t trigger() const { return static_cast<std::size_t>(workspace.active_trigger); }
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
    loom::TypeRef type(std::string spelling) const {
        std::size_t lists = 0, offset = 0;
        while (spelling.compare(offset, 5, "List:") == 0) {
            if (++lists > 64) throw std::invalid_argument("type exceeds 64 List nesting levels");
            offset += 5;
        }
        const auto base = std::string_view(spelling).substr(offset);
        loom::TypeRef result;
        if (base.starts_with("Message:")) {
            const auto* saved = workspace.library.find(base.substr(8));
            if (!saved) throw std::invalid_argument("Message:<saved draft name> needs an existing library schema");
            result = loom::type_message(saved->schema());
        }
        else if (base == "Bytes") result = loom::type_of(loom::Kind::Bytes);
        else result = loom::type_of(flow::scalar_kind(base));
        while (lists > 0) { result = loom::type_list(std::move(result)); --lists; }
        return result;
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
        *this = std::move(candidate);
        return result;
    }
private:
    Action perform(const std::string& action, const std::vector<std::string>& args) {
        const auto previous_notice = notice;
        auto need = [&](std::size_t size) { if (args.size() != size) throw std::invalid_argument(action + " expects " + std::to_string(size) + " arguments"); };
        if (action == "describe") {
            notice = "new(name,discard), state-field(name,type,required), message(name), message-field(index,name,type,required), trigger(message,output), add-node(identity), bind(node,port,$field|%node|constant), result(node), move(node,x,y), remove(node), save(path), open(path,discard), export-project(path), import-project(path,discard), preset(name), drafts, draft-open(index), run, apply, send, inspect, stop";
        } else if (action == "new") {
            need(2); if (dirty && args[1] != "discard") throw std::invalid_argument("save the draft or explicitly choose discard");
            if (running) throw std::invalid_argument("stop this project's session before creating another");
            workspace = flow::Workspace{}; workspace.graph = flow::GraphDraft(args[0]);
            node.reset(); form.reset(); form_key.clear(); form_title.clear(); form_state = false; state_edited = false; connecting.reset(); page = Page::Graph; path.clear(); touched();
        } else if (action == "state-field") {
            need(3); if (args[2] != "required" && args[2] != "optional") throw std::invalid_argument("presence must be required or optional");
            workspace.graph.state_field(args[0], type(args[1]), args[2] == "required"); edited_state(); state_form();
        } else if (action == "message") {
            need(1); message = workspace.graph.message(args[0], {}); touched(); message_form(message);
        } else if (action == "message-field") {
            need(4); if (args[3] != "required" && args[3] != "optional") throw std::invalid_argument("presence must be required or optional");
            message = flow::index_of(args[0]); workspace.graph.message_field(message, args[1], type(args[2]), args[3] == "required"); touched(); message_form(message);
        } else if (action == "trigger") {
            need(2); workspace.active_trigger = static_cast<std::int64_t>(workspace.graph.trigger(flow::index_of(args[0]), args[1])); page = Page::Graph; node.reset(); touched();
        } else if (action == "select-trigger") {
            need(1); const auto at = flow::index_of(args[0]); (void)workspace.graph.project.definition.on.at(at); workspace.active_trigger = static_cast<std::int64_t>(at); node.reset(); connecting.reset(); first_row = 0;
        } else if (action == "add-node") {
            need(1); node = workspace.graph.add(trigger(), workspace.graph.ports(palette, args[0])); page = Page::Graph; touched();
        } else if (action == "bind") {
            need(3); const auto n = flow::index_of(args[0]), port = flow::index_of(args[1]);
            zengine::op::Binding binding = zengine::op::Binding::input("");
            if (!args[2].empty() && args[2][0] == '$') binding = zengine::op::Binding::input(args[2].substr(1));
            else if (!args[2].empty() && args[2][0] == '%') binding = zengine::op::Binding::node(flow::index_of(args[2].substr(1)));
            else {
                const auto& on = workspace.graph.project.definition.on.at(trigger());
                const auto& ports = workspace.graph.ports(palette, on.body.nodes.at(n).identity);
                const auto kind = ports.inputs->fields().at(port).type.kind;
                if (kind != loom::Kind::Int && kind != loom::Kind::Bool) throw std::invalid_argument("operator constants currently support Int and Bool; wire another kind from a field");
                binding = zengine::op::Binding::constant(flow::scalar(kind, args[2]));
            }
            workspace.graph.bind(trigger(), n, port, std::move(binding), palette); touched();
        } else if (action == "result") {
            need(1); workspace.graph.result(trigger(), flow::index_of(args[0]), palette); touched();
        } else if (action == "remove") {
            need(1); workspace.graph.remove(trigger(), flow::index_of(args[0])); node.reset(); connecting.reset(); touched();
        } else if (action == "move") {
            need(3); auto& p = workspace.graph.place(trigger(), flow::index_of(args[0]));
            const auto x = flow::integer(args[1]), y = flow::integer(args[2]);
            if (x < -10000000 || x > 10000000 || y < -10000000 || y > 10000000) throw std::invalid_argument("node position is outside workspace bounds");
            p.x = x; p.y = y; touched();
        } else if (action == "export-project") {
            need(1); const auto errors = workspace.graph.problems(palette);
            if (!errors.empty()) throw std::invalid_argument(errors.front());
            flow::save_project(args[0], workspace.graph.project);
        } else if (action == "import-project") {
            need(2); if (dirty && args[1] != "discard") throw std::invalid_argument("save the draft or explicitly choose discard");
            if (running) throw std::invalid_argument("stop the session before importing another project");
            auto project = flow::open_project(args[0]);
            workspace = flow::Workspace{}; workspace.graph = flow::GraphDraft(std::move(project));
            form.reset(); form_key.clear(); form_title.clear(); form_state = false;
            node.reset(); connecting.reset(); path.clear(); page = Page::Graph; state_edited = false; touched();
        } else if (action == "save") {
            need(1); retain_form(); flow::save_workspace(args[0], workspace); path = args[0]; dirty = false; notice = "Saved " + path; return {};
        } else if (action == "open") {
            need(2); if (dirty && args[1] != "discard") throw std::invalid_argument("save the draft or explicitly choose discard");
            if (running) throw std::invalid_argument("stop this project's session before opening another");
            auto opened = flow::open_workspace(args[0]); workspace = std::move(opened); path = args[0]; dirty = false;
            state_edited = false; node.reset(); restore_form(); connecting.reset(); page = Page::Graph;
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
        if (notice == previous_notice) notice = action + " complete";
        return {};
    }
};
} // namespace zengine::flow_pane
#endif
