# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The single-source build witness (docs/reference/builder.md): install Zengine into a scratch
# prefix under ZEN_WORK, outside both repositories; build and realize a one-file weave written
# there through the real Builder (tests/build/witness.cpp); then break the package so its
# canaries fire. A lane, not a CTest entry, for tests/package/run.cmake's reason.
#   cmake -DZEN_BUILD_DIR=<build> -DZEN_WORK=<dir> [-DZEN_CONFIG=Debug] -P tests/build/run.cmake

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

# ---- install Zengine into an isolated prefix -------------------------------------------
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

# ---- every path that goes into a file is spelled with forward slashes -------------------
# A recipe catalog and a load plan are JSON, where `\` escapes: a Windows path written verbatim
# is a parse error or another path. Windows accepts forward slashes, so paths are normalised
# once, here. It is a fact about the format, not the recipe law, which accepts a backslash.
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

# ---- the host's own artifacts, taken from the installed package ------------------------
# The witness deploys a real Timer service, as the shipped plans do, so its host loop waits on
# beats rather than spinning; the artifacts come from `ZENGINE_ARTIFACT_DIR`, not the build tree.
foreach(shipped zengine-operators-basic zengine-timer)
    if(NOT EXISTS "${prefix}/lib/zengine/${shipped}${artifact_suffix}")
        message(FATAL_ERROR
            "build witness: the installed package carries no ${shipped}${artifact_suffix}, so "
            "this host has no Timer service to pace itself with.")
    endif()
    file(COPY "${prefix}/lib/zengine/${shipped}${artifact_suffix}" DESTINATION "${house}")
endforeach()

# ---- two plans, and the second is what makes a rebuild measurable -----------------------
# The full plan names the maker's artifact, so a run after its build has it loaded; the other
# names only the host's two, so a host that never opened that artifact can build it. That
# separates "did the build converge on the new source" from "can a process relink a library it
# has mapped", a platform question with two answers, which a case below asks on purpose.
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
# The build lands in the recipe's workspace under `out/`, never the file the host has loaded;
# `artifact` above is the plan's file, which a realization copies into.
set(product "${space}/out/zengine-oven${artifact_suffix}")

# ---- one .cpp -> a real artifact -> the running host's realization owner ----------------
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
zen_expect("${said}" "default-image=yes" "a first realization does not run from the plan's file")
if(NOT EXISTS "${product}")
    message(FATAL_ERROR "build witness: ${product} was not produced in the workspace")
endif()
if(NOT EXISTS "${artifact}")
    message(FATAL_ERROR
        "build witness: ${artifact} was not staged beside the host by the realization")
endif()
message(STATUS "build witness: one .cpp -> a real Zengine artifact -> realized, in one run ok")

# ---- rebuilding an artifact this host has loaded: off the loaded path, then in place -------
# The run realizes the artifact at startup, so the process has the plan's file open, and rebuilds
# it from a changed source: the build lands in the workspace and the loaded file is unchanged on
# both platforms (agents/decisions/a-reload-lands-off-the-loaded-path.md).
file(READ "${artifact}" loaded_before HEX)
zen_write_oven("second" OFF)
zen_witness("rebuild an artifact this host has loaded, plainly" live_rebuild --build oven)
zen_expect("${live_rebuild}" "loaded: zengine-oven" "the plan did not realize the built artifact")
zen_expect("${live_rebuild}" "RESULT build=succeeded"
           "a rebuild of a loaded artifact FAILED: the build wrote the loaded file")
zen_expect("${live_rebuild}" "artifact-present=yes" "the rebuilt product is not in the workspace")
file(READ "${artifact}" loaded_after HEX)
if(NOT loaded_after STREQUAL loaded_before)
    message(FATAL_ERROR
        "build witness: a plain rebuild changed ${artifact}, the file the running host has "
        "loaded. The product must land off the loaded path.")
endif()
message(STATUS
    "build witness: a rebuild of a LOADED artifact lands in the workspace and leaves the loaded "
    "file untouched, on this platform ok")

# ---- ...and the reload in place, with its two acts ----------------------------------------
# The rebuilt product, offered: the realization owner reloads the live weave from a copy staged
# off the loaded path, keeping its WeaveId and state. Then the witness promotes (the plan's file
# takes the running image's bytes) and reverts (the image before the last reload runs again).
zen_witness("reload a live artifact in place, promote it, revert it" reloaded
            --build oven --realize --then-promote --then-revert)
if(NOT reloaded_code EQUAL 0)
    message(FATAL_ERROR "build witness: the reload run FAILED (exit ${reloaded_code})")
endif()
zen_expect("${reloaded}" "loaded: zengine-oven" "the plan did not realize the artifact at startup")
zen_expect("${reloaded}" "witness: realization: reloaded in place"
           "a rebuilt live artifact was not reloaded in place")
zen_expect("${reloaded}" "keeps its id and its state" "the reload did not say the id and state were kept")
zen_expect_not("${reloaded}" "restart" "the reload arm still says restart")
zen_expect("${reloaded}" "default-image=no" "a reloaded image claimed to be the file a restart loads")
zen_expect("${reloaded}" "witness: asking to promote" "the witness never asked to promote")
zen_expect("${reloaded}" "witness: realization: promoted" "the promotion was not answered as promoted")
zen_expect("${reloaded}" "witness: asking to revert" "the witness never asked to revert")
zen_expect("${reloaded}" "witness: realization: reverted" "the revert was not answered as reverted")
zen_expect("${reloaded}" "RESULT build=succeeded realization=realized"
           "the reload run did not end realized")
zen_expect("${reloaded}" "loaded=yes" "the Kernel does not hold the artifact after the reload")
file(GLOB reload_copies "${house}/zengine-oven.reloads/zengine-oven-*${artifact_suffix}")
list(LENGTH reload_copies reload_copy_count)
if(reload_copy_count LESS 2)
    message(FATAL_ERROR
        "build witness: expected the reload copy and the kept image under "
        "${house}/zengine-oven.reloads, found ${reload_copy_count}")
endif()
# THE PROMOTION, READ OFF THE BYTES: the plan's file now holds the reload copy, exactly.
file(READ "${house}/zengine-oven.reloads/zengine-oven-1${artifact_suffix}" reload_copy HEX)
file(READ "${artifact}" promoted HEX)
if(NOT promoted STREQUAL reload_copy)
    message(FATAL_ERROR
        "build witness: after the promotion ${artifact} is not byte-identical to the reload "
        "copy, so the next launch would not run the promoted image")
endif()
if(promoted STREQUAL loaded_before)
    message(FATAL_ERROR
        "build witness: the promotion changed nothing: ${artifact} still holds the old image")
endif()
message(STATUS
    "build witness: a live artifact reloads in place, promotes into the plan's file, and "
    "reverts, on this platform ok")

# ---- a changed source converges the artifact to the new source --------------------------
#
# A HOST THAT HAS NOT OPENED THE ARTIFACT, so what is measured is convergence and not the
# platform question above.
file(READ "${product}" before_bytes HEX)
zen_write_oven("third" OFF)
zen_witness_with("${work}/load-plan-host-only.json" "rebuild after changing the source"
                 changed --build oven)
zen_expect("${changed}" "RESULT build=succeeded" "the rebuild did not succeed")
file(READ "${product}" after_bytes HEX)
if(after_bytes STREQUAL before_bytes)
    message(FATAL_ERROR
        "build witness: the artifact is byte-identical after the source changed. The build "
        "did not converge on the new source -- it observed an old artifact.")
endif()
message(STATUS "build witness: a changed source produces a changed artifact ok")

# ---- a compile error: real diagnostics, no artifact claim, no load request ---------------
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
# THE COMPILER'S REASON REACHES THE MAKER THROUGH WHAT THE BUILD SAID, KEPT BY ITS OPERATION --
# the page the Builder's reader shows (WL-OUT-02), which the witness asks for once the build has
# ended -- and not through a status's last lines, which the driver's own footer spends.
string(REGEX MATCH "witness: said: [^\n]*this_symbol_does_not_exist_anywhere" zen_reason
       "${broke}")
if(NOT zen_reason)
    message(FATAL_ERROR "build witness: the compiler's own diagnostic did not reach the maker "
                        "through the build's kept output\n  expected a `witness: said:` line "
                        "naming: this_symbol_does_not_exist_anywhere")
endif()
zen_expect("${broke}" "RESULT build=FAILED" "a compile error was not reported as a failure")
zen_expect_not("${broke}" "realization=realized" "a failed build was realized")
zen_expect_not("${broke}" "realization=offered" "a failed build offered its artifact")
zen_expect("${broke}" "the build failed, so nothing was offered"
           "a failed build did not say why nothing was realized")
if(NOT EXISTS "${product}")
    message(FATAL_ERROR
        "build witness: the failing build removed the previous artifact, so the stale-output "
        "question cannot be asked here at all.")
endif()
message(STATUS "build witness: a compile error fails honestly and offers nothing ok")

# ---- a build that succeeds and produces nothing is not an artifact success ----------------
# A CMake-target recipe, since a single-source one cannot reach this outcome (Zengine generates
# the project and knows its output is the stem); an existing target's product is someone else's
# decision, and a recipe's claim about it can be wrong. It points at the tree the run above
# generated: a real target, building successfully, whose product the recipe misnames.
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

# ---- the canary: break the package and the build must go red -----------------------------
# A consumer that looks like it uses the package while reading Zengine's source tree, which stays
# present and readable: if the generated project builds with the package gone, it found it
# elsewhere and every result above is void. A fresh workspace for each canary, since
# `find_package` caches `zengine_DIR` and would go on using a location the canary broke.
set(canary_a "${work}/prefix-no-package")
set(space_a "${work}/workspace-no-package")
file(TO_CMAKE_PATH "${canary_a}" canary_a)
file(TO_CMAKE_PATH "${space_a}" space_a)
file(REMOVE_RECURSE "${canary_a}" "${space_a}")
file(COPY "${prefix}/" DESTINATION "${canary_a}")
file(REMOVE "${canary_a}/lib/cmake/zengine/zengineConfig.cmake")
zen_write_recipes("\"${canary_a}\", \"${loom_prefix}\"" "${space_a}")
file(REMOVE "${product}")
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

# ---- the sharper canary: one installed header removed -------------------------------------
# The package resolves and one header the maker's source names is gone, while the same header
# sits readable in Zengine's source tree: if the compile succeeds, the project reads that, and
# "an external consumer" was never true of it.
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
file(REMOVE "${product}")
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
