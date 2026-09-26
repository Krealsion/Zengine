// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_IMAGE_HPP
#define ZENGINE_OPERATOR_IMAGE_HPP

// One open image, owned from the first moment it is open. `OperatorOffer` resolves a consumer's
// optional symbol for one load, and `mount_provider` a provider's, holding the image while its
// contributions are installed; both need the same platform calls and RAII, written once so the
// flags cannot drift. The flags are Loom's own (`RTLD_NOW | RTLD_LOCAL`, as `Kernel::load`
// opens; `LoadLibraryA`, with the kernel's ANSI-path limitation), so a second open names the
// same image and the loader's refcount keeps it so.
// Reference: docs/reference/operator-providers.md.

#include <cstdint>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace zengine::op {

/// How many images this package opened and closed for itself, with
/// `loom::kernel_lifetime_counts()`'s shape and caveats: process-wide, monotonic, read deltas,
/// decides nothing. It counts this package's shares (an offer's, a provider mount's), not the
/// Kernel's. Not a cross-image instrument: a vague-linkage static read in a loaded library is
/// its own on PE and usually the executable's on ELF, so it is quoted only from a host.
struct ImageCounts {
    std::uint64_t opens = 0;
    std::uint64_t closes = 0;
};

namespace detail {

inline ImageCounts& image_ledger() noexcept {
    static ImageCounts counts;
    return counts;
}

inline void* image_open(const std::string& path) {
#if defined(_WIN32)
    return static_cast<void*>(::LoadLibraryA(path.c_str()));
#else
    return ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

inline void* image_symbol(void* handle, const char* name) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return ::dlsym(handle, name);
#endif
}

inline void image_close(void* handle) {
#if defined(_WIN32)
    ::FreeLibrary(static_cast<HMODULE>(handle));
#else
    ::dlclose(handle);
#endif
}

} // namespace detail

inline ImageCounts image_counts() noexcept { return detail::image_ledger(); }

/// One share of one image: opened by the constructor, closed by the destructor, and nothing in
/// between can drop it. A holder's refusals just return, and a member owning its handle is
/// released by destruction and unwinding alike -- nothing forgets to close, closes twice or
/// leaks the mapping. Neither copyable nor movable: for its holders, the share is the identity.
class ImageShare {
public:
    explicit ImageShare(const std::string& path) : handle_(detail::image_open(path)) {
        if (handle_ != nullptr) {
            ++detail::image_ledger().opens;
        }
    }
    ~ImageShare() {
        if (handle_ != nullptr) {
            detail::image_close(handle_);
            ++detail::image_ledger().closes;
        }
    }

    ImageShare(const ImageShare&) = delete;
    ImageShare& operator=(const ImageShare&) = delete;
    ImageShare(ImageShare&&) = delete;
    ImageShare& operator=(ImageShare&&) = delete;

    bool open() const noexcept { return handle_ != nullptr; }
    void* get() const noexcept { return handle_; }

    /// The address of an exported symbol, or nullptr. A closed share answers
    /// nullptr rather than asking the platform about a null handle.
    void* symbol(const char* name) const noexcept {
        return handle_ == nullptr ? nullptr : detail::image_symbol(handle_, name);
    }

private:
    void* handle_;
};

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_IMAGE_HPP
