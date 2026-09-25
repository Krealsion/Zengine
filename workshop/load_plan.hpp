// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LOAD_PLAN_HPP
#define ZENGINE_WORKSHOP_LOAD_PLAN_HPP

// Which artifacts participate in this project, and how: the typed load plan and its law
// (docs/reference/load-plan.md, agents/realization.md).

#include "property.hpp" // `Written` -- a refusal carries its reason

#include "operator/catalog.hpp" // `op::MountMode` -- one spelling of overlay

#include <cstddef>
#include <optional>
#include <string>
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

/// How long a choice's name may be: a word a maker types.
inline constexpr std::size_t kMaxChoiceNameLen = 32;

/// How long a weave role may be: `kMaxPaneKeyLen`'s number, for a routing name. Not cut to the
/// roles this build knows: a plan may name a role this build has never heard of.
inline constexpr std::size_t kMaxWeaveRoleLen = 64;

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

/// One authored alternative for an office: this artifact may hold `role`, under the name a maker
/// switches to it by. It loads nothing at startup; a switch loads it by name, so what a switch
/// loads is always one the project named. The name is a maker's word, not a stem.
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
    /// May this project stand without this row (format version 3)? Authored, because only the
    /// plan knows essential startup from a recoverable tool: a refused optional row is recorded
    /// by name and stepped over, and the rows behind it still run in authored order. It is not
    /// "skip what fails", and it cannot rescue a row that something behind it needs.
    bool optional = false;

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
    return Written::ok();
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
    return check_choices(plan);
}

} // namespace zengine::workshop::load

#endif // ZENGINE_WORKSHOP_LOAD_PLAN_HPP
