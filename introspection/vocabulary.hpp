// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_VOCABULARY_HPP
#define ZENGINE_INTROSPECTION_VOCABULARY_HPP

// The Introspection tool's DURABLE NAMES -- four constants, and since SEL-0 exactly
// ONE shape of its own.
//
// UNTIL SEL-0 IT PUBLISHED NO VOCABULARY AT ALL, and the change is worth reading
// rather than skimming, because everything else about this tool is unchanged: it
// still asks nothing new, still answers the same four pane shapes, and still speaks
// somebody else's sentences for everything it RECEIVES (the Workshop pane protocol,
// workshop/pane_vocabulary.hpp, and the Weave Manager's `zen.ListLoaded`). What it
// gained is one thing it can now SAY that nobody else could say for it: that a
// maker selected one of the entries this pane was showing. No existing sentence
// carried that, because the fact is about this pane's own material.
//
// So what a consumer needs from this header is the two halves of one durable
// `PaneRef`, the two lines a maker reads in the picker, and -- for a consumer that
// wants to hear the fact rather than merely open the pane -- `LoadedSelected`.
//
// WHY THE OFFICE IS HERE AND NOT ONLY IN THE .cpp. A `PaneRef` is what a saved
// setup names, so `zengine.introspection/loaded` is a promise to a maker's file --
// it survives this build, this incarnation and this load order, and it is the one
// thing about this tool a later phase must not casually rename. A constant a host,
// a test and the provider all read is what keeps three copies of that promise from
// drifting into two. The shape is here for the same reason one layer out: a
// listener is a STRANGER to `introspection.cpp` and must be able to accept the
// sentence without compiling the tool that says it.

#include <zen/weave/shape.hpp>

#include <string>

namespace zengine::introspection {

/// THE OFFICE THIS TOOL HOLDS -- the durable half of its `PaneRef`, and the only
/// address by which anything reaches it.
///
/// A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather than the
/// incarnation that happened to offer first: a reloaded provider is a different
/// `WeaveId` and the same office (WP-0's B2).
inline constexpr const char* kIntrospectionRole = "zengine.introspection";

/// THE PANE KEY, in this office's namespace and nobody else's. Two offices may
/// both offer `loaded` and they are two panes; that is the `PaneRef` doing its job.
///
/// It names the FACTS rather than the tool, because a second Introspection pane is
/// a plausible later phase and `zengine.introspection/introspection` would have
/// left it nowhere to go.
inline constexpr const char* kLoadedPane = "loaded";

/// The two lines the picker lists, and what Workshop's pane header says after the
/// office (`Loaded @zengine.introspection`).
///
/// Bounded by Workshop's own admission law before a byte is retained -- a name at
/// 32 bytes, a summary at 64 -- so these are written short deliberately rather
/// than trimmed by somebody else later.
///
/// AND SHORTER THAN THE ADMISSION BOUND ON PURPOSE. The picker's name column is TEN
/// cells (`kPickerNameCols`), which is a third of what admission allows, so a name
/// this tool was entitled to would still have arrived at a maker's eye cut. INTR-0
/// found that with `Loaded Weaves` on its first live run and did two things about
/// it: taught the column to MARK its cut, which every later provider now inherits,
/// and then chose a name that does not need the mark. A tool whose name only reads
/// correctly because a truncation is marked has a name too long for the room it
/// lives in.
///
/// `Loaded` rather than `Weaves`, and the difference is this pane's whole caveat:
/// the list is what the kernel LOADED, and a heading reading `Weaves` would promise
/// exactly the population it cannot see.
inline constexpr const char* kLoadedPaneName = "Loaded";
inline constexpr const char* kLoadedPaneSummary = "what the kernel has loaded, and each one's role";

/// THE SECOND AND THIRD PANES (INTR-1), and the FIRST IS NOT WIDENED TO HOLD THEM.
///
/// `loaded` asks what the KERNEL has loaded, and that question stayed legitimate the
/// moment two more facts became askable -- so it is unchanged in meaning, in rows and
/// in wire form. What arrived is two more questions with two more owners:
///
///     loaded        the Kernel's map            which WEAVES are loaded
///     arrangement   LOAD-0's resolved rows      which AUTHORED PARTICIPATIONS resolved
///     powers        PROV-0's live catalog       which POWERS resolve, and whose code
///
/// THEY ARE THREE PANES AND NOT ONE TABLE, because they have three populations, three
/// owners and three currencies. A maker looking for "is my Timer loaded" and a maker
/// looking for "who supplies math.max" are not looking at one list, and a pane that
/// merged them would have to invent a row kind that is neither.
///
/// AND THE APPARENT DISAGREEMENT BETWEEN THE FIRST TWO IS CORRECT.
/// `zengine-operators-basic` is a provider and not a weave: no Kernel loads it, it has
/// no `WeaveId` and no role. It is in `arrangement` and it is NOT in `loaded`, and a
/// build that "fixed" that would have made one of the two panes lie.
inline constexpr const char* kArrangementPane = "arrangement";
inline constexpr const char* kPowersPane = "powers";

/// The picker lines for the two, written to `kLoadedPaneName`'s rule: inside the TEN
/// cells `kPickerNameCols` actually shows, so neither arrives at a maker's eye marked.
inline constexpr const char* kArrangementPaneName = "Project";
inline constexpr const char* kArrangementPaneSummary =
    "what this project asked for, and what resolved";
inline constexpr const char* kPowersPaneName = "Powers";
inline constexpr const char* kPowersPaneSummary = "which operators resolve, and who supplies each";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable
/// the way the two keys above are -- it is a file name, and it is here because the
/// host's boot list and the suite's loader must agree on it.
inline constexpr const char* kIntrospectionStem = "zengine-introspection";

/// A MAKER SELECTED ONE OF THE ENTRIES THIS PANE WAS SHOWING (SEL-0).
///
/// ---- IT IS A FACT, NOT A COMMAND ------------------------------------------
///
/// It says what HAPPENED and nothing about what should happen next. Nothing in
/// this build listens to it, and that is deliberate rather than unfinished: the
/// tool that knows a maker pointed at something is not the tool that knows what
/// pointing at it should mean, and a pane that decided would be a workflow policy
/// compiled into an inventory.
///
/// ---- IT CARRIES DATA, AND AUTHORITY DOES NOT TRAVEL WITH IT ---------------
///
/// A listener that hears `library = "zengine-timer", role = "zengine.timer"` has
/// learned two strings a maker was already looking at. It has NOT thereby been
/// permitted to send that weave anything, interrogate it, load, reload, swap or
/// unload it, read its state, assume its role, or acquire any grant it holds. A
/// grant in this Loom is per `(shape, version, target)` and is written by whoever
/// mounts a weave; a value arriving in a message is not one and can never become
/// one. VALUES MAY FLOW; AUTHORITY MUST NOT FLOW IMPLICITLY WITH THEM -- and the
/// place that rule is easiest to break is exactly here, where a reference to a
/// powerful thing looks like a handle on it.
///
/// ---- WHAT THE IDENTITY IS, AND WHAT IT IS NOT -----------------------------
///
/// `library` is the name the KERNEL LOADED THE LIBRARY UNDER -- the key of
/// `Kernel::loaded()`'s map, as reported through `zen.ListLoaded`. It is not a
/// `WeaveId`, not a participant identity, not a package, publisher or signature,
/// and not proof that anything is alive now: the pane is a snapshot and this fact
/// is about what the snapshot SHOWED. Loom has no participant enumeration, so
/// there is no wider identity available to promote it into and none is invented.
///
/// `role` is the office the kernel bound at load, and an EMPTY `role` IS AN
/// OBSERVED ABSENCE rather than a missing reading -- `LoadedWeave`'s own rule
/// (loaded.hpp), reused rather than re-spelled, because it is the same field
/// travelling one layer further. The pane writes `(no role)` for a maker; the wire
/// carries the empty string, so a listener reads the observation and not the
/// prose.
///
/// `pane` says WHICH of this office's panes the selection happened in. It is here
/// even though only `loaded` exists, for `PaneOffered`'s reason: one provider is
/// not one pane, and a listener that had to assume it would break on the second.
/// WHOSE fact it is, is `mail.authored_role()` -- the same Loom stamp the pane
/// protocol rests on, which no payload can write and no sender can choose, so
/// there is no `provider` field here either.
///
/// ---- WHAT IT IS NOT SHAPED FOR --------------------------------------------
///
/// It is not `selection_changed(old, new)`: there is no previous value and no
/// deselection, because a maker pressing the same row twice performed two
/// gestures and a future trigger reading "whenever a maker selects this" is owed
/// both. It carries no row index, because a row is where a thing was drawn and
/// not what it is; no timestamp, because this weave holds no clock; and no
/// grants, schemas or state, because the pane never observed any.
struct LoadedSelected {
    std::string pane;    ///< the pane key this selection happened in -- `loaded`
    std::string library; ///< the loaded-library name the selected row named
    std::string role;    ///< the role observed at load; EMPTY means the kernel bound none
    ZEN_SHAPE(LoadedSelected, 1, ZEN_FIELD(pane), ZEN_FIELD(library), ZEN_FIELD(role));
};

} // namespace zengine::introspection

#endif // ZENGINE_INTROSPECTION_VOCABULARY_HPP
