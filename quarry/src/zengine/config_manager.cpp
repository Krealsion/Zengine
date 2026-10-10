#include "config_manager.h"

#include <fstream>
#include <filesystem>

#include "logic/utils.h"

namespace fs = std::filesystem;

namespace Zen {

static const char* DEFAULT_PATH = "config/default.json";
static const char* CURRENT_PATH = "config/current.json";

ConfigManager& ConfigManager::instance() {
  static ConfigManager inst;
  if (inst._flat.empty()) {
    inst.load();
  }
  return inst;
}

static bool structurally_match_and_patch(nlohmann::json& runtime,
                                         const nlohmann::json& schema) {
  if (schema.is_object()) {
    if (!runtime.is_object())
      runtime = nlohmann::json::object();

    bool ok = true;
    for (auto& [k, v] : schema.items()) {
      if (!runtime.contains(k)) {
        runtime[k] = v;           // heal missing key
      } else {
        ok &= structurally_match_and_patch(runtime[k], v);
      }
    }
    return ok;
  }

  // Leaf node
  if (runtime.type() != schema.type()) {
    runtime = schema;             // heal invalid type
    return false;
  }
  return true;
}

bool ConfigManager::load() {
  if (!fs::exists(Utility::get_resources_path().append(CURRENT_PATH)))
    fs::copy_file(Utility::get_resources_path().append(DEFAULT_PATH), Utility::get_resources_path().append(CURRENT_PATH));

  _cached_size = fs::file_size(Utility::get_resources_path().append(CURRENT_PATH));

  std::ifstream f(Utility::get_resources_path().append(CURRENT_PATH));
  if (!f.is_open()) return false;
  f >> _root;

  std::ifstream d(Utility::get_resources_path().append(DEFAULT_PATH));
  nlohmann::json schema;
  d >> schema;

  structurally_match_and_patch(_root, schema);

  _flat.clear();
  index_json(_root, "");
  return true;
}

bool ConfigManager::reload() {
  return load();
}
void ConfigManager::reload_if_changed() {
  // TODO this currently doesnt detect changes if no new characters are added (aka if you replace a 1 with a 2, it won't update)
  // if (!fs::exists(Utility::get_resources_path().append(CURRENT_PATH))) return;
  //
  // auto size = fs::file_size(Utility::get_resources_path().append(CURRENT_PATH));
  // if (size == _cached_size) return;   // no change
  //
  // _cached_size = size;
  reload();
}

void ConfigManager::index_json(const nlohmann::json& node, const std::string& prefix) {
  if (node.is_object()) {
    for (auto& [k, v] : node.items()) {
      std::string p = prefix.empty() ? k : prefix + "." + k;
      index_json(v, p);
    }
  } else {
    _flat[prefix] = node;
  }
}

bool ConfigManager::has(const std::string& path) const {
  return _flat.find(path) != _flat.end();
}

template<typename T>
T ConfigManager::get(const std::string& path) const {
  auto it = _flat.find(path);
  if (it == _flat.end())
    throw std::runtime_error("Config key missing: " + path);
  return it->second.get<T>();
}


// Explicit instantiations for common types (optional but helps linker)
template bool ConfigManager::get<bool>(const std::string&) const;
template int  ConfigManager::get<int>(const std::string&) const;
template float ConfigManager::get<float>(const std::string&) const;
template double ConfigManager::get<double>(const std::string&) const;
template std::string ConfigManager::get<std::string>(const std::string&) const;

}
