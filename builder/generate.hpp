// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_GENERATE_HPP
#define ZENGINE_BUILDER_GENERATE_HPP

// TURNING AN AUTHORED RECIPE INTO THE ONE PROCESS THAT CARRIES IT OUT (BLD-1) --
// including, for a single-source recipe, WRITING THE TINY CMAKE PROJECT THAT MAKES
// ONE `.cpp` INTO A REAL ZENGINE ARTIFACT.
//
// ---- The claim this file exists to keep ------------------------------------------
//
//   ONE SOURCE FILE
//        v
//   a generated CMake project  <- this file, and it is small on purpose
//        v
//   the SUPPORTED Zengine package seam (find_package(zengine CONFIG))
//        v
//   ordinary CMake configure and build
//        v
//   a loadable artifact
//
// ZENGINE DOES NOT DRIVE A COMPILER. Nothing here names `g++`, `clang++` or `cl.exe`,
// chooses an ABI flag, discovers a link library, invents an output suffix, or knows
// what a Debug postfix is. Every one of those is CMake's, reached the way any external
// consumer reaches it -- which is what makes the artifact this produces the same kind
// of thing as `tests/package/`'s stranger, and what makes PKG-0's canary applicable to
// it (delete an installed header and this must go red).
//
// ---- Why the whole build is ONE process ------------------------------------------
//
// A single-source build is two CMake invocations: configure, then build. The obvious
// shapes for that are a two-step sequence in the runner (which turns the one process
// custodian into a small workflow engine) or a two-step state machine in the tool
// (which gives a semantic owner a cursor over somebody else's procedure). Both were
// refused, and the third option is the one this repository already uses twice:
//
//     the driver is a GENERATED CMAKE SCRIPT, run as `cmake -P`
//
// `tests/slow_build.cmake` and `tests/package/run.cmake` are the precedent, and the
// reason is theirs: a `-P` script needs no shell, no `/bin/sh`, no `.bat` and no
// assumption about what else is installed, on either platform this repository builds
// for. What it buys here is that ONE operation, ONE identity and ONE ending describe a
// whole build -- so nothing in the vocabulary, the runner or the tool has to learn what
// a step is.
//
// IT IS NOT AN ARBITRARY SCRIPT AND CANNOT BECOME ONE. No maker authors a line of it:
// every one is written here, from typed fields that have already been through
// `check_recipe`, and the only maker-supplied material in it is quoted strings that
// cannot contain a quote, a newline or a NUL (`check_recipe_path`). `$` and `\` are
// escaped on the way in, which is what keeps a Windows path a path and a dollar sign a
// dollar sign.
//
// ---- Which toolchain, and why it is a `load_cache` -------------------------------
//
// A generated project has to be configured with SOME generator and SOME compiler, and
// guessing is the one thing §5.3 of this phase forbids. So the recipe may name a
// CONFIGURED BUILD TREE and the generated driver borrows that tree's own answers with
// CMake's own `load_cache()` -- the generator, its platform, its toolset, its make
// program, its C++ compiler and its build type. No cache parser is written in C++, the
// policy is legible in a file a maker can open, and the three configurations this
// repository builds for are each borrowing what they already use:
//
//     WSL / GCC        Unix Makefiles, /usr/bin/c++
//     Windows MinGW    Ninja, .../mingw/bin/g++.exe
//     Windows MSVC     Ninja, .../Hostx64/x64/cl.exe
//
// MSVC IS THE ONE THAT NEEDS A WORD. A Ninja+`cl.exe` configuration needs the Visual
// Studio environment (INCLUDE, LIB) in the process that runs it. Zengine does not set
// it, invent it or look for it: the child inherits this process's environment
// (builder/run.hpp says so and says why), so a Workshop started from a developer
// prompt configures exactly as a `cmake` typed into that prompt would, and a Workshop
// started from somewhere else fails with the compiler's own words rather than with a
// guess.
//
// An empty `toolchain_from` is a real answer and not an omission: it means "let CMake
// choose for this machine", which is right where there is one compiler and is honest
// about being a default.

#include "builder/recipe.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace zengine::builder {

/// The file the generated project is written to, and the script that drives it. Named
/// once, because the runner starts one and a maker reads the other.
inline constexpr const char* kGeneratedProjectFile = "CMakeLists.txt";
inline constexpr const char* kGeneratedDriverFile = "zengine-build.cmake";
inline constexpr const char* kGeneratedBuildDirName = "build";

/// ONE AUTHORED STRING, AS A CMAKE QUOTED ARGUMENT'S CONTENTS.
///
/// Three characters carry meaning inside `"..."` in CMake source: `\` escapes,
/// `"` ends, and `$` begins a reference. The second cannot be here (`check_recipe_path`
/// refuses it at the door), and the other two are escaped -- which is what keeps
/// `C:\Users\Someone\My Weaves` a path rather than a string with two tab characters and
/// a missing directory.
inline std::string cmake_quoted(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (const char c : text) {
        if (c == '\\' || c == '"' || c == '$') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

/// THE GENERATED CMAKE PROJECT, as text.
///
/// IT IS RETURNED AS A STRING SO IT CAN BE ASSERTED ON. What a phase claims about a
/// generated project -- that it finds the package rather than a source tree, that it
/// links only what was authored, that it aims its output at one directory -- is a
/// claim about these bytes, and a suite that had to run a compiler to read them could
/// only ever check the claim on one platform at a time.
///
/// EVERY LINE HAS A REASON:
///
///   `cmake_minimum_required`  the same floor `tests/package/CMakeLists.txt` states.
///   `project(... CXX)`        a generated project still has to be a project.
///   `CMAKE_CXX_STANDARD 20`   Zengine's public headers are C++20 and a consumer has
///                             to say so; the package deliberately does not impose it.
///   `find_package(zengine)`   THE WHOLE PURITY CLAIM. One line, CONFIG mode, REQUIRED
///                             -- and it resolves the Loom too, because Zengine's own
///                             package config does that (PKG-0).
///   `add_library(... SHARED)` what `zengine_weave()` does in this tree, said by hand
///                             because this project is not in this tree.
///   `loom_weave_build_contract` NOT ceremony: it applies whatever this platform needs
///                             for a loadable image's statics to die with the image.
///                             It arrives with the Loom package, so its absence means
///                             the package resolution went somewhere unexpected -- and
///                             that is worth a sentence rather than a link error.
///   `PREFIX ""`, `OUTPUT_NAME` so the file is `<stem>.so` / `<stem>.dll` and not
///                             `lib<stem>.so`: the host spells a stem exactly one way.
///   the output directories    CMake owns where the file lands, including the import
///                             library a Windows link produces, and including the
///                             per-configuration variants a multi-config generator
///                             would otherwise append `/Debug` to.
inline std::string generated_project(const Recipe& r) {
    const SingleSourceRecipe& one = *r.single_source;
    std::ostringstream out;
    out << "# Generated by Zengine's Builder for recipe `" << r.id << "`. Do not edit:\n"
        << "# it is rewritten from the authored recipe every time this recipe is built.\n"
        << "#\n"
        << "# This is an ORDINARY EXTERNAL CONSUMER of the Zengine package. There is no\n"
        << "# include directory into a Zengine source tree, no path into a Zengine build\n"
        << "# tree, no named .so or .dll and no compile option -- if any of those ever\n"
        << "# became necessary here, the package would have stopped carrying its own\n"
        << "# requirements.\n"
        << "cmake_minimum_required(VERSION 3.16)\n"
        << "project(zengine-recipe-" << r.id << " LANGUAGES CXX)\n"
        << "\n"
        << "set(CMAKE_CXX_STANDARD 20)\n"
        << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        << "\n"
        << "find_package(zengine 0.1 CONFIG REQUIRED)\n"
        << "if(NOT COMMAND loom_weave_build_contract)\n"
        << "    message(FATAL_ERROR\n"
        << "        \"zengine build: the zengine package was found but the Loom's weave build \"\n"
        << "        \"contract is not defined. A loadable image needs it, and its absence \"\n"
        << "        \"means find_package(zengine) resolved somewhere unexpected.\")\n"
        << "endif()\n"
        << "\n"
        << "add_library(" << r.artifact << " SHARED \"" << cmake_quoted(one.source) << "\")\n"
        << "target_link_libraries(" << r.artifact << " PRIVATE\n";
    for (const std::string& link : one.links) {
        out << "    " << link << "\n";
    }
    out << ")\n"
        << "loom_weave_build_contract(" << r.artifact << ")\n"
        << "\n"
        << "# WHERE THE ARTIFACT LANDS IS SET HERE AND NOWHERE ELSE. Nothing stages, copies\n"
        << "# or renames it afterwards, and nothing in Zengine spells its suffix: CMake puts\n"
        << "# the file where this says, under the name this says, with the platform's own\n"
        << "# ending.\n"
        << "set(zengine_artifact_dir \"" << cmake_quoted(r.artifact_dir) << "\")\n"
        << "set_target_properties(" << r.artifact << " PROPERTIES\n"
        << "    PREFIX \"\"\n"
        << "    OUTPUT_NAME \"" << r.artifact << "\"\n"
        << "    LIBRARY_OUTPUT_DIRECTORY \"${zengine_artifact_dir}\"\n"
        << "    RUNTIME_OUTPUT_DIRECTORY \"${zengine_artifact_dir}\"\n"
        << "    ARCHIVE_OUTPUT_DIRECTORY \"${zengine_artifact_dir}\")\n"
        << "foreach(zengine_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)\n"
        << "    string(TOUPPER \"${zengine_cfg}\" zengine_CFG)\n"
        << "    set_target_properties(" << r.artifact << " PROPERTIES\n"
        << "        LIBRARY_OUTPUT_DIRECTORY_${zengine_CFG} \"${zengine_artifact_dir}\"\n"
        << "        RUNTIME_OUTPUT_DIRECTORY_${zengine_CFG} \"${zengine_artifact_dir}\"\n"
        << "        ARCHIVE_OUTPUT_DIRECTORY_${zengine_CFG} \"${zengine_artifact_dir}\")\n"
        << "endforeach()\n";
    return out.str();
}

/// THE GENERATED DRIVER, as text: configure, then build, in one process.
///
/// THE TWO FAILURES ARE TOLD APART BY THE THING THAT CAN TELL THEM APART. A configure
/// that fails and a compile that fails are different problems needing different next
/// actions, and this script is the only party that sees both exit codes -- so it says
/// which one happened, in the output a maker reads, rather than leaving one non-zero
/// status to mean either.
///
/// IT NAMES THE GENERATED PROJECT IN BOTH REFUSALS. A build that failed leaves its
/// project on disk, and a maker who wants to know why needs the path to it. Nothing
/// here writes to a temporary directory and nothing deletes anything.
inline std::string generated_driver(const Recipe& r) {
    const SingleSourceRecipe& one = *r.single_source;
    const std::string src = one.workspace;
    const std::string bin = one.workspace + "/" + kGeneratedBuildDirName;
    std::ostringstream out;
    out << "# Generated by Zengine's Builder for recipe `" << r.id << "`. Do not edit:\n"
        << "# it is rewritten from the authored recipe every time this recipe is built.\n"
        << "#\n"
        << "# Run as `cmake -P " << kGeneratedDriverFile << "`. It configures and builds the\n"
        << "# generated project beside it, in one process, and says which of the two failed.\n"
        << "set(zengine_src \"" << cmake_quoted(src) << "\")\n"
        << "set(zengine_bin \"" << cmake_quoted(bin) << "\")\n"
        // AN EMPTY LIST, NOT A LIST HOLDING ONE EMPTY STRING. `set(x "")` gives a
        // variable whose value is the empty string, and `list(APPEND)` onto that
        // produces a leading empty element -- which is an argument nobody meant.
        << "set(zengine_configure_args)\n";

    if (!one.packages.empty()) {
        // ONE `-D`, WITH THE LIST SEPARATOR ESCAPED. `CMAKE_PREFIX_PATH` is a CMake
        // list, so the separator has to survive being written into a script that is
        // itself CMake -- `\;` is the spelling that does.
        std::string joined;
        for (std::size_t i = 0; i < one.packages.size(); ++i) {
            if (i != 0) {
                joined += "\\;";
            }
            joined += cmake_quoted(one.packages[i]);
        }
        out << "list(APPEND zengine_configure_args \"-DCMAKE_PREFIX_PATH=" << joined << "\")\n";
    }

    out << "\n";
    if (one.toolchain_from.empty()) {
        out << "# NO TOOLCHAIN WAS BORROWED. This recipe names no configured build tree, so\n"
            << "# CMake chooses this machine's default generator and compiler. That is a\n"
            << "# default and is said to be one.\n"
            << "set(zengine_build_type \"\")\n";
    } else {
        out << "# THE TOOLCHAIN IS BORROWED FROM A CONFIGURED BUILD TREE, with CMake's own\n"
            << "# `load_cache`. Nothing here guesses a generator or looks for a compiler.\n"
            << "set(zengine_toolchain \"" << cmake_quoted(one.toolchain_from) << "\")\n"
            << "if(NOT EXISTS \"${zengine_toolchain}/CMakeCache.txt\")\n"
            << "    message(FATAL_ERROR\n"
            << "        \"zengine build: the configured CMake build tree this recipe borrows \"\n"
            << "        \"its toolchain from has no CMakeCache.txt: ${zengine_toolchain}\")\n"
            << "endif()\n"
            // ⚠ THE C COMPILER IS DELIBERATELY NOT BORROWED. The generated project
            // declares `LANGUAGES CXX`, so a `-DCMAKE_C_COMPILER=` would be a
            // manually-specified variable the project never reads -- and CMake says
            // so, in a warning, in the middle of a maker's build output. A borrowed
            // toolchain is the answers the generated project actually asks for.
            << "load_cache(\"${zengine_toolchain}\" READ_WITH_PREFIX borrowed_\n"
            << "           CMAKE_GENERATOR CMAKE_GENERATOR_PLATFORM CMAKE_GENERATOR_TOOLSET\n"
            << "           CMAKE_GENERATOR_INSTANCE CMAKE_MAKE_PROGRAM\n"
            << "           CMAKE_CXX_COMPILER CMAKE_BUILD_TYPE)\n"
            << "set(zengine_build_type \"${borrowed_CMAKE_BUILD_TYPE}\")\n"
            << "if(borrowed_CMAKE_GENERATOR)\n"
            << "    list(APPEND zengine_configure_args -G \"${borrowed_CMAKE_GENERATOR}\")\n"
            << "endif()\n"
            << "if(borrowed_CMAKE_GENERATOR_PLATFORM)\n"
            << "    list(APPEND zengine_configure_args -A \"${borrowed_CMAKE_GENERATOR_PLATFORM}\")\n"
            << "endif()\n"
            << "if(borrowed_CMAKE_GENERATOR_TOOLSET)\n"
            << "    list(APPEND zengine_configure_args -T \"${borrowed_CMAKE_GENERATOR_TOOLSET}\")\n"
            << "endif()\n"
            << "foreach(zengine_pair CMAKE_MAKE_PROGRAM CMAKE_CXX_COMPILER)\n"
            << "    if(borrowed_${zengine_pair})\n"
            << "        list(APPEND zengine_configure_args\n"
            << "             \"-D${zengine_pair}=${borrowed_${zengine_pair}}\")\n"
            << "    endif()\n"
            << "endforeach()\n";
    }

    out << "if(zengine_build_type)\n"
        << "    list(APPEND zengine_configure_args \"-DCMAKE_BUILD_TYPE=${zengine_build_type}\")\n"
        << "endif()\n"
        << "\n"
        << "message(STATUS \"zengine build: configuring ${zengine_src}\")\n"
        << "execute_process(\n"
        << "    COMMAND \"${CMAKE_COMMAND}\" -S \"${zengine_src}\" -B \"${zengine_bin}\"\n"
        << "            ${zengine_configure_args}\n"
        << "    RESULT_VARIABLE zengine_rc)\n"
        << "if(NOT zengine_rc EQUAL 0)\n"
        << "    message(FATAL_ERROR\n"
        << "        \"zengine build: CMake configure FAILED (exit ${zengine_rc}). The generated \"\n"
        << "        \"project is at ${zengine_src} and was not deleted.\")\n"
        << "endif()\n"
        << "\n"
        << "message(STATUS \"zengine build: compiling and linking " << r.artifact << "\")\n"
        << "set(zengine_build_args)\n"
        << "if(zengine_build_type)\n"
        << "    list(APPEND zengine_build_args --config \"${zengine_build_type}\")\n"
        << "endif()\n"
        << "execute_process(\n"
        << "    COMMAND \"${CMAKE_COMMAND}\" --build \"${zengine_bin}\" ${zengine_build_args}\n"
        << "    RESULT_VARIABLE zengine_rc)\n"
        << "if(NOT zengine_rc EQUAL 0)\n"
        << "    message(FATAL_ERROR\n"
        << "        \"zengine build: compile or link FAILED (exit ${zengine_rc}). The generated \"\n"
        << "        \"project is at ${zengine_src} and was not deleted.\")\n"
        << "endif()\n"
        << "message(STATUS \"zengine build: " << r.artifact << " built into "
        << cmake_quoted(r.artifact_dir) << "\")\n";
    return out.str();
}

namespace detail {

/// WRITE ONLY WHAT CHANGED. A generated file rewritten with identical bytes still moves
/// its modification time, and a moved `CMakeLists.txt` makes the next build reconfigure
/// -- so an untouched recipe would pay a configure on every press of Build. Comparing
/// first is what makes the second build of an unchanged recipe the incremental one it
/// should be.
inline bool write_if_different(const std::filesystem::path& file, const std::string& text,
                               std::string& trouble) {
    std::error_code ec;
    if (std::filesystem::exists(file, ec)) {
        std::ifstream in(file, std::ios::binary);
        if (in) {
            std::ostringstream held;
            held << in.rdbuf();
            if (held.str() == text) {
                return true;
            }
        }
    }
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        trouble = "could not write the generated file " + file.string();
        return false;
    }
    out << text;
    out.close();
    if (!out) {
        trouble = "could not finish writing the generated file " + file.string();
        return false;
    }
    return true;
}

} // namespace detail

/// PUT THE GENERATED PROJECT ON DISK. Empty means it is there.
///
/// The workspace is created if it is not there and is never removed: a generated
/// project that deleted itself would take the diagnostics with it exactly when a maker
/// needs to read them, and a maker who wants to see what Zengine wrote can open two
/// small files in a directory that is named in every refusal this build can produce.
inline std::string materialize(const Recipe& r) {
    if (!r.single_source.has_value()) {
        return std::string();
    }
    const std::filesystem::path workspace(r.single_source->workspace);
    std::error_code ec;
    std::filesystem::create_directories(workspace, ec);
    if (ec && !std::filesystem::is_directory(workspace)) {
        return "could not create the build workspace " + workspace.string() + ": " + ec.message();
    }
    std::string trouble;
    if (!detail::write_if_different(workspace / kGeneratedProjectFile, generated_project(r),
                                    trouble)) {
        return trouble;
    }
    if (!detail::write_if_different(workspace / kGeneratedDriverFile, generated_driver(r),
                                    trouble)) {
        return trouble;
    }
    return std::string();
}

/// WHAT ONE RECIPE IS, AS A PROCESS -- and whether it could be made into one at all.
struct PreparedBuild {
    bool ok = false;
    std::string trouble; ///< why not, when not
    BuildCommand command;
};

/// TURN AN AUTHORED RECIPE INTO THE ONE PROCESS THAT CARRIES IT OUT.
///
/// `cmake` IS THE HOST'S OWN CMAKE, BY ABSOLUTE PATH, AND NO RECIPE CAN NAME ONE. That
/// is BLD-0's rule kept exactly: resolving `cmake` from PATH at run time would let a
/// Workshop started from a different shell drive a different CMake against a tree it
/// did not configure, and letting a FILE name the program would turn every recipe
/// catalog into an arbitrary-execution document.
///
/// THE TWO PREFLIGHTS ARE DIAGNOSTICS AND NOT GATES. A missing build tree and a missing
/// source file are both discovered by CMake a moment later anyway; checking here buys a
/// sentence that names the recipe and the path, instead of a tool's report of somebody
/// else's error message. Nothing else is checked, because everything else is genuinely
/// only knowable by trying.
inline PreparedBuild prepare(const Recipe& r, const std::string& cmake) {
    PreparedBuild out;
    if (cmake.empty()) {
        out.trouble = "this Workshop was built without a CMake to build with";
        return out;
    }
    if (r.cmake_target.has_value()) {
        const CMakeTargetRecipe& t = *r.cmake_target;
        std::error_code ec;
        if (!std::filesystem::exists(std::filesystem::path(t.build_dir) / "CMakeCache.txt", ec)) {
            out.trouble = "the configured CMake build tree `" + t.build_dir +
                          "` has no CMakeCache.txt: recipe `" + r.id +
                          "` builds a target in a tree somebody else configured, and that "
                          "tree is not there";
            return out;
        }
        out.command.program = cmake;
        out.command.args = {"--build", t.build_dir, "--target", t.target};
        if (!t.config.empty()) {
            out.command.args.push_back("--config");
            out.command.args.push_back(t.config);
        }
        out.command.dir = t.build_dir;
        out.ok = true;
        return out;
    }
    if (!r.single_source.has_value()) {
        out.trouble = "recipe `" + r.id + "` names no build mechanism";
        return out;
    }
    const SingleSourceRecipe& one = *r.single_source;
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::path(one.source), ec)) {
        out.trouble = "the source file `" + one.source + "` recipe `" + r.id +
                      "` builds is not there";
        return out;
    }
    const std::string wrote = materialize(r);
    if (!wrote.empty()) {
        out.trouble = wrote;
        return out;
    }
    out.command.program = cmake;
    out.command.args = {"-P", one.workspace + "/" + kGeneratedDriverFile};
    out.command.dir = one.workspace;
    out.ok = true;
    return out;
}

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_GENERATE_HPP
