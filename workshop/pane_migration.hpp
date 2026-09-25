// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_MIGRATION_HPP
#define ZENGINE_WORKSHOP_PANE_MIGRATION_HPP

// A pane that changed hands, and the one rewrite that says so: a saved file naming a pane by the
// office that offered it before it moved is converted at load, in memory, before the setup's law.
// Not a format version (a `PaneRef` is opaque data, and a version gate would refuse older files),
// not an alias (one name per pane, WL-CAT-03), and not a general mechanism: a hand-written table.

#include "setup.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace zengine::workshop::pane_migration {

/// The office the browser was offered from before it became a weave, spelled as a literal: a fact
/// about files already written, which must stay true if this host renames its own office.
inline constexpr const char* kRetiredFilesProvider = "zengine.workshop";

/// The office it is offered from now, spelled here because this host does not link the weave
/// (`files/vocabulary.hpp`); a case checks that the two spellings agree.
inline constexpr const char* kFilesProvider = "zengine.files";

/// THE PANE KEY, WHICH DID NOT MOVE. Only the office changed hands; the pane a maker
/// arranged is the same pane, so its key is the same key and the conversion is not a
/// rename.
inline constexpr const char* kFilesPane = "project-files";

/// The office the Builder panel was offered from, spelled a second time rather than aliased: two
/// independent facts about two sets of files.
inline constexpr const char* kRetiredBuilderProvider = "zengine.workshop";

/// The office it is offered from now, for `kFilesProvider`'s reason
/// (`builder-pane/vocabulary.hpp`).
inline constexpr const char* kBuilderProvider = "zengine.builder-pane";

/// THE PANE KEY, WHICH DID NOT MOVE. Only the office changed hands.
inline constexpr const char* kBuilderPane = "builder";

/// The Info panel's old office and its office now, for `kFilesProvider`'s reasons on both sides.
inline constexpr const char* kRetiredInfoProvider = "zengine.workshop";
inline constexpr const char* kInfoProvider = "zengine.info";

/// THE PANE KEY, WHICH DID NOT MOVE.
inline constexpr const char* kInfoPane = "info";

/// The source editor's old office and its office now, for `kFilesProvider`'s reasons
/// (`editor-pane/vocabulary.hpp`).
inline constexpr const char* kRetiredEditorProvider = "zengine.workshop";
inline constexpr const char* kEditorProvider = "zengine.editor";

/// THE PANE KEY, WHICH DID NOT MOVE. Its place is the overlay stack on both sides, so the
/// row's `default` goes on meaning what it always meant and nothing is written.
inline constexpr const char* kEditorPane = "editor";

/// The host's own Pane Manager and the desktop's pane that is the Pane Manager now: the first pair
/// whose pane key moved too (`pane-editor` to `launcher`). What a maker authored carries over;
/// checked against `desktop-pane/vocabulary.hpp` by a case.
inline constexpr const char* kRetiredManagerProvider = "zengine.workshop";
inline constexpr const char* kRetiredManagerPane = "pane-editor";
inline constexpr const char* kManagerProvider = "zengine.desktop";
inline constexpr const char* kManagerPane = "launcher";

// ---- The table -------------------------------------------------------------------------
// Info made it a table: its place moved with its office. A saved `default` place meant the right
// column because the host's catalog put it there, so the conversion rewrites that place too.

/// One pane that changed hands: what a saved file wrote, what it means now, and the place the
/// host's own catalog gave it.
struct Retired {
    const char* was_provider;
    const char* was_pane;
    const char* now_provider;
    const char* now_pane;
    /// `pane_unit::kDefault` when the host's answer and the weave's are the same place, so the
    /// row's own `default` goes on meaning what it always meant and nothing is written.
    std::int64_t place;
};

inline constexpr Retired kRetired[] = {
    {kRetiredFilesProvider, kFilesPane, kFilesProvider, kFilesPane, pane_unit::kDefault},
    {kRetiredBuilderProvider, kBuilderPane, kBuilderProvider, kBuilderPane, pane_unit::kDefault},
    {kRetiredInfoProvider, kInfoPane, kInfoProvider, kInfoPane, pane_unit::kRightColumn},
    {kRetiredEditorProvider, kEditorPane, kEditorProvider, kEditorPane, pane_unit::kDefault},
    {kRetiredManagerProvider, kRetiredManagerPane, kManagerProvider, kManagerPane,
     pane_unit::kDefault},
};

inline constexpr std::size_t kRetiredCount = sizeof(kRetired) / sizeof(kRetired[0]);

/// Is this the reference a saved file wrote for the built-in browser?
inline bool names_the_retired_browser(const PaneRef& ref) {
    return ref.provider == kRetiredFilesProvider && ref.pane == kFilesPane;
}

/// Is this the reference a saved file wrote for the built-in Builder panel?
inline bool names_the_retired_builder(const PaneRef& ref) {
    return ref.provider == kRetiredBuilderProvider && ref.pane == kBuilderPane;
}

/// Is this the reference a saved file wrote for the built-in Info panel?
inline bool names_the_retired_info(const PaneRef& ref) {
    return ref.provider == kRetiredInfoProvider && ref.pane == kInfoPane;
}

/// Is this the reference a saved file wrote for the built-in source Editor?
inline bool names_the_retired_editor(const PaneRef& ref) {
    return ref.provider == kRetiredEditorProvider && ref.pane == kEditorPane;
}

/// Is this the reference a saved file wrote for the host's own Pane Manager?
inline bool names_the_retired_manager(const PaneRef& ref) {
    return ref.provider == kRetiredManagerProvider && ref.pane == kRetiredManagerPane;
}

/// WHICH RETIRED REFERENCES ONE SETUP HELD -- counted per table row, because the sentence a
/// maker reads names what THEIR file held rather than everything that ever moved.
struct Converted {
    std::int64_t rows[kRetiredCount] = {};

    std::int64_t total() const {
        std::int64_t n = 0;
        for (const std::int64_t r : rows) {
            n += r;
        }
        return n;
    }
    /// Did this file hold the reference in table row `which`? Total over the index.
    bool held(std::size_t which) const { return which < kRetiredCount && rows[which] > 0; }
};

/// HOW MANY ROWS OF ONE SETUP HELD ONE PARTICULAR RETIRED REFERENCE -- named by the office the
/// pane is offered from NOW, so a caller says which pane it means rather than an index into a
/// table it did not write. Total over the string: an office no row names answers zero.
inline std::int64_t held_count(const Converted& converted, const char* now_provider) {
    if (now_provider == nullptr) {
        return 0;
    }
    for (std::size_t i = 0; i < kRetiredCount; ++i) {
        if (std::string(kRetired[i].now_provider) == now_provider) {
            return converted.rows[i];
        }
    }
    return 0;
}

/// Rewrite every retired reference in one setup, and say which. The place is written only over a
/// `default`: a maker's own place outranks the one the catalog gave. It runs before the setup's
/// law, so a file naming both spellings of one pane is refused by `check_setup`.
inline Converted convert_retired_panes(Setup& s) {
    Converted converted;
    for (SetupPane& row : s.panes) {
        for (std::size_t i = 0; i < kRetiredCount; ++i) {
            const Retired& moved = kRetired[i];
            if (row.ref.provider != moved.was_provider || row.ref.pane != moved.was_pane) {
                continue;
            }
            row.ref.provider = moved.now_provider;
            row.ref.pane = moved.now_pane;
            if (moved.place != pane_unit::kDefault && row.place.mode == pane_unit::kDefault) {
                row.place.mode = moved.place;
            }
            ++converted.rows[i];
            break;
        }
    }
    return converted;
}

/// What a maker is told, once per run, when a file named a pane that changed hands: in the panes'
/// durable names, and only for what this run's file held (`converted` counts rows).
inline std::string converted_note(const Converted& converted) {
    std::string said = "panes moved to their own offices";
    std::int64_t named = 0;
    for (std::size_t i = 0; i < kRetiredCount; ++i) {
        if (!converted.held(i)) {
            continue;
        }
        const Retired& moved = kRetired[i];
        said += std::string(named == 0 ? " -- " : ", and ") + moved.was_provider + "/" +
                moved.was_pane + " is now " + moved.now_provider + "/" + moved.now_pane;
        ++named;
    }
    return said;
}

} // namespace zengine::workshop::pane_migration

#endif // ZENGINE_WORKSHOP_PANE_MIGRATION_HPP
