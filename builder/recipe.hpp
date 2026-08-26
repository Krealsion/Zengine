// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_RECIPE_HPP
#define ZENGINE_BUILDER_RECIPE_HPP

// WHAT A BUILD RECIPE IS: authored knowledge of HOW ONE ARTIFACT CAN BE PRODUCED
// (BLD-1).
//
// ---- The law -------------------------------------------------------------------
//
//   A recipe has an IDENTITY, names the ARTIFACT it is expected to produce, and
//   names ONE build mechanism with the inputs that mechanism needs.
//
//   A recipe is BUILD PROCEDURE. It is never runtime participation.
//
//   The program is always CMake, and it is the host's CMake. A recipe cannot
//   name one.
//
// ---- Why this is not the same thing as a load plan row -------------------------
//
// `workshop/load_plan.hpp` says WHICH ARTIFACTS PARTICIPATE IN THIS PROJECT AND HOW;
// this file says HOW AN ARTIFACT CAN BE MADE. They are two truths about one noun and
// they have different lifetimes, different authorities and different readers: a
// project can participate in artifacts nobody here can build (every artifact shipped
// in the package is one), and a recipe can produce an artifact no project runs.
//
// THE CROSS-REFERENCE IS THE STEM AND THERE IS NO SECOND EDGE. A recipe says which
// artifact it produces; a plan row IS an artifact. So "can this project produce
// `zengine-oven`?" and "does this project run `zengine-oven`?" are answered by
// comparing one name, and neither file carries a copy of the other's fields. A
// `recipe:` field on a plan row would be a second edge that could disagree with the
// first, and duplicating a role, a mode or a load order into a recipe would be the
// build system acquiring authority over runtime intent -- which is the one thing
// BLD-1 must not do.
//
// ---- Two recipe kinds, and no third --------------------------------------------
//
//   CMakeTarget    an artifact a CMake project already owns. The recipe names the
//                  CONFIGURED build tree and the target, and the action is the very
//                  command a maker would type:
//
//                      cmake --build <build tree> --target <target>
//
//   SingleSource   ONE C++ source file and nothing else a maker has to write.
//                  Zengine generates a tiny CMake project around it (generate.hpp)
//                  and CMake compiles and links it. Zengine does not.
//
// THERE IS NO `command` KIND AND THERE MUST NOT BE. A recipe carrying a program and
// an argument vector would make every recipe FILE an arbitrary-execution document --
// which is a different authority from the one this package has always had, where the
// catalog of runnable things is written by the party that composed the process. Both
// kinds above name INPUTS to a mechanism this package already holds; neither can name
// what runs.
//
// ---- What a refusal is, here ----------------------------------------------------
//
// A `std::string`: empty means accepted, and anything else is the sentence a reader
// is shown. This package deliberately does not reach for `workshop::Written`, which
// is the shape the maker-facing files use: `builder/` is BELOW `workshop/` in this
// tree's dependency order (the Workshop vocabulary links the Builder vocabulary and
// not the other way round), so borrowing that type would invert the edge to save one
// bool. The one place the two meet is `workshop/recipe_persist.hpp`, which wraps
// these sentences in `Written` at the file boundary.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace zengine::builder {

// ---- What a recipe may hold -----------------------------------------------------

/// How many recipes one catalog may hold.
///
/// `kMaxPlanArtifacts`' argument at a smaller number: a project that grew a buildable
/// artifact a quarter for a decade would not reach this, and what it bounds is a
/// forged file -- a recipe catalog is read before anything is built, and a hostile one
/// does not get to choose the cost of refusing it.
inline constexpr std::size_t kMaxRecipes = 32;

/// How long a recipe identity may be. A routing-name bound for a routing-shaped name.
inline constexpr std::size_t kMaxRecipeIdLen = 64;

/// How long any path a recipe names may be.
///
/// Long enough for a deep checkout on either platform and short enough that a whole
/// catalog is still a small file. It is a bound on ONE authored string, not on the
/// operating system's own limit, which this package has no business restating.
inline constexpr std::size_t kMaxRecipePathLen = 1024;

/// How many CMake packages a single-source recipe may be pointed at, and how many
/// exported targets it may link. Two small numbers because both lists are hand-written
/// by a maker describing ONE source file's public dependencies; a source file needing
/// more than this is a project, and a project has a CMakeLists.
inline constexpr std::size_t kMaxRecipePackages = 8;
inline constexpr std::size_t kMaxRecipeLinks = 16;

/// How long one link target name may be (`zengine::operator-consumer` is twenty-eight).
inline constexpr std::size_t kMaxLinkTargetLen = 64;

// ---- The command a recipe becomes ------------------------------------------------

/// ONE PROCESS, AS THE RUNNER WILL START IT.
///
/// A PROGRAM AND AN ARGUMENT VECTOR, NEVER A COMMAND LINE. There is no field here
/// that takes a shell line and nothing composes one, so the whole family of "a quote
/// in the wrong place became an extra command" cannot occur -- not because the
/// arguments are checked, but because there is no shell in the picture to check them
/// for. That was BLD-0's decision and BLD-1 does not widen it: every command this
/// package can produce has `program` set to the host's own CMake.
///
/// IT USED TO BE CALLED `BuildRecipe` AND IT CARRIED A `target`. Both were true when a
/// recipe WAS a command; neither is now. A command is what one process will be, a
/// recipe is authored knowledge about an artifact, and one of them is derived from the
/// other -- so the derived thing gets the name that says so, and the name that promises
/// authored knowledge is spent on the thing that carries it (FRIC-1: a name that
/// promises the wrong contract cannot be bought back with documentation).
struct BuildCommand {
    std::string program;           ///< an executable, chosen by this package and never authored
    std::vector<std::string> args; ///< its arguments, already separated
    std::string dir;               ///< the working directory to run it in ("" = inherit)

    /// The command as one readable line, for a maker who wants to know what a button
    /// actually did. Deliberately not a re-runnable command line: it is a description,
    /// and nothing parses it back.
    std::string as_line() const {
        std::string line = program;
        for (const std::string& a : args) {
            line += ' ';
            line += a;
        }
        return line;
    }
};

// ---- The two authored kinds -------------------------------------------------------

/// AN ARTIFACT A CMAKE PROJECT ALREADY OWNS.
///
/// IT NAMES A CONFIGURED BUILD TREE AND NOT A SOURCE TREE, and that is the whole of
/// why this kind carries no generator, no compiler, no toolchain file and no cache
/// arguments: the project it builds has already been configured by whoever owns it,
/// with whatever policy they chose, and a Builder that re-configured somebody else's
/// tree would be a Builder deciding a policy that is not its to decide.
///
/// `config` is for a MULTI-CONFIG generator and is empty everywhere else. It is
/// passed straight to `cmake --build --config`, which single-config generators accept
/// and ignore, so one recipe is legal against either kind of tree.
struct CMakeTargetRecipe {
    std::string build_dir; ///< a CONFIGURED CMake build tree
    std::string target;    ///< the target in it that produces this recipe's artifact
    std::string config;    ///< a multi-config generator's configuration, or empty

    friend bool operator==(const CMakeTargetRecipe&, const CMakeTargetRecipe&) = default;
};

/// ONE C++ SOURCE FILE, AND THE PROJECT ZENGINE WRITES AROUND IT.
///
/// THE MAKER AUTHORS A SOURCE FILE AND ITS PUBLIC DEPENDENCIES. Nothing else: no
/// CMakeLists, no compiler, no flags, no library paths, no output naming and no
/// platform suffix. What Zengine adds is a tiny generated CMake project
/// (`builder/generate.hpp`) and what CMake adds is everything about compiling and
/// linking -- which is the point, and the boundary this kind exists to hold.
///
/// `packages` IS `CMAKE_PREFIX_PATH` AND IT IS WHY THIS IS AN EXTERNAL CONSUMER. The
/// generated project says `find_package(zengine CONFIG REQUIRED)` and nothing else; if
/// the prefix named here does not carry a Zengine package the configure fails, and it
/// must -- a fallback that reached into a source tree would make every green here
/// meaningless (PKG-0's canary is exactly that experiment).
///
/// `links` IS A LIST OF EXPORTED TARGET NAMES and never a link line. `zengine::timer`,
/// `loom::switchboard`: names CMake resolves, refuses when unknown, and expands into
/// whatever include directories, definitions and libraries the package says they carry.
/// A raw `-l` or a path to a `.so` is not spellable here.
///
/// `toolchain_from` IS A CONFIGURED BUILD TREE WHOSE TOOLCHAIN THIS BORROWS, and it is
/// how this recipe answers "which compiler" without guessing. See `builder/generate.hpp`,
/// where the borrowing is one `load_cache()` in a generated script rather than a cache
/// parser written in C++. Empty means "let CMake choose for this machine", which is
/// right on a host with one compiler and honest about being a default rather than a
/// decision.
///
/// `workspace` IS WHERE THE GENERATED PROJECT LIVES, and empty means the host picks one
/// beside its own artifacts. It is a durable directory and never a temporary: a
/// generated project that deleted itself would take the diagnostics with it exactly
/// when a maker needs to read them.
struct SingleSourceRecipe {
    std::string source;                    ///< the one .cpp a maker wrote
    std::vector<std::string> packages;     ///< CMAKE_PREFIX_PATH entries
    std::vector<std::string> links;        ///< exported CMake target names to link
    std::string toolchain_from;            ///< a configured build tree to borrow a toolchain from
    std::string workspace;                 ///< where to generate the project ("" = host's choice)

    friend bool operator==(const SingleSourceRecipe&, const SingleSourceRecipe&) = default;
};

/// ONE AUTHORED BUILD RECIPE.
///
/// EXACTLY ONE KIND. A recipe with neither mechanism describes nothing and a recipe
/// with both describes two builds under one name; both are refused rather than
/// resolved by precedence, because a precedence rule is a thing a maker has to
/// remember and a refusal is a thing they are told.
///
/// `artifact` IS A STEM, spelled to a file by the HOST's one rule, exactly as a load
/// plan's stem is. That is not a coincidence and it is not reuse for its own sake: it
/// is what makes the cross-reference between a recipe and a plan row an exact string
/// comparison, and what keeps `.so`, `.dll`, a Debug postfix and an import library out
/// of every file a person edits.
///
/// `artifact_dir` IS WHERE THE BUILT FILE LANDS, and empty means the host's own
/// artifact directory -- which for a single-source recipe is where CMake is told to
/// put it, and for a CMake-target recipe is where that project already puts it. It is
/// a DIRECTORY and never a file: the file's name is the stem and its suffix is the
/// platform's, and neither is a maker's to spell.
struct Recipe {
    std::string id;           ///< what a maker and the tool call this recipe
    std::string artifact;     ///< the artifact STEM this recipe is expected to produce
    std::string artifact_dir; ///< where that artifact lands ("" = the host's artifact directory)
    std::optional<CMakeTargetRecipe> cmake_target;
    std::optional<SingleSourceRecipe> single_source;

    friend bool operator==(const Recipe&, const Recipe&) = default;
};

/// WHAT THE BUILDER TOOL IS TOLD ABOUT A RECIPE -- and it is deliberately less.
///
/// The tool holds an identity, the artifact that identity is about, and the one file
/// that artifact means. It holds NO build tree, NO source path, NO package prefix, NO
/// link list and NO command, because none of those is a question the tool can be asked:
/// its subjects are "which recipe is this" and "did the artifact appear". The runner
/// holds the rest, which is the same split BLD-0 drew between a NAME and a COMMAND,
/// drawn one level further out now that a name has an artifact behind it.
///
/// `path` IS RESOLVED BY THE HOST, once, from `artifact_dir` and the host's own rule
/// for spelling a stem as a file. Deriving it here would put a platform suffix in a
/// package that has no business knowing one.
struct RecipeView {
    std::string id;
    std::string artifact;
    std::string path; ///< the exact file this recipe is expected to produce

    friend bool operator==(const RecipeView&, const RecipeView&) = default;
};

// ---- The recipe's own law ----------------------------------------------------------

/// What this application accepts as a recipe identity.
///
/// A NAME, NOT A PATH AND NOT PROSE. It is what a maker types, what a message carries
/// and what a refusal quotes back, so it must be short, printable, and free of the two
/// characters that would let it be mistaken for a location.
inline std::string check_recipe_id(const std::string& id) {
    if (id.empty()) {
        return "a recipe needs a name";
    }
    if (id.size() > kMaxRecipeIdLen) {
        return "a recipe name is at most " + std::to_string(kMaxRecipeIdLen) + " bytes";
    }
    for (const char c : id) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte <= ' ' || byte == 0x7Fu) {
            return "recipe name `" + id + "` cannot contain spaces or control characters";
        }
        if (c == '/' || c == '\\') {
            return "recipe name `" + id + "` cannot contain a path separator: it is a name, not "
                                          "a location";
        }
    }
    return std::string();
}

/// What this application accepts as an artifact stem in a recipe.
///
/// THE SAME FIVE RULES A LOAD PLAN APPLIES, and they are restated here rather than
/// shared because the two files are below and above each other in this tree and the
/// rule belongs to both. It is checked in BOTH places on purpose: a stem is a FILE
/// THIS HOST WILL EXECUTE, and a rule enforced in one document and trusted in the
/// other is a rule with a door in it.
inline std::string check_recipe_artifact(const std::string& stem) {
    if (stem.empty()) {
        return "a recipe must say which artifact it produces";
    }
    if (stem.size() > kMaxRecipeIdLen) {
        return "an artifact stem is at most " + std::to_string(kMaxRecipeIdLen) + " bytes";
    }
    for (const char c : stem) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte <= ' ' || byte == 0x7Fu) {
            return "artifact stem `" + stem + "` cannot contain spaces or control characters";
        }
        if (c == '/' || c == '\\') {
            return "artifact stem `" + stem +
                   "` cannot contain a path separator: a stem names a file, and where that "
                   "file lands is the recipe's `artifact_dir`";
        }
    }
    if (stem.find("..") != std::string::npos) {
        return "artifact stem `" + stem + "` cannot contain `..`";
    }
    return std::string();
}

/// What this application accepts as a path a recipe names.
///
/// SPACES ARE LEGAL AND CONTROL CHARACTERS ARE NOT, and the asymmetry is the honest
/// one: a maker's checkout genuinely lives under `C:\Users\...\My Documents\...` on one
/// of the two platforms this repository builds for, and every place a path is spent
/// here is either one element of an argument vector or one quoted CMake string. What
/// cannot be made safe is a newline, a NUL or a quote -- the first two end a line in a
/// generated script and the third ends a string in one -- so those are refused at the
/// door rather than escaped into something a reader has to trust.
inline std::string check_recipe_path(const std::string& what, const std::string& path) {
    if (path.empty()) {
        return what + " cannot be empty";
    }
    if (path.size() > kMaxRecipePathLen) {
        return what + " is at most " + std::to_string(kMaxRecipePathLen) + " bytes";
    }
    for (const char c : path) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < ' ' || byte == 0x7Fu) {
            return what + " cannot contain control characters";
        }
        if (c == '"') {
            return what + " cannot contain a double quote";
        }
    }
    return std::string();
}

/// What this application accepts as the name of an exported CMake target to link.
///
/// A TARGET NAME, WHICH IS NOT A LINK FLAG. CMake target names are identifiers with
/// `:`, `-`, `_` and `.` in them; a `-l`, a path, a `$`, a `)` or a `;` is either a
/// flag, a location or a way out of the argument it is written in, and none of those
/// is a thing this file will carry into a generated project.
inline std::string check_link_target(const std::string& target) {
    if (target.empty()) {
        return "a link target cannot be empty";
    }
    if (target.size() > kMaxLinkTargetLen) {
        return "a link target is at most " + std::to_string(kMaxLinkTargetLen) + " bytes";
    }
    // A LEADING `-` IS A FLAG AND NOT A NAME, and it has to be refused separately
    // because `-` is legal INSIDE one (`zengine::operator-consumer` is a shipped
    // target). Without this line `-lpthread` is a perfectly well-formed identifier
    // by every other rule here, which is exactly the shape this check exists to keep
    // out of a generated link line.
    if (target.front() == '-') {
        return "link target `" + target +
               "` starts with `-`: a target is linked by NAME, and a linker flag is not one";
    }
    for (const char c : target) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == ':' || c == '-' || c == '_' || c == '.' ||
                        c == '+';
        if (!ok) {
            return "link target `" + target +
                   "` is not a CMake target name: a target is linked by NAME, and a flag, a "
                   "path or a library file is not one";
        }
    }
    return std::string();
}

/// EVERY LAW ONE RECIPE MEETS, minus the one that is about the whole catalog.
inline std::string check_recipe(const Recipe& r) {
    const std::string id = check_recipe_id(r.id);
    if (!id.empty()) {
        return id;
    }
    const std::string stem = check_recipe_artifact(r.artifact);
    if (!stem.empty()) {
        return "recipe `" + r.id + "`: " + stem;
    }
    if (!r.artifact_dir.empty()) {
        const std::string dir = check_recipe_path("an artifact directory", r.artifact_dir);
        if (!dir.empty()) {
            return "recipe `" + r.id + "`: " + dir;
        }
    }
    const int kinds = (r.cmake_target.has_value() ? 1 : 0) + (r.single_source.has_value() ? 1 : 0);
    if (kinds == 0) {
        return "recipe `" + r.id + "` names no build mechanism";
    }
    if (kinds > 1) {
        return "recipe `" + r.id +
               "` names two build mechanisms: one recipe is one procedure, and which of two "
               "would run is not a question a maker should have to answer from a precedence "
               "rule";
    }
    if (r.cmake_target.has_value()) {
        const std::string dir = check_recipe_path("a CMake build tree", r.cmake_target->build_dir);
        if (!dir.empty()) {
            return "recipe `" + r.id + "`: " + dir;
        }
        if (r.cmake_target->target.empty()) {
            return "recipe `" + r.id + "` names a CMake build tree and no target in it";
        }
        const std::string target = check_link_target(r.cmake_target->target);
        if (!target.empty()) {
            return "recipe `" + r.id + "`: `" + r.cmake_target->target +
                   "` is not a CMake target name";
        }
        if (!r.cmake_target->config.empty()) {
            const std::string cfg = check_link_target(r.cmake_target->config);
            if (!cfg.empty()) {
                return "recipe `" + r.id + "`: `" + r.cmake_target->config +
                       "` is not a CMake configuration name";
            }
        }
        return std::string();
    }
    const SingleSourceRecipe& one = *r.single_source;
    const std::string source = check_recipe_path("a source file", one.source);
    if (!source.empty()) {
        return "recipe `" + r.id + "`: " + source;
    }
    if (one.packages.size() > kMaxRecipePackages) {
        return "recipe `" + r.id + "` names more than " + std::to_string(kMaxRecipePackages) +
               " package prefixes";
    }
    for (const std::string& p : one.packages) {
        const std::string bad = check_recipe_path("a package prefix", p);
        if (!bad.empty()) {
            return "recipe `" + r.id + "`: " + bad;
        }
    }
    if (one.links.empty()) {
        return "recipe `" + r.id +
               "` links nothing: a loadable Zengine artifact needs at least the Loom targets "
               "its own headers name, and a generated project that linked nothing would fail "
               "later and less clearly";
    }
    if (one.links.size() > kMaxRecipeLinks) {
        return "recipe `" + r.id + "` links more than " + std::to_string(kMaxRecipeLinks) +
               " targets";
    }
    for (const std::string& l : one.links) {
        const std::string bad = check_link_target(l);
        if (!bad.empty()) {
            return "recipe `" + r.id + "`: " + bad;
        }
    }
    if (!one.toolchain_from.empty()) {
        const std::string bad = check_recipe_path("a toolchain build tree", one.toolchain_from);
        if (!bad.empty()) {
            return "recipe `" + r.id + "`: " + bad;
        }
    }
    if (!one.workspace.empty()) {
        const std::string bad = check_recipe_path("a build workspace", one.workspace);
        if (!bad.empty()) {
            return "recipe `" + r.id + "`: " + bad;
        }
    }
    return std::string();
}

/// EVERY LAW A WHOLE CATALOG MEETS.
///
/// THE DUPLICATE RULE IS ON THE IDENTITY AND NOT ON THE ARTIFACT, and the asymmetry is
/// deliberate. Two recipes named the same thing cannot be told apart by anything that
/// asks for one, so the second is refused. Two recipes producing the SAME artifact by
/// different procedures is an ordinary thing a project may want -- the same weave built
/// against two package prefixes, say -- and nothing here has to choose between them,
/// because what asks for a build asks for a RECIPE.
///
/// AN EMPTY CATALOG IS LEGAL, for `check_plan`'s reason: a project with nothing to
/// build is a project, and refusing here would be this file deciding what a project
/// must contain.
inline std::string check_recipes(const std::vector<Recipe>& recipes) {
    if (recipes.size() > kMaxRecipes) {
        return "a build recipe catalog names at most " + std::to_string(kMaxRecipes) + " recipes";
    }
    for (std::size_t i = 0; i < recipes.size(); ++i) {
        const std::string row = check_recipe(recipes[i]);
        if (!row.empty()) {
            return row;
        }
        for (std::size_t k = 0; k < i; ++k) {
            if (recipes[k].id == recipes[i].id) {
                return "recipe `" + recipes[i].id +
                       "` is declared twice: a recipe name is how a maker asks for one";
            }
        }
    }
    return std::string();
}

// ---- Finding one ------------------------------------------------------------------

/// The recipe with this name, or null. Written once because three readers need it.
inline const Recipe* recipe_named(const std::vector<Recipe>& recipes, const std::string& id) {
    for (const Recipe& r : recipes) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}

/// The tool's view of the recipe with this name, or null.
inline const RecipeView* view_named(const std::vector<RecipeView>& views, const std::string& id) {
    for (const RecipeView& v : views) {
        if (v.id == id) {
            return &v;
        }
    }
    return nullptr;
}

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_RECIPE_HPP
