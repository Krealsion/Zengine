// zen/kernel.hpp
#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <any>
#include <vector>
#include <optional>
#include <iostream>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace zen {

using SenseProvider = std::function<std::any()>;

class Kernel {
public:
    static Kernel& instance();

    // Register a sense (called by loaded programs)
    void provide(const std::string& path, SenseProvider provider);

    // Universal getter — returns nullptr on miss
    template<typename T>
    T* get_sense(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = providers_.find(path);
        if (it == providers_.end()) {
            return nullptr;
        }
        auto val = it->second();
        try {
            return std::any_cast<T*>(val);
        } catch (...) {
            return nullptr;  // type mismatch → treat as absent
        }
    }

    // Load a dynamic program (DLL/so/dylib)
    bool load_program(const std::string& path);

    // Unload by registered name (programs must register a "program.name" sense)
    void unload_program(const std::string& name);

    // For debugging: list all current senses
    void dump_senses() const;

private:
    Kernel() = default;
    ~Kernel();

    std::unordered_map<std::string, SenseProvider> providers_;
    std::vector<std::pair<std::string, void*>> loaded_handles_;  // name + OS handle
    mutable std::mutex mutex_;

#ifdef _WIN32
    using Handle = HMODULE;
#else
    using Handle = void*;
#endif

    Handle open_lib(const std::string& path);
    void* get_symbol(Handle h, const std::string& symbol);
    void close_lib(Handle h);
};

} // zen
