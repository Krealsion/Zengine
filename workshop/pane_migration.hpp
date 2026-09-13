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
// IT IS NOT A GENERAL MECHANISM. Retired references, named in full, in a table this file
// writes out by hand -- no pattern, no registration seam, and nothing any other party may add
// a row to. The previous revision of this comment predicted the cost of the second one exactly
// -- "a second named pair and one more line in the loop" -- and the Builder's migration paid
// it: three constants and one `if`. The revision before this one predicted the THRESHOLD, and
// Info's migration met it exactly as written. Both predictions are left standing rather than
// deleted, because a design note that turned out to be right about its own next step is worth
// more than the sentence that would replace it.
//
// ⚠ AND THE THRESHOLD IT NAMED HAS BEEN REACHED, WHICH IS WHY THERE IS A TABLE. The rule it
// wrote was: two pairs is cheaper than a table, and the case for a table is "a THIRD reference
// plus something a pair cannot say -- a pane key that moved as well as its office, or a
// reference that resolves to two panes". Info is the third reference, and what it says that a
// pair cannot is its PLACE. The next threshold, named for whoever meets it: a row whose
// conversion depends on anything OUTSIDE the row -- the file's version, another row, the
// screen -- because that is a converter with a context, and a table of independent rewrites is
// not one.

#include "setup.hpp"

#include <cstddef>
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

/// THE OFFICE THE INFO PANEL USED TO BE OFFERED FROM, and the one it is offered from now --
/// the third pair, spelled for `kFilesProvider`'s reasons on both sides.
inline constexpr const char* kRetiredInfoProvider = "zengine.workshop";
inline constexpr const char* kInfoProvider = "zengine.info";

/// THE PANE KEY, WHICH DID NOT MOVE.
inline constexpr const char* kInfoPane = "info";

/// THE OFFICE THE SOURCE EDITOR USED TO BE OFFERED FROM, and the one it is offered from now
/// -- the fourth pair, spelled for `kFilesProvider`'s reasons on both sides, and the last of
/// the arc: `editor-pane/vocabulary.hpp` is the weave's own header, this host does not link
/// the weave, and a case checks the two spellings against each other.
inline constexpr const char* kRetiredEditorProvider = "zengine.workshop";
inline constexpr const char* kEditorProvider = "zengine.editor";

/// THE PANE KEY, WHICH DID NOT MOVE. Its place is the overlay stack on both sides, so the
/// row's `default` goes on meaning what it always meant and nothing is written.
inline constexpr const char* kEditorPane = "editor";

// ---- THE TABLE, AT THE THRESHOLD THIS FILE NAMED FOR ONE --------------------------------
//
// ⚠ THE THIRD PAIR IS THE ONE THAT BOUGHT THE TABLE, AND FOR THE REASON WRITTEN ABOVE RATHER
// THAN FOR ITS NUMBER. The note above says two pairs is cheaper than a table and that the case
// for one is "a THIRD reference plus something a pair cannot say". Info is the third reference
// and it says the extra thing: its PLACE moved with its office. Every desk a maker saved holds
// `zengine.workshop/info` with a `default` place, and `default` used to mean the right column
// because Workshop's own catalog put it there; the catalog row leaves in this commit, so
// `default` would start meaning the overlay stack and a maker's Info would appear over their
// material. A pair rewrites a provider. This rewrites a provider AND the place the host used
// to answer with, which is one more column and one more line in one loop.

/// ONE PANE THAT CHANGED HANDS: what a saved file wrote, what it means now, and the place the
/// host's own catalog used to give it.
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

/// REWRITE EVERY RETIRED REFERENCE IN ONE SETUP, and say which ones. A row's two sizes and its
/// front order are untouched, because none of those changed hands.
///
/// ⚠ THE PLACE IS WRITTEN ONLY OVER A `default`. A maker who moved their Info pane said where
/// it goes, and that sentence outranks the one the catalog used to say for them: converting it
/// would move a pane they had already put somewhere. A row that said nothing gets the place the
/// host would have given it, which is the whole of what the conversion preserves.
///
/// ⚠ IT RUNS BEFORE THE SETUP'S OWN LAW. A file that names BOTH spellings of one pane holds
/// two rows for it after this, and `check_setup` refuses it by name -- which is the true
/// sentence about a contradictory file, and is what a converter that ran afterwards would
/// have hidden.
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

/// WHAT A MAKER IS TOLD, ONCE, when a file they wrote named a pane this host used to offer.
/// Said in the panes' own durable names, because those are what they would find in the file
/// if they went and looked.
///
/// ⚠ ONE SENTENCE FOR A RUN, NOT ONE PER ROW. The reader says this once from a count
/// (`setup_persist::setup_in`), so a maker whose desk holds a pane twice over two layouts is
/// told once -- and not told which of their rows was which.
///
/// ⚠ ...AND IT NAMES WHAT ACTUALLY MOVED. `converted` counts rows, so the sentence is composed
/// from what this run's file HELD: a maker who never opened the Builder is not told about a
/// pane they would not find if they went and looked, which is the whole reason the note spells
/// durable names in the first place.
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
