
// config_codegen.cpp — complete generator (C++20)
// JSON → C++ header/impl with nested namespaces and leaf functions
// - Uses Zen Logger (Logger::log(LogLevel::*, ...))
// - Only OBJECT keys become namespaces
// - LEAF keys become functions in their parent namespace (no extra leaf namespace)
// - Preserves JSON order by default; optional --sort-keys=on for deterministic alpha
// - CLI: --in, --out-dir, --root-ns, --style=get|raw, --fail-on-unsupported, --sort-keys=on|off, --dry-run, --help

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

// Project headers
#include <iostream>

#include "../logic/utils.h"        // Zen::Utility::get_resources_path()
#include "logger.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// -----------------------------------------------------------------------------------------------
// CLI parsing
// -----------------------------------------------------------------------------------------------
struct Args {
  std::vector<std::string> positionals;             // e.g., input json as positional
  std::unordered_map<std::string, std::string> kv;  // --key=value or --key value
};

static Args parse_args(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    std::string s(argv[i]);
    if (s.rfind("--", 0) == 0) {
      auto eq = s.find('=');
      if (eq != std::string::npos) {
        a.kv.emplace(s.substr(2, eq - 2), s.substr(eq + 1));
      } else if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
        a.kv.emplace(s.substr(2), argv[++i]);
      } else {
        a.kv.emplace(s.substr(2), "true");
      }
    } else {
      a.positionals.emplace_back(std::move(s));
    }
  }
  return a;
}

static std::string get_arg(const Args& a, std::string_view k, std::string def = {}) {
  auto it = a.kv.find(std::string(k));
  return it == a.kv.end() ? std::move(def) : it->second;
}
static bool has_arg(const Args& a, std::string_view k) {
  return a.kv.find(std::string(k)) != a.kv.end();
}

static void print_help(const char* exe) {
  std::string x = exe ? exe : "configgen";
  Zen::Logger::log(Zen::LogLevel::INFO,
                   R"HELP(Usage:
  )HELP" + x + R"HELP( [--in=<file.json>] [--out-dir=<dir>] [--root-ns=<A::B>]
                 [--style=get|raw] [--sort-keys=on|off]
                 [--fail-on-unsupported] [--dry-run] [--help]

Defaults:
  --in          resources/config/default.json
  --out-dir     resources/generated
  --root-ns     Zen::Config
  --style       get           (emit get_<key>())
  --sort-keys   off           (preserve JSON order)

Notes:
  - Object keys → namespaces; leaf keys → functions in parent namespace.
  - Warnings are logged when a key is mangled to a valid C++ identifier.
)HELP");
}

// -----------------------------------------------------------------------------------------------
// Identifier & typing utilities
// -----------------------------------------------------------------------------------------------
static bool is_cpp_keyword(std::string_view s) {
  static const std::vector<std::string_view> k = {
    "alignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit","atomic_noexcept",
    "auto","bitand","bitor","bool","break","case","catch","char","char8_t","char16_t","char32_t",
    "class","compl","concept","const","consteval","constexpr","constinit","const_cast","continue",
    "co_await","co_return","co_yield","decltype","default","delete","do","double","dynamic_cast",
    "else","enum","explicit","export","extern","false","float","for","friend","goto","if","inline",
    "int","long","mutable","namespace","new","noexcept","not","not_eq","nullptr","operator","or",
    "or_eq","private","protected","public","reflexpr","register","reinterpret_cast","requires","return",
    "short","signed","sizeof","static","static_assert","static_cast","struct","switch","synchronized",
    "template","this","thread_local","throw","true","try","typedef","typeid","typename","union",
    "unsigned","using","virtual","void","volatile","wchar_t","while","xor","xor_eq"
  };
  return std::find(k.begin(), k.end(), s) != k.end();
}

static std::string to_snake(std::string_view s) {
  std::string o; o.reserve(s.size());
  char prev = 0;
  for (char c : s) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      if (std::isupper(static_cast<unsigned char>(c))) {
        if (prev && std::isalnum(static_cast<unsigned char>(prev))) o.push_back('_');
        o.push_back(static_cast<char>(std::tolower(c)));
      } else {
        o.push_back(c);
      }
    } else {
      if (!o.empty() && o.back() != '_') o.push_back('_');
    }
    prev = c;
  }
  while (!o.empty() && o.back() == '_') o.pop_back();
  if (o.empty() || !(std::isalpha(static_cast<unsigned char>(o[0])) || o[0] == '_')) o.insert(o.begin(), '_');
  if (is_cpp_keyword(o)) o.push_back('_');
  return o;
}

static void warn_if_mangled(std::string_view orig, std::string_view mangled) {
  if (orig != mangled) {
    Zen::Logger::log(Zen::LogLevel::WARNING, std::string("[configgen] key '") + std::string(orig) + "' mangled to '" + std::string(mangled) + "'");
  }
}

static std::string infer_type(const json& v) {
  if (v.is_boolean()) return "bool";
  if (v.is_number_integer() || v.is_number_unsigned()) return "int64_t";
  if (v.is_number_float()) return "double";
  if (v.is_string()) return "std::string";
  return "/* unsupported */";
}

// -----------------------------------------------------------------------------------------------
// Options
// -----------------------------------------------------------------------------------------------
struct GenOptions {
  std::string root_ns = "Zen::Config";
  bool style_get = true;                 // true → get_<name>(), false → <name>()
  bool fail_on_unsupported = false;
  bool sort_keys = false;                // default preserve JSON order
  bool dry_run = false;                  // log output to console only
};

static std::string join_ns(const GenOptions& G, const std::vector<std::string>& ns_stack) {
  std::string s = G.root_ns;
  for (const auto& p : ns_stack) { s += "::"; s += p; }
  return s;
}

// -----------------------------------------------------------------------------------------------
// Emission — HEADER
//   - Only object keys open a `namespace <name> {}` block
//   - Leaf keys emit a function *inside the current* namespace
// -----------------------------------------------------------------------------------------------
static void emit_header(std::ostream& h, const json& node, const GenOptions& G,
                        std::vector<std::string> ns_stack, int indent = 0)
{
  auto indent_out = [&](int n){ for (int i=0;i<n;++i) h << "  "; };

  if (!ns_stack.empty()) {
    indent_out(indent); h << "namespace " << ns_stack.back() << " {\n";
    ++indent;
  }

  // Gather keys (preserve insertion order or sort deterministically)
  std::vector<std::string> keys; keys.reserve(node.size());
  for (auto it = node.begin(); it != node.end(); ++it) keys.push_back(it.key());
  if (G.sort_keys) std::sort(keys.begin(), keys.end());

  // First pass: emit leaf function declarations in this namespace
  for (const auto& k : keys) {
    const auto& v = node.at(k);
    if (v.is_object()) continue; // handled in second pass

    std::string type = infer_type(v);
    if (type == "/* unsupported */") {
      Zen::Logger::log(Zen::LogLevel::WARNING, std::string("[configgen] unsupported leaf '") + k + "' (skipped)");
      if (G.fail_on_unsupported) throw std::runtime_error("unsupported JSON type encountered");
      continue;
    }

    std::string m = to_snake(k); warn_if_mangled(k, m);
    indent_out(indent);
    h << type << " " << (G.style_get ? ("get_" + m) : m) << "();\n";
  }

  // Second pass: recurse into child namespaces for object values
  for (const auto& k : keys) {
    const auto& v = node.at(k);
    if (!v.is_object()) continue;
    std::string m = to_snake(k); warn_if_mangled(k, m);
    auto next = ns_stack; next.push_back(m);
    emit_header(h, v, G, next, indent);
  }

  if (!ns_stack.empty()) {
    --indent; indent_out(indent); h << "}\n";
  }
}

// -----------------------------------------------------------------------------------------------
// Emission — CPP
//   - Definitions are placed in the *parent* namespace chain (objects only)
//   - JSON path uses original keys (not mangled)
// -----------------------------------------------------------------------------------------------
static void emit_cpp(std::ostream& cpp, const json& node, const GenOptions& G,
                     std::vector<std::string> ns_stack,
                     std::vector<std::string> json_stack)
{
  if (!node.is_object()) return;

  // Gather keys (preserve insertion order or sort deterministically)
  std::vector<std::string> keys; keys.reserve(node.size());
  for (auto it = node.begin(); it != node.end(); ++it) keys.push_back(it.key());
  if (G.sort_keys) std::sort(keys.begin(), keys.end());

  // First pass: define leaves in current namespace
  for (const auto& k : keys) {
    const auto& v = node.at(k);
    if (v.is_object()) continue;

    std::string type = infer_type(v);
    if (type == "/* unsupported */") {
      Zen::Logger::log(Zen::LogLevel::WARNING, std::string("[configgen] unsupported leaf '") + k + "' (skipped)");
      if (G.fail_on_unsupported) throw std::runtime_error("unsupported JSON type encountered");
      continue;
    }

    std::string m = to_snake(k); warn_if_mangled(k, m);
    const std::string qns = join_ns(G, ns_stack);
    cpp << type << " " << qns << "::" << (G.style_get ? ("get_" + m) : m) << "() {\n";
    cpp << "  return Zen::ConfigManager::instance().get<" << type << ">(\"";
    // build json path: parent path + "." + leaf key
    for (size_t i=0;i<json_stack.size();++i) { if (i) cpp << "."; cpp << json_stack[i]; }
    if (!json_stack.empty()) cpp << "."; cpp << k;
    cpp << "\");}\n\n\n";
  }

  // Second pass: recurse into child namespaces
  for (const auto& k : keys) {
    const auto& v = node.at(k);
    if (!v.is_object()) continue;

    std::string m = to_snake(k); warn_if_mangled(k, m);
    auto next_ns = ns_stack;   next_ns.push_back(m);
    auto next_js = json_stack; next_js.push_back(k);
    emit_cpp(cpp, v, G, next_ns, next_js);
  }
}

// -----------------------------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
  try {
    const auto args = parse_args(argc, argv);
    if (has_arg(args, "help")) { print_help(argv[0]); return 0; }

    GenOptions G;
    if (auto v = get_arg(args, "root-ns", ""); !v.empty()) G.root_ns = v;
    if (auto v = get_arg(args, "style", "get"); v == "raw") G.style_get = false;
    if (auto v = get_arg(args, "sort-keys", "off"); v == "on") G.sort_keys = true;
    if (has_arg(args, "fail-on-unsupported")) G.fail_on_unsupported = true;
    if (has_arg(args, "dry-run")) G.dry_run = true;

    fs::path in_json;
    if (!args.positionals.empty()) in_json = args.positionals.front();
    if (auto v = get_arg(args, "in", ""); !v.empty()) in_json = v;
    if (in_json.empty()) in_json = Zen::Utility::get_resources_path() / "config/default.json";

    fs::path out_dir;
    if (auto v = get_arg(args, "out-dir", ""); !v.empty()) out_dir = v;
    else out_dir = Zen::Utility::get_resources_path() / "generated";

    Zen::Logger::log(Zen::LogLevel::INFO, std::string("[configgen] input: ") + in_json.string());
    Zen::Logger::log(Zen::LogLevel::INFO, std::string("[configgen] out dir: ") + out_dir.string());
    Zen::Logger::log(Zen::LogLevel::INFO, std::string("[configgen] root ns: ") + G.root_ns);

    std::ifstream f(in_json);
    if (!f.is_open()) {
      Zen::Logger::log(Zen::LogLevel::ERROR, std::string("[configgen] failed to open ") + in_json.string());
      return 2;
    }
    json root; f >> root;

    // Compose header/cpp in-memory first (so --dry-run works and failures don't write partial files)
    std::ostringstream hbuf, cppbuf;

    hbuf << "#pragma once\n#include <cstdint>\n#include <string>\nnamespace " << G.root_ns << " {\n";
    emit_header(hbuf, root, G, {} /* ns stack */ , 0);
    hbuf << "}\n";

    cppbuf << "#include \"config.h\"\n#include \"engine/config_manager.h\"\n";
    emit_cpp(cppbuf, root, G, {} /* ns stack */, {} /* json path */);

    if (G.dry_run) {
      Zen::Logger::log(Zen::LogLevel::INFO, "[configgen] --dry-run: header preview follows");
      std::cout << hbuf.str() << std::endl;
      Zen::Logger::log(Zen::LogLevel::INFO, "[configgen] --dry-run: source preview follows");
      std::cout << cppbuf.str() << std::endl;
      return 0;
    }

    fs::create_directories(out_dir);
    const fs::path hpath = out_dir / "config.h";
    const fs::path cppath = out_dir / "config.cpp";

    {
      std::ofstream h(hpath, std::ios::binary);
      if (!h) { Zen::Logger::log(Zen::LogLevel::ERROR, std::string("[configgen] cannot write: ") + hpath.string()); return 3; }
      h << hbuf.str();
    }
    {
      std::ofstream cpp(cppath, std::ios::binary);
      if (!cpp) { Zen::Logger::log(Zen::LogLevel::ERROR, std::string("[configgen] cannot write: ") + cppath.string()); return 4; }
      cpp << cppbuf.str();
    }

    Zen::Logger::log(Zen::LogLevel::INFO, std::string("[configgen] wrote ") + hpath.string());
    Zen::Logger::log(Zen::LogLevel::INFO, std::string("[configgen] wrote ") + cppath.string());
    Zen::Logger::log(Zen::LogLevel::INFO, "[configgen] done");
    return 0;
  } catch (const std::exception& e) {
    Zen::Logger::log(Zen::LogLevel::ERROR, std::string("[configgen] fatal: ") + e.what());
    return 128;
  }
}
