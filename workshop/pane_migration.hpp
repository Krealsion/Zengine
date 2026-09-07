// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_MIGRATION_HPP
#define ZENGINE_WORKSHOP_PANE_MIGRATION_HPP

// A PANE THAT CHANGED HANDS, AND THE ONE REWRITE THAT SAYS SO.
//
// The project browser was a built-in pane of this host: every setup a maker ever saved and
// every session this Workshop ever wrote spelled it `zengine.workshop/project-files`,
// because that is who was offering it. It is a loaded weave now, in an office of its own,
// and the same pane is spelled `zengine.files/project-files`. One durable name became
// another durable name, for the same pane, with nothing else about it changed.
//
// SO A SAVED FILE NAMING THE OLD ONE IS CONVERTED AT LOAD, HERE, ONCE. A maker who arranged
// their desk last month opens this build and their Files pane is where they left it, the
// size they left it, in the order they left it.
//
// ---- WHY THIS IS NOT A FORMAT VERSION ------------------------------------------
//
// ⚠ THE BYTES DID NOT CHANGE MEANING; A REFERENCE CHANGED WHAT IT RESOLVES TO. The setup
// format is explicit that a `PaneRef` is opaque data -- copied verbatim, never resolved, and
// "a reference that resolves to nothing is still a reference this setup holds"
// (`setup_persist::setup_in`). A version-3 file naming the built-in is a perfectly legal
// version-3 file, in the same units, under the same law; what moved is the ANSWER to
// "whose pane is this", which is `resolve_pane`'s question and has never been the file's.
//
// AND A VERSION GATE WOULD LOSE FILES THIS ONE KEEPS. The setup reader carries exactly one
// legacy rung (`kLegacyFormatVersion`), so bumping the format to 4 would convert version-3
// files and REFUSE version-2 ones outright -- and a version-2 setup can name the built-in
// just as easily, because the browser is older than either number. This rewrite is uniform
// over every version the reader admits, present and legacy, because it runs inside
// `setup_in` after the file has become a `Setup` and before that `Setup` meets its law.
//
// ---- WHAT IT IS NOT ------------------------------------------------------------
//
// IT IS NOT AN ALIAS. Nothing here teaches the catalog that two provider names mean one
// pane; that would be a second answer to "whose pane is this" and the pane catalog refuses
// exactly that (WL-CAT-03). The old spelling stops existing at the moment a file is read,
// and every party downstream sees one name.
//
// IT REWRITES NOTHING ON DISK. The converted setup is in memory; the file first changes at
// the ordinary save the maker's next close performs, which writes the current shape. That is
// the migration register's own rule about reading, and it is the reason this converter is
// needed only while yesterday's bytes exist.
//
// IT IS NOT A GENERAL MECHANISM. One retired reference, named in full, with no table, no
// pattern and no registration seam. When a second built-in migrates, this file gains a
// second named pair and one more line in the loop -- which is the whole cost, and is
// cheaper than the framework somebody would otherwise be tempted to build for two rows.

#include "setup.hpp"

#include <cstdint>
#include <string>

namespace zengine::workshop::pane_migration {

/// THE OFFICE THE BROWSER USED TO BE OFFERED FROM -- this host's own. Spelled here as a
/// literal rather than taken from `kWorkshopProvider`, because it is a HISTORICAL fact
/// about files already written: if this host ever changed its own office name, every
/// sentence about what those files say must go on being true.
inline constexpr const char* kRetiredFilesProvider = "zengine.workshop";

/// THE OFFICE IT IS OFFERED FROM NOW. Spelled here for the same reason, from the other
/// side: `files/vocabulary.hpp` is the weave's own header and this host does not link the
/// weave. The two spellings are checked against each other by a case, which is the seam
/// where a divergence would actually be caught.
inline constexpr const char* kFilesProvider = "zengine.files";

/// THE PANE KEY, WHICH DID NOT MOVE. Only the office changed hands; the pane a maker
/// arranged is the same pane, so its key is the same key and the conversion is not a
/// rename.
inline constexpr const char* kFilesPane = "project-files";

/// Is this the reference a saved file wrote for the built-in browser?
inline bool names_the_retired_browser(const PaneRef& ref) {
    return ref.provider == kRetiredFilesProvider && ref.pane == kFilesPane;
}

/// REWRITE EVERY RETIRED REFERENCE IN ONE SETUP, and say how many. Everything else about
/// the row -- its place, its two sizes, its front order -- is untouched, because none of it
/// changed hands.
///
/// ⚠ IT RUNS BEFORE THE SETUP'S OWN LAW. A file that names BOTH spellings holds two rows
/// for one pane after this, and `check_setup` refuses it by name ("`zengine.files/
/// project-files` is named twice") -- which is the true sentence about a contradictory
/// file, and is what a converter that ran afterwards would have hidden.
inline std::int64_t convert_retired_panes(Setup& s) {
    std::int64_t converted = 0;
    for (SetupPane& row : s.panes) {
        if (names_the_retired_browser(row.ref)) {
            row.ref.provider = kFilesProvider;
            ++converted;
        }
    }
    return converted;
}

/// WHAT A MAKER IS TOLD, ONCE, when a file they wrote named the browser this host used to
/// offer. Said in the pane's own durable names, because those are what they would find in
/// the file if they went and looked.
inline std::string converted_note() {
    return std::string("the Files pane moved to its own office -- ") + kRetiredFilesProvider +
           "/" + kFilesPane + " is now " + kFilesProvider + "/" + kFilesPane;
}

} // namespace zengine::workshop::pane_migration

#endif // ZENGINE_WORKSHOP_PANE_MIGRATION_HPP
