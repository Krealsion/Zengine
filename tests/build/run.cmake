# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE SINGLE-SOURCE BUILD WITNESS DRIVER (BLD-1).
#
#   cmake -DZEN_BUILD_DIR=<a configured, built Zengine build tree> \
#         -DZEN_WORK=<scratch dir outside both repositories> \
#         [-DZEN_CONFIG=Debug] [-DZEN_CMAKE_ARGS=...] \
#         -P tests/build/run.cmake
#
# It installs Zengine into an isolated prefix, writes a ONE-FILE Zengine weave outside this
# repository, authors a build recipe and a load plan for it, and drives the real Builder --
# through `tests/build/witness.cpp`, which is Workshop's Builder wiring with the picture
# removed -- to build it and hand it to the running host's realization owner.
#
# WHY IT IS NOT A CTEST ENTRY: `tests/package/run.cmake`'s reason exactly. A nested CMake
# CONFIGURE inside a ctest entry puts a second build system into a test binary's population.
# It is one command a human runs and the same one CI runs.
#
# WHAT IT ASKS, in order:
#   1. does a maker's ONE .cpp become a real loadable artifact, with no CMakeLists of theirs
#   2. does the generated project consume the PACKAGE (and does it stop working when the
#      package stops carrying what it needs -- the canary that makes 1 mean something)
#   3. does a compile error produce real compiler diagnostics and NEVER claim an artifact
#   4. does a build whose expected artifact is absent refuse to be called a success
#   5. does a source path with a SPACE in it work
#   6. does changing the source converge the artifact to the new source
#   7. does a successful, eligible build enter the ALREADY RUNNING host's realization owner
#   8. is an already-loaded artifact refused rather than silently reloaded
#   9. does a FAILED build send no load request at all

foreach(v ZEN_BUILD_DIR ZEN_WORK)
    if(NOT DEFINED ${v})
        message(FATAL_ERROR "build witness: -D${v}=... is required")
    endif()
endforeach()
if(NOT DEFINED ZEN_CONFIG OR ZEN_CONFIG STREQUAL "")
    set(ZEN_CONFIG Debug)
endif()

get_filename_component(here "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(repo "${here}/../.." ABSOLUTE)
get_filename_component(build_dir "${ZEN_BUILD_DIR}" ABSOLUTE)
get_filename_component(work "${ZEN_WORK}" ABSOLUTE)

# The maker's source has to live outside the source tree, or "an external consumer" is a
# claim rather than a fact about where the build happened.
string(FIND "${work}" "${repo}/" work_inside)
if(work_inside EQUAL 0)
    message(FATAL_ERROR
        "build witness: ZEN_WORK is inside the Zengine repository (${work}). A maker's weave "
        "must be built outside it, or 'it consumes the package' is unproven.")
endif()

if(WIN32)
    set(artifact_suffix ".dll")
else()
    set(artifact_suffix ".so")
endif()

# ---- Where the witness host is, and the Loom this build consumed -----------------------
find_program(ZEN_WITNESS zengine-build-witness
             PATHS "${build_dir}/tests" "${build_dir}/tests/${ZEN_CONFIG}" NO_DEFAULT_PATH)
if(NOT ZEN_WITNESS)
    message(FATAL_ERROR
        "build witness: zengine-build-witness was not built in ${build_dir}/tests. Build the "
        "tree first (and with BUILD_TESTING on).")
endif()

file(READ "${build_dir}/CMakeCache.txt" cache)
string(REGEX MATCH "CMAKE_PREFIX_PATH:[A-Z]+=([^\n]*)" _m "${cache}")
set(loom_prefix "${CMAKE_MATCH_1}")
if(loom_prefix STREQUAL "")
    message(FATAL_ERROR
        "build witness: ${build_dir}/CMakeCache.txt has no CMAKE_PREFIX_PATH, so the Loom this "
        "package depends on cannot be located for the generated project.")
endif()

# ---- 1. install Zengine into an isolated prefix -----------------------------------------
set(prefix "${work}/prefix")
file(REMOVE_RECURSE "${prefix}")
execute_process(COMMAND ${CMAKE_COMMAND} --install "${build_dir}" --config "${ZEN_CONFIG}"
                        --prefix "${prefix}"
                RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "build witness: installing into ${prefix} FAILED (exit ${rc})")
endif()
if(NOT EXISTS "${prefix}/lib/cmake/zengine/zengineConfig.cmake")
    message(FATAL_ERROR
        "build witness: no zengineConfig.cmake in ${prefix}. The build tree was configured "
        "with ZENGINE_INSTALL=OFF, or the install rules did not run.")
endif()
message(STATUS "build witness: installed an isolated prefix ok")

# ---- The maker's material, written OUTSIDE both repositories ----------------------------
#
# A SPACE IN THE DIRECTORY NAME, deliberately and from the first case rather than as an
# afterthought: a maker's checkout genuinely lives under `My Documents` on one of the two
# platforms this repository builds for, and a route that only works without one is a route
# that works for the founder.
set(sources "${work}/My Weaves")
set(house "${work}/house")          # where the running host resolves an artifact from
set(space "${work}/workspace")      # where Zengine generates the project
file(REMOVE_RECURSE "${sources}" "${house}" "${space}")
file(MAKE_DIRECTORY "${sources}" "${house}" "${space}")

# ---- EVERY PATH THAT GOES INTO A FILE IS SPELLED WITH FORWARD SLASHES -------------------
#
# A recipe catalog and a load plan are JSON, and `\` is an ESCAPE in JSON: a Windows path
# written into one verbatim is a parse error at best and a different path at worst. Windows
# accepts forward slashes everywhere, so the whole class goes away by normalising once, here,
# rather than by escaping at each of the six places a path is written.
#
# ⚠ IT IS A FACT ABOUT THE FILE FORMAT AND NOT ABOUT ZENGINE. The recipe LAW accepts a
# backslash in a path quite happily -- a maker who writes one in a hand-authored catalog gets
# a working recipe -- and what cannot survive it is the JSON they would have to write it in.
foreach(p sources house space prefix loom_prefix build_dir)
    file(TO_CMAKE_PATH "${${p}}" ${p})
endforeach()

# THE WHOLE OF WHAT A MAKER WRITES. One file, no CMakeLists, no build script, and the only
# Zengine headers in it are ones the package publishes.
function(zen_write_oven mark broken)
    set(body
"// A one-file Zengine weave, written by a maker who has never seen this repository.
// It names installed headers and nothing else -- no source tree, no build tree, no
// path into either, and no CMakeLists of its own.
//
// It consumes BOTH halves of the package on purpose: the Loom underneath (a weave IS a
// Loom participant) and Zengine's own Timer vocabulary, spelled the way the
// documentation spells it. Consuming only the Loom would leave 'the ZENGINE package
// was consumed' untested, and the canaries below could not fire.
#include \"timer/vocabulary.hpp\"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace {

struct OvenState {
    std::int64_t baked = 0;
    std::string mark;
    ZEN_EXPOSE();
    ZEN_SHAPE(OvenState, 1, ZEN_FIELD(baked), ZEN_FIELD(mark));
};

class Oven : public loom::WeaveBase<Oven, OvenState,
                                    loom::Accept<zengine::timer::TimerFired>, loom::Emit<>> {
public:
    Oven() { state_.mark = \"${mark}\"; }
    void on(const zengine::timer::TimerFired&, loom::Mail&) { ++state_.baked; }
};

} // namespace
")
    if(broken)
        # A REAL COMPILE ERROR, of the ordinary kind: a name that does not exist. The
        # compiler's own words are what a maker has to be shown.
        string(APPEND body
"int zengine_oven_broken() { return this_symbol_does_not_exist_anywhere; }\n")
    endif()
    string(APPEND body "ZEN_EXPORT_WEAVE(Oven)\n")
    file(WRITE "${sources}/my oven.cpp" "${body}")
endfunction()

zen_write_oven("first" OFF)

# ---- ...and the two authored FILES a project carries ------------------------------------
#
# Both are written here as a maker would write them: a recipe saying HOW the artifact is
# produced, and a plan row saying HOW IT PARTICIPATES. Neither carries a field of the
# other's, and the only thing joining them is the artifact stem.
function(zen_write_recipes prefixes workspace)
    file(WRITE "${work}/build-recipes.json"
"{
  \"zen\": 1,
  \"schema\": \"WorkshopRecipeFile\",
  \"version\": 1,
  \"fields\": {
    \"format\": \"zengine-build-recipes\",
    \"format_version\": \"1\",
    \"recipes\": [
      {
        \"recipe\": \"oven\",
        \"artifact\": \"zengine-oven\",
        \"artifact_dir\": \"\",
        \"cmake_target\": [],
        \"single_source\": [
          {
            \"source\": \"${sources}/my oven.cpp\",
            \"packages\": [ ${prefixes} ],
            \"links\": [ \"zengine::timer\", \"loom::switchboard\" ],
            \"toolchain_from\": \"${build_dir}\",
            \"workspace\": \"${workspace}\"
          }
        ]
      }
    ]
  }
}
")
endfunction()

zen_write_recipes("\"${prefix}\", \"${loom_prefix}\"" "${space}")

# ---- THE HOST'S OWN ARTIFACTS, TAKEN FROM THE INSTALLED PACKAGE ------------------------
#
# The witness deploys a real TIMER SERVICE, exactly as the shipped Workshop plans do, and
# that is what makes its host loop honest rather than a spin: the build runner asks the
# Timer for a beat while it holds an operation, so nothing in the host has to poll. They
# come from `ZENGINE_ARTIFACT_DIR` -- the package says where its own artifacts are -- and
# not from the build tree.
foreach(shipped zengine-operators-basic zengine-timer)
    if(NOT EXISTS "${prefix}/lib/zengine/${shipped}${artifact_suffix}")
        message(FATAL_ERROR
            "build witness: the installed package carries no ${shipped}${artifact_suffix}, so "
            "this host has no Timer service to pace itself with.")
    endif()
    file(COPY "${prefix}/lib/zengine/${shipped}${artifact_suffix}" DESTINATION "${house}")
endforeach()

# ---- TWO PLANS, AND THE SECOND IS WHAT MAKES A REBUILD MEASURABLE -----------------------
#
# The full plan names the maker's artifact, so a run started after it has been built has it
# LOADED. The other names only the host's own two artifacts, so the same recipe can be built
# by a host that has never opened it.
#
# ⚠ THAT DISTINCTION IS NOT A TEST CONVENIENCE. It is the difference between "did the build
# converge on the new source" and "can a process relink a library it has mapped", which is a
# PLATFORM question with two different answers -- and a case below asks it deliberately.
file(WRITE "${work}/load-plan-host-only.json"
"{
  \"zen\": 1,
  \"schema\": \"WorkshopLoadFile\",
  \"version\": 1,
  \"fields\": {
    \"format\": \"zengine-workshop-load-plan\",
    \"format_version\": \"1\",
    \"artifacts\": [
      {
        \"artifact\": \"zengine-operators-basic\",
        \"provider\": [ { \"mode\": \"normal\" } ],
        \"weave\": []
      },
      {
        \"artifact\": \"zengine-timer\",
        \"provider\": [ { \"mode\": \"normal\" } ],
        \"weave\": [ { \"role\": \"zengine.timer\" } ]
      }
    ]
  }
}
")

file(WRITE "${work}/load-plan.json"
"{
  \"zen\": 1,
  \"schema\": \"WorkshopLoadFile\",
  \"version\": 1,
  \"fields\": {
    \"format\": \"zengine-workshop-load-plan\",
    \"format_version\": \"1\",
    \"artifacts\": [
      {
        \"artifact\": \"zengine-operators-basic\",
        \"provider\": [ { \"mode\": \"normal\" } ],
        \"weave\": []
      },
      {
        \"artifact\": \"zengine-timer\",
        \"provider\": [ { \"mode\": \"normal\" } ],
        \"weave\": [ { \"role\": \"zengine.timer\" } ]
      },
      {
        \"artifact\": \"zengine-oven\",
        \"provider\": [],
        \"weave\": [ { \"role\": \"zengine.oven\" } ]
      }
    ]
  }
}
")

# ---- Driving the witness ----------------------------------------------------------------
function(zen_witness_with plan label out_var)
    execute_process(
        COMMAND "${ZEN_WITNESS}" --dir "${house}" --load-plan "${plan}"
                --recipes "${work}/build-recipes.json" ${ARGN}
        # THE WALL CLOCK IS BOUNDED HERE AND NOWHERE ELSE. A running operation is simply
        # running; how long is too long is a judgement about a LANE, and this is the lane.
        TIMEOUT 900
        OUTPUT_VARIABLE said ERROR_VARIABLE said_err RESULT_VARIABLE code)
    set(whole "${said}${said_err}")
    set(${out_var} "${whole}" PARENT_SCOPE)
    set(${out_var}_code "${code}" PARENT_SCOPE)
    message(STATUS "build witness: ---- ${label} (exit ${code}) ----")
    message(STATUS "${whole}")
endfunction()

# The ordinary case: the project's own plan, which names the maker's artifact.
macro(zen_witness label out_var)
    zen_witness_with("${work}/load-plan.json" "${label}" ${out_var} ${ARGN})
endmacro()

function(zen_expect said needle what)
    string(FIND "${said}" "${needle}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "build witness: ${what}\n  expected to find: ${needle}")
    endif()
endfunction()

function(zen_expect_not said needle what)
    string(FIND "${said}" "${needle}" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "build witness: ${what}\n  should NOT have found: ${needle}")
    endif()
endfunction()

set(artifact "${house}/zengine-oven${artifact_suffix}")

# ---- 2, 5, 7. one .cpp -> a real artifact -> the RUNNING host's realization owner -------
zen_witness("one source, built and realized" said --build oven --realize)
if(NOT said_code EQUAL 0)
    message(FATAL_ERROR "build witness: the single-source build FAILED (exit ${said_code})")
endif()
zen_expect("${said}" "waiting to be built: zengine-oven"
           "the authored row was performed at startup, so build->realize was never reachable")
zen_expect("${said}" "RESULT build=succeeded realization=realized"
           "one .cpp did not become a realized artifact")
zen_expect("${said}" "artifact-present=yes" "the artifact the recipe names is not on disk")
zen_expect("${said}" "loaded=yes" "the Kernel does not hold the artifact it was handed")
if(NOT EXISTS "${artifact}")
    message(FATAL_ERROR "build witness: ${artifact} was not produced")
endif()
message(STATUS "build witness: one .cpp -> a real Zengine artifact -> realized, in one run ok")

# ---- 8. an already-loaded artifact is REFUSED, not silently reloaded --------------------
#
# The artifact is on disk now, so the row is no longer waiting: the plan realizes it at
# startup, and a maker asking to realize it again is told what BLD-1 does not do.
zen_witness("build and realize an artifact that is already live" again --build oven --realize)
zen_expect("${again}" "loaded: zengine-oven" "the plan did not realize the built artifact")
zen_expect("${again}" "RESULT build=succeeded realization=REFUSED"
           "an already-loaded artifact was not refused")
zen_expect("${again}" "already part of this running project"
           "the refusal did not say why")
zen_expect("${again}" "restart" "the refusal did not say what a maker should do instead")
message(STATUS "build witness: an already-loaded artifact is refused in words ok")

# ---- REBUILDING AN ARTIFACT THIS HOST HAS LOADED: two platforms, two answers -------------
#
# MEASURED RATHER THAN AVOIDED, because it is the hazard this repository has been writing
# comments about since BLD-0 and nobody had put a number on it. The run below realizes the
# artifact at startup -- so the process has the image OPEN -- and then rebuilds it:
#
#   Windows   the linker cannot open its own output. The build FAILS, with the platform's
#             own words, and the running image is untouched.
#   Linux     the link SUCCEEDS and replaces the file under a process that has it mapped.
#
# Neither is BLD-1 doing anything: it is what an operating system does with a mapped image,
# and it is the strongest argument there is for the refusal above -- what a maker gets from
# a rebuild of something live is a locked file or a silently divergent one, never a reload.
zen_write_oven("second" OFF)
zen_witness("rebuild an artifact this host has loaded" live_rebuild --build oven)
if(WIN32)
    zen_expect("${live_rebuild}" "RESULT build=FAILED"
               "Windows let a process relink a DLL it has mapped")
    # ⚠ TWO LINKERS, TWO SENTENCES FOR ONE FACT, and both are the toolchain's own words
    # reaching the maker rather than anything Zengine wrote: GNU `ld` says
    # `cannot open output file ... Permission denied`, and MSVC `link` says
    # `LNK1168: cannot open ... for writing`. Accepting either is the honest check;
    # accepting neither would pin one lane's wording as if it were the platform's.
    string(FIND "${live_rebuild}" "Permission denied" said_gnu)
    string(FIND "${live_rebuild}" "LNK1168" said_msvc)
    if(said_gnu EQUAL -1 AND said_msvc EQUAL -1)
        message(FATAL_ERROR
            "build witness: the link failed but neither linker's own reason reached the "
            "maker -- expected `Permission denied` (GNU ld) or `LNK1168` (MSVC link).")
    endif()
    message(STATUS
        "build witness: rebuilding a LOADED artifact fails at the link on Windows, and says "
        "why ok")
else()
    zen_expect("${live_rebuild}" "RESULT build=succeeded"
               "a rebuild of a loaded artifact failed on a platform that permits it")
    message(STATUS
        "build witness: rebuilding a LOADED artifact is permitted on this platform and "
        "changes nothing about the image already running ok")
endif()

# ---- 6. a changed source converges the artifact to the NEW source -----------------------
#
# A HOST THAT HAS NOT OPENED THE ARTIFACT, so what is measured is convergence and not the
# platform question above.
file(READ "${artifact}" before_bytes HEX)
zen_write_oven("third" OFF)
zen_witness_with("${work}/load-plan-host-only.json" "rebuild after changing the source"
                 changed --build oven)
zen_expect("${changed}" "RESULT build=succeeded" "the rebuild did not succeed")
file(READ "${artifact}" after_bytes HEX)
if(after_bytes STREQUAL before_bytes)
    message(FATAL_ERROR
        "build witness: the artifact is byte-identical after the source changed. The build "
        "did not converge on the new source -- it observed an old artifact.")
endif()
message(STATUS "build witness: a changed source produces a changed artifact ok")

# ---- 3, 9. a compile error: real diagnostics, no artifact claim, no load request ---------
#
# THE OLD, GOOD ARTIFACT IS LEFT WHERE IT IS. That is the stale-output question asked at
# its sharpest: a failing build with a perfectly good previous product sitting at the
# destination must be a FAILURE, and must offer nothing to any project.
zen_write_oven("fourth" ON)
zen_witness_with("${work}/load-plan-host-only.json" "a source that does not compile" broke
                 --build oven --realize)
if(broke_code EQUAL 0)
    message(FATAL_ERROR "build witness: a source that cannot compile reported success")
endif()
zen_expect("${broke}" "compile or link FAILED"
           "the driver did not tell a compile failure from a configure failure")
zen_expect("${broke}" "this_symbol_does_not_exist_anywhere"
           "the compiler's own diagnostic did not reach the maker")
zen_expect("${broke}" "RESULT build=FAILED" "a compile error was not reported as a failure")
zen_expect_not("${broke}" "realization=realized" "a failed build was realized")
zen_expect_not("${broke}" "realization=offered" "a failed build offered its artifact")
zen_expect("${broke}" "the build failed, so nothing was offered"
           "a failed build did not say why nothing was realized")
if(NOT EXISTS "${artifact}")
    message(FATAL_ERROR
        "build witness: the failing build removed the previous artifact, so the stale-output "
        "question cannot be asked here at all.")
endif()
message(STATUS "build witness: a compile error fails honestly and offers nothing ok")

# ---- 4. a build that succeeds and produces nothing is not an artifact success ------------
#
# A CMAKE-TARGET RECIPE, and it has to be one: a SINGLE-SOURCE recipe cannot reach this
# outcome, because Zengine generates the project and therefore knows the target's output
# name IS the artifact stem. That is a real property of the single-source route and worth
# saying out loud -- and it is exactly why the outcome still has to exist, because an
# EXISTING CMake target's product is somebody else's decision and a recipe's claim about
# it can simply be wrong.
#
# The tree it points at is the one the run above generated and configured, so this is a
# real target in a real build tree, building successfully, whose product the recipe
# mis-names. The ordinary maker mistake, made on purpose.
zen_write_oven("fifth" OFF)
file(WRITE "${work}/build-recipes-wrong.json"
"{
  \"zen\": 1,
  \"schema\": \"WorkshopRecipeFile\",
  \"version\": 1,
  \"fields\": {
    \"format\": \"zengine-build-recipes\",
    \"format_version\": \"1\",
    \"recipes\": [
      {
        \"recipe\": \"oven\",
        \"artifact\": \"zengine-not-this-one\",
        \"artifact_dir\": \"\",
        \"cmake_target\": [
          {
            \"build_dir\": \"${space}/build\",
            \"target\": \"zengine-oven\",
            \"config\": \"\"
          }
        ],
        \"single_source\": []
      }
    ]
  }
}
")
execute_process(
    COMMAND "${ZEN_WITNESS}" --dir "${house}" --load-plan "${work}/load-plan-host-only.json"
            --recipes "${work}/build-recipes-wrong.json" --build oven --realize
    TIMEOUT 900
    OUTPUT_VARIABLE absent ERROR_VARIABLE absent_err RESULT_VARIABLE absent_code)
set(absent "${absent}${absent_err}")
message(STATUS "build witness: ---- a recipe that names the wrong artifact (exit ${absent_code}) ----")
message(STATUS "${absent}")
if(absent_code EQUAL 0)
    message(FATAL_ERROR
        "build witness: a build whose expected artifact is absent reported success")
endif()
zen_expect("${absent}" "RESULT build=NO ARTIFACT"
           "a green build with no product was not told apart from a success")
zen_expect_not("${absent}" "realization=realized" "an absent artifact was realized")
message(STATUS "build witness: exit zero without the expected artifact is not success ok")

# ---- 2b. THE CANARY: break the package and the build must go red -------------------------
#
# The failure mode this discriminates is the one PKG-0 was written for: a consumer that
# looks like it uses the package while actually reading Zengine's source tree. The Zengine
# checkout is fully present and readable during this step -- if the generated project still
# builds with `<zen/weave.hpp>`'s package gone, it is finding it somewhere else and every
# result above is void.
# A FRESH WORKSPACE FOR EACH CANARY, and it is not tidiness. `find_package` caches
# `zengine_DIR`, so a workspace that already configured against a whole prefix would go on
# using the cached location however thoroughly the prefix it was pointed at was broken --
# and the canary would report a package resolution that never happened.
set(canary_a "${work}/prefix-no-package")
set(space_a "${work}/workspace-no-package")
file(TO_CMAKE_PATH "${canary_a}" canary_a)
file(TO_CMAKE_PATH "${space_a}" space_a)
file(REMOVE_RECURSE "${canary_a}" "${space_a}")
file(COPY "${prefix}/" DESTINATION "${canary_a}")
file(REMOVE "${canary_a}/lib/cmake/zengine/zengineConfig.cmake")
zen_write_recipes("\"${canary_a}\", \"${loom_prefix}\"" "${space_a}")
file(REMOVE "${artifact}")
zen_witness_with("${work}/load-plan-host-only.json" "the package config removed from the prefix"
                 canary_said --build oven)
if(canary_said_code EQUAL 0)
    message(FATAL_ERROR
        "build witness: CANARY DID NOT FIRE. The generated project built with "
        "zengineConfig.cmake deleted from the prefix, so it is not consuming the package -- "
        "it is finding Zengine's source tree, which is still present at ${repo}. Every "
        "result above is void.")
endif()
zen_expect("${canary_said}" "CMake configure FAILED"
           "the canary failed somewhere other than package resolution")
zen_expect("${canary_said}" "RESULT build=FAILED" "the canary was not reported as a failure")
message(STATUS "build witness: canary fired -- a missing package config is fatal ok")

# ---- 2c. THE SHARPER CANARY: one installed HEADER removed --------------------------------
#
# PKG-0's own canary, aimed at this route. The package RESOLVES here -- the config is
# whole and the targets import -- and one header the maker's source names is gone. The
# same header is sitting in Zengine's source tree, fully readable, four directories away.
# If the compile succeeds, the generated project is reading THAT, and "an external
# consumer" was never true of it.
set(canary_b "${work}/prefix-no-header")
set(space_b "${work}/workspace-no-header")
file(TO_CMAKE_PATH "${canary_b}" canary_b)
file(TO_CMAKE_PATH "${space_b}" space_b)
file(REMOVE_RECURSE "${canary_b}" "${space_b}")
file(COPY "${prefix}/" DESTINATION "${canary_b}")
file(REMOVE "${canary_b}/include/zengine/timer/vocabulary.hpp")
if(EXISTS "${canary_b}/include/zengine/timer/vocabulary.hpp")
    message(FATAL_ERROR "build witness: the canary could not remove the header it needs to")
endif()
zen_write_recipes("\"${canary_b}\", \"${loom_prefix}\"" "${space_b}")
file(REMOVE "${artifact}")
zen_witness_with("${work}/load-plan-host-only.json" "one installed header removed" header_said
                 --build oven)
if(header_said_code EQUAL 0)
    message(FATAL_ERROR
        "build witness: CANARY DID NOT FIRE. The generated project compiled with "
        "include/zengine/timer/vocabulary.hpp deleted from the prefix, so it is not reading "
        "the installed headers -- it is finding Zengine's source tree, which is still "
        "present at ${repo}. Every result above is void.")
endif()
zen_expect("${header_said}" "compile or link FAILED"
           "the header canary failed at configure rather than at the compile it is about")
zen_expect("${header_said}" "timer/vocabulary.hpp"
           "the compiler did not name the header it could not find")
message(STATUS "build witness: canary fired -- a missing installed header is fatal ok")

# ---- ...and the ordinary route still works, so the canaries proved something -------------
zen_write_recipes("\"${prefix}\", \"${loom_prefix}\"" "${space}")
zen_witness_with("${work}/load-plan-host-only.json" "the same recipe against the whole prefix again"
                 restored --build oven)
if(NOT restored_code EQUAL 0)
    message(FATAL_ERROR
        "build witness: the route stopped working after the canaries (exit ${restored_code}), "
        "so what the canaries measured is not what they claim to measure.")
endif()
zen_expect("${restored}" "RESULT build=succeeded" "the restored route did not build")
message(STATUS "build witness: the whole prefix still builds it ok")

# ---- and the generated project itself, read as bytes ------------------------------------
#
# The suite asserts on these bytes on every lane; this is the one place they can also be
# shown to be the bytes that a real compiler actually consumed.
if(NOT EXISTS "${space}/CMakeLists.txt")
    message(FATAL_ERROR "build witness: no generated project at ${space}")
endif()
file(READ "${space}/CMakeLists.txt" generated)
zen_expect("${generated}" "find_package(zengine 0.1 CONFIG REQUIRED)"
           "the generated project does not consume the package")
zen_expect("${generated}" "${sources}/my oven.cpp"
           "the generated project does not name the maker's source")
message(STATUS "build witness: the generated project is on disk and inspectable ok")

message(STATUS "build witness: PASSED")
