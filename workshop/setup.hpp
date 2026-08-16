// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_HPP
#define ZENGINE_WORKSHOP_SETUP_HPP

// WHAT A MAKER CALLS THE ARRANGEMENT THEY ARE WORKING IN, and the whole of what
// that is allowed to mean in WS-0.
//
// A SETUP IS A NAME AND AN ORDERED LIST OF PANE REFERENCES. Nothing else. It is
// the third kind of fact this application has, and the only interesting thing
// about it is where its edges are:
//
//   DOCUMENT   the objects a maker authored (vocabulary.hpp, persist.hpp). A
//              setup neither contains nor touches one, and saving or restoring
//              a setup changes no document byte.
//   SETUP      the human name, and which panes the maker meant to have open, in
//              order. Authored, persisted, and this file.
//   SESSION    the picker's cursor, a Builder panel's copy of a tool's status, a
//              Terminal's draft, a drag, a notice. Belongs to the run.
//   DERIVED    every rectangle a pane resolves to on the current screen. Never
//              written down anywhere, here least of all.
//
// ---- Why a reference is two strings ---------------------------------------
//
// `Panels::open` holds `panel::kBuilder` and `panel::kInfo`, which are indices
// into THIS BUILD'S OWN vocabulary. A file cannot hold one, for two reasons and
// the second is the load-bearing one:
//
//   an ordinal is not durable    renumber the two constants in panel.hpp and
//                                every saved setup silently opens the other
//                                panel. The same argument persist.hpp makes for
//                                writing an extent mode as the WORD `cells`.
//   an ordinal cannot be ABSENT  there is no integer meaning "a pane this build
//                                has never heard of". A setup that could not say
//                                that would have to drop such an entry on load,
//                                which is a saved file quietly editing itself.
//
// So a durable reference is a PROVIDER/SERVICE KEY and a PANE KEY, both text,
// both readable by a person looking at their own file:
//
//     zengine.workshop / info
//     zengine.workshop / builder
//
// The built-ins carry the same shape a later external provider will carry
// (WP-R0), which is the whole reason it is two strings today rather than one:
// EXTERNAL-SAFE DOES NOT MEAN EXTERNAL-NOW, and the cost of being external-safe
// on day one is that today's file needs no migration on the day it stops being
// the only kind.
//
// WHAT `provider` IS NOT is stated on `kWorkshopProvider` in panel.hpp and is
// worth reaching from here too: it is a ROUTE, not an author identity and not
// stamped provenance. Nothing in this file authenticates anything, and nothing
// in this file may come to.
//
// ---- Resolution is FALLIBLE, and internal lookup stays total ---------------
//
// `panel_kind(unknown)` answers with the Builder, and that is correct for its
// callers: they hand it a kind derived from bounded local state (a picker
// cursor, an open panel), so a total function is cheaper than an invariant
// somebody has to maintain. It is exactly wrong for a FILE, where the kind
// arrives as two strings a stranger wrote, and where answering "Builder" to an
// unknown reference would paint a maker's third-party pane as Workshop's build
// tool.
//
// So the totality stays where it was and `resolve_pane` is a SECOND, narrower
// door: it answers with nothing rather than with a fallback, and no placement,
// occupancy, picker or painter path can be reached through it without the
// caller having said what to do about the nothing.
//
// ---- What a setup deliberately does not have ------------------------------
//
// No placement, no rectangle, no screen extent, no text metric, no pane-instance
// identity, no opaque provider configuration, no focus, no tabs, no docking, no
// layout tree, no plugin manifest, no schema field of its own beyond the file's
// (setup_persist.hpp). Every one of those is either DERIVED from the current
// screen -- and would go stale the moment the window changed -- or a decision
// the first provider that actually needs it should get to make.

#include "panel.hpp"
#include "property.hpp"

#include "component/text_box.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The name a fresh Workshop's setup carries. It is a real name and not an
/// empty one, because a setup ALWAYS has a name -- "unnamed" would be a second
/// state for every reader of a name to handle, bought with nothing.
inline constexpr const char* kDefaultSetupName = "Default";

/// The setup file's suggested name. Beside the document's own
/// (`persist::kDefaultDocumentName`), and deliberately a different file: the two
/// artifacts answer different questions and a maker may want one without the
/// other.
inline constexpr const char* kDefaultSetupFileName = "workshop-setup.json";

// ---- The bounds, and why each one is the number it is -----------------------
//
// EVERY ONE OF THESE IS AN INPUT BOUNDARY AND NOT A CAPACITY. They exist because
// a setup can arrive from a file somebody else wrote, and material from a file
// must be refused BEFORE it is copied into the live setup -- the Loom's decoder
// already refuses a payload that would materialise more than its own budget, and
// these say what this application will additionally accept as a setup. None of
// them is a statement about how many panes Workshop can usefully show.

/// How long a setup's human name may be.
///
/// THIRTY-TWO, and it is the same number `doc::kMaxNameLen` uses for an object's
/// label, measured the same way (bytes), because it is the same kind of fact: a
/// name a maker types and then reads back on one line of a screen. The screen is
/// the reason it is not larger -- the setup line at the minimum composition
/// carries the name, the file it is stored in and its saved marker, and a
/// thirty-two-byte name leaves that line fitting whole at 78 cells with the
/// default file name (measured, screen.hpp's `setup_line`).
inline constexpr std::size_t kMaxSetupNameLen = 32;

/// How long either half of a `PaneRef` may be.
///
/// A key is a ROUTING NAME and not prose: sixty-four bytes holds a
/// reverse-domain provider several levels deep and any pane word a provider
/// would choose, and it bounds what one line of a forged file can push into the
/// active setup. It is deliberately far above anything the catalog uses --
/// `zengine.workshop` is sixteen -- because a setup must be able to retain a
/// reference to a pane THIS BUILD HAS NEVER HEARD OF, and a bound cut to today's
/// catalog would make tomorrow's reference unrepresentable.
inline constexpr std::size_t kMaxPaneKeyLen = 64;

/// How many pane references one setup may carry.
///
/// NOT `kPanelKinds`, emphatically. The catalog's population is a fact about
/// what THIS build can present; the file's limit is a fact about what a setup
/// may REMEMBER, and those are different numbers precisely because an
/// unresolved reference is a legal thing to keep. A bound cut to the catalog
/// would make today's population into tomorrow's file law and would refuse the
/// one case this whole design exists to allow.
///
/// Thirty-two is chosen against the other end: Workshop's honest simultaneous
/// capacity is one to two panes (the side region holds one; an overlay slot past
/// the first does not fit the minimum screen), and the picker's own ceiling is
/// eight kinds. Thirty-two is four times the largest catalog this composition
/// could present and still bounds a file at a few kilobytes.
inline constexpr std::size_t kMaxSetupPanes = 32;

// ---- The value ---------------------------------------------------------------

/// WHICH PANE A MAKER MEANT -- durably, and without naming a catalog slot.
///
/// Two strings, and the pair is the identity: `provider` says whose namespace to
/// read `pane` in. Neither half means anything alone, which is why they are one
/// struct rather than two fields on a setup entry.
///
/// EQUALITY IS BYTE EQUALITY of both halves. There is no normalisation, no case
/// folding and no aliasing here on purpose: a reference this build cannot
/// resolve must come back out of the file exactly as it went in, and a
/// comparison that tidied would be the first step towards a save that rewrote a
/// stranger's entry.
struct PaneRef {
    std::string provider;
    std::string pane;

    friend bool operator==(const PaneRef&, const PaneRef&) = default;
};

/// A setup: what a maker calls this arrangement, and which panes it has, in
/// order.
///
/// ORDER IS PRESERVED AND IT IS MEANING, not tidiness -- the same claim
/// `persist.hpp` makes about the objects in a document, and for a smaller but
/// real version of the same reason. An open panel placed in the overlay stack
/// takes the next SLOT in that stack, counted over the open list in order
/// (`bounds_of`), so with two stacked kinds the order of this list is the order
/// they appear down the screen. Today's two built-ins are placed in different
/// regions and cannot demonstrate it; the list is still never sorted, because a
/// save that tidied would silently choose a future stacking a maker did not.
///
/// AN EMPTY PANE LIST IS A LEGAL SETUP. "I want nothing open" is a thing a maker
/// can mean and a thing this application can present -- the picker removes the
/// last panel already -- so refusing to save it would make one reachable
/// arrangement unnameable.
struct Setup {
    std::string name;
    std::vector<PaneRef> panes;

    friend bool operator==(const Setup&, const Setup&) = default;
};

// ---- The reference a built-in kind carries, and the kind a reference names ---

/// THE DURABLE REFERENCE FOR AN INTERNAL KIND. Total, for exactly the reason
/// `panel_kind` is total and with exactly the same callers: every one of them
/// derives the kind from bounded local state -- a catalog row the picker's
/// cursor is on, or a panel already open -- so there is no unknown kind for this
/// to have an opinion about.
///
/// IT IS NOT THE FALLIBLE DIRECTION. The direction that meets a stranger's
/// bytes is `resolve_pane` below, and it is the one that answers with nothing.
inline PaneRef pane_ref_of(std::int64_t kind) {
    const PanelKind& row = panel_kind(kind);
    return PaneRef{row.provider, row.pane};
}

/// WHICH INTERNAL KIND THIS REFERENCE NAMES, OR NOTHING.
///
/// The one fallible boundary, and the only door through which text from a file
/// becomes a panel. An unknown provider and an unknown pane key are the same
/// answer -- nothing -- because a caller has the same thing to do about both,
/// and because a finer answer would invite a reader to believe Workshop knows
/// something about the provider it does not (see kWorkshopProvider: silence
/// proves unresolved, never unavailable).
///
/// AN UNKNOWN REFERENCE NEVER BECOMES THE BUILDER. That is the whole of what
/// this function is for; `panel_kind`'s fall-through is correct where it is and
/// would be a lie here.
inline std::optional<std::int64_t> resolve_pane(const PaneRef& ref) {
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        if (ref.provider == kPanelCatalog[i].provider && ref.pane == kPanelCatalog[i].pane) {
            return kPanelCatalog[i].kind;
        }
    }
    return std::nullopt;
}

/// Whether this build can currently present the pane this reference names.
inline bool resolvable(const PaneRef& ref) { return resolve_pane(ref).has_value(); }

/// A reference as a person reads it, for a notice or a status line: the two
/// halves with a slash between them, which is how this phase's prose spells one.
inline std::string ref_text(const PaneRef& ref) { return ref.provider + "/" + ref.pane; }

/// A SETUP'S NAME AS ONE QUOTED TOKEN OF MAKER-FACING PROSE (WS-0a).
///
/// AUTHORED NAMES STAY AUTHORED; THE SENTENCE THAT QUOTES ONE OWNS THE ESCAPING.
/// Nothing here mutates a setup, nothing here reaches a file, and no string this
/// returns is ever written back into a `Setup` -- it is a spelling, built at the
/// moment a sentence is and discarded with it.
///
/// WHY IT EXISTS. `check_setup_name` accepts ordinary punctuation, `"` and `\`
/// included, and it is right to: `Ops" run` is a name a maker may mean, and
/// narrowing the law after version 1 shipped would make a file that accepted
/// WS-0 wrote unreadable to the build that reads it next. The BYTES were never
/// the problem -- a control character is already refused, so no name can move a
/// terminal's cursor out of the line it was given. The SENTENCE was: three
/// callers spelled this token by hand as a quote, the name, and a quote, so a
/// name carrying a quote could manufacture the very delimiter a maker uses to
/// tell an identity from the status beside it, and a setup honestly named
/// `Ops" UNSAVED | decoy` read back as
///
///     setup "Ops" UNSAVED | decoy" saved | <path> | s name/save  r restore
///
/// THE OWNER HOLDS THE QUOTES, not only the escaping, and that is the
/// load-bearing half of the shape: a caller handed an escaped INTERIOR could
/// still improvise its own boundary around it, which is the same defect one
/// indirection further away. There is one implementation, and the setup status
/// line, the save notice and the restore notice all spend it.
///
/// ONE PASS, WITH THE ESCAPE WRITTEN BEFORE THE BYTE IT ESCAPES, so a backslash
/// this function inserts is never itself examined -- which is what makes an
/// authored backslash followed by an authored quote come back as two backslashes
/// and then an escaped quote, rather than collapsing into a spelling that some
/// other authored name also has. The substitution is INJECTIVE on purpose:
/// rendering a quote as an apostrophe would be shorter and would make two names
/// a maker can tell apart present as one.
///
/// NO POLICY OF ANY OTHER KIND. No locale, no normalisation, no case folding, no
/// width, no JSON. Two bytes mean something to this sentence; every other
/// accepted byte is itself.
inline std::string quoted_setup_name(const std::string& name) {
    std::string quoted;
    quoted.reserve(name.size() + 2);
    quoted += '"';
    for (const char c : name) {
        if (c == '\\' || c == '"') {
            quoted += '\\';
        }
        quoted += c;
    }
    quoted += '"';
    return quoted;
}

// ---- The law: what this application will accept as a setup -------------------
//
// ONE LAW, REACHED TWO WAYS -- W-5's discipline, applied to the second artifact
// this application persists. The name a maker types in the one-line editor and
// the name a forged file carries meet the SAME function, because two spellings
// of one rule is how a typed name and a loaded one come to disagree about what
// is legal.

/// What this application accepts as a setup's human name.
///
/// FOUR RULES AND NO MORE. It must be there; it must not be only spaces; it must
/// carry no control character; and it must be short enough to read on one line.
///
/// SPACES ARE ALLOWED INSIDE IT, because `Morning build` is a name a maker
/// means, and the reason all-spaces is refused is not tidiness -- a name that
/// renders as nothing would leave the setup line saying `setup ""` and the maker
/// unable to tell a named setup from an unnamed one.
///
/// CONTROL CHARACTERS ARE REFUSED RATHER THAN RENDERED SAFE, and that is the
/// deliberate half. A maker cannot type one; only a forged file can carry one;
/// and what it would do is move the terminal's cursor out of the line this name
/// was given. Refusing names the field and leaves the live setup untouched,
/// which is strictly more useful than a silent substitution a maker would then
/// have to discover.
///
/// WHAT IS NOT HERE: any Unicode policy. Valid UTF-8 is already the Loom gate's
/// answer on the way in from a file, and the platform's own answer on the way in
/// from a keyboard; normalisation, width, case and script are questions this
/// phase has no consumer for and would get wrong by guessing.
///
/// THE LENGTH IS A BYTE COUNT, AND THE REFUSAL SAYS SO (WS-0a).
/// `std::string::size()` is the whole of the measurement, so a nine-character
/// name written in four-byte UTF-8 is thirty-six bytes and this law refuses it --
/// and telling that maker they had exceeded thirty-two CHARACTERS would be a
/// false sentence about a true refusal. Saying `bytes` corrects the wording and
/// is emphatically not a new text policy: nothing here counts a code point, a
/// grapheme or a cell, which is the same absence the paragraph above declares.
inline Written check_setup_name(const std::string& name) {
    if (name.empty()) {
        return Written::no("a setup name cannot be empty");
    }
    if (name.size() > kMaxSetupNameLen) {
        return Written::no("a setup name is at most " + std::to_string(kMaxSetupNameLen) +
                           " bytes");
    }
    bool anything = false;
    for (const char c : name) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20u || byte == 0x7Fu) {
            return Written::no("a setup name cannot contain control characters");
        }
        if (byte != ' ') {
            anything = true;
        }
    }
    if (!anything) {
        return Written::no("a setup name needs more than spaces in it");
    }
    return Written::ok();
}

/// What this application accepts as either half of a `PaneRef`.
///
/// Deliberately narrow about SHAPE and deliberately silent about MEANING: a key
/// must be present, short enough to be a name rather than a payload, and free of
/// whitespace and control characters so that `provider/pane` remains one legible
/// token in a notice. Whether the key names anything is `resolve_pane`'s
/// question and is not an error.
///
/// ITS LENGTH IS A BYTE COUNT TOO, and its refusal says so for
/// `check_setup_name`'s reason (WS-0a): `kMaxPaneKeyLen` is spent against
/// `std::string::size()`, and a key is a routing name a provider may write in
/// any script the Loom's UTF-8 gate accepts.
inline Written check_pane_key(const std::string& key, const char* which) {
    if (key.empty()) {
        return Written::no(std::string("a pane reference's ") + which + " cannot be empty");
    }
    if (key.size() > kMaxPaneKeyLen) {
        return Written::no(std::string("a pane reference's ") + which + " is at most " +
                           std::to_string(kMaxPaneKeyLen) + " bytes");
    }
    for (const char c : key) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte <= ' ' || byte == 0x7Fu) {
            return Written::no(std::string("a pane reference's ") + which +
                               " cannot contain spaces or control characters");
        }
    }
    return Written::ok();
}

inline Written check_pane_ref(const PaneRef& ref) {
    const Written provider = check_pane_key(ref.provider, "provider");
    if (!provider.accepted) {
        return provider;
    }
    return check_pane_key(ref.pane, "pane key");
}

/// THE WHOLE-SETUP LAW, asked once on a complete candidate.
///
/// It judges the name, every reference, how many there are, and whether any two
/// are the same one. A DUPLICATE IS REFUSED rather than collapsed: a kind is
/// open or it is not (there is no multi-instance policy to make a second entry
/// mean anything), so a file naming one twice is a file whose author believed
/// something this application does not do, and silently keeping one of the two
/// would hide that.
inline Written check_setup(const Setup& s) {
    const Written named = check_setup_name(s.name);
    if (!named.accepted) {
        return named;
    }
    if (s.panes.size() > kMaxSetupPanes) {
        return Written::no("a setup holds at most " + std::to_string(kMaxSetupPanes) +
                           " panes -- this one names " + std::to_string(s.panes.size()));
    }
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        const Written legal = check_pane_ref(s.panes[i]);
        if (!legal.accepted) {
            return legal;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (s.panes[j] == s.panes[i]) {
                return Written::no("`" + ref_text(s.panes[i]) + "` is named twice");
            }
        }
    }
    return Written::ok();
}

// ---- Operations on the authored intent ---------------------------------------

inline bool has_pane(const Setup& s, const PaneRef& ref) {
    for (const PaneRef& p : s.panes) {
        if (p == ref) {
            return true;
        }
    }
    return false;
}

/// Add a reference to the end of the setup's order, or say it was already there.
///
/// THE END, because that is where `open_panel` has always put a newly opened
/// panel and therefore what a maker has always seen: the panel the picker just
/// opened takes the last slot of the stack. Making the authored order agree with
/// the resolved order a maker was already watching is the whole of the choice.
inline bool add_pane(Setup& s, const PaneRef& ref) {
    if (has_pane(s, ref)) {
        return false;
    }
    s.panes.push_back(ref);
    return true;
}

inline bool remove_pane(Setup& s, const PaneRef& ref) {
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        if (s.panes[i] == ref) {
            s.panes.erase(s.panes.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

/// The references this build cannot currently present, IN THE ORDER THE SETUP
/// HOLDS THEM. Asked by the status line and by the notice a restore leaves, so
/// that "unresolved" is a thing a maker can be told the identity of rather than
/// only the count of.
inline std::vector<PaneRef> unresolved_panes(const Setup& s) {
    std::vector<PaneRef> out;
    for (const PaneRef& p : s.panes) {
        if (!resolvable(p)) {
            out.push_back(p);
        }
    }
    return out;
}

/// WHAT A FRESH WORKSHOP'S SETUP IS -- derived from `kDefaultPanels`, which is
/// the ONE place "a fresh Workshop shows Info" is decided (panel.hpp). This
/// function holds no opinion of its own about which panes those are, which is
/// what makes it impossible for it to drift from `default_panels()`.
inline Setup default_setup() {
    Setup s;
    s.name = kDefaultSetupName;
    s.panes.reserve(kDefaultPanelCount);
    for (const std::int64_t kind : kDefaultPanels) {
        s.panes.push_back(pane_ref_of(kind));
    }
    return s;
}

// ---- Authored intent, reconciled onto resolved presentations ------------------

/// WHAT RECONCILING A SETUP ONTO THE LIVE PANELS ACTUALLY DID.
///
/// The kinds that were closed and are now open, the kinds that were open and are
/// now closed, and how many authored references this build could not present.
/// It is a REPORT and not a plan: `reconcile` has already done the work by the
/// time a caller reads one. What the caller does with it is the part that needs
/// a bus -- a newly opened Builder asks its tool what it is -- and that is the
/// weave's, which is why this struct exists rather than a `Mail&` parameter on a
/// function that otherwise touches nothing but presentation.
struct Reconciled {
    std::vector<std::int64_t> opened;
    std::vector<std::int64_t> closed;
    std::size_t unresolved = 0;
};

/// MAKE THE OPEN PANELS BE WHAT THE SETUP SAYS -- the one path, and the only
/// thing in this application that opens or closes a panel on a setup's behalf.
///
/// THREE CASES, DELIBERATELY DISTINGUISHED (WS-0 §9), because they are three
/// different things to do with a presentation that already exists:
///
///   open before, open after    the presentation is LEFT ALONE. `Panels::builder`
///                              is not touched, so a Builder panel that was
///                              showing a status goes on showing it and no
///                              second refresh is asked for. A reconcile that
///                              rebuilt every presentation would make restoring
///                              a setup you are already in a visible event.
///   open before, closed after  `close_panel` -- the existing door, so the
///                              per-kind view is forgotten by the same act that
///                              removes the panel, exactly as the picker's
///                              removal does. The TOOL is untouched: nothing
///                              here reaches a bus.
///   closed before, open after  opened, and NAMED in `opened` so the caller can
///                              perform whatever asking that kind does on open.
///
/// UNRESOLVED REFERENCES ARE COUNTED AND OTHERWISE IGNORED. They stay in the
/// setup -- this function takes it by const reference and could not remove one
/// if it wanted to -- and they produce no panel, no placeholder, no slot and no
/// message. That last is the point: a placeholder would have to be painted by
/// somebody, and the only kind available to paint it with is the Builder.
inline Reconciled reconcile(Panels& panels, const Setup& setup) {
    Reconciled done;
    std::vector<std::int64_t> wanted;
    wanted.reserve(setup.panes.size());
    for (const PaneRef& ref : setup.panes) {
        const std::optional<std::int64_t> kind = resolve_pane(ref);
        if (!kind.has_value()) {
            ++done.unresolved;
            continue;
        }
        wanted.push_back(*kind);
    }

    const auto wants = [&wanted](std::int64_t kind) {
        for (const std::int64_t k : wanted) {
            if (k == kind) {
                return true;
            }
        }
        return false;
    };

    // CLOSE FIRST, THROUGH THE EXISTING DOOR. `panels.open` is copied because
    // `close_panel` erases from it, and the per-kind view has to be forgotten by
    // the same call the picker uses -- a loop that rebuilt the vector directly
    // would leave a removed Builder's copied status alive beside no Builder.
    const std::vector<Panel> before = panels.open;
    for (const Panel& p : before) {
        if (!wants(p.kind)) {
            if (close_panel(panels, p.kind)) {
                done.closed.push_back(p.kind);
            }
        }
    }

    // ...THEN PUT WHAT REMAINS INTO THE SETUP'S ORDER. Assigning the vector
    // rather than opening one by one is what keeps the order authored rather
    // than incidental: `open_panel` appends, so a kind already open would have
    // kept whatever position it had.
    std::vector<Panel> now;
    now.reserve(wanted.size());
    for (const std::int64_t kind : wanted) {
        bool was_open = false;
        for (const Panel& p : before) {
            if (p.kind == kind) {
                was_open = true;
                break;
            }
        }
        if (!was_open) {
            done.opened.push_back(kind);
        }
        now.push_back(Panel{kind});
    }
    panels.open = std::move(now);
    return done;
}

// ---- The session's side of it -------------------------------------------------

/// THE ONE-LINE SETUP-NAME EDITOR: open or not, and the line being typed.
///
/// A MODE, like the picker and like the terminal overlay, and not a panel: it
/// has no catalog row, nothing presents it, and it closes the moment it has been
/// used. It is reachable only from command mode, which is exactly the state in
/// which no inspector row is being edited and the picker is closed -- so the
/// three cannot be live at once, and the routing in `on(KeyPressed)` says so in
/// its order rather than leaving it to that argument.
///
/// THE LINE IS A `component::TextBox` (HD-5) and not a `std::string`: the text,
/// the caret in it and which part of it a one-line editor can show are one fact,
/// and this is the third consumer of the component that fact earned. Nothing
/// bespoke was written for it.
struct SetupNaming {
    bool open = false;
    component::TextBox line;
};

/// THE SETUP A SESSION IS SHOWING, THE ONE IN ITS FILE, AND THE EDITOR OVER IT.
///
/// SAVED IS COMPUTED, NEVER FLAGGED -- the discipline W-5 established for the
/// document, applied to the second artifact. `on_file` is a COPY of the setup as
/// it was last successfully written or read, so `saved()` is a comparison and
/// cannot drift from the thing it describes. A dirty flag would need a hand at
/// every place a pane is added or removed and would be wrong the first time one
/// was missed -- and this phase adds three such places.
///
/// AN UNSAVED FRESH SESSION IS STRUCTURAL. `on_file` starts default-constructed,
/// with an EMPTY NAME, and `check_setup_name` refuses an empty name -- so no
/// setup a maker or a file can produce is ever equal to it. A fresh Workshop
/// therefore says UNSAVED because its setup has genuinely never been written,
/// which is the same sentence the document's own status has always said and for
/// the same reason.
struct SetupState {
    Setup active = default_setup();
    Setup on_file;
    SetupNaming naming;

    bool saved() const { return active == on_file; }
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SETUP_HPP
