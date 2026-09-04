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
#                     under agents/; each file is under its byte budget; an entry whose
#                     PROVEN BY says `witness: none` is repeated under its register's
#                     `## Do not assume`, and a Do-not-assume bullet that says those words
#                     names only entries of its own register that write them; a law
#                     witnessed except for one clause writes `UNWITNESSED -- <clause>` on
#                     the line after PROVEN BY, and that debt is repeated and reciprocal
#                     the same way, by the word UNWITNESSED
#   PROVEN BY         every backticked path exists; every backticked identifier occurs in
#                     the file named before it; every quoted witness is a TEST_CASE or
#                     SUBCASE literal under tests/
#   decision records  every WHY target exists; a record's "Laws supported" is exactly the
#                     set of ids whose WHY names it
#   source pointers   every `// WL-... -- agents/workshop/<register>.md` line names
#                     registers that exist and ids that are entries of the register named
#                     on that line; every `// Workshop law:` header names existing files
#   method registers  every `##` under a method-register directory (agents/verification/) is
#                     one VM entry, or the one table heading `## Where a case goes`; an entry
#                     has a METHOD of one line whose sentence is at most 210 bytes, a BECAUSE
#                     of at most two lines, and a SEEN that is `nowhere yet` or names paths,
#                     identifiers and witnesses, resolved exactly as a PROVEN BY's are; no
#                     line of an entry carries a phase tag; VM ids are unique under agents/
#                     together with the WL ids
#   budgets           every *.md under agents/ is within its byte budget -- a router 8,192, a
#                     register and every other file under agents/ 16,384, AGENTS.md 20,480;
#                     the routed documents not yet turned into registers are named in
#                     ZEN_LAW_UNBUDGETED and must stay OVER the register budget while they
#                     are, so that list can only shrink; a decision record over 4,096 bytes
#                     is counted and printed, never failed (AGENTS.md rule i says flagged)
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
# TWO CHECKS ARE STRICTER THAN THE PROTOTYPE, and they are behind LAW_REGISTER_STRICT,
# ON by default since the lists they produce were worked down to nothing:
#
#   rule m   an owner identifier is present only if it occurs as a whole token in the CODE
#            of the named file, `//` comments and `/* */` comments stripped. The lenient
#            check above is satisfied by a mention in a comment -- measured: 24 wrong
#            attributions survived three steps that way -- and a substring (`Rect` inside
#            `SurfaceRect`). A member is spelled `Struct::member` and an overload
#            `name(Type)`; both are read as their parts, each of which must be a whole
#            token in the code (zen_law_token_parts() below).
#   rule n   a `// WL-...` pointer is PROVEN BY inverted: for each pointer line, the
#            declaration on the next code line is named by the PROVEN BY of EVERY law on
#            that line, under this file -- an id whose law does not name the declaration
#            is a content citation, and the pointer form has no room for one. What "the
#            declaration" means here is a heuristic parse, stated at zen_law_declared()
#            below. A qualified spelling `Scope::name` names the declaration only when
#            the declaration is a member of Scope -- declared inside `struct|class Scope {`
#            (or a namespace of that name), or defined out of line as `Scope::name` -- and
#            is refused above a declaration of another scope: `LayoutTabPress::create`
#            does not name the free function `create` (zen_law_pointer_names() below;
#            measured: that respelling sat green for a phase under a suffix match).
#
# With STRICT ON, the default, both fail the entry; -DLAW_REGISTER_STRICT=OFF prints their
# lists and a count without failing, which is the setting for a phase working a list down.
#
# WHAT IT DELIBERATELY DOES NOT DO
#
#   * it does not know whether a law is TRUE. A phase that edits a witnessed TEST_CASE
#     re-verifies every law naming it, in the same commit; that rule is procedural and
#     lives in the router, because only the phase that changed a test knows;
#   * it does not count witnesses per law. `witness: none` is a written debt, and so is
#     `UNWITNESSED -- <clause>` at the size of one clause; each is repeated under the
#     register's `## Do not assume`, and what is checked is that a debt is written in both
#     places or in neither, never how many there are, and the router forbids lowering it;
#   * it does not police prose width in decision records, only in registers and routers.
#
# HONEST LIMITS OF THE PARSE, all in the direction of a visible red rather than a quiet
# green: a `//` inside a string literal is read as a comment start (rule m then sees less
# code, never more); a pointer written after code on the same line is not a pointer line,
# and a comment line that BEGINS `// WL-` is read as a pointer and fails as malformed when
# it is prose (reword the comment; the check does not guess); a `## WL-...-NN -- RETIRED`
# heading is an entry with no LAW or PROVEN BY owed, exactly as the router says a retired
# law keeps its number and one line.
#
# THE SELF-TEST IS NOT OPTIONAL. A well-formed tree and a checker that finds nothing produce
# byte-identical output, so before answering it makes each predicate say NO -- a bad
# heading, an identifier that is only in a comment, a witness no test declares, a malformed
# pointer, a phase tag, a VM entry with a long METHOD, a three-line BECAUSE and a stray
# heading -- and say YES to their well-formed twins, one of them a case name read out of
# the real test sources at runtime. The VM walker is exercised on a synthetic register held
# in this file, never on a file written to the tree.
#
#   cmake -P tests/check_law_register.cmake                          (from the repository root)
#   cmake -DLAW_REGISTER_STRICT=OFF -P tests/check_law_register.cmake
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
    set(LAW_REGISTER_STRICT ON)
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

# The method registers (the VM form), their router, and the one heading a method register may
# carry that is not an entry. A phase tag is a parenthesised token shaped like an id whose
# family is not one of these law families; the families are the only list this check keeps.
set(ZEN_VM_REGISTER_DIRS agents/verification)
set(ZEN_VM_ROUTERS agents/verification.md)
set(ZEN_VM_TABLE_HEADING "## Where a case goes")
set(ZEN_VM_ID_FAMILIES WL VM TIMER POP KERN)

# Every *.md under this directory is budgeted (below). The routed documents that are not yet
# registers are over the register budget today and are named here, so the sweep can demand
# that each one either stays over it or leaves this list -- the list shrinks, never grows.
set(ZEN_LAW_AGENTS_DIR agents)
set(ZEN_LAW_UNBUDGETED agents/operators.md agents/panes.md agents/realization.md agents/surface.md)

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
# A METHOD's budget is measured on the sentence after `METHOD -- `, which is the measure the
# sentences were reviewed at; a LAW's is measured on the whole line, as the registers were
# written to. A record over ZEN_LAW_RECORD_FLAG_BYTES is counted, not failed.
set(ZEN_VM_METHOD_BYTES 210)
set(ZEN_VM_BECAUSE_MAX 2)
set(ZEN_LAW_RECORD_FLAG_BYTES 4096)

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
set(ZEN_LAW_PATH_RE "^[A-Za-z0-9_./-]+\\.(hpp|cpp|h|ipp|cc|cxx|c|md|txt|cmake|json|in|yml|yaml|py|sh|tsv)$")

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

# A PROVEN BY identifier is one token, a qualified spelling `Scope::member` (any depth), or
# an overload spelling `name(Type)` -- and its PARTS are what the file is asked for.
# `Row::section` asks for `Row` and `section`; `on(PointerWheel)` asks for `on` and
# `PointerWheel`. The check reads the parts and not their relation: a member spelled under
# the wrong scope passes when both names occur in the file, so the scope is the register's
# claim, not the check's.
function(zen_law_token_parts token out)
    set(t "${token}")
    if(t MATCHES "^([A-Za-z_][A-Za-z0-9_:]*)\\(([A-Za-z_][A-Za-z0-9_:]*)\\)$")
        set(t "${CMAKE_MATCH_1}::${CMAKE_MATCH_2}")
    endif()
    string(REPLACE "::" ";" pieces "${t}")
    set(parts "")
    foreach(piece IN LISTS pieces)
        if(NOT piece STREQUAL "")
            list(APPEND parts "${piece}")
        endif()
    endforeach()
    set(${out} "${parts}" PARENT_SCOPE)
endfunction()

# Lenient: every part occurs somewhere in the text, comments included, as a substring.
# This is the prototype's check and the one the lane enforces with STRICT OFF.
function(zen_law_mentions text token out)
    zen_law_token_parts("${token}" parts)
    foreach(part IN LISTS parts)
        string(FIND "${text}" "${part}" pos)
        if(pos EQUAL -1)
            set(${out} 0 PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out} 1 PARENT_SCOPE)
endfunction()

# Strict (rule m): every part occurs as a whole token -- bounded by non-identifier
# characters or the ends -- in text that should already have had its comments stripped.
# `:` is a boundary, so a bare `section` is also found inside `Row::section`.
function(zen_law_token_in code token out)
    zen_law_token_parts("${token}" parts)
    foreach(part IN LISTS parts)
        zen_law_regex_escape("${part}" t)
        if(NOT code MATCHES "(^|[^A-Za-z0-9_])${t}([^A-Za-z0-9_]|$)")
            set(${out} 0 PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out} 1 PARENT_SCOPE)
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

# A method-register heading. Sets ${out_id} to the VM id, or to "" for anything else -- a WL
# heading included, since a law id has one home and it is not a method register.
function(zen_vm_entry_heading line out_id)
    set(${out_id} "" PARENT_SCOPE)
    if(line MATCHES "^## (VM-[A-Z]+-[0-9][0-9]) (—|--) (.+)$")
        set(${out_id} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    endif()
endfunction()

# A phase tag on a line: a parenthesised token shaped like an id -- letters, a dash, an optional
# letter, a digit, whatever follows to the closing parenthesis -- whose family is not a law
# family (ZEN_VM_ID_FAMILIES). Quoted text is removed first: a witness title may carry a fossil,
# and the router says a tag inside a TEST_CASE literal is one. Sets ${out} to the first tag
# found, or "".
function(zen_vm_phase_tag line out)
    set(${out} "" PARENT_SCOPE)
    string(REGEX REPLACE "\"[^\"]*\"" "" bare "${line}")
    string(REGEX MATCHALL "\\([A-Z]+-[A-Z]?[0-9][^)]*\\)" tags "${bare}")
    foreach(tag IN LISTS tags)
        string(REGEX REPLACE "^\\(([A-Z]+)-.*$" "\\1" family "${tag}")
        if(NOT family IN_LIST ZEN_VM_ID_FAMILIES)
            set(${out} "${tag}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
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
# line declares nothing it can name, ${out_kind} to one of: scope | alias | function |
# variable | none, and ${out_qualifier} to the scope an out-of-line definition names before
# its `::` (`WorkshopWeave` for `void WorkshopWeave::quit() {`; `A::B` for `A::B::f`), ""
# for an unqualified declaration.
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
#     is the last identifier before it (a function or a constructor), and ${out_param} is
#     the type of its FIRST parameter -- the last identifier before the parameter's `&` or
#     `*`, or the identifier before its name when it has neither; `const`, `volatile` and
#     the template arguments are not it -- so that an overload set is told apart by the
#     spelling `name(Type)`; else the name is the last identifier before the first `{`,
#     `;`, `,` or `[` (a member, a constant, an enumerator);
#   * a name that is a C++ keyword (`else`, `return`, `operator`, ...) -> none.
#
# The line handed in is the first line after the pointer that is not blank, not a comment,
# not a preprocessor line, not a bare `template <...>` head (a one-line forward declaration
# such as `template <class T> struct TextForm;` IS the declaration, its head stripped), not
# a bare attribute, and not an access specifier; a closing brace is handed in and yields
# none. A line that holds none of
# `(`, `=`, `{`, `;`, `,` and starts with no declaring keyword is a return type (or a type)
# on a line of its own, and the next code line is joined to it before the parse.
set(ZEN_LAW_KEYWORDS
    alignas alignof asm auto bool break case catch char class const constexpr continue
    decltype default delete do double else enum explicit export extern false float for
    friend goto if inline int long mutable namespace new noexcept nullptr operator override
    final private protected public register return short signed sizeof static struct switch
    template this throw true try typedef typeid typename union unsigned using virtual void
    volatile while)

function(zen_law_declared line out_name out_kind out_param out_qualifier)
    set(${out_name} "" PARENT_SCOPE)
    set(${out_kind} "none" PARENT_SCOPE)
    set(${out_param} "" PARENT_SCOPE)
    set(${out_qualifier} "" PARENT_SCOPE)
    string(REGEX REPLACE "//.*$" "" l "${line}")
    string(STRIP "${l}" l)
    foreach(round RANGE 1 4)
        string(REGEX REPLACE "<[^<>]*>" "" l "${l}")
    endforeach()
    string(REGEX REPLACE "${ZEN_STX}${ZEN_STX}[^${ZEN_ETX}]*${ZEN_ETX}${ZEN_ETX}" "" l "${l}")
    string(STRIP "${l}" l)
    string(REGEX REPLACE "^template[ \t]+" "" l "${l}")
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
    set(param "")
    if(NOT eq EQUAL -1 AND (par EQUAL -1 OR eq LESS par))
        string(SUBSTRING "${l}" 0 ${eq} head)
    elseif(NOT par EQUAL -1)
        string(SUBSTRING "${l}" 0 ${par} head)
        set(kind "function")
        # The first parameter's type, for the `name(Type)` spelling.
        math(EXPR after "${par} + 1")
        string(SUBSTRING "${l}" ${after} -1 tail)
        string(FIND "${tail}" "," comma)
        string(FIND "${tail}" ")" close)
        set(cut ${comma})
        if(NOT close EQUAL -1 AND (cut EQUAL -1 OR close LESS cut))
            set(cut ${close})
        endif()
        if(NOT cut EQUAL -1)
            string(SUBSTRING "${tail}" 0 ${cut} tail)
        endif()
        string(FIND "${tail}" "&" amp)
        string(FIND "${tail}" "*" star)
        set(ref ${amp})
        if(NOT star EQUAL -1 AND (ref EQUAL -1 OR star LESS ref))
            set(ref ${star})
        endif()
        if(NOT ref EQUAL -1)
            string(SUBSTRING "${tail}" 0 ${ref} tail)
        endif()
        string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*" pwords "${tail}")
        if(pwords)
            list(REMOVE_ITEM pwords const volatile struct class enum)
        endif()
        list(LENGTH pwords np)
        if(np GREATER 0)
            if(ref EQUAL -1 AND np GREATER 1)
                math(EXPR idx "${np} - 2")
                list(GET pwords ${idx} param)
            else()
                list(GET pwords -1 param)
            endif()
        endif()
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
    # An out-of-line definition's qualifier: the `A::B::` run ending at the name.
    set(qualifier "")
    string(STRIP "${head}" head_stripped)
    if(head_stripped MATCHES "(([A-Za-z_][A-Za-z0-9_]*::)+)${name}$")
        string(REGEX REPLACE "::$" "" qualifier "${CMAKE_MATCH_1}")
    endif()
    set(${out_name} "${name}" PARENT_SCOPE)
    set(${out_kind} "${kind}" PARENT_SCOPE)
    set(${out_param} "${param}" PARENT_SCOPE)
    set(${out_qualifier} "${qualifier}" PARENT_SCOPE)
endfunction()

# THE SCOPE A CODE LINE SITS IN (rule n's qualifier check). Brace depth is counted on the
# line with its `//` comment and its string and character literals removed (an escaped
# character first -- the backslash arrives as ZEN_EOT); a scope opener
# `[template <...>] namespace|struct|class|union|enum [class|struct] NAME ... {` pushes NAME
# with the depth inside it (the head stripped is one balanced `<...>` and the name stops at
# its own `<`, so `template <> struct TextForm<ui::Extent> {` pushes TextForm -- measured: a
# greedy strip swallowed that opener whole and four members lost their scope), an opener
# whose `{` is on a later line (`struct Foo` / `: Base {`)
# waits in ${pending_var} until that brace, a forward declaration (`;` on the line, no `{`)
# pushes nothing, and a scope is popped when the depth falls below the one it opened. A
# function body, a lambda, an initializer: braces counted, nothing pushed. Three variables
# are carried by name: the depth, the stack (`NAME|inner depth` entries) and the pending
# opener. zen_law_scope_path() joins the stack's names with `::`.
function(zen_law_scope_step line depth_var stack_var pending_var)
    set(depth "${${depth_var}}")
    set(stack "${${stack_var}}")
    set(pending "${${pending_var}}")
    string(REGEX REPLACE "//.*$" "" l "${line}")
    if(l MATCHES "^[ \t]*#")
        return()
    endif()
    # Most lines open nothing and close nothing: two finds and one anchored match settle them.
    string(FIND "${l}" "{" quick_open)
    string(FIND "${l}" "}" quick_close)
    if(quick_open EQUAL -1 AND quick_close EQUAL -1 AND pending STREQUAL ""
       AND NOT l MATCHES "^[ \t]*(template|namespace|struct|class|union|enum)[ \t<]")
        return()
    endif()
    string(REGEX REPLACE "${ZEN_EOT}." "" l "${l}")
    string(REGEX REPLACE "\"[^\"]*\"" "" l "${l}")
    string(REGEX REPLACE "'[^']*'" "" l "${l}")
    string(REGEX REPLACE "^[ \t]*template[ \t]*<[^<>]*>[ \t]*" "" h "${l}")
    string(STRIP "${h}" h)
    string(FIND "${l}" "{" has_open)
    string(FIND "${l}" "${ZEN_SOH}" has_semi)
    set(opener "")
    if(h MATCHES "^(namespace|struct|class|union|enum class|enum struct|enum)[ \t]+([A-Za-z_][A-Za-z0-9_:]*)")
        set(pending "")
        if(NOT has_open EQUAL -1)
            set(opener "${CMAKE_MATCH_2}")
        elseif(has_semi EQUAL -1)
            set(pending "${CMAKE_MATCH_2}")
        endif()
    elseif(NOT pending STREQUAL "")
        if(NOT has_open EQUAL -1)
            set(opener "${pending}")
            set(pending "")
        elseif(NOT has_semi EQUAL -1)
            set(pending "")
        endif()
    endif()
    string(REGEX MATCHALL "{" opens "${l}")
    string(REGEX MATCHALL "}" closes "${l}")
    list(LENGTH opens n_open)
    list(LENGTH closes n_close)
    if(NOT opener STREQUAL "" AND n_open GREATER n_close)
        math(EXPR inner "${depth} + 1")
        list(APPEND stack "${opener}|${inner}")
    endif()
    math(EXPR depth "${depth} + ${n_open} - ${n_close}")
    while(stack)
        list(GET stack -1 top)
        string(REGEX REPLACE "^.*\\|" "" opened_at "${top}")
        if(depth LESS opened_at)
            list(REMOVE_AT stack -1)
        else()
            break()
        endif()
    endwhile()
    set(${depth_var} "${depth}" PARENT_SCOPE)
    set(${stack_var} "${stack}" PARENT_SCOPE)
    set(${pending_var} "${pending}" PARENT_SCOPE)
endfunction()

function(zen_law_scope_path stack out)
    set(names "")
    foreach(entry IN LISTS stack)
        string(REGEX REPLACE "\\|.*$" "" name "${entry}")
        list(APPEND names "${name}")
    endforeach()
    string(REPLACE ";" "::" path "${names}")
    set(${out} "${path}" PARENT_SCOPE)
endfunction()

# DOES A LAW NAME THE DECLARATION UNDER A POINTER (rule n)? `owned` is what the law's PROVEN
# BY spells under this file; `name`, `kind` and `param` are zen_law_declared()'s reading of
# the line; `scope` is the path the line sits in -- the enclosing namespaces and classes, then
# an out-of-line definition's own qualifier. The bare name names it; `Scope::name` (and the
# overload form `Scope::name(Type)`) names it only when `scope` is Scope or ends in `::Scope`;
# above a scope's own declaration, a law spelling any `NAME::member` names the scope.
function(zen_law_pointer_names owned name kind param scope out)
    set(named 0)
    foreach(o IN LISTS owned)
        if(o STREQUAL "${name}")
            set(named 1)
        elseif(kind STREQUAL "scope" AND o MATCHES "^${name}::")
            set(named 1)
        elseif(NOT param STREQUAL "" AND o STREQUAL "${name}(${param})")
            set(named 1)
        else()
            set(q "")
            if(o MATCHES "^(.+)::${name}$")
                set(q "${CMAKE_MATCH_1}")
            elseif(NOT param STREQUAL "" AND o MATCHES "^(.+)::${name}\\(${param}\\)$")
                set(q "${CMAKE_MATCH_1}")
            endif()
            if(NOT q STREQUAL "")
                zen_law_regex_escape("${q}" q_re)
                if(scope STREQUAL "${q}" OR scope MATCHES "::${q_re}$")
                    set(named 1)
                endif()
            endif()
        endif()
    endforeach()
    set(${out} "${named}" PARENT_SCOPE)
endfunction()

# ---- witness debts: `witness: none`, `UNWITNESSED -- <clause>`, and their echoes ------------
#
# A law with no witness writes `witness: none` in its PROVEN BY; a law witnessed except for
# one clause writes `UNWITNESSED -- <clause>` on the line after PROVEN BY. Either debt is
# repeated in a bullet under its register's `## Do not assume` that says the same word, so it
# is visible in both places and neither copy can quietly outlive the other. The predicates
# take the marker word, so the two debts are one mechanism and one self-test, each predicate
# in one place so the self-test exercises the real one.

function(zen_law_proven_owes proven out)
    if(proven MATCHES "witness: none")
        set(${out} 1 PARENT_SCOPE)
    else()
        set(${out} 0 PARENT_SCOPE)
    endif()
endfunction()

# The clause an `UNWITNESSED -- <clause>` line names, or "" for any other line (including an
# UNWITNESSED line that names none, which the walker then fails as malformed).
function(zen_law_unwitnessed_clause line out)
    set(${out} "" PARENT_SCOPE)
    if(line MATCHES "^UNWITNESSED (—|--) (.+)$")
        string(STRIP "${CMAKE_MATCH_2}" clause)
        set(${out} "${clause}" PARENT_SCOPE)
    endif()
endfunction()

# The ids a Do-not-assume bullet repeats as debts of one kind: every WL id on a bullet that
# says the marker (`witness: none` or `UNWITNESSED`), and nothing at all for a bullet that
# does not say it, whatever else it names -- the prose bullets that cite a law for another
# reason are not debt statements.
function(zen_law_debt_ids bullet marker out)
    set(${out} "" PARENT_SCOPE)
    string(FIND "${bullet}" "${marker}" said)
    if(said EQUAL -1)
        return()
    endif()
    string(REGEX MATCHALL "WL-[A-Z]+-[0-9]+" ids "${bullet}")
    if(ids)
        list(REMOVE_DUPLICATES ids)
    endif()
    set(${out} "${ids}" PARENT_SCOPE)
endfunction()

# The reciprocity verdict for one register and one marker: the problem sentences, empty when
# the debts written by entries and the debts repeated under Do not assume are the same set.
function(zen_law_debt_problems rel written echoed entries marker out)
    set(problems "")
    foreach(id IN LISTS written)
        if(NOT id IN_LIST echoed)
            list(APPEND problems "${rel} ${id} writes ${marker} and is not repeated under ## Do not assume")
        endif()
    endforeach()
    foreach(id IN LISTS echoed)
        if(NOT id IN_LIST entries)
            list(APPEND problems "${rel} Do not assume says ${marker} of ${id}, but it is no entry of this register")
        elseif(NOT id IN_LIST written)
            list(APPEND problems "${rel} Do not assume says ${marker} of ${id}, but its entry does not write it")
        endif()
    endforeach()
    set(${out} "${problems}" PARENT_SCOPE)
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

zen_vm_entry_heading("## VM-ZZZ-01 — A method heading" vm_id)
zen_vm_entry_heading("## WL-ZZZ-01 — A law heading is not a method heading" vm_not_law)
zen_vm_entry_heading("## Where a case goes" vm_not_table)
if(NOT vm_id STREQUAL "VM-ZZZ-01" OR NOT vm_not_law STREQUAL "" OR NOT vm_not_table STREQUAL "")
    message(FATAL_ERROR
        "law-register: SELF-TEST FAILED -- the method heading predicate read '${vm_id}', "
        "'${vm_not_law}' for a law heading and '${vm_not_table}' for the table heading.")
endif()
zen_vm_phase_tag("BECAUSE — measured on a 9p mount (QR-13), and again later" tag_phase)
zen_vm_phase_tag("BECAUSE — the same rule (ZOOM-P2) one level down" tag_letter)
zen_vm_phase_tag("METHOD — a verdict read through a pipe is no verdict (VM-LANE-16)." tag_law)
zen_vm_phase_tag("SEEN — `tests/t.cpp` case `\"HD-7: the policy (QR-9) holds\"`" tag_quoted)
zen_vm_phase_tag("BECAUSE — a tolerance (one pixel) of rounding is fine" tag_prose)
if(NOT tag_phase STREQUAL "(QR-13)" OR NOT tag_letter STREQUAL "(ZOOM-P2)" OR NOT tag_law STREQUAL ""
   OR NOT tag_quoted STREQUAL "" OR NOT tag_prose STREQUAL "")
    message(FATAL_ERROR
        "law-register: SELF-TEST FAILED -- the phase-tag predicate answered '${tag_phase}', "
        "'${tag_letter}', law id '${tag_law}', quoted '${tag_quoted}', prose '${tag_prose}'. A "
        "phase chronicle would then walk back into the registers unnoticed.")
endif()

set(selftest_src "int kAlpha = 1${ZEN_SOH} // kBeta is only here\n/* kGamma */ SurfaceRect r${ZEN_SOH}\nvoid on(const SurfaceRect& r)${ZEN_SOH}\n")
zen_law_mentions("${selftest_src}" "kBeta" yes)
zen_law_strip_comments("${selftest_src}" selftest_code)
zen_law_token_in("${selftest_code}" "kAlpha" yes_code)
zen_law_token_in("${selftest_code}" "kBeta" no_comment)
zen_law_token_in("${selftest_code}" "kGamma" no_block)
zen_law_token_in("${selftest_code}" "Rect" no_substring)
zen_law_token_in("${selftest_code}" "SurfaceRect" yes_whole)
zen_law_token_in("${selftest_code}" "SurfaceRect::kAlpha" yes_qualified)
zen_law_token_in("${selftest_code}" "Rect::kAlpha" no_scope)
zen_law_token_in("${selftest_code}" "on(SurfaceRect)" yes_overload)
zen_law_token_in("${selftest_code}" "on(kBeta)" no_overload)
if(NOT yes OR NOT yes_code OR no_comment OR no_block OR no_substring OR NOT yes_whole
   OR NOT yes_qualified OR no_scope OR NOT yes_overload OR no_overload)
    message(FATAL_ERROR
        "law-register: SELF-TEST FAILED -- the identifier predicates disagree with a three-line "
        "sample (mention ${yes}, code ${yes_code}, comment ${no_comment}, block ${no_block}, "
        "substring ${no_substring}, whole ${yes_whole}, qualified ${yes_qualified}, scope "
        "${no_scope}, overload ${yes_overload}, overload-in-comment ${no_overload}). Every "
        "'present' below would then be meaningless.")
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

zen_law_declared("inline constexpr std::int64_t kSideRegion = 0${ZEN_SOH}" name kind param qual)
if(NOT name STREQUAL "kSideRegion" OR NOT param STREQUAL "")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a constant's declaration parsed as '${name}' (${param}).")
endif()
zen_law_declared("struct PanelKind {" name kind param qual)
if(NOT name STREQUAL "PanelKind" OR NOT kind STREQUAL "scope")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a struct's declaration parsed as '${name}' (${kind}).")
endif()
zen_law_declared("template <class T> struct TextForm${ZEN_SOH}" name kind param qual)
if(NOT name STREQUAL "TextForm" OR NOT kind STREQUAL "scope")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a one-line template forward declaration parsed as '${name}' (${kind}).")
endif()
zen_law_declared("std::function<RecipeSwap(const Recipe&)> swap = {}${ZEN_SOH}" name kind param qual)
if(NOT name STREQUAL "swap")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a member behind a template argument parsed as '${name}'.")
endif()
zen_law_declared("${ZEN_STX}${ZEN_STX}nodiscard${ZEN_ETX}${ZEN_ETX} bool on(const zengine::surface::SurfaceExtent& e, loom::Mail& mail) noexcept {" name kind param qual)
if(NOT name STREQUAL "on" OR NOT kind STREQUAL "function" OR NOT param STREQUAL "SurfaceExtent")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a function's declaration parsed as '${name}' (${kind}, first parameter '${param}').")
endif()
zen_law_declared("inline std::size_t pane_row(const Setup& s, const PaneRef& ref) {" name kind param qual)
if(NOT name STREQUAL "pane_row" OR NOT param STREQUAL "Setup")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a free function's declaration parsed as '${name}' (first parameter '${param}').")
endif()
zen_law_declared("void picker_move(std::int64_t by) {" name kind param qual)
if(NOT name STREQUAL "picker_move" OR NOT param STREQUAL "int64_t")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a by-value parameter parsed as '${param}'.")
endif()
zen_law_declared("static_assert(kTopRows + kBottomRows == 6, \"six\")${ZEN_SOH}" name kind param qual)
if(NOT name STREQUAL "")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a static_assert was read as declaring '${name}'.")
endif()
zen_law_declared("void WorkshopWeave::quit() {" name kind param qual)
if(NOT name STREQUAL "quit" OR NOT kind STREQUAL "function" OR NOT qual STREQUAL "WorkshopWeave")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- an out-of-line member parsed as '${name}' (${kind}) qualified by '${qual}'.")
endif()
zen_law_declared("WorkshopWeave::GesturesEnded WorkshopWeave::end_held_gestures() {" name kind param qual)
if(NOT name STREQUAL "end_held_gestures" OR NOT qual STREQUAL "WorkshopWeave")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- a nested return type was read as the qualifier ('${qual}' before '${name}').")
endif()
zen_law_declared("bool create = false${ZEN_SOH}" name kind param qual)
if(NOT name STREQUAL "create" OR NOT qual STREQUAL "")
    message(FATAL_ERROR "law-register: SELF-TEST FAILED -- an unqualified member parsed as '${name}' qualified by '${qual}'.")
endif()

# The scope tracker over a sample: a namespace, a struct and its member, the struct closed, a
# free function whose body holds a brace inside a string literal, an opener whose brace is on
# the next line, the namespace closed.
set(st_depth 0)
set(st_stack "")
set(st_pending "")
zen_law_scope_step("namespace zengine::workshop {" st_depth st_stack st_pending)
zen_law_scope_step("struct LayoutTabPress {" st_depth st_stack st_pending)
zen_law_scope_path("${st_stack}" st_in_struct)
zen_law_scope_step("    bool create = false${ZEN_SOH}" st_depth st_stack st_pending)
zen_law_scope_step("}${ZEN_SOH}" st_depth st_stack st_pending)
zen_law_scope_path("${st_stack}" st_after_struct)
zen_law_scope_step("inline std::int64_t create(WorkshopDoc& d, Session& s) {" st_depth st_stack st_pending)
zen_law_scope_step("    return mint(d, s, \"{\")${ZEN_SOH}" st_depth st_stack st_pending)
zen_law_scope_step("}" st_depth st_stack st_pending)
zen_law_scope_path("${st_stack}" st_after_function)
zen_law_scope_step("struct Late" st_depth st_stack st_pending)
zen_law_scope_step("    : Base {" st_depth st_stack st_pending)
zen_law_scope_path("${st_stack}" st_late)
zen_law_scope_step("}${ZEN_SOH}" st_depth st_stack st_pending)
zen_law_scope_step("} // namespace zengine::workshop" st_depth st_stack st_pending)
zen_law_scope_path("${st_stack}" st_end)
if(NOT st_in_struct STREQUAL "zengine::workshop::LayoutTabPress" OR NOT st_after_struct STREQUAL "zengine::workshop"
   OR NOT st_after_function STREQUAL "zengine::workshop" OR NOT st_late STREQUAL "zengine::workshop::Late"
   OR NOT st_end STREQUAL "" OR NOT st_depth EQUAL 0)
    message(FATAL_ERROR
        "law-register: SELF-TEST FAILED -- the scope tracker read a nine-line sample as: in the struct "
        "'${st_in_struct}', after it '${st_after_struct}', after a function '${st_after_function}', in a "
        "late-braced struct '${st_late}', at the end '${st_end}' at depth ${st_depth}. Every qualified "
        "spelling below would then be judged against the wrong scope.")
endif()

# The qualified twin: `LayoutTabPress::create` is refused above the free function `create`
# and accepted above the in-class member and above the out-of-line definition; a nested
# namespace's own qualifier is accepted from inside it and refused from outside; the bare
# name, the scope form and the overload forms still answer.
zen_law_pointer_names("LayoutTabPress::create" create function "WorkshopDoc" "zengine::workshop" over_free)
zen_law_pointer_names("LayoutTabPress::create" create variable "" "zengine::workshop::LayoutTabPress" over_member)
zen_law_pointer_names("LayoutTabPress::create" create function "int64_t" "zengine::workshop::LayoutTabPress" over_outofline)
zen_law_pointer_names("create" create function "WorkshopDoc" "zengine::workshop" bare_free)
zen_law_pointer_names("detail::fit" fit function "string" "zengine::workshop::detail" ns_inside)
zen_law_pointer_names("detail::fit" fit function "string" "zengine::workshop" ns_outside)
zen_law_pointer_names("Session::normal_w" Session scope "" "zengine::workshop" scope_named)
zen_law_pointer_names("on(SurfaceExtent)" on function "SurfaceExtent" "zengine::workshop::WorkshopWeave" overload)
zen_law_pointer_names("WorkshopWeave::on(SurfaceExtent)" on function "SurfaceExtent" "zengine::workshop::WorkshopWeave" q_overload)
zen_law_pointer_names("Other::on(SurfaceExtent)" on function "SurfaceExtent" "zengine::workshop::WorkshopWeave" q_overload_wrong)
if(over_free OR NOT over_member OR NOT over_outofline OR NOT bare_free OR NOT ns_inside OR ns_outside
   OR NOT scope_named OR NOT overload OR NOT q_overload OR q_overload_wrong)
    message(FATAL_ERROR
        "law-register: SELF-TEST FAILED -- rule n's naming predicate disagrees with its twins (qualified "
        "over a free function ${over_free}, over the member ${over_member}, over the out-of-line "
        "definition ${over_outofline}, bare ${bare_free}, namespace inside ${ns_inside}, outside "
        "${ns_outside}, scope ${scope_named}, overload ${overload}, qualified overload ${q_overload}, "
        "wrong scope's overload ${q_overload_wrong}). A wrong `Scope::name` would then sit green.")
endif()

zen_law_proven_owes("PROVEN BY — `a.hpp` `x`; witness: none" owes)
zen_law_proven_owes("PROVEN BY — `a.hpp` `x`; `tests/t.cpp` case `\"a case\"`" cites)
zen_law_debt_ids("- That WL-ZZZ-01 is witnessed — it is not (witness: none)." "witness: none" debt)
zen_law_debt_ids("- That WL-ZZZ-02 has a runtime witness — its pins are compile-time only." "witness: none" prose)
zen_law_debt_problems("r.md" "WL-ZZZ-01" "WL-ZZZ-01" "WL-ZZZ-01;WL-ZZZ-02" "witness: none" reciprocal)
zen_law_debt_problems("r.md" "WL-ZZZ-01" "" "WL-ZZZ-01;WL-ZZZ-02" "witness: none" unrepeated)
zen_law_debt_problems("r.md" "" "WL-ZZZ-02" "WL-ZZZ-01;WL-ZZZ-02" "witness: none" witnessed)
zen_law_debt_problems("r.md" "" "WL-ZZZ-09" "WL-ZZZ-01;WL-ZZZ-02" "witness: none" stranger)
list(LENGTH unrepeated n_unrepeated)
list(LENGTH witnessed n_witnessed)
list(LENGTH stranger n_stranger)
if(NOT owes OR cites OR NOT debt STREQUAL "WL-ZZZ-01" OR NOT prose STREQUAL "" OR reciprocal
   OR NOT n_unrepeated EQUAL 1 OR NOT n_witnessed EQUAL 1 OR NOT n_stranger EQUAL 1)
    message(FATAL_ERROR
        "law-register: SELF-TEST FAILED -- the witness-debt predicates disagree with a four-line "
        "sample (owes ${owes}, cites ${cites}, debt '${debt}', prose '${prose}', reciprocal "
        "'${reciprocal}', unrepeated ${n_unrepeated}, witnessed ${n_witnessed}, stranger "
        "${n_stranger}). A debt written in one place only would then go unnoticed.")
endif()

# The clause debt, through the same predicates with the other marker: a clause is read off
# an UNWITNESSED line and off nothing else; a bullet that says UNWITNESSED names its law and a
# bullet that says only witness: none is not a clause echo; one-sided is refused both ways.
zen_law_unwitnessed_clause("UNWITNESSED — the second arm was not measured" clause)
zen_law_unwitnessed_clause("UNWITNESSED -- the second arm was not measured" clause_ascii)
zen_law_unwitnessed_clause("PROVEN BY — `a.hpp` `x`; witness: none" not_clause)
zen_law_unwitnessed_clause("UNWITNESSED —" bare)
zen_law_debt_ids("- That WL-ZZZ-03 is whole — one clause is UNWITNESSED, and it says which." "UNWITNESSED" partial)
zen_law_debt_ids("- That WL-ZZZ-01 is witnessed — it is not (witness: none)." "UNWITNESSED" not_partial)
zen_law_debt_problems("r.md" "WL-ZZZ-03" "WL-ZZZ-03" "WL-ZZZ-01;WL-ZZZ-03" "UNWITNESSED" p_reciprocal)
zen_law_debt_problems("r.md" "WL-ZZZ-03" "" "WL-ZZZ-01;WL-ZZZ-03" "UNWITNESSED" p_unrepeated)
zen_law_debt_problems("r.md" "" "WL-ZZZ-03" "WL-ZZZ-01;WL-ZZZ-03" "UNWITNESSED" p_unwritten)
list(LENGTH p_unrepeated n_p_unrepeated)
list(LENGTH p_unwritten n_p_unwritten)
if(NOT clause STREQUAL "the second arm was not measured" OR NOT clause_ascii STREQUAL "${clause}"
   OR NOT not_clause STREQUAL "" OR NOT bare STREQUAL "" OR NOT partial STREQUAL "WL-ZZZ-03"
   OR NOT not_partial STREQUAL "" OR p_reciprocal OR NOT n_p_unrepeated EQUAL 1
   OR NOT n_p_unwritten EQUAL 1)
    message(FATAL_ERROR
        "law-register: SELF-TEST FAILED -- the clause-debt predicates disagree with a sample "
        "(clause '${clause}', ascii '${clause_ascii}', not-clause '${not_clause}', bare '${bare}', "
        "partial '${partial}', not-partial '${not_partial}', reciprocal '${p_reciprocal}', "
        "unrepeated ${n_p_unrepeated}, unwritten ${n_p_unwritten}). A clause debt written in "
        "one place only would then go unnoticed.")
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
    "member under a substring scope, an overload whose type is only in a comment, a tagged "
    "pointer, an undeclared witness, a one-sided witness debt, a one-sided clause debt, a "
    "qualified spelling over a free function and a phase tag are refused; their well-formed "
    "twins are accepted")

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
set_property(GLOBAL PROPERTY zen_law_debt_bullets "")

# A Do-not-assume bullet, judged whole once its continuation lines are in: the ids it repeats
# as debts are recorded for the reciprocity pass below; a bullet that says the words and
# names no law is a debt with no owner.
macro(zen_law_flush_bullet)
    if(NOT bullet STREQUAL "")
        set(bullet_counted 0)
        foreach(marker IN ITEMS "witness: none" "UNWITNESSED")
            zen_law_debt_ids("${bullet}" "${marker}" debt_ids)
            if(debt_ids)
                if(marker STREQUAL "UNWITNESSED")
                    set_property(GLOBAL APPEND PROPERTY "zen_law_partial_echo_${relkey}" ${debt_ids})
                else()
                    set_property(GLOBAL APPEND PROPERTY "zen_law_debts_${relkey}" ${debt_ids})
                endif()
                set(bullet_counted 1)
            else()
                string(FIND "${bullet}" "${marker}" said)
                if(NOT said EQUAL -1)
                    zen_law_fail("${rel} has a Do-not-assume bullet that says ${marker} and names no law")
                endif()
            endif()
        endforeach()
        if(bullet_counted)
            set_property(GLOBAL APPEND PROPERTY zen_law_debt_bullets "${rel}")
        endif()
    endif()
    set(bullet "")
endmacro()

# One register (or router), walked line by line. Everything an entry declares is recorded
# on GLOBAL properties keyed by its id; the structural rules are applied at the flush.
macro(zen_law_flush_entry)
    if(NOT id STREQUAL "")
        zen_law_proven_owes("${proven_text}" owes)
        if(owes)
            set_property(GLOBAL APPEND PROPERTY "zen_law_nowitness_${relkey}" "${id}")
            if(proven_text MATCHES "\"")
                zen_law_fail("${rel} ${id} PROVEN BY says witness: none and also cites a witness")
            endif()
        endif()
        if(NOT unwitnessed_text STREQUAL "")
            set_property(GLOBAL APPEND PROPERTY "zen_law_partial_${relkey}" "${id}")
            if(owes)
                zen_law_fail("${rel} ${id} writes both witness: none and UNWITNESSED; a law with no witness at all writes the first alone")
            elseif(NOT proven_text MATCHES "\"")
                zen_law_fail("${rel} ${id} writes UNWITNESSED and its PROVEN BY cites no witness; a law with none writes witness: none")
            endif()
        endif()
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
    set(unwitnessed_text "")
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
    set(in_dna 0)
    set(bullet "")
    set(n 0)
    foreach(line IN LISTS lines)
        math(EXPR n "${n} + 1")
        string(LENGTH "${line}" len)
        if(len GREATER ZEN_LAW_LINE_BYTES AND NOT line MATCHES "^(LAW|METHOD) " AND NOT line MATCHES "^\\|")
            zen_law_fail("${rel}:${n} is ${len} bytes (at most ${ZEN_LAW_LINE_BYTES}; LAW, METHOD and table rows excepted)")
        endif()
        if(line MATCHES "^SINCE")
            zen_law_fail("${rel}:${n} has a SINCE line; phase codes are retired, provenance is the WHY line's record")
        endif()
        if(line MATCHES "^## ")
            zen_law_flush_entry()
            zen_law_flush_bullet()
            set(in_dna 0)
            if(is_router)
                continue()
            endif()
            if(line STREQUAL "## Do not assume")
                math(EXPR dna "${dna} + 1")
                if(dna GREATER 1)
                    zen_law_fail("${rel}:${n} is a second '## Do not assume'; a register carries one")
                endif()
                set(in_dna 1)
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
        if(in_dna)
            if(line MATCHES "^- ")
                zen_law_flush_bullet()
                set(bullet "${line}")
            elseif(line MATCHES "^  " AND NOT bullet STREQUAL "")
                string(APPEND bullet " ${line}")
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
        if(line MATCHES "^UNWITNESSED")
            zen_law_unwitnessed_clause("${line}" clause)
            if(clause STREQUAL "")
                zen_law_fail("${rel}:${n} ${id} UNWITNESSED names no clause (the form is `UNWITNESSED — <clause>`)")
            elseif(NOT proven)
                zen_law_fail("${rel}:${n} ${id} UNWITNESSED comes before PROVEN BY; it is the line after it")
            endif()
            string(APPEND unwitnessed_text " ${clause}")
            string(STRIP "${unwitnessed_text}" unwitnessed_text)
            set(section "unwitnessed")
            continue()
        endif()
        if(line MATCHES "^[ \t]*$")
            if(section STREQUAL "law" OR section STREQUAL "proven" OR section STREQUAL "unwitnessed")
                set(section "")
            endif()
            continue()
        endif()
        if(section STREQUAL "proven")
            string(APPEND proven_text " ${line}")
            continue()
        endif()
        if(section STREQUAL "unwitnessed")
            string(APPEND unwitnessed_text " ${line}")
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
    zen_law_flush_bullet()
endfunction()

# ---- the VM form: a method register ---------------------------------------------------------
#
# `## VM-<AREA>-NN — <title>` / `METHOD — <one sentence>` / `BECAUSE — <at most two lines>` /
# `SEEN — nowhere yet`, or `SEEN — <paths, identifiers, witnesses>`. Everything a SEEN names
# is resolved by the PROVEN BY walk below, unchanged: a backticked path exists, a backticked
# identifier occurs in the file named before it (as a whole token in the code, for a C/C++
# file), a quoted witness is a TEST_CASE/SUBCASE literal. A method register may carry one
# heading that is not an entry, ZEN_VM_TABLE_HEADING, whose section is free text (the table of
# which suite witnesses which area). No line of an entry carries a phase tag: a method is keyed
# by the lesson, never by the phase that paid for it. The walker takes the register's text so
# the self-test can hand it a synthetic one.
macro(zen_vm_flush_entry)
    if(NOT id STREQUAL "")
        if(method EQUAL 0)
            zen_law_fail("${rel} ${id} has no METHOD line")
        endif()
        if(because_lines EQUAL 0)
            zen_law_fail("${rel} ${id} has no BECAUSE line")
        elseif(because_lines GREATER ZEN_VM_BECAUSE_MAX)
            zen_law_fail("${rel} ${id} BECAUSE is ${because_lines} lines (at most ${ZEN_VM_BECAUSE_MAX})")
        endif()
        if(NOT seen)
            zen_law_fail("${rel} ${id} has no SEEN line")
        elseif(seen_text STREQUAL "nowhere yet")
            set_property(GLOBAL APPEND PROPERTY zen_vm_nowhere "${id}")
            set(seen_text "")
        elseif(NOT seen_text MATCHES "`")
            zen_law_fail("${rel} ${id} SEEN names nothing in backticks and is not `nowhere yet`")
        endif()
        set_property(GLOBAL PROPERTY "zen_law_file_${id}" "${rel}")
        set_property(GLOBAL PROPERTY "zen_law_proven_${id}" "${seen_text}")
        set_property(GLOBAL PROPERTY "zen_law_word_${id}" "SEEN")
        set_property(GLOBAL APPEND PROPERTY zen_vm_ids "${id}")
    endif()
    set(id "")
    set(method 0)
    set(because_lines 0)
    set(seen 0)
    set(seen_text "")
    set(section "")
endmacro()

function(zen_vm_walk_text rel content)
    string(REPLACE "\n" ";" lines "${content}")
    set(id "")
    zen_vm_flush_entry()
    set(tables 0)
    set(in_table 0)
    set(n 0)
    foreach(line IN LISTS lines)
        math(EXPR n "${n} + 1")
        string(LENGTH "${line}" len)
        if(len GREATER ZEN_LAW_LINE_BYTES AND NOT line MATCHES "^METHOD " AND NOT line MATCHES "^\\|")
            zen_law_fail("${rel}:${n} is ${len} bytes (at most ${ZEN_LAW_LINE_BYTES}; METHOD and table rows excepted)")
        endif()
        if(line MATCHES "^SINCE")
            zen_law_fail("${rel}:${n} has a SINCE line; phase codes are retired")
        endif()
        if(line MATCHES "^## ")
            zen_vm_flush_entry()
            set(in_table 0)
            if(line STREQUAL "${ZEN_VM_TABLE_HEADING}")
                math(EXPR tables "${tables} + 1")
                if(tables GREATER 1)
                    zen_law_fail("${rel}:${n} is a second '${ZEN_VM_TABLE_HEADING}'; a register carries one")
                endif()
                set(in_table 1)
                continue()
            endif()
            zen_vm_entry_heading("${line}" id)
            if(id STREQUAL "")
                zen_law_show("${line}" shown)
                zen_law_fail("${rel}:${n} heading is neither a VM entry nor '${ZEN_VM_TABLE_HEADING}': ${shown}")
                continue()
            endif()
            get_property(known GLOBAL PROPERTY "zen_law_file_${id}" SET)
            if(known)
                get_property(where GLOBAL PROPERTY "zen_law_file_${id}")
                zen_law_fail("${rel}:${n} duplicates id ${id}, already an entry of ${where}")
            endif()
            zen_vm_phase_tag("${line}" tag)
            if(NOT tag STREQUAL "")
                zen_law_fail("${rel}:${n} ${id} carries a phase tag ${tag}; a method is keyed by the lesson")
            endif()
            continue()
        endif()
        if(in_table OR id STREQUAL "")
            continue()
        endif()
        zen_vm_phase_tag("${line}" tag)
        if(NOT tag STREQUAL "")
            zen_law_fail("${rel}:${n} ${id} carries a phase tag ${tag}; a method is keyed by the lesson")
        endif()
        if(line MATCHES "^METHOD (—|--) (.*)$")
            set(sentence "${CMAKE_MATCH_2}")
            math(EXPR method "${method} + 1")
            if(method GREATER 1)
                zen_law_fail("${rel}:${n} ${id} has ${method} METHOD lines (one sentence, one line)")
            endif()
            string(LENGTH "${sentence}" mlen)
            if(mlen GREATER ZEN_VM_METHOD_BYTES)
                zen_law_fail("${rel}:${n} ${id} METHOD is ${mlen} bytes (at most ${ZEN_VM_METHOD_BYTES})")
            endif()
            set(section "method")
            continue()
        endif()
        if(line MATCHES "^BECAUSE (—|--) ")
            set(because_lines 1)
            set(section "because")
            continue()
        endif()
        if(line MATCHES "^SEEN (—|--) (.*)$")
            set(seen 1)
            string(STRIP "${CMAKE_MATCH_2}" seen_text)
            set(section "seen")
            continue()
        endif()
        if(line MATCHES "^[ \t]*$")
            set(section "")
            continue()
        endif()
        if(section STREQUAL "method")
            zen_law_fail("${rel}:${n} ${id} METHOD wraps onto a second line (it is one line)")
        elseif(section STREQUAL "because")
            math(EXPR because_lines "${because_lines} + 1")
        elseif(section STREQUAL "seen")
            string(APPEND seen_text " ${line}")
        else()
            zen_law_show("${line}" shown)
            zen_law_fail("${rel}:${n} ${id} has a line outside METHOD, BECAUSE and SEEN: ${shown}")
        endif()
    endforeach()
    zen_vm_flush_entry()
endfunction()

function(zen_vm_walk_register rel)
    zen_law_text("${rel}" content)
    zen_vm_walk_text("${rel}" "${content}")
endfunction()

# The VM walker over two synthetic registers: a well-formed one must raise nothing, and one
# carrying a long METHOD, a three-line BECAUSE, a phase tag and a heading that is no entry must
# raise exactly those four. The problems property is saved around the run and the synthetic ids
# are dropped, so nothing here reaches the real walk.
get_property(vm_saved_problems GLOBAL PROPERTY zen_law_problems)
set_property(GLOBAL PROPERTY zen_law_problems "")
set(vm_good "## VM-ZZZ-01 — A well-formed method\n\nMETHOD — One sentence.\nBECAUSE — one line,\nand a second.\nSEEN — nowhere yet\n\n## Where a case goes\n\n| a | b |\n")
zen_vm_walk_text("selftest-good.md" "${vm_good}")
get_property(vm_good_problems GLOBAL PROPERTY zen_law_problems)
set_property(GLOBAL PROPERTY zen_law_problems "")
string(REPEAT "x" 211 vm_long)
set(vm_bad "## VM-ZZZ-02 — A long method\n\nMETHOD — ${vm_long}\nBECAUSE — one,\ntwo,\nthree.\nSEEN — nowhere yet\n\n## VM-ZZZ-03 — A tagged method\n\nMETHOD — Short.\nBECAUSE — measured (QR-13) once.\nSEEN — nowhere yet\n\n## Not an entry\n")
zen_vm_walk_text("selftest-bad.md" "${vm_bad}")
get_property(vm_bad_problems GLOBAL PROPERTY zen_law_problems)
set_property(GLOBAL PROPERTY zen_law_problems "${vm_saved_problems}")
set_property(GLOBAL PROPERTY zen_vm_ids "")
set_property(GLOBAL PROPERTY zen_vm_nowhere "")
zen_law_count_lines("${vm_bad_problems}" vm_bad_count)
if(NOT vm_good_problems STREQUAL "" OR NOT vm_bad_count EQUAL 4
   OR NOT vm_bad_problems MATCHES "METHOD is 211 bytes" OR NOT vm_bad_problems MATCHES "BECAUSE is 3 lines"
   OR NOT vm_bad_problems MATCHES "phase tag \\(QR-13\\)" OR NOT vm_bad_problems MATCHES "heading is neither")
    message(FATAL_ERROR
        "law-register: SELF-TEST FAILED -- the method-register walker raised '${vm_good_problems}' "
        "on a well-formed register and ${vm_bad_count} problem(s) on one with four defects:\n"
        "${vm_bad_problems}\nEvery VM entry below would then be judged by a walker that cannot say no.")
endif()

foreach(rel IN LISTS register_files)
    zen_law_walk_register("${rel}" 0)
endforeach()
foreach(rel IN LISTS ZEN_LAW_ROUTERS)
    if(NOT EXISTS "${ZEN_REPO}/${rel}")
        zen_law_fail("router ${rel} does not exist")
        continue()
    endif()
    zen_law_walk_register("${rel}" 1)
endforeach()

# ---- population 2b: the method registers -----------------------------------------------------

set(vm_register_files "")
foreach(dir IN LISTS ZEN_VM_REGISTER_DIRS)
    file(GLOB found RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/${dir}/*.md")
    list(APPEND vm_register_files ${found})
endforeach()
list(SORT vm_register_files)
list(LENGTH vm_register_files vm_register_count)
if(vm_register_count EQUAL 0)
    message(FATAL_ERROR
        "law-register: no method register was found under ${ZEN_VM_REGISTER_DIRS}. An empty "
        "population is a failure here and not a quiet pass.")
endif()
foreach(rel IN LISTS vm_register_files)
    zen_vm_walk_register("${rel}")
endforeach()
foreach(rel IN LISTS ZEN_VM_ROUTERS)
    if(NOT EXISTS "${ZEN_REPO}/${rel}")
        zen_law_fail("router ${rel} does not exist")
        continue()
    endif()
    zen_law_walk_register("${rel}" 1)
endforeach()
get_property(vm_ids GLOBAL PROPERTY zen_vm_ids)
get_property(vm_nowhere GLOBAL PROPERTY zen_vm_nowhere)
list(LENGTH vm_ids vm_entry_count)
list(LENGTH vm_nowhere vm_nowhere_count)
if(vm_entry_count EQUAL 0)
    message(FATAL_ERROR
        "law-register: ${vm_register_count} method register(s) yielded ZERO entries. Either the "
        "walker stopped recognising the entry form or every method is gone; both are failures.")
endif()

# ---- the budgets: every *.md under agents/ -----------------------------------------------------
#
# `file(SIZE)` counts bytes. A router is one named in ZEN_LAW_ROUTERS or ZEN_VM_ROUTERS; every
# other file under agents/ -- a register, a routed document, a decision record -- has the
# register budget, except the documents ZEN_LAW_UNBUDGETED names, which must stay over it
# while they are listed. A record over the flag size is counted for the report.

file(GLOB_RECURSE agents_md RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/${ZEN_LAW_AGENTS_DIR}/*.md")
list(SORT agents_md)
list(LENGTH agents_md agents_md_count)
if(agents_md_count EQUAL 0)
    message(FATAL_ERROR "law-register: no *.md under ${ZEN_LAW_AGENTS_DIR}/ at all; check ZEN_REPO.")
endif()
set(records_flagged 0)
set(unbudgeted_report "")
foreach(rel IN LISTS agents_md)
    file(SIZE "${ZEN_REPO}/${rel}" bytes)
    if(rel IN_LIST ZEN_LAW_ROUTERS OR rel IN_LIST ZEN_VM_ROUTERS)
        if(bytes GREATER ZEN_LAW_ROUTER_BYTES)
            zen_law_fail("${rel} is ${bytes} bytes (a router is at most ${ZEN_LAW_ROUTER_BYTES})")
        endif()
    elseif(rel IN_LIST ZEN_LAW_UNBUDGETED)
        if(bytes GREATER ZEN_LAW_REGISTER_BYTES)
            string(APPEND unbudgeted_report " ${rel} ${bytes}")
        else()
            zen_law_fail("${rel} is ${bytes} bytes, within the register budget, and is still named in ZEN_LAW_UNBUDGETED; remove it there")
        endif()
    else()
        if(bytes GREATER ZEN_LAW_REGISTER_BYTES)
            zen_law_fail("${rel} is ${bytes} bytes (a file under ${ZEN_LAW_AGENTS_DIR}/ is at most ${ZEN_LAW_REGISTER_BYTES}; a router ${ZEN_LAW_ROUTER_BYTES})")
        endif()
        if(rel MATCHES "^${ZEN_LAW_RECORD_DIR}/" AND bytes GREATER ZEN_LAW_RECORD_FLAG_BYTES)
            math(EXPR records_flagged "${records_flagged} + 1")
        endif()
    endif()
endforeach()
foreach(rel IN LISTS ZEN_LAW_UNBUDGETED)
    if(NOT rel IN_LIST agents_md)
        zen_law_fail("ZEN_LAW_UNBUDGETED names ${rel}, which does not exist; remove it there")
    endif()
endforeach()
file(SIZE "${ZEN_REPO}/${ZEN_LAW_CORE}" bytes)
if(bytes GREATER ZEN_LAW_CORE_BYTES)
    zen_law_fail("${ZEN_LAW_CORE} is ${bytes} bytes (at most ${ZEN_LAW_CORE_BYTES})")
endif()

# ---- witness debts are written in both places or in neither -------------------------------
#
# Per register and per marker: every entry whose PROVEN BY says `witness: none`, and every
# entry that writes `UNWITNESSED -- <clause>`, is named by a Do-not-assume bullet that says
# the same word, and every id such a bullet names is an entry of that register that writes
# that debt. Zero on both sides is the goal, not an empty population.

set(nowitness_total 0)
set(partial_total 0)
set(debt_total 0)
set(debt_problem_count 0)
foreach(rel IN LISTS register_files)
    string(MAKE_C_IDENTIFIER "${rel}" relkey)
    get_property(entries GLOBAL PROPERTY "zen_law_ids_of_${relkey}")
    foreach(marker IN ITEMS "witness: none" "UNWITNESSED")
        if(marker STREQUAL "UNWITNESSED")
            get_property(written GLOBAL PROPERTY "zen_law_partial_${relkey}")
            get_property(echoed GLOBAL PROPERTY "zen_law_partial_echo_${relkey}")
        else()
            get_property(written GLOBAL PROPERTY "zen_law_nowitness_${relkey}")
            get_property(echoed GLOBAL PROPERTY "zen_law_debts_${relkey}")
        endif()
        if(echoed)
            list(REMOVE_DUPLICATES echoed)
        endif()
        list(LENGTH written written_count)
        list(LENGTH echoed echoed_count)
        if(marker STREQUAL "UNWITNESSED")
            math(EXPR partial_total "${partial_total} + ${written_count}")
        else()
            math(EXPR nowitness_total "${nowitness_total} + ${written_count}")
        endif()
        math(EXPR debt_total "${debt_total} + ${echoed_count}")
        zen_law_debt_problems("${rel}" "${written}" "${echoed}" "${entries}" "${marker}" debt_problems)
        foreach(p IN LISTS debt_problems)
            zen_law_fail("${p}")
            math(EXPR debt_problem_count "${debt_problem_count} + 1")
        endforeach()
    endforeach()
endforeach()
get_property(debt_bullets GLOBAL PROPERTY zen_law_debt_bullets)
list(LENGTH debt_bullets debt_bullet_count)
if(debt_problem_count EQUAL 0)
    set(debt_word "reciprocal")
else()
    set(debt_word "${debt_problem_count} one-sided (below)")
endif()

get_property(all_ids GLOBAL PROPERTY zen_law_ids)
list(LENGTH all_ids entry_count)
if(entry_count EQUAL 0)
    message(FATAL_ERROR
        "law-register: ${register_count} register(s) yielded ZERO entries. Either the parser "
        "stopped recognising the entry form or every law is gone; both are failures, and "
        "neither is a green.")
endif()

# Ids are unique across agents/, not only across the registers: a `## WL-` or `## VM-` heading
# in any other document under agents/ is a second copy of an entry that has one home.
foreach(rel IN LISTS agents_md)
    if(rel IN_LIST register_files OR rel IN_LIST vm_register_files)
        continue()
    endif()
    zen_law_text("${rel}" content)
    string(REGEX MATCHALL "\n## (WL|VM)-[A-Z]+-[0-9]+" strays "\n${content}")
    foreach(stray IN LISTS strays)
        string(REGEX REPLACE "^\n## " "" stray "${stray}")
        zen_law_fail("${rel} carries a '## ${stray}' heading outside the registers; an id has one home")
    endforeach()
    string(REGEX MATCHALL "\nSINCE" sinces "\n${content}")
    list(LENGTH sinces since_count)
    if(since_count GREATER 0)
        zen_law_fail("${rel} has ${since_count} SINCE line(s); phase codes are retired")
    endif()
endforeach()

# ---- population 3: PROVEN BY and SEEN -- paths, identifiers, witnesses ------------------------
#
# The paragraph is one line here. Quoted witnesses come out first and are removed, so a
# case name that itself contains backticks cannot shed fragments into the identifier walk;
# what remains in backticks is a path (which sets the file every later identifier is
# checked against) or an identifier. A VM entry's SEEN is the same paragraph under another
# word, and walks through the same predicates.

set(path_count 0)
set(ident_count 0)
set(witness_checked 0)
set(rule_m_count 0)
foreach(id IN LISTS all_ids vm_ids)
    get_property(rel GLOBAL PROPERTY "zen_law_file_${id}")
    get_property(para GLOBAL PROPERTY "zen_law_proven_${id}")
    get_property(word GLOBAL PROPERTY "zen_law_word_${id}")
    if(word STREQUAL "")
        set(word "PROVEN BY")
    endif()
    if(para STREQUAL "")
        continue()
    endif()
    string(REGEX MATCHALL "\"[^\"]*\"" witnesses "${para}")
    foreach(w IN LISTS witnesses)
        math(EXPR witness_checked "${witness_checked} + 1")
        zen_law_witnessed("${w}" ok)
        if(NOT ok)
            zen_law_show("${w}" shown)
            zen_law_fail("${rel} ${id} ${word} witness is not a TEST_CASE/SUBCASE literal under ${ZEN_LAW_WITNESS_DIR}/: ${shown}")
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
                zen_law_fail("${rel} ${id} ${word} names a path that does not exist: ${tok}")
                set(cur "")
            endif()
            continue()
        endif()
        math(EXPR ident_count "${ident_count} + 1")
        zen_law_show("${tok}" shown)
        if(cur STREQUAL "")
            zen_law_fail("${rel} ${id} ${word} names identifier ${shown} before any existing path")
            continue()
        endif()
        zen_law_text("${cur}" text)
        zen_law_mentions("${text}" "${tok}" mentioned)
        if(NOT mentioned)
            zen_law_fail("${rel} ${id} ${word} names ${shown}, which does not occur in ${cur}")
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
    set(carry "")
    set(scope_depth 0)
    set(scope_stack "")
    set(scope_pending "")
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
        # The scope this line sits in is read before the line's own braces are counted, so a
        # pointer above `struct Foo {` sees Foo's enclosing scope, and one above a member
        # sees Foo. A line with no brace and no scope keyword steps nothing (two finds and
        # one match, no call: the check's wall clock is paid per line of every pointer file).
        if(pending)
            zen_law_scope_path("${scope_stack}" scope_here)
        endif()
        string(FIND "${line}" "{" quick_open)
        string(FIND "${line}" "}" quick_close)
        if(NOT quick_open EQUAL -1 OR NOT quick_close EQUAL -1 OR NOT scope_pending STREQUAL ""
           OR line MATCHES "^[ \t]*(template|namespace|struct|class|union|enum)[ \t<]")
            zen_law_scope_step("${line}" scope_depth scope_stack scope_pending)
        endif()
        if(NOT pending)
            continue()
        endif()
        # Lines a pointer looks past on its way to the declaration.
        if(line MATCHES "^[ \t]*$" OR line MATCHES "^[ \t]*//" OR line MATCHES "^[ \t]*#"
           OR line MATCHES "^[ \t]*(public|private|protected):[ \t]*$"
           OR line MATCHES "^[ \t]*${ZEN_STX}${ZEN_STX}[^${ZEN_ETX}]*${ZEN_ETX}${ZEN_ETX}[ \t]*$")
            continue()
        endif()
        # A bare `template <...>` head is looked past too; a one-line forward declaration
        # (`template <class T> struct TextForm;`) is the declaration itself, and the parse
        # reads it once the head is stripped.
        string(REGEX REPLACE "//.*$" "" head "${line}")
        foreach(round RANGE 1 4)
            string(REGEX REPLACE "<[^<>]*>" "" head "${head}")
        endforeach()
        string(STRIP "${head}" head)
        if(head MATCHES "^template([ \t]*<.*)?$")
            continue()
        endif()
        # A return type on a line of its own -- nothing on the line can end a declaration, and
        # no declaring keyword starts it -- is joined with the next code line, once.
        string(REGEX REPLACE "//.*$" "" bare "${line}")
        string(STRIP "${bare}" bare)
        if(carry STREQUAL "" AND NOT bare STREQUAL ""
           AND NOT bare MATCHES "[(={,${ZEN_SOH}]"
           AND NOT bare MATCHES "^(namespace|struct|class|union|enum|using|template)[ \t]"
           AND NOT bare MATCHES "^}")
            set(carry "${line}")
            continue()
        endif()
        if(NOT carry STREQUAL "")
            set(line "${carry} ${line}")
            set(carry "")
        endif()
        zen_law_declared("${line}" name kind param qual)
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
            # What names the declaration: its bare name; `Scope::name` only from inside Scope
            # (the enclosing namespaces and classes, then an out-of-line definition's own
            # qualifier); for a function also the overload spelling `name(Type)` with its first
            # parameter's type (zen_law_pointer_names()). EVERY id on the line must name the
            # declaration: a pointer is the inverse of each law it cites, not of one of them.
            set(scope_of_line "${scope_here}")
            if(NOT qual STREQUAL "")
                if(scope_of_line STREQUAL "")
                    set(scope_of_line "${qual}")
                else()
                    set(scope_of_line "${scope_of_line}::${qual}")
                endif()
            endif()
            set(unnamed "")
            foreach(id IN LISTS ids)
                get_property(owned GLOBAL PROPERTY "zen_law_owns_${id}_${relkey}")
                zen_law_pointer_names("${owned}" "${name}" "${kind}" "${param}" "${scope_of_line}" named)
                if(NOT named)
                    list(APPEND unnamed "${id}")
                endif()
            endforeach()
            if(unnamed)
                string(REPLACE ";" ", " unnamed "${unnamed}")
                math(EXPR rule_n_count "${rule_n_count} + 1")
                zen_law_strict(n "${rel}:${at} (${idtext}) points at ${kind} `${name}` in `${scope_of_line}` (line ${n}), which ${unnamed} does not name under ${rel}")
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

math(EXPR vm_applied_count "${vm_entry_count} - ${vm_nowhere_count}")
message(STATUS "law-register: ${register_count} registers, ${entry_count} entries; ${record_count} "
               "records, ${why_target_count} WHY targets, ${why_count} WHY lines")
message(STATUS "law-register: ${vm_register_count} method registers, ${vm_entry_count} entries -- "
               "${vm_applied_count} applied in the tree, ${vm_nowhere_count} nowhere yet")
message(STATUS "law-register: ${agents_md_count} files under ${ZEN_LAW_AGENTS_DIR}/ within budget; "
               "unbudgeted, still over:${unbudgeted_report}; records over "
               "${ZEN_LAW_RECORD_FLAG_BYTES} bytes: ${records_flagged}")
message(STATUS "law-register: ${path_count} PROVEN BY paths, ${ident_count} identifiers, "
               "${witness_checked} witnesses checked against ${witness_count} TEST_CASE/SUBCASE "
               "literals in ${witness_file_count} files")
message(STATUS "law-register: ${pointer_files} source files carry pointers -- ${pointer_lines} "
               "pointer lines, ${pointer_ids} pointer ids, ${header_pointers} header pointers")
message(STATUS "law-register: witness debts -- ${nowitness_total} entries write witness: none, "
               "${partial_total} write UNWITNESSED; ${debt_bullet_count} Do-not-assume bullets "
               "repeat them (${debt_total} ids), ${debt_word}")

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
        set(what "pointer line(s) whose declaration a law on the line does not name (of ${rule_n_pointers})")
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
