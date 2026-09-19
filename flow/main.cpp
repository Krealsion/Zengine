// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#include "flow/author.hpp"
#include "flow/compiled.hpp"
#include "flow/example.hpp"
#include "flow/generate.hpp"
#include "flow/graph.hpp"
#include "operator/primitives.hpp"

#include <zen/kernel/kernel.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
namespace flow = zengine::flow;
namespace maker = zengine::maker;
namespace op = zengine::op;

class Observer final : public loom::Weave {
public:
    explicit Observer(std::vector<std::shared_ptr<const loom::Schema>> accepts)
        : accepts_(std::move(accepts)) {}
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override { return accepts_; }
    void handle(const loom::Message& message, loom::Bus&) override { messages.push_back(message); }
    loom::Value snapshot() const override {
        return loom::Value(loom::make_schema("zengine.flow.Observer", 1, {}));
    }
    loom::Value policy() const override { return maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
    std::vector<loom::Message> messages;
private:
    std::vector<std::shared_ptr<const loom::Schema>> accepts_;
};

class Session {
public:
    explicit Session(flow::Project project, const std::string& artifact = {})
        : definition(std::move(project.definition)), kernel(bus) {
        op::publish_primitives(catalog);
        if (artifact.empty()) {
            const auto registered = maker::register_definition(bus, catalog, definition);
            if (!registered) throw std::runtime_error(registered.reason);
            subject = registered.id;
        } else {
            module = flow::CompiledModule::open(catalog, artifact);
            if (maker::definition_bytes(module->definition()) != maker::definition_bytes(definition))
                throw std::runtime_error("artifact does not contain this exact definition; generate/build it first");
            const auto mounted = module->mount();
            if (!mounted) throw std::runtime_error(mounted.reason);
            const auto offer = module->offer();
            const auto loaded = kernel.load("flow", artifact, definition.name, maker::default_grant(definition));
            if (!loaded.ok) throw std::runtime_error(loaded.error);
            subject = loaded.id;
        }
        const auto restored = bus.swap_state(subject, loom::serialize(project.state));
        if (!restored.revived) throw std::runtime_error(restored.refusal.message());
        auto accepts = definition.emits;
        accepts.push_back(loom::schema_of<loom::Refused>());
        auto observer = std::make_unique<Observer>(std::move(accepts));
        observed = observer.get();
        loom::Grant grant;
        for (const auto& schema : definition.accepts) grant.allow_to_any(schema->name(), schema->version());
        client = bus.register_weave(std::move(observer), std::move(grant));
    }

    flow::Project project() const { return {definition, bus.weave(subject)->snapshot()}; }
    void send(const std::vector<std::string>& args) {
        if (args.size() < 2) throw std::invalid_argument("send Message field=value ...");
        auto name = args[1];
        if (name.find('.') == std::string::npos) name = definition.name + "." + name;
        std::shared_ptr<const loom::Schema> schema;
        for (const auto& candidate : definition.accepts) if (candidate->name() == name) schema = candidate;
        if (!schema) throw std::invalid_argument("unknown accepted message " + name);
        bus.send_as(client, subject, loom::Message(flow::fields(schema, args, 2), client, client, ++correlation));
        pump();
    }
    void pump() {
        // Yield after a bounded number of queue generations. Feedback can remain pending.
        for (unsigned turn = 0; turn < 8 && bus.pending() != 0; ++turn) bus.pump_pending();
        for (const auto& message : observed->messages)
            std::cout << "message " << loom::compat::serialize(message.payload) << '\n';
        observed->messages.clear();
        std::cout << "state " << loom::compat::serialize(project().state) << '\n';
        if (bus.pending() != 0) std::cout << "pending " << bus.pending() << "; use pump to continue\n";
    }
    void apply(const flow::Project& edit) {
        if (module) throw std::invalid_argument("use interpret before editing; regenerate to change native code");
        const auto edited = maker::apply_behaviour_edit(bus, catalog, subject, edit.definition);
        if (!edited) throw std::runtime_error(edited.reason);
        definition = edit.definition;
    }

    maker::Definition definition;
    op::Catalog catalog;
    std::shared_ptr<flow::CompiledModule> module;
    loom::Switchboard bus;
    loom::Kernel kernel;
    loom::WeaveId subject{}, client{};
    Observer* observed = nullptr;
    std::uint64_t correlation = 0;
};

// Generated source expands the retained binary definition and its graph statements.
constexpr std::uintmax_t kMaxGeneratedBytes = 64u << 20;

std::string read(const std::string& path, std::uintmax_t limit = maker::kMaxFileBytes) {
    const auto result = maker::read_file(path, limit);
    if (!result) throw std::runtime_error(result.reason);
    return result.bytes;
}

void write(const std::filesystem::path& path, const std::string& bytes) {
    const auto error = maker::write_file(path.string(), bytes);
    if (!error.empty()) throw std::runtime_error(error);
}

void generate(const maker::Definition& definition, const std::filesystem::path& directory) {
    const auto source = directory / "generated.cpp";
    const auto cmake = directory / "CMakeLists.txt";
    if (std::filesystem::exists(source)) {
        const auto previous = flow::recover_cpp(read(source.string(), kMaxGeneratedBytes));
        if (previous.name != definition.name) throw std::runtime_error("output belongs to a different definition");
    }
    if (std::filesystem::exists(cmake) && read(cmake.string()) != flow::generated_cmake())
        throw std::runtime_error("CMakeLists.txt has local edits; choose a new output directory");
    const auto cpp = flow::generate_cpp(definition);
    if (cpp.size() > kMaxGeneratedBytes)
        throw std::runtime_error("generated source exceeds the workbench's 64 MiB limit");
    std::filesystem::create_directories(directory);
    write(source, cpp);
    write(cmake, flow::generated_cmake());
    write(directory / "definition.bin", maker::definition_bytes(definition));
    write(directory / "graph.svg", flow::graph_svg(definition));
    std::cout << "generated " << source.string() << '\n';
}

constexpr const char* help = R"help(Flow workbench — author, run, inspect, save and generate maker definitions.
  example                         start the thermostat example
  new NAME                        start an empty draft (run commits it)
  state FIELD KIND INITIAL        declare scalar state before triggers
  accept MESSAGE FIELD:KIND ...   declare an input (KIND? makes a field optional)
  publish MESSAGE FIELD:KIND ...  declare an output
  on MESSAGE STATE_FIELD         start/replace one trigger
  node OPERATOR $input %node ...  add a node (Int/Bool constants allowed)
  result NODE                    select the answer node
  emit MESSAGE FIELD=$state ...  publish after state write
  end                            finish the trigger
  run                            run the completed new draft
  edit / apply / cancel          edit live behavior, preserve its current state
  send MESSAGE FIELD=value ...  send an input; print outputs and state
  state / graph / operators      inspect current state, graph or operator contracts
  graph FILE.svg                 export a graph
  pump                           continue pending feedback, yielding again if needed
  save FILE.flow / open FILE.flow save/reopen definition and live state together
  export-json DIRECTORY           export editable debug-codec projections
  import-json DEFINITION [STATE]  admit full maker schemas as a new project
  generate DIRECTORY              write native C++ and installed-package CMake project
  native ARTIFACT                 load matching compiled artifact, preserve state
  interpret                      return to interpreted form, preserve state
  help / quit
Use quoted text for spaces; paths inside quotes escape backslashes as \\.
Native loading executes code you selected. No compiler runs implicitly.
)help";

class Workbench {
public:
    Workbench() { op::publish_primitives(authoring_); }
    bool command(const std::vector<std::string>& args) {
        if (args.empty()) return true;
        const auto& verb = args[0];
        auto count = [&](std::size_t n) {
            if (args.size() != n) throw std::invalid_argument("wrong argument count for " + verb);
        };
        if (verb == "quit") { count(1); return false; }
        if (verb == "help") { count(1); std::cout << help; }
        else if (verb == "example") { count(1); replace(flow::thermostat(authoring_)); }
        else if (verb == "new") { count(2); draft_ = std::make_unique<flow::Draft>(authoring_, args[1]); editing_ = false; }
        else if (verb == "edit") {
            count(1); require_session();
            draft_ = std::make_unique<flow::Draft>(authoring_, session_->project()); editing_ = true;
        } else if (verb == "cancel") { count(1); draft_.reset(); editing_ = false; }
        else if (verb == "run" || verb == "apply") {
            count(1); if (!draft_) throw std::invalid_argument("no draft");
            const auto project = draft_->finish();
            if (verb == "apply") {
                require_session();
                if (!editing_) throw std::invalid_argument("use run for a new project");
                session_->apply(project);
            } else {
                if (editing_) throw std::invalid_argument("use apply for a live edit");
                replace(project);
            }
            draft_.reset(); editing_ = false;
        } else if (verb == "open") { count(2); replace(flow::open_project(args[1])); }
        else if (verb == "import-json") {
            if (args.size() < 2 || args.size() > 3) throw std::invalid_argument("import-json DEFINITION [STATE]");
            auto definition = flow::definition_json(read(args[1]));
            auto state = maker::default_value(definition.state);
            if (args.size() == 3) {
                auto admitted = loom::admit(loom::compat::parse(read(args[2])), definition.state);
                if (!admitted) throw std::invalid_argument(admitted.first_error().message());
                state = std::move(admitted).value();
            }
            replace(flow::make_project(std::move(definition), std::move(state)));
        } else if (verb == "operators") {
            count(1);
            for (const auto& id : authoring_.identities()) {
                const auto* definition = authoring_.find(id);
                std::cout << id << " (";
                for (const auto& field : definition->inputs()->fields())
                    std::cout << field.name << ':' << loom::name_of(field.type.kind) << ' ';
                std::cout << ") -> " << loom::name_of(definition->outputs()->fields()[0].type.kind) << '\n';
            }
        } else if (verb == "graph") {
            if (args.size() > 2) throw std::invalid_argument("graph [FILE.svg]");
            const auto definition = draft_ ? draft_->finish().definition : project().definition;
            if (args.size() == 2) write(args[1], flow::graph_svg(definition));
            else for (const auto& line : flow::graph_lines(definition)) std::cout << line << '\n';
        } else if (verb == "save") { count(2); flow::save_project(args[1], project()); }
        else if (verb == "generate") { count(2); generate(project().definition, args[1]); }
        else if (verb == "export-json") {
            count(2); const auto current = project();
            std::filesystem::create_directories(args[1]);
            write(std::filesystem::path(args[1]) / "definition.json", loom::compat::serialize(maker::encode_definition(current.definition)));
            write(std::filesystem::path(args[1]) / "state.json", loom::compat::serialize(current.state));
        } else if (verb == "native" || verb == "interpret") {
            count(verb == "native" ? 2 : 1);
            const auto current = project();
            if (session_->bus.pending() != 0) throw std::invalid_argument("pending messages must finish before switching forms");
            replace(current, verb == "native" ? args[1] : std::string{});
        } else if (verb == "send") { require_session(); session_->send(args); }
        else if (verb == "pump") { count(1); require_session(); session_->pump(); }
        else if (verb == "state" && args.size() == 1) {
            std::cout << "state " << loom::compat::serialize(project().state) << '\n';
        } else {
            if (!draft_) throw std::invalid_argument("no draft; use new or edit (help lists commands)");
            draft_->command(args);
        }
        return true;
    }
private:
    void require_session() const { if (!session_) throw std::invalid_argument("no running project"); }
    flow::Project project() const {
        require_session();
        if (draft_) throw std::invalid_argument("apply, run or cancel the draft first");
        return session_->project();
    }
    void replace(const flow::Project& next, const std::string& artifact = {}) {
        auto candidate = std::make_unique<Session>(next, artifact);
        session_ = std::move(candidate);
        draft_.reset(); editing_ = false;
        std::cout << "running " << next.definition.name << " r" << next.definition.revision
                  << (artifact.empty() ? " interpreted\n" : " native\n");
    }
    op::Catalog authoring_;
    std::unique_ptr<Session> session_;
    std::unique_ptr<flow::Draft> draft_;
    bool editing_ = false;
};
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") { std::cout << help; return 0; }
        if (argc == 3 && std::string_view(argv[1]) == "--generate-example") {
            op::Catalog catalog; op::publish_primitives(catalog);
            const auto example = flow::thermostat(catalog);
            generate(example.definition, argv[2]);
            flow::save_project((std::filesystem::path(argv[2]) / "thermostat.flow").string(), example);
            return 0;
        }
        if (argc == 4 && std::string_view(argv[1]) == "--recover") {
            write(argv[3], maker::definition_bytes(flow::recover_cpp(read(argv[2], kMaxGeneratedBytes))));
            return 0;
        }
        std::ifstream file;
        const bool interactive = argc == 1;
        if (!interactive) {
            if (argc != 3 || std::string_view(argv[1]) != "--script")
                throw std::invalid_argument("use --help, --script FILE, --generate-example DIR or --recover CPP DEFINITION");
            file.open(argv[2]);
            if (!file) throw std::runtime_error("cannot read command script");
        }
        auto& input = interactive ? std::cin : file;
        Workbench workbench;
        if (interactive) std::cout << "Zengine Flow — help lists commands.\n";
        std::string line;
        std::size_t number = 0;
        while (true) {
            if (interactive) std::cout << "flow> " << std::flush;
            if (!std::getline(input, line)) break;
            ++number;
            try { if (!workbench.command(flow::words(line))) break; }
            catch (const std::exception& e) {
                std::cerr << "refused at line " << number << ": " << e.what() << '\n';
                if (!interactive) return 1;
            }
        }
        if (input.bad()) throw std::runtime_error("command input failed");
        return 0;
    } catch (const std::exception& e) { std::cerr << "refused: " << e.what() << '\n'; return 1; }
}
