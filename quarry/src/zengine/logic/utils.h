#pragma once

#include <filesystem>
#include <string>

namespace Zen {
class Utility {
public:
  static std::string demangle(const char* mangled_name);
  static std::filesystem::path get_resources_path();
  static std::filesystem::path getResourcePath(const std::string& relativePath);
};
}
