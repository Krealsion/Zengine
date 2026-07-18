#include "utils.h"

#include <memory>
#include <typeinfo>
#include <SDL3/SDL_filesystem.h>

namespace Zen {

std::string Utility::demangle(const char* mangled_name) {
  return mangled_name; // TODO FIXME CROSS PLATFORM
}

std::filesystem::path Utility::get_resources_path() {
  static std::optional<std::filesystem::path> cached;
  if (cached.has_value()) { return cached.value(); }

  const char* base = SDL_GetBasePath();
  if (!base) {
    throw std::runtime_error("SDL_GetBasePath failed: " + std::string(SDL_GetError()));
  }

  auto current = std::filesystem::path(base);
  constexpr size_t maxDepth = 20;
  size_t depth = 0;

  while (depth++ < maxDepth) {
    if (std::filesystem::path candidate = current / "resources"; std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate)) {
      std::filesystem::path resolved = std::filesystem::weakly_canonical(candidate) /= "";
      cached = resolved;
      return resolved;
    }

    // Move up one level
    if (current.parent_path() == current) {
      break;  // reached filesystem root
    }
    current = current.parent_path();
  }

  throw std::runtime_error(
      "Resources directory not found. Searched upwards from: " +
      (std::filesystem::path(base) / "").string()
  );
}

std::filesystem::path Utility::getResourcePath(const std::string& relativePath) {
  return get_resources_path() / relativePath;
}
}
