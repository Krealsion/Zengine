#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace Zen {

class ConfigManager {
public:
  static ConfigManager& instance();

  // Load current.json (or copy default.json if missing)
  bool load();

  // Reload current.json at runtime
  bool reload();
  void reload_if_changed();

  // Typed getters
  template<typename T>
  T get(const std::string& path) const;

  // Exists?
  bool has(const std::string& path) const;

private:
  ConfigManager() = default;

  void index_json(const nlohmann::json& node, const std::string& prefix);
  uintmax_t _cached_size = 0;
  std::unordered_map<std::string, nlohmann::json> _flat;
  nlohmann::json _root;
};

}
