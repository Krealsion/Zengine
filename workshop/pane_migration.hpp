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
// IT IS NOT A GENERAL MECHANISM. Retired references, named in full, with no table, no
// pattern and no registration seam. The previous revision of this comment predicted the
// cost of the second one exactly -- "a second named pair and one more line in the loop" --
// and the Builder's migration paid it: three constants and one `if`. The prediction is left
// standing rather than deleted, because a design note that turned out to be right about its
// own next step is worth more than the sentence that would replace it.
//
// ⚠ AND THE THRESHOLD IS NAMED SO NOBODY HAS TO GUESS IT. Two pairs is still cheaper than a
// table; the case for a table is a THIRD reference plus something a pair cannot say -- a
// pane key that moved as well as its office, or a reference that resolves to two panes. Two
// spellings of "the office moved and the key did not" are two lines, and two lines are not a
// framework.

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

/// THE OFFICE THE BUILDER PANEL USED TO BE OFFERED FROM -- this host's own, and the same
/// literal as `kRetiredFilesProvider` for the same reason, spelled a second time rather than
/// aliased: these are two independent historical facts about two sets of files, and one of
/// them ceasing to be true must not silently move the other.
inline constexpr const char* kRetiredBuilderProvider = "zengine.workshop";

/// THE OFFICE IT IS OFFERED FROM NOW. Spelled here rather than taken from
/// `builder-pane/vocabulary.hpp`, for `kFilesProvider`'s reason: that is the weave's own
/// header and this host does not link the weave. A case checks the two spellings against
/// each other, which is the seam where a divergence would actually be caught.
inline constexpr const char* kBuilderProvider = "zengine.builder-pane";

/// THE PANE KEY, WHICH DID NOT MOVE. Only the office changed hands.
inline constexpr const char* kBuilderPane = "builder";

/// Is this the reference a saved file wrote for the built-in browser?
inline bool names_the_retired_browser(const PaneRef& ref) {
    return ref.provider == kRetiredFilesProvider && ref.pane == kFilesPane;
}

/// Is this the reference a saved file wrote for the built-in Builder panel?
inline bool names_the_retired_builder(const PaneRef& ref) {
    return ref.provider == kRetiredBuilderProvider && ref.pane == kBuilderPane;
}

/// WHICH RETIRED REFERENCES ONE SETUP HELD -- counted apart, because the sentence a maker
/// reads names what THEIR file held rather than everything that ever moved.
struct Converted {
    std::int64_t files = 0;
    std::int64_t builder = 0;
    std::int64_t total() const { return files + builder; }
};

/// REWRITE EVERY RETIRED REFERENCE IN ONE SETUP, and say which ones. Everything else about
/// the row -- its place, its two sizes, its front order -- is untouched, because none of it
/// changed hands.
///
/// ⚠ IT RUNS BEFORE THE SETUP'S OWN LAW. A file that names BOTH spellings of one pane holds
/// two rows for it after this, and `check_setup` refuses it by name ("`zengine.files/
/// project-files` is named twice") -- which is the true sentence about a contradictory file,
/// and is what a converter that ran afterwards would have hidden.
inline Converted convert_retired_panes(Setup& s) {
    Converted converted;
    for (SetupPane& row : s.panes) {
        if (names_the_retired_browser(row.ref)) {
            row.ref.provider = kFilesProvider;
            ++converted.files;
        } else if (names_the_retired_builder(row.ref)) {
            row.ref.provider = kBuilderProvider;
            ++converted.builder;
        }
    }
    return converted;
}

/// WHAT A MAKER IS TOLD, ONCE, when a file they wrote named a pane this host used to offer.
/// Said in the panes' own durable names, because those are what they would find in the file
/// if they went and looked.
///
/// ⚠ ONE SENTENCE FOR A RUN, NOT ONE PER ROW. The reader says this once from a count
/// (`setup_persist::setup_in`), so a maker whose desk holds a pane twice over two layouts is
/// told once -- and not told which of their rows was which.
///
/// ⚠ ...AND IT NAMES WHAT ACTUALLY MOVED. `converted` counts rows, not pairs, so the sentence
/// is composed from what this run's file HELD: a maker who never opened the Builder is not
/// told about a pane they would not find if they went and looked, which is the whole reason
/// the note spells durable names in the first place.
inline std::string converted_note(bool files, bool builder) {
    std::string said = "panes moved to their own offices";
    if (files) {
        said += std::string(" -- ") + kRetiredFilesProvider + "/" + kFilesPane + " is now " +
                kFilesProvider + "/" + kFilesPane;
    }
    if (builder) {
        said += std::string(files ? ", and " : " -- ") + kRetiredBuilderProvider + "/" +
                kBuilderPane + " is now " + kBuilderProvider + "/" + kBuilderPane;
    }
    return said;
}

} // namespace zengine::workshop::pane_migration

#endif // ZENGINE_WORKSHOP_PANE_MIGRATION_HPP
