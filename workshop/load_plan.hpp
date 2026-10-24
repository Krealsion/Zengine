// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LOAD_PLAN_HPP
#define ZENGINE_WORKSHOP_LOAD_PLAN_HPP

// Which artifacts participate in this project, and how: the typed load plan and its law
// (workshop/docs/load-plan.md, agents/realization.md).

#include "property.hpp" // `Written` -- a refusal carries its reason

#include "operator/catalog.hpp" // `op::MountMode` -- one spelling of overlay

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop::load {

// ---- What a plan may hold -----------------------------------------------------

/// How many artifacts one plan may name: far above any real plan. It bounds a forged file, which
/// is read before anything is mounted or loaded.
inline constexpr std::size_t kMaxPlanArtifacts = 64;

/// How long an artifact stem may be: a file name without its suffix, not prose.
inline constexpr std::size_t kMaxArtifactStemLen = 64;

/// How many choices one plan may author. Two per switchable office is the use; sixteen bounds a
/// forged file the way `kMaxPlanArtifacts` does.
inline constexpr std::size_t kMaxPlanChoices = 16;

/// How long a choice's name may be: a word a weaver types.
inline constexpr std::size_t kMaxChoiceNameLen = 32;

/// How long a weave role may be: `kMaxPaneKeyLen`'s number, for a routing name. Not cut to the
/// roles this build knows: a plan may name a role this build has never heard of.
inline constexpr std::size_t kMaxWeaveRoleLen = 64;

/// How many artifacts one row may name to try in its place. One is the use -- the OS window, else
/// the terminal window; four bounds a forged file.
inline constexpr std::size_t kMaxRowAlternatives = 4;

// ---- One artifact's optional surfaces -----------------------------------------

/// Provider participation: let this artifact contribute executable semantic power to the host's
/// catalog. The mode is `op::MountMode` itself, never a second spelling of the one rule.
struct ProviderIntent {
    op::MountMode mode = op::MountMode::Ordinary;

    friend bool operator==(const ProviderIntent&, const ProviderIntent&) = default;
};

/// Weave participation: let this artifact be loaded as a Loom weave under this role. A role and
/// no name, since the artifact record is the name; an empty role is refused, because every weave
/// this host loads holds one.
struct WeaveIntent {
    std::string role;

    friend bool operator==(const WeaveIntent&, const WeaveIntent&) = default;
};

/// One authored alternative for an office: this artifact may hold `role`, under the name a weaver
/// switches to it by. It loads nothing at startup; a switch loads it by name, so what a switch
/// loads is always one the project named. The name is a weaver's word, not a stem.
struct ChoiceIntent {
    std::string role;
    std::string name;
    std::string stem;

    friend bool operator==(const ChoiceIntent&, const ChoiceIntent&) = default;
};

/// ONE ARTIFACT TRIED IN A ROW'S PLACE (format version 4): loaded as a weave under its own role
/// when the row's own artifact, and every one named before it, could not be. A weave and no
/// provider, since a provider's contribution is not one a load can take back.
struct AlternativeIntent {
    std::string stem;
    WeaveIntent weave;

    friend bool operator==(const AlternativeIntent&, const AlternativeIntent&) = default;
};

/// ONE AUTHORED PROJECT PARTICIPANT.
///
/// Both surfaces are optional and independent; an artifact requesting NEITHER is
/// refused, because a row that asks for nothing is a row a weaver wrote by mistake.
struct ArtifactIntent {
    std::string stem;
    std::optional<ProviderIntent> provider;
    std::optional<WeaveIntent> weave;
    /// May this project stand without this row (format version 3)? Authored, because only the
    /// plan knows essential startup from a recoverable tool: a refused optional row is recorded
    /// by name and stepped over, and the rows behind it still run in authored order. It is not
    /// "skip what fails", and it cannot rescue a row that something behind it needs.
    bool optional = false;
    /// What this row loads where its own artifact cannot (format version 4), in the order tried:
    /// one whose load is refused passes to the next, one this installation does not hold is
    /// passed over unsaid, and the first that loads stands in the row for the run. The OS window,
    /// else the terminal window, is the shipped plan's.
    std::vector<AlternativeIntent> otherwise{};

    friend bool operator==(const ArtifactIntent&, const ArtifactIntent&) = default;
};

/// The whole authored load plan: which artifacts participate, in what order. The order is the
/// dependency model, a list a person wrote and the host executes, with no solver. `choices`
/// (format version 2) are the authored alternatives for offices.
struct LoadPlan {
    std::vector<ArtifactIntent> artifacts;
    std::vector<ChoiceIntent> choices;

    friend bool operator==(const LoadPlan&, const LoadPlan&) = default;
};

// ---- The plan's own law -------------------------------------------------------

/// What this application accepts as an artifact stem: present, short, no space or control byte,
/// no path separator, no `..` segment. The last two are authority rules, which is why this is not
/// `check_pane_key`: a stem names a file this host will execute, so both separators are refused
/// on both platforms.
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

/// Every law one artifact row meets; the duplicate question is `check_plan`'s.
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
    if (a.otherwise.empty()) {
        return Written::ok();
    }
    if (a.provider.has_value() || !a.weave.has_value()) {
        return Written::no("artifact `" + a.stem +
                           "` names what to load in its place, which only a row loading a weave "
                           "and supplying no operators may: a provider's contribution is not one "
                           "a load can take back");
    }
    if (a.otherwise.size() > kMaxRowAlternatives) {
        return Written::no("artifact `" + a.stem + "` names more than " +
                           std::to_string(kMaxRowAlternatives) + " artifacts to load in its place");
    }
    for (std::size_t i = 0; i < a.otherwise.size(); ++i) {
        const AlternativeIntent& alt = a.otherwise[i];
        const Written named = check_artifact_stem(alt.stem);
        if (!named.accepted) {
            return Written::no("artifact `" + a.stem + "`, otherwise: " + named.refusal);
        }
        const Written office = check_weave_role(alt.weave.role);
        if (!office.accepted) {
            return Written::no("artifact `" + a.stem + "`, otherwise `" + alt.stem +
                               "`: " + office.refusal);
        }
        if (alt.stem == a.stem) {
            return Written::no("artifact `" + a.stem + "` names itself to load in its own place");
        }
        for (std::size_t k = 0; k < i; ++k) {
            if (a.otherwise[k].stem == alt.stem) {
                return Written::no("artifact `" + a.stem + "` names `" + alt.stem +
                                   "` twice to load in its place");
            }
        }
    }
    return Written::ok();
}

/// THE ROW TAKEN BY ITS NEXT ALTERNATIVE: the first artifact it names in its place loads in the
/// row, and the ones after it stay its alternatives. Nothing when it names none.
inline void take_next_alternative(ArtifactIntent& row) {
    if (row.otherwise.empty()) {
        return;
    }
    row.stem = row.otherwise.front().stem;
    row.weave = row.otherwise.front().weave;
    row.otherwise.erase(row.otherwise.begin());
}

/// THE ROW THAT LOADS INTO `role`, TAKEN ALONE: the row whose own weave holds `role` keeps it and
/// tries nothing else; a row naming an alternative that holds `role` loads that alternative and
/// nothing else. Refused, changing nothing, when no row may load into `role`.
inline Written take_role(LoadPlan& plan, const std::string& role) {
    for (ArtifactIntent& row : plan.artifacts) {
        if (row.weave.has_value() && row.weave->role == role) {
            row.otherwise.clear();
            return Written::ok();
        }
        for (const AlternativeIntent& alt : row.otherwise) {
            if (alt.weave.role == role) {
                row.stem = alt.stem;
                row.weave = alt.weave;
                row.otherwise.clear();
                return Written::ok();
            }
        }
    }
    return Written::no("no row of this plan loads anything into `" + role + "`");
}

/// A remembered choice: an office, and the name of the choice that held it.
using RememberedChoice = std::pair<std::string, std::string>;

/// THE PLAN A START BEGINS: each remembered choice -- an office and a choice's name, as the last
/// session left them -- loaded in place of the row loading that office, while the plan still
/// authors it. A choice its own row already loads changes nothing. Returns the remembered choices
/// the plan no longer authors, which load nothing and are said.
// WL-SWITCH-08 -- agents/workshop/editor-switch.md
inline auto remember_choices(LoadPlan& plan, const std::vector<RememberedChoice>& remembered)
    -> std::vector<RememberedChoice> {
    std::vector<RememberedChoice> unauthored;
    for (const auto& [role, name] : remembered) {
        const ChoiceIntent* chosen = nullptr;
        for (const ChoiceIntent& c : plan.choices) {
            if (c.role == role && c.name == name) {
                chosen = &c;
            }
        }
        if (chosen == nullptr) {
            unauthored.emplace_back(role, name);
            continue;
        }
        bool loaded = false;
        for (const ArtifactIntent& row : plan.artifacts) {
            loaded = loaded || row.stem == chosen->stem;
        }
        for (ArtifactIntent& row : plan.artifacts) {
            if (!loaded && row.weave.has_value() && row.weave->role == role) {
                row.stem = chosen->stem;
                loaded = true;
            }
        }
    }
    return unauthored;
}

/// The choices' own law, over the whole plan. A choice not named in `artifacts` is the ordinary
/// case: it is realized only when a switch asks for it.
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

/// The alternatives' own law, over the whole plan: what a row may load in its place is an
/// artifact no other row or alternative names, into an office no other row may load into and no
/// choice is authored for, since whichever stands in the row is that row for the run.
inline Written check_alternatives(const LoadPlan& plan) {
    for (std::size_t i = 0; i < plan.artifacts.size(); ++i) {
        const ArtifactIntent& row = plan.artifacts[i];
        for (const AlternativeIntent& alt : row.otherwise) {
            for (std::size_t k = 0; k < plan.artifacts.size(); ++k) {
                const ArtifactIntent& other = plan.artifacts[k];
                if (other.stem == alt.stem) {
                    return Written::no("artifact `" + alt.stem + "` is a row of its own and is "
                                       "named to load in the place of `" + row.stem + "`");
                }
                if (k == i) {
                    continue;
                }
                if (other.weave.has_value() && other.weave->role == alt.weave.role) {
                    return Written::no("artifact `" + alt.stem + "`, named to load in the place "
                                       "of `" + row.stem + "`, would load into `" +
                                       alt.weave.role + "`, which `" + other.stem +
                                       "` loads into");
                }
                for (const AlternativeIntent& theirs : other.otherwise) {
                    if (theirs.stem == alt.stem) {
                        return Written::no("artifact `" + alt.stem + "` is named to load in the "
                                           "place of both `" + other.stem + "` and `" +
                                           row.stem + "`");
                    }
                    if (theirs.weave.role == alt.weave.role) {
                        return Written::no("`" + other.stem + "` and `" + row.stem +
                                           "` each name an artifact to load into `" +
                                           alt.weave.role + "` in their place");
                    }
                }
            }
            for (const ChoiceIntent& c : plan.choices) {
                if (c.stem == alt.stem || c.role == alt.weave.role ||
                    (row.weave.has_value() && c.role == row.weave->role)) {
                    return Written::no("artifact `" + row.stem + "` names `" + alt.stem +
                                       "` to load in its place, and `" + c.role +
                                       "` has choices: an office is switched between choices "
                                       "or has a row's alternatives, not both");
                }
            }
        }
    }
    return Written::ok();
}

/// Every law a whole plan meets. Duplicates are exact-stem, with no canonicalisation: a stem
/// carries no separator and resolves against one directory. An empty plan is legal; what a
/// project must contain is not the format's to decide.
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
    if (const Written alternatives = check_alternatives(plan); !alternatives.accepted) {
        return alternatives;
    }
    return check_choices(plan);
}

} // namespace zengine::workshop::load

#endif // ZENGINE_WORKSHOP_LOAD_PLAN_HPP
