// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_WORKSPACE_HPP
#define ZENGINE_FLOW_WORKSPACE_HPP
#include "flow/graph_edit.hpp"
#include "message-draft/library.hpp"
#include <set>

namespace zengine::flow {
struct EmptyTrigger {
    std::int64_t index = 0, message = 0;
    std::string output;
    ZEN_SHAPE(EmptyTrigger, 1, ZEN_FIELD(index), ZEN_FIELD(message), ZEN_FIELD(output));
};
struct StoredFormFile {
    std::string key, title;
    bool state = false;
    loom::Bytes draft;
    ZEN_SHAPE(StoredFormFile, 1, ZEN_FIELD(key), ZEN_FIELD(title), ZEN_FIELD(state), ZEN_FIELD(draft));
};
struct WorkspaceFile {
    loom::Bytes project, library;
    std::vector<NodePlace> places;
    std::vector<EmptyTrigger> empty_triggers;
    std::vector<StoredFormFile> forms;
    std::string active_form;
    std::int64_t next_id = 1, active_trigger = 0, pan_x = 0, pan_y = 0, zoom = 100;
    ZEN_SHAPE(WorkspaceFile, 1, ZEN_FIELD(project), ZEN_FIELD(library), ZEN_FIELD(places),
              ZEN_FIELD(empty_triggers), ZEN_FIELD(forms), ZEN_FIELD(active_form), ZEN_FIELD(next_id), ZEN_FIELD(active_trigger),
              ZEN_FIELD(pan_x), ZEN_FIELD(pan_y), ZEN_FIELD(zoom));
};
inline loom::Bytes byte_vector(std::string_view text) { return {text.begin(), text.end()}; }
inline std::string byte_string(const loom::Bytes& bytes) { return {bytes.begin(), bytes.end()}; }
// Historical form schemas are separate observations. They must not be combined
// into one Registry or silently rewritten when an author changes an unlaunched schema.
struct StoredForm {
    std::string key, title;
    bool state = false;
    message_draft::Draft draft;
};
struct Workspace {
    GraphDraft graph{};
    message_draft::Library library;
    std::vector<StoredForm> forms;
    std::string active_form;
    std::int64_t active_trigger = 0, pan_x = 0, pan_y = 0, zoom = 100;
};

// Empty triggers are editor work, not executable compositions. Preserve them as
// draft metadata without changing maker's admission rule or inventing dummy nodes.
inline std::string workspace_bytes(const Workspace& workspace) {
    WorkspaceFile file;
    auto project = workspace.graph.project;
    auto& ons = project.definition.on;
    for (std::size_t i = 0; i < ons.size(); ++i) {
        if (!ons[i].body.nodes.empty()) continue;
        const auto& accepts = project.definition.accepts;
        const auto found = std::find_if(accepts.begin(), accepts.end(), [&](const auto& s) {
            return loom::same_identity(*s, *ons[i].message);
        });
        if (found == accepts.end()) throw std::invalid_argument("draft trigger has no accepted message");
        file.empty_triggers.push_back({static_cast<std::int64_t>(i),
            static_cast<std::int64_t>(found - accepts.begin()), ons[i].output});
    }
    ons.erase(std::remove_if(ons.begin(), ons.end(), [](const auto& on) { return on.body.nodes.empty(); }), ons.end());
    file.project = byte_vector(project_bytes(project));
    file.library = byte_vector(message_draft::library_bytes(workspace.library));
    std::set<std::string> form_keys;
    for (const auto& form : workspace.forms) {
        if (form.key.empty() || !form_keys.insert(form.key).second)
            throw std::invalid_argument("saved forms need distinct nonempty keys");
        message_draft::Library draft;
        draft.put("value", form.draft);
        file.forms.push_back({form.key, form.title, form.state,
            byte_vector(message_draft::library_bytes(draft))});
    }
    if (!workspace.active_form.empty() && !form_keys.contains(workspace.active_form))
        throw std::invalid_argument("active form is not retained in the workspace");
    file.active_form = workspace.active_form;
    file.places = workspace.graph.places;
    file.next_id = workspace.graph.next_id;
    file.active_trigger = workspace.active_trigger;
    file.pan_x = workspace.pan_x; file.pan_y = workspace.pan_y; file.zoom = workspace.zoom;
    auto bytes = loom::serialize(loom::to_value(file));
    if (bytes.size() > maker::kMaxFileBytes) throw std::invalid_argument("workspace exceeds 1 MiB; export or remove saved drafts before adding more");
    return bytes;
}

inline Workspace read_workspace(std::string_view bytes) {
    if (bytes.size() > maker::kMaxFileBytes) throw std::invalid_argument("workspace exceeds 1 MiB");
    const auto gate = loom::admit(loom::parse(bytes), loom::schema_of<WorkspaceFile>());
    if (!gate) throw std::invalid_argument(gate.first_error().message());
    const auto file = loom::from_value<WorkspaceFile>(gate.value());
    Workspace out;
    out.graph = GraphDraft(read_project(byte_string(file.project)));
    auto& def = out.graph.project.definition;
    std::int64_t previous = -1;
    for (const auto& empty : file.empty_triggers) {
        if (empty.index <= previous || empty.index < 0 || static_cast<std::size_t>(empty.index) > def.on.size() ||
            empty.message < 0 || static_cast<std::size_t>(empty.message) >= def.accepts.size() || !def.state->find(empty.output))
            throw std::invalid_argument("invalid empty trigger in workspace");
        def.on.insert(def.on.begin() + empty.index, {def.accepts[static_cast<std::size_t>(empty.message)], {}, empty.output, {}});
        previous = empty.index;
    }
    out.library = message_draft::read_library(byte_string(file.library));
    std::set<std::string> form_keys;
    for (const auto& stored : file.forms) {
        if (stored.key.empty() || !form_keys.insert(stored.key).second)
            throw std::invalid_argument("saved forms need distinct nonempty keys");
        const auto draft = message_draft::read_library(byte_string(stored.draft));
        if (draft.entries().size() != 1 || !draft.find("value"))
            throw std::invalid_argument("saved form must contain exactly its one value draft");
        out.forms.push_back({stored.key, stored.title, stored.state, *draft.find("value")});
    }
    if (!file.active_form.empty() && !form_keys.contains(file.active_form))
        throw std::invalid_argument("active form is not retained in the workspace");
    out.active_form = file.active_form;
    std::set<std::int64_t> ids;
    std::set<std::pair<std::int64_t, std::int64_t>> subjects;
    std::size_t count = 0;
    for (const auto& on : def.on) count += on.body.nodes.size();
    if (file.places.size() != count) throw std::invalid_argument("workspace node layout is incomplete");
    for (const auto& p : file.places) {
        if (p.id <= 0 || p.id >= file.next_id || p.trigger < 0 || static_cast<std::size_t>(p.trigger) >= def.on.size() ||
            p.node < 0 || static_cast<std::size_t>(p.node) >= def.on[static_cast<std::size_t>(p.trigger)].body.nodes.size() ||
            p.x < -10000000 || p.x > 10000000 || p.y < -10000000 || p.y > 10000000 || !ids.insert(p.id).second ||
            !subjects.insert({p.trigger, p.node}).second) throw std::invalid_argument("invalid node layout identity or position");
    }
    if (file.next_id <= 0 || file.zoom < 50 || file.zoom > 200 || file.pan_x < -10000000 || file.pan_x > 10000000 ||
        file.pan_y < -10000000 || file.pan_y > 10000000 || file.active_trigger < 0 ||
        (!def.on.empty() && static_cast<std::size_t>(file.active_trigger) >= def.on.size()))
        throw std::invalid_argument("invalid workspace view");
    out.graph.places = file.places; out.graph.next_id = file.next_id;
    out.active_trigger = file.active_trigger; out.pan_x = file.pan_x; out.pan_y = file.pan_y; out.zoom = file.zoom;
    return out;
}
inline void save_workspace(const std::string& path, const Workspace& workspace) {
    const auto bytes = workspace_bytes(workspace);
    if (bytes.size() > maker::kMaxFileBytes) throw std::invalid_argument("workspace exceeds 1 MiB");
    (void)read_workspace(bytes); // a successful save must be one this reader can reopen
    const auto error = maker::write_file(path, bytes);
    if (!error.empty()) throw std::runtime_error(error);
}
inline Workspace open_workspace(const std::string& path) {
    const auto read = maker::read_file(path);
    if (!read) throw std::runtime_error(read.reason);
    return read_workspace(read.bytes);
}
} // namespace zengine::flow
#endif
