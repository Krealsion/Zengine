# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE LAW-REGISTER CHECK -- the `law_register` CTest entry.
#
# It answers one question: is Workshop's law, as written in the registers under agents/,
# still well-formed, and does every name it makes still resolve? Four populations, one rule:
#
#   registers         every `##` under a register directory is one law entry (or the one
#                     `## Do not assume`); an entry has a LAW of one line and a PROVEN BY;
#                     MEANS and DOES NOT MEAN are bounded; no SINCE line; ids are unique
#                     under agents/; each file is under its byte budget
#   PROVEN BY         every backticked path exists; every backticked identifier occurs in
#                     the file named before it; every quoted witness is a TEST_CASE or
#                     SUBCASE literal under tests/
#   decision records  every WHY target exists; a record's "Laws supported" is exactly the
#                     set of ids whose WHY names it
#   source pointers   every `// WL-... -- agents/workshop/<register>.md` line names
#                     registers that exist and ids that are entries of the register named
#                     on that line; every `// Workshop law:` header names existing files
#
# WHY IT IS MECHANICAL. The register replaced a 200 KB document in which a law existed
# three times -- the document, a header essay above the function, and the code -- and went
# stale between them. The register is the one copy; PROVEN BY is where it touches the code
# and the tests; the pointer is where the code touches it back. Every one of those joints
# is a name, and a rename breaks a name silently. This entry rides the official lane so the
# red reaches whoever renamed the thing rather than whoever reads the law six phases later.
# The rules it enforces are the router's (agents/workshop.md, "Ongoing rules").
#
# WHY CMAKE AND NOT A SHELL SCRIPT. The prototype was a bash script in a report-back. Every
# repository-owned check here is a CMake script (see check_doc_links.cmake): CMake is a
# dependency this project has on every lane by construction, and a check that is absent on
# the lane most likely to break a name is not a weaker check, it is no check.
#
# TWO CHECKS ARE STRICTER THAN THE PROTOTYPE, and they are behind LAW_REGISTER_STRICT
# (default OFF) until the lists they produce have been worked down:
#
#   rule m   an owner identifier is present only if it occurs as a whole token in the CODE
#            of the named file, `//` comments and `/* */` comments stripped. The lenient
#            check above is satisfied by a mention in a comment -- measured: 24 wrong
#            attributions survived three steps that way -- and a substring (`Rect` inside
#            `SurfaceRect`).
#   rule n   a `// WL-...` pointer is PROVEN BY inverted: for each pointer line, the
#            declaration on the next code line is named by the PROVEN BY of at least one
#            law on that line, under this file. What "the declaration" means here is a
#            heuristic parse, stated at zen_law_declared() below.
#
# With STRICT OFF both print their failures and a count and do not fail the entry; with
# STRICT ON they fail it. Flip the default here once the lists are empty.
#
# WHAT IT DELIBERATELY DOES NOT DO
#
#   * it does not know whether a law is TRUE. A phase that edits a witnessed TEST_CASE
#     re-verifies every law naming it, in the same commit; that rule is procedural and
#     lives in the router, because only the phase that changed a test knows;
#   * it does not count witnesses per law. `witness: none` is a written debt, repeated
#     under the register's `## Do not assume`, and the router forbids lowering it;
#   * it does not police prose width in decision records, only in registers and routers.
#
# HONEST LIMITS OF THE PARSE, all in the direction of a visible red rather than a quiet
# green: a `//` inside a string literal is read as a comment start (rule m then sees less
# code, never more); a pointer written after code on the same line is not a pointer line;
# a `## WL-...-NN -- RETIRED` heading is an entry with no LAW or PROVEN BY owed, exactly as
# the router says a retired law keeps its number and one line.
#
# THE SELF-TEST IS NOT OPTIONAL. A well-formed tree and a checker that finds nothing produce
# byte-identical output, so before answering it makes each predicate say NO -- a bad
# heading, an identifier that is only in a comment, a witness no test declares, a malformed
# pointer -- and say YES to their well-formed twins, one of them a case name read out of
# the real test sources at runtime.
#
#   cmake -P tests/check_law_register.cmake                          (from the repository root)
#   cmake -DLAW_REGISTER_STRICT=ON -P tests/check_law_register.cmake
#   cmake -DZEN_REPO=<repo> -P tests/check_law_register.cmake

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ZEN_REPO OR ZEN_REPO STREQUAL "")
    set(ZEN_REPO "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
get_filename_component(ZEN_REPO "${ZEN_REPO}" ABSOLUTE)
if(NOT EXISTS "${ZEN_REPO}/AGENTS.md")
    message(FATAL_ERROR
        "law-register: '${ZEN_REPO}' does not look like this repository's root (no AGENTS.md). "
        "Pass -DZEN_REPO=<repository root>.")
endif()
if(NOT DEFINED LAW_REGISTER_STRICT)
    set(LAW_REGISTER_STRICT OFF)
endif()

# ---- scope, declared here so a standalone clone carries its own rule -----------------
#
# One line per routed area that has been turned into registers. The instruction that made
# Workshop's registers applies to every routed document under agents/ in turn; each one
# joins this list when its registers exist.
set(ZEN_LAW_REGISTER_DIRS agents/workshop)
set(ZEN_LAW_ROUTERS agents/workshop.md)
set(ZEN_LAW_RECORD_DIR agents/decisions)
set(ZEN_LAW_CORE AGENTS.md)
set(ZEN_LAW_WITNESS_DIR tests)

# The budgets, in BYTES. `string(LENGTH)` and `file(SIZE)` both count bytes, and an em dash
# is three of them; a byte count is the stricter reading and the one the registers were
# written to.
set(ZEN_LAW_REGISTER_BYTES 16384)
set(ZEN_LAW_ROUTER_BYTES 8192)
set(ZEN_LAW_CORE_BYTES 20480)
set(ZEN_LAW_LAW_BYTES 210)
set(ZEN_LAW_LINE_BYTES 98)
set(ZEN_LAW_MEANS_MAX 3)
set(ZEN_LAW_DNM_MAX 2)

# Frozen, generated or vendored. Matched against the repository-relative path.
set(ZEN_LAW_EXCLUDE
    "^build"
    "^cmake-build"
    "^_install"
    "^\\.git/"
    "^docs/history/"
    "^reference/"
    "third_party/")
set(ZEN_LAW_SOURCE_GLOBS *.h *.hpp *.ipp *.c *.cc *.cpp *.cxx)

# A backticked token in PROVEN BY is a path when it ends in one of these; otherwise it is
# an identifier checked against the path named before it.
set(ZEN_LAW_PATH_RE "^[A-Za-z0-9_./-]+\\.(hpp|cpp|h|ipp|cc|cxx|c|md|txt|cmake|json|in|yml|yaml)$")

# ---- reading a file without letting its punctuation reshape a CMake list ---------------
#
# Four characters would otherwise decide answers for us. `;` is CMake's list separator; `[`
# and `]` suspend it (a `;` between brackets is not a separator, and one unbalanced `[`
# welds every later line into one element -- measured on both CMake versions this
# repository configures with); `\` before a separator escapes it. Each is swapped for a
# control character that no source or document contains, so lengths are preserved byte for
# byte and every comparison below is made on identically transformed text. The swap is
# undone only for printing.
string(ASCII 1 ZEN_SOH)   # was ;
string(ASCII 2 ZEN_STX)   # was [
string(ASCII 3 ZEN_ETX)   # was ]
string(ASCII 4 ZEN_EOT)   # was \

function(zen_law_read path out)
    file(READ "${path}" content)
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE ";" "${ZEN_SOH}" content "${content}")
    string(REPLACE "[" "${ZEN_STX}" content "${content}")
    string(REPLACE "]" "${ZEN_ETX}" content "${content}")
    string(REPLACE "\\" "${ZEN_EOT}" content "${content}")
    set(${out} "${content}" PARENT_SCOPE)
endfunction()

function(zen_law_show text out)
    string(REPLACE "${ZEN_SOH}" ";" text "${text}")
    string(REPLACE "${ZEN_STX}" "[" text "${text}")
    string(REPLACE "${ZEN_ETX}" "]" text "${text}")
    string(REPLACE "${ZEN_EOT}" "\\" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# The transformed text of a file, read once. A GLOBAL property rather than a variable: a
# function cannot write its caller's scope, and the same source file is named by dozens of
# PROVEN BY lines.
function(zen_law_text rel out)
    string(MAKE_C_IDENTIFIER "${rel}" key)
    get_property(cached GLOBAL PROPERTY "zen_law_text_${key}" SET)
    if(NOT cached)
        zen_law_read("${ZEN_REPO}/${rel}" content)
        set_property(GLOBAL PROPERTY "zen_law_text_${key}" "${content}")
    endif()
    get_property(content GLOBAL PROPERTY "zen_law_text_${key}")
    set(${out} "${content}" PARENT_SCOPE)
endfunction()

# The CODE of a C/C++ file: `/* */` comments go first (bounded by their own closer), then
# every `//` comment to the end of its line. Whole-content pattern replacement rather than
# a line walk, for the reason check_doc_links.cmake measured: a file split into a list
# arrives welded. String literals are kept -- a literal is code.
function(zen_law_strip_comments content out)
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" code "${content}")
    string(REGEX REPLACE "//[^\n]*" "" code "${code}")
    set(${out} "${code}" PARENT_SCOPE)
endfunction()

function(zen_law_code rel out)
    string(MAKE_C_IDENTIFIER "${rel}" key)
    get_property(cached GLOBAL PROPERTY "zen_law_code_${key}" SET)
    if(NOT cached)
        zen_law_text("${rel}" content)
        zen_law_strip_comments("${content}" code)
        set_property(GLOBAL PROPERTY "zen_law_code_${key}" "${code}")
    endif()
    get_property(code GLOBAL PROPERTY "zen_law_code_${key}")
    set(${out} "${code}" PARENT_SCOPE)
endfunction()

function(zen_law_regex_escape s out)
    string(REGEX REPLACE "([+.*?^$|(){}])" "\\\\\\1" e "${s}")
    set(${out} "${e}" PARENT_SCOPE)
endfunction()

# ---- the predicates, each in one place so the self-test exercises the real one ---------

# Lenient: the token occurs anywhere in the text, comments included, as a substring. This
# is the prototype's check and the one the lane enforces today.
function(zen_law_mentions text token out)
    string(FIND "${text}" "${token}" pos)
    if(pos EQUAL -1)
        set(${out} 0 PARENT_SCOPE)
    else()
        set(${out} 1 PARENT_SCOPE)
    endif()
endfunction()

# Strict (rule m): the token occurs as a whole token -- bounded by non-identifier
# characters or the ends -- in text that should already have had its comments stripped. A
# qualified token (`detail::pane_inside_at`, `Row::section`) must occur qualified; `:` is a
# boundary, so a bare `section` is also found inside `Row::section`.
function(zen_law_token_in code token out)
    zen_law_regex_escape("${token}" t)
    if(code MATCHES "(^|[^A-Za-z0-9_])${t}([^A-Za-z0-9_]|$)")
        set(${out} 1 PARENT_SCOPE)
    else()
        set(${out} 0 PARENT_SCOPE)
    endif()
endfunction()

# A register heading. Sets ${out_id} to the law id, or to "" for a heading that is not an
# entry; ${out_retired} to 1 for the one-line retired form.
function(zen_law_entry_heading line out_id out_retired)
    set(${out_id} "" PARENT_SCOPE)
    set(${out_retired} 0 PARENT_SCOPE)
    if(line MATCHES "^## (WL-[A-Z]+-[0-9][0-9]) (—|--) (.*)$")
        set(${out_id} "${CMAKE_MATCH_1}" PARENT_SCOPE)
        if(CMAKE_MATCH_3 MATCHES "^RETIRED")
            set(${out_retired} 1 PARENT_SCOPE)
        endif()
    endif()
endfunction()

# A pointer line: `// WL-A-01, WL-A-02 -- agents/workshop/a.md; WL-B-03 -- agents/workshop/b.md`.
# Sets ${out_ok} to 0 for anything else, and ${out_segments} to a list whose elements are
# `<register>|<id>,<id>,...`. The separator between segments arrives as ZEN_SOH.
function(zen_law_parse_pointer line out_ok out_segments)
    set(${out_ok} 0 PARENT_SCOPE)
    set(${out_segments} "" PARENT_SCOPE)
    string(STRIP "${line}" l)
    if(NOT l MATCHES "^// (WL-.*)$")
        return()
    endif()
    string(REPLACE "${ZEN_SOH}" ";" pieces "${CMAKE_MATCH_1}")
    set(segments "")
    foreach(piece IN LISTS pieces)
        string(STRIP "${piece}" piece)
        if(NOT piece MATCHES "^(WL-[A-Z]+-[0-9]+([ \t]*,[ \t]*WL-[A-Z]+-[0-9]+)*)[ \t]+--[ \t]+([A-Za-z0-9_./-]+\\.md)$")
            return()
        endif()
        set(ids "${CMAKE_MATCH_1}")
        set(register "${CMAKE_MATCH_3}")
        string(REGEX REPLACE "[ \t]" "" ids "${ids}")
        list(APPEND segments "${register}|${ids}")
    endforeach()
    set(${out_ok} 1 PARENT_SCOPE)
    set(${out_segments} "${segments}" PARENT_SCOPE)
endfunction()

# THE DECLARATION A POINTER POINTS AT (rule n) -- a heuristic over one code line, and this
# comment is what it accepts. Sets ${out_name} to the declared identifier or "" when the
# line declares nothing it can name, and ${out_kind} to one of: scope | alias | function |
# variable | none.
#
#   * a trailing `//` comment is dropped; template argument lists `<...>` (four levels)
#     and attributes `[[...]]` are dropped;
#   * `namespace|struct|class|union|enum [class|struct] NAME` -> NAME, kind scope. A law
#     naming any `NAME::member` under the file is taken to name the scope;
#   * `using NAME =` -> NAME, kind alias;
#   * `static_assert(` declares nothing -> none;
#   * `MACRO(NAME, ...)` with an all-capitals macro -> NAME when the first argument is an
#     identifier (`ZEN_SHAPE(WorkshopSession, ...)`), else none;
#   * otherwise, if a `=` comes before any `(`, the line is a variable with an initializer
#     and the name is the last identifier before the `=`; else if there is a `(`, the name
#     is the last identifier before it (a function, a constructor, or `on` for an overload
#     set -- overloads are not told apart); else the name is the last identifier before the
#     first `{`, `;`, `,` or `[` (a member, a constant, an enumerator);
#   * a name that is a C++ keyword (`else`, `return`, `operator`, ...) -> none.
#
# The line handed in is the first line after the pointer that is not blank, not a comment,
# not a preprocessor line, not a bare `template <...>`, not a bare attribute, and not an
# access specifier; a closing brace is handed in and yields none.
set(ZEN_LAW_KEYWORDS
    alignas alignof asm auto bool break case catch char class const constexpr continue
    decltype default delete do double else enum explicit export extern false float for
    friend goto if inline int long mutable namespace new noexcept nullptr operator override
    final private protected public register return short signed sizeof static struct switch
    template this throw true try typedef typeid typename union unsigned using virtual void
    volatile while)

function(zen_law_declared line out_name out_kind)
    set(${out_name} "" PARENT_SCOPE)
    set(${out_kind} "none" PARENT_SCOPE)
    string(REGEX REPLACE "//.*$" "" l "${line}")
    string(STRIP "${l}" l)
    foreach(round RANGE 1 4)
        string(REGEX REPLACE "<[^<>]*>" "" l "${l}")
    endforeach()
    string(REGEX REPLACE "${ZEN_STX}${ZEN_STX}[^${ZEN_ETX}]*${ZEN_ETX}${ZEN_ETX}" "" l "${l}")
    string(STRIP "${l}" l)
    if(l MATCHES "^(namespace|struct|class|union|enum class|enum struct|enum)[ \t]+([A-Za-z_][A-Za-z0-9_]*)")
        set(${out_name} "${CMAKE_MATCH_2}" PARENT_SCOPE)
        set(${out_kind} "scope" PARENT_SCOPE)
        return()
    endif()
    if(l MATCHES "^using[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*=")
        set(${out_name} "${CMAKE_MATCH_1}" PARENT_SCOPE)
        set(${out_kind} "alias" PARENT_SCOPE)
        return()
    endif()
    if(l MATCHES "^static_assert[ \t]*\\(")
        return()
    endif()
    if(l MATCHES "^([A-Z][A-Z0-9_]*)[ \t]*\\(")
        if(l MATCHES "^[A-Z][A-Z0-9_]*[ \t]*\\([ \t]*([A-Za-z_][A-Za-z0-9_]*)[ \t]*[,)]")
            set(${out_name} "${CMAKE_MATCH_1}" PARENT_SCOPE)
            set(${out_kind} "variable" PARENT_SCOPE)
        endif()
        return()
    endif()
    string(FIND "${l}" "=" eq)
    string(FIND "${l}" "(" par)
    set(kind "variable")
    if(NOT eq EQUAL -1 AND (par EQUAL -1 OR eq LESS par))
        string(SUBSTRING "${l}" 0 ${eq} head)
    elseif(NOT par EQUAL -1)
        string(SUBSTRING "${l}" 0 ${par} head)
        set(kind "function")
    else()
        set(head "${l}")
        foreach(stop "{" "${ZEN_SOH}" "," "${ZEN_STX}")
            string(FIND "${head}" "${stop}" at)
            if(NOT at EQUAL -1)
                string(SUBSTRING "${head}" 0 ${at} head)
            endif()
        endforeach()
    endif()
    string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*" words "${head}")
    list(LENGTH words n)
    if(n EQUAL 0)
        return()
    endif()
    list(GET words -1 name)
    if(name IN_LIST ZEN_LAW_KEYWORDS)
        return()
    endif()
    set(${out_name} "${name}" PARENT_SCOPE)
    set(${out_kind} "${kind}" PARENT_SCOPE)
endfunction()

# ---- problem collection ----------------------------------------------------------------
#
# Problems are gathered and reported together so one run shows every broken name, not the
# first. Strict findings go to their own lists, whose fate LAW_REGISTER_STRICT decides.
set_property(GLOBAL PROPERTY zen_law_problems "")
set_property(GLOBAL PROPERTY zen_law_rule_m "")
set_property(GLOBAL PROPERTY zen_law_rule_n "")
function(zen_law_fail text)
    set_property(GLOBAL APPEND_STRING PROPERTY zen_law_problems "${text}\n")
endfunction()
function(zen_law_strict which text)
    set_property(GLOBAL APPEND_STRING PROPERTY "zen_law_rule_${which}" "${text}\n")
endfunction()
function(zen_law_count_lines text out)
    string(REGEX MATCHALL "\n" nl "${text}")
    list(LENGTH nl n)
    set(${out} "${n}" PARENT_SCOPE)
endfunction()

# ---- the self-test: make it say NO, and make it say YES ------------------------------

zen_law_entry_heading("## Not a law — prose" id retired)
if(NOT id STREQUAL "")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a heading that is not an entry was read as one.")
endif()
zen_law_entry_heading("## WL-ZZZ-01 — A heading that is an entry" id retired)
if(NOT id STREQUAL "WL-ZZZ-01" OR retired)
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a well-formed heading was not read as an entry.")
endif()
zen_law_entry_heading("## WL-ZZZ-02 — RETIRED (x) — replaced by WL-ZZZ-01" id retired)
if(NOT id STREQUAL "WL-ZZZ-02" OR NOT retired)
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- the retired one-line form was not recognised.")
endif()

set(selftest_src "int kAlpha = 1${ZEN_SOH} // kBeta is only here\n/* kGamma */ SurfaceRect r${ZEN_SOH}\n")
zen_law_mentions("${selftest_src}" "kBeta" yes)
zen_law_strip_comments("${selftest_src}" selftest_code)
zen_law_token_in("${selftest_code}" "kAlpha" yes_code)
zen_law_token_in("${selftest_code}" "kBeta" no_comment)
zen_law_token_in("${selftest_code}" "kGamma" no_block)
zen_law_token_in("${selftest_code}" "Rect" no_substring)
zen_law_token_in("${selftest_code}" "SurfaceRect" yes_whole)
if(NOT yes OR NOT yes_code OR no_comment OR no_block OR no_substring OR NOT yes_whole)
    message(FATAL_ERROR
        "law-register: SELF-TEST FAILED -- the identifier predicates disagree with a two-line "
        "sample (mention ${yes}, code ${yes_code}, comment ${no_comment}, block ${no_block}, "
        "substring ${no_substring}, whole ${yes_whole}). Every 'present' below would then be "
        "meaningless.")
endif()

zen_law_parse_pointer("// WL-AAA-01, WL-AAA-02 -- agents/workshop/a.md${ZEN_SOH} WL-BBB-03 -- agents/workshop/b.md" ok segs)
list(LENGTH segs nsegs)
if(NOT ok OR NOT nsegs EQUAL 2)
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a well-formed two-register pointer was refused (${ok}, ${nsegs}).")
endif()
zen_law_parse_pointer("// WL-AAA-01 (WUX-8) -- agents/workshop/a.md" ok segs)
if(ok)
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a pointer carrying a retired phase tag was accepted.")
endif()

zen_law_declared("inline constexpr std::int64_t kSideRegion = 0${ZEN_SOH}" name kind)
if(NOT name STREQUAL "kSideRegion")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a constant's declaration parsed as '${name}'.")
endif()
zen_law_declared("struct PanelKind {" name kind)
if(NOT name STREQUAL "PanelKind" OR NOT kind STREQUAL "scope")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a struct's declaration parsed as '${name}' (${kind}).")
endif()
zen_law_declared("std::function<RecipeSwap(const Recipe&)> swap = {}${ZEN_SOH}" name kind)
if(NOT name STREQUAL "swap")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a member behind a template argument parsed as '${name}'.")
endif()
zen_law_declared("${ZEN_STX}${ZEN_STX}nodiscard${ZEN_ETX}${ZEN_ETX} bool on(const SurfaceExtent& e) noexcept {" name kind)
if(NOT name STREQUAL "on" OR NOT kind STREQUAL "function")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a function's declaration parsed as '${name}' (${kind}).")
endif()
zen_law_declared("static_assert(kTopRows + kBottomRows == 6, \"six\")${ZEN_SOH}" name kind)
if(NOT name STREQUAL "")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a static_assert was read as declaring '${name}'.")
endif()

# ---- sweep helpers -----------------------------------------------------------------------

function(zen_law_excluded rel out)
    foreach(pattern IN LISTS ZEN_LAW_EXCLUDE)
        if(rel MATCHES "${pattern}")
            set(${out} 1 PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out} 0 PARENT_SCOPE)
endfunction()

# Every current-facing file matching the globs, pruned before the walk (the same shape as
# check_doc_links.cmake, for the same measured reason).
function(zen_law_sweep globs out)
    set(root_globs "")
    foreach(g IN LISTS globs)
        list(APPEND root_globs "${ZEN_REPO}/${g}")
    endforeach()
    file(GLOB root_files RELATIVE "${ZEN_REPO}" ${root_globs})
    file(GLOB top_entries RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/*")
    set(nested_globs "")
    foreach(entry IN LISTS top_entries)
        if(IS_DIRECTORY "${ZEN_REPO}/${entry}")
            zen_law_excluded("${entry}/" skip)
            if(NOT skip)
                foreach(g IN LISTS globs)
                    list(APPEND nested_globs "${ZEN_REPO}/${entry}/${g}")
                endforeach()
            endif()
        endif()
    endforeach()
    set(nested_files "")
    if(nested_globs)
        file(GLOB_RECURSE nested_files RELATIVE "${ZEN_REPO}" ${nested_globs})
    endif()
    set(files "")
    foreach(rel IN LISTS root_files nested_files)
        zen_law_excluded("${rel}" skip)
        if(NOT skip)
            list(APPEND files "${rel}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES files)
    set(${out} "${files}" PARENT_SCOPE)
endfunction()

# ---- population 1: the witnesses -- every TEST_CASE / SUBCASE literal under tests/ ----------
#
# Adjacent string literals are joined first, as the language joins them, so a case name
# wrapped over two source lines is one name here. The blob holds one `TEST_CASE("...")` or
# `SUBCASE("...")` per line and is searched by exact line.

file(GLOB_RECURSE witness_files RELATIVE "${ZEN_REPO}"
     "${ZEN_REPO}/${ZEN_LAW_WITNESS_DIR}/*.cpp" "${ZEN_REPO}/${ZEN_LAW_WITNESS_DIR}/*.hpp")
set(witness_blob "\n")
set(witness_count 0)
set(witness_file_count 0)
foreach(rel IN LISTS witness_files)
    zen_law_excluded("${rel}" skip)
    if(skip)
        continue()
    endif()
    math(EXPR witness_file_count "${witness_file_count} + 1")
    zen_law_text("${rel}" content)
    string(REGEX REPLACE "\"[ \t]*\n[ \t]*\"" "" content "${content}")
    string(REGEX REPLACE "\"[ \t]+\"" "" content "${content}")
    string(REGEX MATCHALL "(TEST_CASE|SUBCASE)[ \t]*\\([ \t]*\"[^\"\n]*\"" hits "${content}")
    foreach(hit IN LISTS hits)
        string(REGEX REPLACE "^(TEST_CASE|SUBCASE)[ \t]*\\([ \t]*" "\\1(" hit "${hit}")
        string(APPEND witness_blob "${hit}\n")
        math(EXPR witness_count "${witness_count} + 1")
    endforeach()
endforeach()
if(witness_count EQUAL 0)
    message(FATAL_ERROR
        "law-register: no TEST_CASE or SUBCASE literal was found under ${ZEN_LAW_WITNESS_DIR}/ "
        "(${witness_file_count} files read). An expectation of nothing is satisfied by anything, "
        "so an empty witness population is a failure here and not a quiet pass.")
endif()

function(zen_law_witnessed quoted out)
    string(FIND "${witness_blob}" "\nTEST_CASE(${quoted}\n" a)
    string(FIND "${witness_blob}" "\nSUBCASE(${quoted}\n" b)
    if(a EQUAL -1 AND b EQUAL -1)
        set(${out} 0 PARENT_SCOPE)
    else()
        set(${out} 1 PARENT_SCOPE)
    endif()
endfunction()

# The witness half of the self-test needs the real blob: a case name read out of it moments
# ago must be accepted, and one no test declares refused.
string(REGEX MATCH "\nTEST_CASE\\(\"[^\"\n]*\"\n" selftest_case "${witness_blob}")
string(REGEX REPLACE "^\nTEST_CASE\\(|\n$" "" selftest_case "${selftest_case}")
zen_law_witnessed("${selftest_case}" yes)
zen_law_witnessed("\"zen-no-such-case-law-register-selftest\"" no)
if(NOT yes OR no)
    message(FATAL_ERROR
        "law-register: SELF-TEST FAILED -- the witness lookup answered ${yes} for a case name "
        "read out of the tests just now and ${no} for one no test declares.")
endif()

message(STATUS
    "law-register: self-test OK -- a bad heading, a commented-out identifier, a substring, a "
    "tagged pointer and an undeclared witness are refused; their well-formed twins are accepted")

# ---- population 2: the registers ---------------------------------------------------------

set(register_files "")
foreach(dir IN LISTS ZEN_LAW_REGISTER_DIRS)
    file(GLOB found RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/${dir}/*.md")
    list(APPEND register_files ${found})
endforeach()
list(SORT register_files)
list(LENGTH register_files register_count)
if(register_count EQUAL 0)
    message(FATAL_ERROR
        "law-register: no register was found under ${ZEN_LAW_REGISTER_DIRS}. An empty "
        "population is a failure here and not a quiet pass.")
endif()

set_property(GLOBAL PROPERTY zen_law_ids "")

# One register (or router), walked line by line. Everything an entry declares is recorded
# on GLOBAL properties keyed by its id; the structural rules are applied at the flush.
macro(zen_law_flush_entry)
    if(NOT id STREQUAL "")
        if(NOT retired)
            if(NOT law)
                zen_law_fail("${rel} ${id} has no LAW line")
            endif()
            if(NOT proven)
                zen_law_fail("${rel} ${id} has no PROVEN BY line")
            endif()
            if(lawlines GREATER 1)
                zen_law_fail("${rel} ${id} LAW wraps onto ${lawlines} lines (must be one)")
            endif()
            if(means GREATER ZEN_LAW_MEANS_MAX)
                zen_law_fail("${rel} ${id} has ${means} MEANS bullets (at most ${ZEN_LAW_MEANS_MAX})")
            endif()
            if(dnm GREATER ZEN_LAW_DNM_MAX)
                zen_law_fail("${rel} ${id} has ${dnm} DOES NOT MEAN bullets (at most ${ZEN_LAW_DNM_MAX})")
            endif()
        endif()
        if(why_count GREATER 1)
            zen_law_fail("${rel} ${id} has ${why_count} WHY lines (at most one)")
        endif()
        set_property(GLOBAL PROPERTY "zen_law_file_${id}" "${rel}")
        set_property(GLOBAL PROPERTY "zen_law_proven_${id}" "${proven_text}")
        set_property(GLOBAL PROPERTY "zen_law_why_${id}" "${why_target}")
        set_property(GLOBAL PROPERTY "zen_law_retired_${id}" "${retired}")
        set_property(GLOBAL APPEND PROPERTY zen_law_ids "${id}")
        set_property(GLOBAL APPEND PROPERTY "zen_law_ids_of_${relkey}" "${id}")
    endif()
    set(id "")
    set(retired 0)
    set(law 0)
    set(proven 0)
    set(lawlines 0)
    set(means 0)
    set(dnm 0)
    set(section "")
    set(proven_text "")
    set(why_count 0)
    set(why_target "")
endmacro()

function(zen_law_walk_register rel is_router)
    string(MAKE_C_IDENTIFIER "${rel}" relkey)
    zen_law_text("${rel}" content)
    string(REPLACE "\n" ";" lines "${content}")
    set(id "")
    zen_law_flush_entry()
    set(dna 0)
    set(n 0)
    foreach(line IN LISTS lines)
        math(EXPR n "${n} + 1")
        string(LENGTH "${line}" len)
        if(len GREATER ZEN_LAW_LINE_BYTES AND NOT line MATCHES "^LAW " AND NOT line MATCHES "^\\|")
            zen_law_fail("${rel}:${n} is ${len} bytes (at most ${ZEN_LAW_LINE_BYTES}; LAW and table rows excepted)")
        endif()
        if(line MATCHES "^SINCE")
            zen_law_fail("${rel}:${n} has a SINCE line; phase codes are retired, provenance is the WHY line's record")
        endif()
        if(line MATCHES "^## ")
            zen_law_flush_entry()
            if(is_router)
                continue()
            endif()
            if(line STREQUAL "## Do not assume")
                math(EXPR dna "${dna} + 1")
                if(dna GREATER 1)
                    zen_law_fail("${rel}:${n} is a second '## Do not assume'; a register carries one")
                endif()
                continue()
            endif()
            zen_law_entry_heading("${line}" id retired)
            if(id STREQUAL "")
                zen_law_show("${line}" shown)
                zen_law_fail("${rel}:${n} heading is neither a WL entry nor the one Do-not-assume: ${shown}")
                continue()
            endif()
            get_property(known GLOBAL PROPERTY "zen_law_file_${id}" SET)
            if(known)
                get_property(where GLOBAL PROPERTY "zen_law_file_${id}")
                zen_law_fail("${rel}:${n} duplicates id ${id}, already an entry of ${where}")
            endif()
            continue()
        endif()
        if(id STREQUAL "")
            continue()
        endif()
        if(line MATCHES "^LAW (—|--) ")
            set(law 1)
            set(section "law")
            set(lawlines 1)
            if(len GREATER ZEN_LAW_LAW_BYTES)
                zen_law_fail("${rel}:${n} ${id} LAW is ${len} bytes (at most ${ZEN_LAW_LAW_BYTES})")
            endif()
            continue()
        endif()
        if(section STREQUAL "law" AND NOT line MATCHES "^[ \t]*$")
            math(EXPR lawlines "${lawlines} + 1")
            continue()
        endif()
        if(line STREQUAL "MEANS")
            set(section "means")
            continue()
        endif()
        if(line STREQUAL "DOES NOT MEAN")
            set(section "dnm")
            continue()
        endif()
        if(line MATCHES "^PROVEN BY (—|--) ")
            set(proven 1)
            set(section "proven")
            set(proven_text "${line}")
            continue()
        endif()
        if(line MATCHES "^WHY (—|--) ")
            set(section "why")
            math(EXPR why_count "${why_count} + 1")
            if(line MATCHES "`([^`]*)`")
                set(why_target "${CMAKE_MATCH_1}")
            else()
                zen_law_fail("${rel}:${n} ${id} WHY names no backticked record")
            endif()
            continue()
        endif()
        if(line MATCHES "^[ \t]*$")
            if(section STREQUAL "law" OR section STREQUAL "proven")
                set(section "")
            endif()
            continue()
        endif()
        if(section STREQUAL "proven")
            string(APPEND proven_text " ${line}")
            continue()
        endif()
        if(line MATCHES "^- ")
            if(section STREQUAL "means")
                math(EXPR means "${means} + 1")
            elseif(section STREQUAL "dnm")
                math(EXPR dnm "${dnm} + 1")
            endif()
            continue()
        endif()
        if(line MATCHES "^  " AND (section STREQUAL "means" OR section STREQUAL "dnm"))
            zen_law_show("${line}" shown)
            zen_law_fail("${rel}:${n} ${id} has a wrapped bullet (each is one line): ${shown}")
            continue()
        endif()
    endforeach()
    zen_law_flush_entry()
endfunction()

foreach(rel IN LISTS register_files)
    file(SIZE "${ZEN_REPO}/${rel}" bytes)
    if(bytes GREATER ZEN_LAW_REGISTER_BYTES)
        zen_law_fail("${rel} is ${bytes} bytes (a register is at most ${ZEN_LAW_REGISTER_BYTES})")
    endif()
    zen_law_walk_register("${rel}" 0)
endforeach()
foreach(rel IN LISTS ZEN_LAW_ROUTERS)
    if(NOT EXISTS "${ZEN_REPO}/${rel}")
        zen_law_fail("router ${rel} does not exist")
        continue()
    endif()
    file(SIZE "${ZEN_REPO}/${rel}" bytes)
    if(bytes GREATER ZEN_LAW_ROUTER_BYTES)
        zen_law_fail("${rel} is ${bytes} bytes (a router is at most ${ZEN_LAW_ROUTER_BYTES})")
    endif()
    zen_law_walk_register("${rel}" 1)
endforeach()
file(SIZE "${ZEN_REPO}/${ZEN_LAW_CORE}" bytes)
if(bytes GREATER ZEN_LAW_CORE_BYTES)
    zen_law_fail("${ZEN_LAW_CORE} is ${bytes} bytes (at most ${ZEN_LAW_CORE_BYTES})")
endif()

get_property(all_ids GLOBAL PROPERTY zen_law_ids)
list(LENGTH all_ids entry_count)
if(entry_count EQUAL 0)
    message(FATAL_ERROR
        "law-register: ${register_count} register(s) yielded ZERO entries. Either the parser "
        "stopped recognising the entry form or every law is gone; both are failures, and "
        "neither is a green.")
endif()

# Ids are unique across agents/, not only across the registers: a `## WL-` heading in any
# other document under agents/ is a second copy of a law that has one home.
file(GLOB_RECURSE agents_md RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/agents/*.md")
foreach(rel IN LISTS agents_md)
    if(rel IN_LIST register_files)
        continue()
    endif()
    zen_law_text("${rel}" content)
    string(REGEX MATCHALL "\n## WL-[A-Z]+-[0-9]+" strays "\n${content}")
    foreach(stray IN LISTS strays)
        string(REGEX REPLACE "^\n## " "" stray "${stray}")
        zen_law_fail("${rel} carries a '## ${stray}' heading outside the registers; a law id has one home")
    endforeach()
    string(REGEX MATCHALL "\nSINCE" sinces "\n${content}")
    list(LENGTH sinces since_count)
    if(since_count GREATER 0)
        zen_law_fail("${rel} has ${since_count} SINCE line(s); phase codes are retired")
    endif()
endforeach()

# ---- population 3: PROVEN BY -- paths, identifiers, witnesses --------------------------------
#
# The paragraph is one line here. Quoted witnesses come out first and are removed, so a
# case name that itself contains backticks cannot shed fragments into the identifier walk;
# what remains in backticks is a path (which sets the file every later identifier is
# checked against) or an identifier.

set(path_count 0)
set(ident_count 0)
set(witness_checked 0)
set(rule_m_count 0)
foreach(id IN LISTS all_ids)
    get_property(rel GLOBAL PROPERTY "zen_law_file_${id}")
    get_property(para GLOBAL PROPERTY "zen_law_proven_${id}")
    if(para STREQUAL "")
        continue()
    endif()
    string(REGEX MATCHALL "\"[^\"]*\"" witnesses "${para}")
    foreach(w IN LISTS witnesses)
        math(EXPR witness_checked "${witness_checked} + 1")
        zen_law_witnessed("${w}" ok)
        if(NOT ok)
            zen_law_show("${w}" shown)
            zen_law_fail("${rel} ${id} witness is not a TEST_CASE/SUBCASE literal under ${ZEN_LAW_WITNESS_DIR}/: ${shown}")
        endif()
    endforeach()
    string(REGEX REPLACE "\"[^\"]*\"" "" rest "${para}")
    string(REGEX MATCHALL "`[^`]*`" tokens "${rest}")
    set(cur "")
    foreach(tok IN LISTS tokens)
        string(REGEX REPLACE "^`|`$" "" tok "${tok}")
        string(STRIP "${tok}" tok)
        if(tok STREQUAL "")
            continue()
        endif()
        if(tok MATCHES "${ZEN_LAW_PATH_RE}")
            math(EXPR path_count "${path_count} + 1")
            set(cur "${tok}")
            if(NOT EXISTS "${ZEN_REPO}/${tok}")
                zen_law_fail("${rel} ${id} PROVEN BY names a path that does not exist: ${tok}")
                set(cur "")
            endif()
            continue()
        endif()
        math(EXPR ident_count "${ident_count} + 1")
        zen_law_show("${tok}" shown)
        if(cur STREQUAL "")
            zen_law_fail("${rel} ${id} PROVEN BY names identifier ${shown} before any existing path")
            continue()
        endif()
        zen_law_text("${cur}" text)
        zen_law_mentions("${text}" "${tok}" mentioned)
        if(NOT mentioned)
            zen_law_fail("${rel} ${id} PROVEN BY names ${shown}, which does not occur in ${cur}")
            continue()
        endif()
        string(MAKE_C_IDENTIFIER "${cur}" curkey)
        set_property(GLOBAL APPEND PROPERTY "zen_law_owns_${id}_${curkey}" "${tok}")
        if(cur MATCHES "\\.(h|hpp|ipp|c|cc|cpp|cxx)$")
            zen_law_code("${cur}" code)
            zen_law_token_in("${code}" "${tok}" in_code)
            if(NOT in_code)
                math(EXPR rule_m_count "${rule_m_count} + 1")
                zen_law_strict(m "${rel} ${id}: ${shown} is not a whole token in the code of ${cur} (comments stripped)")
            endif()
        endif()
    endforeach()
endforeach()
if(witness_checked EQUAL 0)
    message(FATAL_ERROR
        "law-register: ${entry_count} entries cited ZERO witnesses. Either the extractor "
        "stopped recognising a quoted witness or every law lost its proof; both are failures.")
endif()

# ---- population 4: decision records -- WHY targets and reciprocity ------------------------------

set(why_count 0)
set(why_targets "")
foreach(id IN LISTS all_ids)
    get_property(target GLOBAL PROPERTY "zen_law_why_${id}")
    if(target STREQUAL "")
        continue()
    endif()
    math(EXPR why_count "${why_count} + 1")
    list(APPEND why_targets "${target}")
    if(NOT EXISTS "${ZEN_REPO}/${target}")
        get_property(rel GLOBAL PROPERTY "zen_law_file_${id}")
        zen_law_fail("${rel} ${id} WHY names a record that does not exist: ${target}")
    endif()
endforeach()
list(REMOVE_DUPLICATES why_targets)
list(LENGTH why_targets why_target_count)

file(GLOB record_files RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/${ZEN_LAW_RECORD_DIR}/*.md")
foreach(target IN LISTS why_targets)
    if(EXISTS "${ZEN_REPO}/${target}" AND NOT target IN_LIST record_files)
        list(APPEND record_files "${target}")
    endif()
endforeach()
list(SORT record_files)
list(LENGTH record_files record_count)
foreach(rel IN LISTS record_files)
    zen_law_text("${rel}" content)
    string(FIND "${content}" "**Laws supported.**" at)
    set(listed "")
    if(NOT at EQUAL -1)
        string(SUBSTRING "${content}" ${at} -1 tail)
        string(REGEX MATCHALL "WL-[A-Z]+-[0-9]+" listed "${tail}")
        if(listed)
            list(REMOVE_DUPLICATES listed)
        endif()
    endif()
    if(NOT listed)
        zen_law_fail("${rel} lists no laws under '**Laws supported.**'")
    endif()
    foreach(id IN LISTS listed)
        if(NOT id IN_LIST all_ids)
            zen_law_fail("${rel} lists ${id}, which is no entry of any register")
            continue()
        endif()
        get_property(target GLOBAL PROPERTY "zen_law_why_${id}")
        if(NOT target STREQUAL "${rel}")
            if(target STREQUAL "")
                set(target "nothing")
            endif()
            zen_law_fail("${rel} lists ${id}, whose WHY names ${target}")
        endif()
    endforeach()
    foreach(id IN LISTS all_ids)
        get_property(target GLOBAL PROPERTY "zen_law_why_${id}")
        if(target STREQUAL "${rel}" AND NOT id IN_LIST listed)
            zen_law_fail("${id}'s WHY names ${rel}, which does not list it")
        endif()
    endforeach()
endforeach()

# ---- population 5: source pointers ---------------------------------------------------------
#
# Every first-party C/C++ file is read; the ones carrying no `WL-` are skipped whole. A
# pointer line is a line of its own beginning `// WL-`. Rule n walks on from each pointer to
# the next code line and asks whether a law on the pointer names what is declared there.

zen_law_sweep("${ZEN_LAW_SOURCE_GLOBS}" source_files)
list(LENGTH source_files source_count)
if(source_count EQUAL 0)
    message(FATAL_ERROR "law-register: the sweep found no first-party C/C++ source at all; check ZEN_REPO.")
endif()

set(pointer_lines 0)
set(pointer_ids 0)
set(header_pointers 0)
set(pointer_files 0)
set(rule_n_count 0)
set(rule_n_pointers 0)
foreach(rel IN LISTS source_files)
    zen_law_text("${rel}" content)
    string(FIND "${content}" "WL-" any)
    string(FIND "${content}" "// Workshop law:" anyheader)
    if(any EQUAL -1 AND anyheader EQUAL -1)
        continue()
    endif()
    math(EXPR pointer_files "${pointer_files} + 1")
    string(MAKE_C_IDENTIFIER "${rel}" relkey)
    string(REPLACE "\n" ";" lines "${content}")
    set(pending "")
    set(n 0)
    foreach(line IN LISTS lines)
        math(EXPR n "${n} + 1")
        if(line MATCHES "^[ \t]*// Workshop law:")
            math(EXPR header_pointers "${header_pointers} + 1")
            string(REGEX MATCHALL "[A-Za-z0-9_./-]+\\.md" named "${line}")
            foreach(path IN LISTS named)
                if(NOT EXISTS "${ZEN_REPO}/${path}")
                    zen_law_fail("${rel}:${n} header pointer names a file that does not exist: ${path}")
                endif()
            endforeach()
            continue()
        endif()
        if(line MATCHES "^[ \t]*// WL-")
            math(EXPR pointer_lines "${pointer_lines} + 1")
            zen_law_parse_pointer("${line}" ok segments)
            if(NOT ok)
                zen_law_show("${line}" shown)
                string(STRIP "${shown}" shown)
                zen_law_fail("${rel}:${n} is not a well-formed pointer (`// WL-X-NN[, ...] -- agents/<area>/<register>.md[; ...]`): ${shown}")
                continue()
            endif()
            set(line_ids "")
            foreach(segment IN LISTS segments)
                string(REGEX MATCH "^([^|]*)\\|(.*)$" _ "${segment}")
                set(register "${CMAKE_MATCH_1}")
                string(REPLACE "," ";" ids "${CMAKE_MATCH_2}")
                if(NOT EXISTS "${ZEN_REPO}/${register}")
                    zen_law_fail("${rel}:${n} pointer names a register that does not exist: ${register}")
                    continue()
                endif()
                string(MAKE_C_IDENTIFIER "${register}" regkey)
                get_property(entries GLOBAL PROPERTY "zen_law_ids_of_${regkey}")
                foreach(id IN LISTS ids)
                    math(EXPR pointer_ids "${pointer_ids} + 1")
                    list(APPEND line_ids "${id}")
                    if(NOT id IN_LIST all_ids)
                        zen_law_fail("${rel}:${n} pointer names ${id}, which is no entry of any register")
                    elseif(NOT id IN_LIST entries)
                        get_property(where GLOBAL PROPERTY "zen_law_file_${id}")
                        zen_law_fail("${rel}:${n} pointer names ${id} under ${register}, but it is an entry of ${where}")
                    endif()
                endforeach()
            endforeach()
            if(line_ids)
                string(REPLACE ";" "," joined "${line_ids}")
                list(APPEND pending "${n}|${joined}")
            endif()
            continue()
        endif()
        if(NOT pending)
            continue()
        endif()
        # Lines a pointer looks past on its way to the declaration.
        if(line MATCHES "^[ \t]*$" OR line MATCHES "^[ \t]*//" OR line MATCHES "^[ \t]*#"
           OR line MATCHES "^[ \t]*template[ \t]*<" OR line MATCHES "^[ \t]*(public|private|protected):[ \t]*$"
           OR line MATCHES "^[ \t]*${ZEN_STX}${ZEN_STX}[^${ZEN_ETX}]*${ZEN_ETX}${ZEN_ETX}[ \t]*$")
            continue()
        endif()
        zen_law_declared("${line}" name kind)
        zen_law_show("${line}" shown_line)
        string(STRIP "${shown_line}" shown_line)
        foreach(p IN LISTS pending)
            string(REGEX MATCH "^([0-9]+)\\|(.*)$" _ "${p}")
            set(at "${CMAKE_MATCH_1}")
            set(idtext "${CMAKE_MATCH_2}")
            string(REPLACE "," ";" ids "${idtext}")
            math(EXPR rule_n_pointers "${rule_n_pointers} + 1")
            if(name STREQUAL "")
                math(EXPR rule_n_count "${rule_n_count} + 1")
                zen_law_strict(n "${rel}:${at} (${idtext}) is followed by no declaration this parse can name (line ${n}): ${shown_line}")
                continue()
            endif()
            set(named 0)
            foreach(id IN LISTS ids)
                get_property(owned GLOBAL PROPERTY "zen_law_owns_${id}_${relkey}")
                foreach(o IN LISTS owned)
                    if(o STREQUAL "${name}" OR o MATCHES "::${name}$")
                        set(named 1)
                    elseif(kind STREQUAL "scope" AND o MATCHES "^${name}::")
                        set(named 1)
                    endif()
                endforeach()
            endforeach()
            if(NOT named)
                math(EXPR rule_n_count "${rule_n_count} + 1")
                zen_law_strict(n "${rel}:${at} (${idtext}) points at ${kind} `${name}` (line ${n}), which no law on the line names under ${rel}")
            endif()
        endforeach()
        set(pending "")
    endforeach()
    foreach(p IN LISTS pending)
        string(REGEX MATCH "^([0-9]+)\\|(.*)$" _ "${p}")
        set(at "${CMAKE_MATCH_1}")
        set(idtext "${CMAKE_MATCH_2}")
        math(EXPR rule_n_pointers "${rule_n_pointers} + 1")
        math(EXPR rule_n_count "${rule_n_count} + 1")
        zen_law_strict(n "${rel}:${at} (${idtext}) is followed by no code line at all")
    endforeach()
endforeach()
if(pointer_lines EQUAL 0)
    message(FATAL_ERROR
        "law-register: ${source_count} first-party C/C++ files carry ZERO `// WL-` pointer lines. "
        "The router's rule 3 puts one above every declaration a law names; none at all means "
        "the sweep is not reading this repository.")
endif()

# ---- the report --------------------------------------------------------------------------

message(STATUS "law-register: ${register_count} registers, ${entry_count} entries; ${record_count} "
               "records, ${why_target_count} WHY targets, ${why_count} WHY lines")
message(STATUS "law-register: ${path_count} PROVEN BY paths, ${ident_count} identifiers, "
               "${witness_checked} witnesses checked against ${witness_count} TEST_CASE/SUBCASE "
               "literals in ${witness_file_count} files")
message(STATUS "law-register: ${pointer_files} source files carry pointers -- ${pointer_lines} "
               "pointer lines, ${pointer_ids} pointer ids, ${header_pointers} header pointers")

get_property(rule_m GLOBAL PROPERTY zen_law_rule_m)
get_property(rule_n GLOBAL PROPERTY zen_law_rule_n)
if(LAW_REGISTER_STRICT)
    set(strict_word "ON")
else()
    set(strict_word "OFF")
endif()
foreach(which m n)
    if(which STREQUAL "m")
        set(text "${rule_m}")
        set(count "${rule_m_count}")
        set(what "identifier(s) present only outside the code of the file claiming them")
    else()
        set(text "${rule_n}")
        set(count "${rule_n_count}")
        set(what "pointer line(s) whose declaration no law on the line names (of ${rule_n_pointers})")
    endif()
    zen_law_show("${text}" text)
    message(STATUS "law-register: STRICT ${strict_word} -- rule ${which}: ${count} ${what}")
    if(NOT text STREQUAL "")
        string(REGEX REPLACE "\n$" "" text "${text}")
        string(REPLACE "\n" "\n  " text "  ${text}")
        message("${text}")
    endif()
endforeach()

get_property(problems GLOBAL PROPERTY zen_law_problems)
if(LAW_REGISTER_STRICT)
    string(APPEND problems "${rule_m}${rule_n}")
endif()
if(NOT problems STREQUAL "")
    zen_law_show("${problems}" problems)
    zen_law_count_lines("${problems}" problem_count)
    string(REGEX REPLACE "\n$" "" problems "${problems}")
    string(REPLACE "\n" "\n  " problems "  ${problems}")
    message(FATAL_ERROR
        "law-register FAILED: ${problem_count} problem(s).\n"
        "${problems}\n\n"
        "  A register entry that contradicts a passing test is the thing that is wrong: fix "
        "downward (tests > code > register > decision record), never upward. The form is the "
        "router's (agents/workshop.md, Ongoing rules).")
endif()

message(STATUS "law-register: PASSED -- every register is well-formed and every name it makes resolves"
               " (STRICT ${strict_word})")
