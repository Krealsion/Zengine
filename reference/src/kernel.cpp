// zen/kernel.cpp
#include "kernel.hpp"
#include <iostream>

namespace zen {

Kernel& Kernel::instance() {
    static Kernel k;
    return k;
}

Kernel::~Kernel() {
    for (auto& [name, h] : loaded_handles_) {
        close_lib(static_cast<Handle>(h));
    }
}

void Kernel::provide(const std::string& path, SenseProvider p) {
    std::lock_guard<std::mutex> lock(mutex_);
    providers_[path] = std::move(p);
}

bool Kernel::load_program(const std::string& path) {
    Handle h = open_lib(path);
    if (!h) {
        std::cerr << "Failed to load " << path << "\n";
        return false;
    }

    // Look for required entry point (every program must export this)
    using EntryFn = void(*)(Kernel&);
    auto entry = reinterpret_cast<EntryFn>(get_symbol(h, "zen_program_entry"));
    if (!entry) {
        std::cerr << "No zen_program_entry in " << path << "\n";
        close_lib(h);
        return false;
    }

    entry(*this);  // Program registers its senses here

    // Optional: let program register its own name for later unload
    loaded_handles_.emplace_back("program." + path, h);  // simplistic key for now

    std::cout << "Loaded program: " << path << "\n";
    return true;
}

void Kernel::unload_program(const std::string& name) {
    // Find and close (real impl would reverse dependencies, call shutdown, etc.)
    // For v0.0: naive
    for (auto it = loaded_handles_.begin(); it != loaded_handles_.end(); ++it) {
        if (it->first == name) {
            close_lib(static_cast<Handle>(it->second));
            loaded_handles_.erase(it);
            // Remove provided senses? For now we don't — graceful degradation
            std::cout << "Unloaded: " << name << "\n";
            return;
        }
    }
}

void Kernel::dump_senses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Current senses:\n";
    for (const auto& [path, _] : providers_) {
        std::cout << "  " << path << "\n";
    }
}

#ifdef _WIN32
Kernel::Handle Kernel::open_lib(const std::string& path) {
    return LoadLibraryA(path.c_str());
}
void* Kernel::get_symbol(Handle h, const std::string& symbol) {
    return GetProcAddress(h, symbol.c_str());
}
void Kernel::close_lib(Handle h) {
    FreeLibrary(h);
}
#else
Kernel::Handle Kernel::open_lib(const std::string& path) {
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}
void* Kernel::get_symbol(Handle h, const std::string& symbol) {
    return dlsym(h, symbol.c_str());
}
void Kernel::close_lib(Handle h) {
    dlclose(h);
}
#endif

} // zen
