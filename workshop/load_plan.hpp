// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LOAD_PLAN_HPP
#define ZENGINE_WORKSHOP_LOAD_PLAN_HPP

// WHICH ARTIFACTS PARTICIPATE IN THIS PROJECT, AND HOW.
//
// ---- The law -----------------------------------------------------------------
//
//   An artifact is ONE authored project participant which may expose ZERO OR MORE
//   runtime surfaces.
//
//   The project DECLARES participation. The host PERFORMS it.
//
//   LIST, not SCAN.
//
//   Order may be authored explicitly before Zen earns dependency solving.
//
//   The plan records INTENT; the runtime still discovers what the artifact
//   actually provides.
//
// ---- Why ONE record per artifact ---------------------------------------------
//
// Before this file, Workshop's `main()` held two independent hard-coded lists: two
// provider mounts and five weave boots. `zengine-timer` was in BOTH, because it is
// one shared library that participates in two ways -- it SUPPLIES
// `timer.normalize_delay` and it CONSTRUCTS the `zengine.timer` weave -- and its
// provider contribution has to be mounted before its weave is created, because a
// host-backed Timer validates that rule inside its own constructor.
//
// Two lists cannot say that. They can only be maintained so that they happen to
// agree, and the ordering law between them lived in a comment. One record with two
// OPTIONAL surfaces says it once, in one place, in the order it is executed.
//
// ---- Why the surfaces stay SEPARATE optional intentions -----------------------
//
//   load this participant
//
// and
//
//   let this artifact change the host's semantic world
//
// are different authored decisions, and an artifact that can do both must still be
// asked which one is meant. `zengine-timer` deliberately requests both;
// `zengine-composer` exports no provider surface and would not get one if it grew
// one tomorrow; `zengine-operators-basic` is not a weave at all and mounting it
// must never make a Kernel look for one. A single `participates: true`
// bit would collapse three real authored states into one.
//
// ---- What an artifact is NAMED by, and why it is a STEM ------------------------
//
// A STEM, exactly as `--skin` and `--input` were and as `HostContext::so()` already
// spells one: `zengine-timer`, not `zengine-timer.so` and not an absolute path. The
// host owns the one rule that turns a stem into a file (a directory, a separator,
// the platform's suffix), which is what makes ONE plan legal on Linux and on
// Windows without a platform matrix, a per-OS field or a package locator.
//
// A stem carries NO path separator and NO `..` (`check_artifact_stem`). That is not
// tidiness: a load plan is an execution-authority document, and a stem that could
// climb out of the host's artifact directory would make "which files may run in
// this process" a question about the plan's text rather than about the directory
// the host was deployed into.
//
// ---- What this file deliberately does NOT contain -----------------------------
//
// No version constraint, no dependency, no hash, no description, no trust level, no
// capability list, no restart or reload policy, no platform matrix, no metadata, and
// -- emphatically -- NO OPERATOR IDENTITY. `math.max` and `timer.normalize_delay`
// belong to the providers that supply them; a plan that copied them into the project
// file would be a host authoring semantics again, one indirection further out.
// What is written down is *mount this provider artifact*, never *these are
// the powers it has*.
//
// It also holds no RESOLVED truth: no WeaveId, no mounted provider identity, no
// contribution count, no outcome. Those exist only at runtime and only in
// `load_execute.hpp`'s `ResolvedArtifact`. Authored intent and resolved state are
// different truths, and an unresolvable entry stays in the plan exactly as written
// (the same law an unresolvable `PaneRef` already lives under).

#include "property.hpp" // `Written` -- a refusal carries its reason

#include "operator/catalog.hpp" // `op::MountMode` -- one spelling of overlay

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace zengine::workshop::load {

// ---- What a plan may hold -----------------------------------------------------

/// How many artifacts one plan may name.
///
/// Measured against the other end, `kMaxSetupPanes`' way: the production plan is
/// six rows, the graphical one is six, and a project that grew a tool a month for a
/// decade would not reach this. What it bounds is a forged file -- a plan is read
/// before anything is mounted or loaded, and a hostile one does not get to choose
/// the cost of refusing it.
inline constexpr std::size_t kMaxPlanArtifacts = 64;

/// How long an artifact stem may be.
///
/// A stem is a FILE NAME without its suffix, not prose. Sixty-four bytes holds
/// every artifact this repository ships several times over (`zengine-skin-tui-classic`
/// is twenty-four) and bounds what one line of a forged file can push at the loader.
inline constexpr std::size_t kMaxArtifactStemLen = 64;

/// How many choices one plan may author. Two per switchable office is the use; sixteen bounds a
/// forged file the way `kMaxPlanArtifacts` does.
inline constexpr std::size_t kMaxPlanChoices = 16;

/// How long a choice's name may be: a word a maker types.
inline constexpr std::size_t kMaxChoiceNameLen = 32;

/// How long a weave role may be.
///
/// `kMaxPaneKeyLen`'s number for `kMaxPaneKeyLen`'s reason: a role is a ROUTING NAME
/// and sixty-four bytes holds a reverse-domain role several levels deep
/// (`zengine.introspection` is twenty-one). It is deliberately not cut to the roles
/// this build knows, because a plan must be able to name a role belonging to an
/// artifact this build has never heard of.
inline constexpr std::size_t kMaxWeaveRoleLen = 64;

// ---- One artifact's optional surfaces -----------------------------------------

/// PROVIDER PARTICIPATION: let this artifact contribute executable semantic power
/// to the host's catalog.
///
/// The mode is `op::MountMode` ITSELF and not a second enumeration of it. The operator package
/// owns exactly one spelling of "cover a power somebody already supplies", the
/// catalog is the thing that enforces it, and a plan-local copy would be a second
/// vocabulary for one decision -- which is how two spellings of one rule come to
/// disagree. What this phase adds is that the intent is now DURABLE: an overlay can
/// be authored project arrangement rather than ad hoc runtime test code.
struct ProviderIntent {
    op::MountMode mode = op::MountMode::Ordinary;

    friend bool operator==(const ProviderIntent&, const ProviderIntent&) = default;
};

/// WEAVE PARTICIPATION: let this artifact be loaded as a Loom weave under this role.
///
/// IT CARRIES A ROLE AND NOT A NAME, and the absence is the point. `zen.LoadWeave`
/// takes a name, a path and a role, and every call site in this repository passes
/// the artifact's own stem as the name -- so a plan carrying both would carry the
/// same string twice and let the two drift. The artifact record IS the name, which
/// is also §3's whole claim: `zengine-timer` appears ONCE.
///
/// An empty role is refused rather than passed through. `zen.LoadWeave` documents
/// empty as *bind no role*, and every weave this host loads is a role holder; a plan
/// that could silently mean "load it as nobody" would make a typo into a weave with
/// no way to be addressed.
struct WeaveIntent {
    std::string role;

    friend bool operator==(const WeaveIntent&, const WeaveIntent&) = default;
};

/// ONE AUTHORED ALTERNATIVE FOR AN OFFICE: this artifact may hold `role`, under the name a maker
/// switches to it by.
///
/// A CHOICE IS PARTICIPATION AUTHORED IN ADVANCE, AND ONLY THAT. It loads nothing at startup: the
/// office starts with the one artifact the plan's `artifacts` list loads into it, and another
/// choice is loaded only when a switch asks for it by name (`workshop/editor_switch.hpp`). What
/// the choice buys is that the artifact a switch loads is one the PROJECT named -- the host may
/// not name an artifact stem, and a switch that could load any file would be exactly the reach
/// the load plan exists to keep in an authored document.
///
/// THE NAME IS A MAKER'S WORD (`standard`, `neovim`), not a stem: it is what they type in the
/// Terminal and read in an answer, and it stays the same when the artifact behind it is rebuilt
/// under another stem.
struct ChoiceIntent {
    std::string role;
    std::string name;
    std::string stem;

    friend bool operator==(const ChoiceIntent&, const ChoiceIntent&) = default;
};

/// ONE AUTHORED PROJECT PARTICIPANT.
///
/// Both surfaces are optional and independent; an artifact requesting NEITHER is
/// refused, because a row that asks for nothing is a row a maker wrote by mistake.
struct ArtifactIntent {
    std::string stem;
    std::optional<ProviderIntent> provider;
    std::optional<WeaveIntent> weave;
    /// ⭐ MAY THIS PROJECT STAND WITHOUT THIS ROW? (P-WORK-22, format version 3.)
    ///
    /// THE DISTINCTION IS ESSENTIAL STARTUP AGAINST RECOVERABLE TOOL AVAILABILITY, and it is
    /// AUTHORED because only the plan knows which is which. The shipped plan's third row is the
    /// SKIN: a Workshop that survived its refusal would have no painter and no input -- a live
    /// process a maker can neither see nor quit -- so that row stops everything, exactly as
    /// every row did before this. The pane rows are the other kind: a Workshop with no Info
    /// pane is a Workshop, and ending it denies the maker the one surface that could have told
    /// them which artifact to build.
    ///
    /// ⚠ IT IS NOT "SKIP WHAT FAILS", AND THE DIFFERENCE IS THE WHOLE POLICY. A row is stepped
    /// over only because a maker WROTE that it may be; the rows behind it are still performed
    /// in authored order, nothing is reordered, no dependency is inferred and no authority is
    /// relaxed -- an optional row that refuses is refused, recorded by name with the refusing
    /// layer's own sentence, and reported as an unavailable tool. `allow_any()` and "continue
    /// past every failure" are the two things this field is written to not be.
    ///
    /// ⚠ AND IT CANNOT RESCUE A ROW SOMETHING BEHIND IT NEEDS. Order is still the dependency
    /// model (there is no solver): a maker who marks a row optional is saying that the rows
    /// after it can stand without it, and if that is untrue the later row refuses on its own
    /// and is judged on its own terms.
    bool optional = false;

    friend bool operator==(const ArtifactIntent&, const ArtifactIntent&) = default;
};

/// THE WHOLE AUTHORED LOAD PLAN: which artifacts participate, in what order.
///
/// THE ORDER IS THE V0 DEPENDENCY MODEL and it is a list, deliberately. Nothing here
/// infers that `zengine-timer`'s composition needs `zengine-operators-basic`'s
/// primitives; a person wrote them down in the order they must happen, and the host
/// executes that order. A solver is a later phase and would be a solver over facts
/// this plan does not carry.
///
/// `choices` ARE THE AUTHORED ALTERNATIVES FOR OFFICES (format version 2): which artifacts may
/// hold an office a maker can switch, and by which names. Empty for every plan that authors none,
/// and such a plan is written as version 1, byte for byte what it always was.
struct LoadPlan {
    std::vector<ArtifactIntent> artifacts;
    std::vector<ChoiceIntent> choices;

    friend bool operator==(const LoadPlan&, const LoadPlan&) = default;
};

// ---- The plan's own law -------------------------------------------------------

/// What this application accepts as an artifact stem.
///
/// FIVE RULES. It must be there; it must be short enough to be a file name; it must
/// carry no space or control character; it must contain no path separator; and it
/// must not be `..` or contain a `..` segment.
///
/// THE LAST TWO ARE THE AUTHORITY RULES and they are the reason this function is not
/// `check_pane_key` reused. A pane key names something inside this process; a stem
/// names a FILE THIS HOST WILL EXECUTE. `../../../tmp/evil` is a perfectly legal
/// routing name and is not a legal thing to hand a dynamic loader from a text file.
/// Both separators are refused on both platforms, because a plan is one document and
/// a rule that varied by host would make a plan legal in one deployment and a
/// traversal in another.
inline Written check_artifact_stem(const std::string& stem) {
    if (stem.empty()) {
        return Written::no("an artifact stem cannot be empty");
    }
    if (stem.size() > kMaxArtifactStemLen) {
        return Written::no("an artifact stem is at most " + std::to_string(kMaxArtifactStemLen) +
                           " bytes");
    }
    for (const char c : stem) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte <= ' ' || byte == 0x7Fu) {
            return Written::no("artifact stem `" + stem +
                               "` cannot contain spaces or control characters");
        }
        if (c == '/' || c == '\\') {
            return Written::no("artifact stem `" + stem +
                               "` cannot contain a path separator: a stem names a file beside "
                               "the host, and the host owns the rule that spells it");
        }
    }
    if (stem == ".." || stem.find("..") != std::string::npos) {
        return Written::no("artifact stem `" + stem + "` cannot contain `..`");
    }
    return Written::ok();
}

/// What this application accepts as a weave role. Shape only; whether anything
/// holds it, or can, is the Kernel's question and is answered at load.
inline Written check_weave_role(const std::string& role) {
    if (role.empty()) {
        return Written::no("a weave declaration needs a role");
    }
    if (role.size() > kMaxWeaveRoleLen) {
        return Written::no("a weave role is at most " + std::to_string(kMaxWeaveRoleLen) +
                           " bytes");
    }
    for (const char c : role) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte <= ' ' || byte == 0x7Fu) {
            return Written::no("weave role `" + role +
                               "` cannot contain spaces or control characters");
        }
    }
    return Written::ok();
}

/// What a choice may be called: a lowercase letter, then lowercase letters, digits and `-`. A
/// word the Terminal reads as text in any position of a typed sentence, and one spelling per
/// meaning (no case to fold).
inline Written check_choice_name(const std::string& name) {
    if (name.empty()) {
        return Written::no("a choice needs a name");
    }
    if (name.size() > kMaxChoiceNameLen) {
        return Written::no("a choice name is at most " + std::to_string(kMaxChoiceNameLen) +
                           " bytes");
    }
    if (name[0] < 'a' || name[0] > 'z') {
        return Written::no("choice name `" + name + "` must begin with a lowercase letter");
    }
    for (const char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return Written::no("choice name `" + name +
                               "` may hold only lowercase letters, digits and `-`");
        }
    }
    return Written::ok();
}

/// EVERY LAW ONE ARTIFACT ROW MEETS, minus the one that is about the WHOLE plan.
///
/// The row cannot see its neighbours, so it cannot answer the duplicate question --
/// which is `check_plan`'s, below, for `check_setup`'s reason exactly.
inline Written check_artifact(const ArtifactIntent& a) {
    const Written stem = check_artifact_stem(a.stem);
    if (!stem.accepted) {
        return stem;
    }
    if (!a.provider.has_value() && !a.weave.has_value()) {
        return Written::no("artifact `" + a.stem +
                           "` requests neither provider nor weave participation");
    }
    if (a.weave.has_value()) {
        const Written role = check_weave_role(a.weave->role);
        if (!role.accepted) {
            return Written::no("artifact `" + a.stem + "`: " + role.refusal);
        }
    }
    return Written::ok();
}

/// THE CHOICES' OWN LAW, over the whole plan.
///
///   each choice      a legal role, a legal name, a legal stem
///   one per word     a role names each choice once, and each artifact once
///   one office       an artifact is a choice for one role only
///   one start        a role with choices has EXACTLY ONE row in `artifacts` loading it, and
///                    that row's artifact is one of the role's choices -- so the office starts
///                    held by a choice, and a switch always knows which one it is leaving
///   weave only       a choice's artifact, where `artifacts` names it, is that start row and
///                    asks for weave participation alone: a switch moves an office, and it
///                    would not move a provider's contribution to the catalog with it
///
/// A choice NOT named in `artifacts` is legal and is the ordinary case: it is realized only when
/// a switch asks for it.
// WL-SWITCH-01 -- agents/workshop/editor-switch.md
inline Written check_choices(const LoadPlan& plan) {
    if (plan.choices.size() > kMaxPlanChoices) {
        return Written::no("a load plan authors at most " + std::to_string(kMaxPlanChoices) +
                           " choices");
    }
    for (std::size_t i = 0; i < plan.choices.size(); ++i) {
        const ChoiceIntent& c = plan.choices[i];
        const Written role = check_weave_role(c.role);
        if (!role.accepted) {
            return Written::no("choice `" + c.name + "`: " + role.refusal);
        }
        const Written name = check_choice_name(c.name);
        if (!name.accepted) {
            return name;
        }
        const Written stem = check_artifact_stem(c.stem);
        if (!stem.accepted) {
            return Written::no("choice `" + c.name + "`: " + stem.refusal);
        }
        for (std::size_t k = 0; k < i; ++k) {
            const ChoiceIntent& e = plan.choices[k];
            if (e.role == c.role && e.name == c.name) {
                return Written::no("choice `" + c.name + "` is authored twice for `" + c.role +
                                   "`");
            }
            if (e.role == c.role && e.stem == c.stem) {
                return Written::no("artifact `" + c.stem + "` is authored twice as a choice for `" +
                                   c.role + "` (as `" + e.name + "` and as `" + c.name + "`)");
            }
            if (e.role != c.role && e.stem == c.stem) {
                return Written::no("artifact `" + c.stem + "` is a choice for both `" + e.role +
                                   "` and `" + c.role +
                                   "`: an artifact is a choice for one office");
            }
        }
    }
    for (const ChoiceIntent& c : plan.choices) {
        std::size_t holders = 0;
        const ArtifactIntent* start = nullptr;
        for (const ArtifactIntent& a : plan.artifacts) {
            if (a.weave.has_value() && a.weave->role == c.role) {
                ++holders;
                start = &a;
            }
        }
        if (holders != 1 || start == nullptr) {
            return Written::no("the choices for `" + c.role +
                               "` need exactly one artifact loaded into it at start, and the "
                               "plan loads " + std::to_string(holders));
        }
        bool start_is_choice = false;
        for (const ChoiceIntent& d : plan.choices) {
            start_is_choice = start_is_choice || (d.role == c.role && d.stem == start->stem);
        }
        if (!start_is_choice) {
            return Written::no("artifact `" + start->stem + "` starts in `" + c.role +
                               "`, which has choices, and is not one of them");
        }
        for (const ArtifactIntent& a : plan.artifacts) {
            if (a.stem != c.stem) {
                continue;
            }
            if (!a.weave.has_value() || a.weave->role != c.role) {
                return Written::no("artifact `" + c.stem + "` is a choice for `" + c.role +
                                   "` and is loaded into something else");
            }
            if (a.provider.has_value()) {
                return Written::no("artifact `" + c.stem + "` is a choice for `" + c.role +
                                   "` and also supplies operators; a switch moves an office, "
                                   "not a provider's contribution");
            }
        }
    }
    return Written::ok();
}

/// THE CHOICES AUTHORED FOR `role`, in authored order (empty when it has none).
inline std::vector<ChoiceIntent> choices_for(const LoadPlan& plan, const std::string& role) {
    std::vector<ChoiceIntent> out;
    for (const ChoiceIntent& c : plan.choices) {
        if (c.role == role) {
            out.push_back(c);
        }
    }
    return out;
}

/// EVERY LAW A WHOLE PLAN MEETS.
///
/// THE DUPLICATE RULE IS EXACT-STEM AND THAT IS ENOUGH FOR V0. Two rows naming one
/// stem cannot mean anything today -- the catalog refuses a second mount of one
/// provider identity by name, and a second `zen.LoadWeave` of one artifact under one
/// role is a role collision -- so a plan that says it twice is refused rather than
/// executed twice and half-refused halfway down. What this deliberately is NOT is
/// identity canonicalisation: no symlink resolution, no case folding, no realpath.
/// A stem carries no separator, resolves against exactly one directory, and is
/// compared as the bytes a person wrote.
///
/// AN EMPTY PLAN IS LEGAL. A host with no artifacts is a host that paints nothing and
/// says so at its own layer; refusing here would be this file deciding what a project
/// must contain, which is the founder's decision and not the format's.
inline Written check_plan(const LoadPlan& plan) {
    if (plan.artifacts.size() > kMaxPlanArtifacts) {
        return Written::no("a load plan names at most " + std::to_string(kMaxPlanArtifacts) +
                           " artifacts");
    }
    for (std::size_t i = 0; i < plan.artifacts.size(); ++i) {
        const Written row = check_artifact(plan.artifacts[i]);
        if (!row.accepted) {
            return row;
        }
        for (std::size_t k = 0; k < i; ++k) {
            if (plan.artifacts[k].stem == plan.artifacts[i].stem) {
                return Written::no("artifact `" + plan.artifacts[i].stem +
                                   "` is declared twice: one artifact is one record, and its "
                                   "provider and weave participation are fields of it");
            }
        }
    }
    return check_choices(plan);
}

} // namespace zengine::workshop::load

#endif // ZENGINE_WORKSHOP_LOAD_PLAN_HPP
