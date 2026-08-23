// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_IMAGE_HPP
#define ZENGINE_OPERATOR_IMAGE_HPP

// ONE OPEN IMAGE, OWNED FROM THE FIRST MOMENT IT IS OPEN (PROV-0).
//
// Two things in this package open a shared library for themselves: `OperatorOffer`,
// which resolves a consumer's optional symbol for the length of one load, and
// `mount_provider`, which resolves a provider's optional symbol and then HOLDS the
// image for as long as its contributions are installed. Both want the same three
// platform calls and the same RAII, so they are written once, here, rather than
// twice in two headers that would then be free to drift about flags.
//
// THE FLAGS ARE LOOM'S OWN, deliberately: `RTLD_NOW | RTLD_LOCAL` is what
// `Kernel::load` opens with, so a host that opens the same file names the SAME
// image and the loader's refcount is what keeps it that way. `LoadLibraryA` is the
// same ANSI-path limitation the kernel states. There is no public door to a second
// exported symbol of a kernel-loaded image (LOG-R1 measured that), so opening the
// file again is the whole mechanism, and it is cheap precisely because it is not a
// second image.
//
// WHY A SHARE AND NOT A HANDLE. Every refusal in a caller's constructor simply
// returns; a member that owns its own handle is released by ordinary destruction
// and by unwinding alike, so there is no failure path that can forget to close, none
// that can close twice, and none that can leak the mapping on the way out. That is
// `loom::LoadedLibrary`'s argument, applied one layer out, and the reason this type
// is neither copyable nor movable: for the objects that hold one, the share IS the
// identity.

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

/// How many images this package has opened and closed for itself.
///
/// `loom::kernel_lifetime_counts()`'s shape and its caveats, one layer out: it is
/// process-wide, monotonic, and decides nothing -- read DELTAS. It counts the
/// shares THIS package takes (an offer's, a provider mount's) and knows nothing
/// about the Kernel's own, which is what makes the two ledgers answer different
/// questions rather than two versions of one.
///
/// It is observability, not a stability guarantee, and it is NOT a cross-image
/// instrument: the counter is a vague-linkage static, so a loaded library that
/// somehow read it would be reading its own on PE and (usually) the executable's on
/// ELF. Nothing but a HOST opens an image for itself, and a host is an executable,
/// which is why one counter is enough here and why it must never be quoted from
/// inside a loaded artifact.
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

/// ONE SHARE OF ONE IMAGE. Opened by the constructor, closed by the destructor,
/// and nothing in between can drop it.
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
