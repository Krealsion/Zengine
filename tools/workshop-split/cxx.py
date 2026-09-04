# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE ONE PARSER THE WORKSHOP SPLIT SHARES -- a lexical scan of a C++ header that is enough
# to find every function defined at namespace scope or in one class body, its signature,
# its body, and the `// WL-` pointer lines that sit above it. It is not a C++ parser: it
# counts braces and parentheses outside comments, string literals and character literals,
# and reads a declaration's head by the same rules tests/check_law_register.cmake's
# zen_law_declared() reads a pointer's declaration, so that what the sheet names is what
# the law-register check will see.
#
# apply.py and prove.py both import it; prove.py deliberately re-derives what it checks
# from the START commit rather than trusting anything apply.py wrote.

import re

KEYWORDS = {
    "alignas", "alignof", "asm", "auto", "bool", "break", "case", "catch", "char", "class",
    "const", "constexpr", "continue", "decltype", "default", "delete", "do", "double", "else",
    "enum", "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if",
    "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "nullptr", "operator",
    "override", "final", "private", "protected", "public", "register", "return", "short",
    "signed", "sizeof", "static", "struct", "switch", "template", "this", "throw", "true",
    "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void",
    "volatile", "while",
}

IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
POINTER_LINE = re.compile(r"^[ \t]*// WL-")


def mask(text):
    """Return (masked, literals): `masked` is `text` with every comment, string literal,
    character literal and preprocessor line replaced by spaces (newlines kept), so braces
    and parentheses can be counted on it; `literals` is the list of string and character
    literals in source order, bytes as written, preprocessor lines aside."""
    out = []
    literals = []
    i, n = 0, len(text)
    at_line_start = True
    while i < n:
        c = text[i]
        if at_line_start and text[i:].lstrip(" \t").startswith("#"):
            j = text.find("\n", i)
            if j == -1:
                j = n
            # A preprocessor line may continue with a trailing backslash.
            while j < n and text[j - 1] == "\\":
                k = text.find("\n", j + 1)
                j = n if k == -1 else k
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
            continue
        at_line_start = False
        if c == "\n":
            out.append("\n")
            at_line_start = True
            i += 1
            continue
        if text.startswith("//", i):
            j = text.find("\n", i)
            if j == -1:
                j = n
            out.append(" " * (j - i))
            i = j
            continue
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j == -1 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
            continue
        if c == '"' and text[i - 1:i] == "R" and (i < 2 or not (text[i - 2].isalnum() or text[i - 2] == "_")):
            # raw string R"delim( ... )delim"
            m = re.match(r'R"([^ ()\\\t\n]{0,16})\(', text[i - 1:])
            if m:
                delim = m.group(1)
                end = text.find(")" + delim + '"', i + 1 + len(m.group(0)) - 1)
                j = n if end == -1 else end + len(delim) + 2
                literals.append(text[i - 1:j])
                out[-1] = " "  # the R
                out.append(re.sub(r"[^\n]", " ", text[i:j]))
                i = j
                continue
        if c == '"' or c == "'":
            q = c
            j = i + 1
            while j < n and text[j] != q:
                if text[j] == "\\":
                    j += 1
                j += 1
            j = min(j + 1, n)
            literals.append(text[i:j])
            out.append(" " * (j - i))
            i = j
            continue
        out.append(c)
        i += 1
    return "".join(out), literals


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


def match_brace(masked, open_pos):
    """Position of the brace/paren closing the one at open_pos, over masked text."""
    opener = masked[open_pos]
    closer = {"{": "}", "(": ")", "[": "]"}[opener]
    depth = 0
    i = open_pos
    n = len(masked)
    while i < n:
        c = masked[i]
        if c == opener:
            depth += 1
        elif c == closer:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("unbalanced %s at %d" % (opener, open_pos))


def strip_angles(s):
    for _ in range(6):
        s = re.sub(r"<[^<>]*>", "", s)
    return s


def first_param_type(params):
    """zen_law_declared()'s rule for the `name(Type)` overload spelling: the type of the
    first parameter -- the last identifier before its `&` or `*`, or the identifier before
    its name when it has neither; const, volatile and template arguments are not it."""
    p = strip_angles(params)
    p = re.sub(r"\[\[[^\]]*\]\]", "", p)
    cut = len(p)
    depth = 0
    for i, c in enumerate(p):
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        elif c == "," and depth == 0:
            cut = i
            break
    p = p[:cut]
    ref = len(p)
    for i, c in enumerate(p):
        if c in "&*":
            ref = i
            break
    words = [w for w in IDENT.findall(p[:ref]) if w not in ("const", "volatile", "struct", "class", "enum")]
    if not words:
        return ""
    if ref == len(p) and len(words) > 1:
        return words[-2]
    return words[-1]


class Decl:
    """One declaration at a scope. kind: function | variable | type | alias | namespace |
    other. For a function: `defined` says whether a body follows, `body_open`/`body_close`
    are the positions of its braces, `specifiers` the words before the declarator."""

    def __init__(self, **kw):
        self.__dict__.update(kw)

    def __repr__(self):
        return "Decl(%s %s %s L%d-%d)" % (self.kind, "::".join(self.scope + [self.name]), "def" if getattr(self, "defined", False) else "", self.first_line, self.last_line)


DECLARING = ("namespace", "struct", "class", "union", "enum")


def walk_scope(text, masked, start, end, scope, out, depth_limit=2, depth=0):
    """Append a Decl for every declaration whose statement starts in masked[start:end]
    at this scope. Recurses into namespaces (always) and into class/struct bodies
    (one level, for member names)."""
    i = start
    n = end
    while i < n:
        # skip blanks
        while i < n and masked[i] in " \t\n\r":
            i += 1
        if i >= n:
            break
        if masked[i] == "}":
            i += 1
            continue
        stmt_start = i
        # Access specifier?
        m = re.match(r"(public|private|protected)\s*:", masked[i:i + 12])
        if m:
            i += m.end()
            continue
        # Read the head up to the first `{`, `;` or `=` at paren depth 0; `(` groups are
        # skipped over so that a parameter list's own `=` or `{` does not end the head.
        pdepth = 0
        j = i
        head_end = None
        stopper = None
        paren_open = None
        paren_close = None
        adepth = 0  # template angle depth, only while no paren seen
        while j < n:
            c = masked[j]
            if c == "(" and pdepth == 0 and paren_open is None and adepth == 0:
                paren_open = j
                paren_close = match_brace(masked, j)
                j = paren_close + 1
                continue
            if c == "(":
                pdepth += 1
            elif c == ")":
                pdepth -= 1
            elif c == "<" and paren_open is None and pdepth == 0:
                adepth += 1
            elif c == ">" and paren_open is None and pdepth == 0 and adepth > 0:
                adepth -= 1
            elif pdepth == 0 and adepth == 0 and c in "{;=":
                if c == "=" and masked[j + 1:j + 2] == "=":
                    j += 2
                    continue
                head_end = j
                stopper = c
                break
            j += 1
        if head_end is None:
            break
        head = masked[stmt_start:head_end]
        head_words = IDENT.findall(strip_angles(head[:paren_open - stmt_start] if paren_open is not None else head))
        first_word = head_words[0] if head_words else ""
        kind = "other"
        name = ""
        stmt_end = None
        body_open = body_close = None
        defined = False
        specifiers = []
        first_param = ""
        params_text = ""
        is_template = first_word == "template"
        if first_word == "template":
            head_words = head_words[1:]
            first_word = head_words[0] if head_words else ""
        if first_word == "static_assert":
            kind = "other"
            stmt_end = masked.find(";", paren_close if paren_close else head_end)
        elif first_word == "namespace":
            kind = "namespace"
            mm = re.search(r"namespace\s+([A-Za-z_][A-Za-z0-9_:]*)", head)
            name = mm.group(1) if mm else ""
            body_open = head_end
            body_close = match_brace(masked, body_open)
            stmt_end = body_close
            out.append(Decl(kind=kind, name=name, scope=list(scope), first_line=line_of(text, stmt_start),
                            last_line=line_of(text, stmt_end), start=stmt_start, end=stmt_end,
                            body_open=body_open, body_close=body_close, template=is_template))
            walk_scope(text, masked, body_open + 1, body_close, scope + [name], out, depth_limit, depth)
            i = stmt_end + 1
            continue
        elif first_word in ("struct", "class", "union", "enum") and stopper == "{":
            kind = "type"
            words = [w for w in head_words[1:] if w not in ("class", "struct", "final", "alignas")]
            name = words[0] if words else ""
            body_open = head_end
            body_close = match_brace(masked, body_open)
            semi = masked.find(";", body_close)
            stmt_end = semi
            out.append(Decl(kind=kind, name=name, scope=list(scope), first_line=line_of(text, stmt_start),
                            last_line=line_of(text, stmt_end), start=stmt_start, end=stmt_end,
                            body_open=body_open, body_close=body_close, template=is_template,
                            is_enum=first_word == "enum"))
            if first_word == "enum":
                for piece in masked[body_open + 1:body_close].split(","):
                    mm = IDENT.search(piece)
                    if mm:
                        out.append(Decl(kind="enumerator", name=mm.group(0), scope=scope + [name], first_line=0,
                                        last_line=0, start=body_open, end=body_close, template=False))
            elif depth < depth_limit:
                walk_scope(text, masked, body_open + 1, body_close, scope + [name], out, depth_limit, depth + 1)
            i = stmt_end + 1
            continue
        elif first_word in ("struct", "class", "union", "enum") and stopper == ";":
            kind = "type"
            words = [w for w in head_words[1:] if w not in ("class", "struct", "final")]
            name = words[0] if words else ""
            stmt_end = head_end
        elif first_word == "using" or first_word == "typedef":
            kind = "alias"
            name = head_words[1] if first_word == "using" and len(head_words) > 1 else (head_words[-1] if head_words else "")
            stmt_end = masked.find(";", head_end)
        elif first_word == "friend":
            kind = "other"
            if stopper == "{":
                body_open = head_end
                body_close = match_brace(masked, body_open)
                stmt_end = body_close
            else:
                stmt_end = masked.find(";", head_end)
        elif paren_open is not None and stopper in ("{", ";", "=") and (stopper != "=" or paren_open < head_end):
            # A function: declarator `name(params)`, then qualifiers, then `{` or `;` or
            # `= default|delete|0 ;`. A variable whose initializer holds parentheses has its
            # `=` BEFORE the `(`, which the head scan already stopped at.
            kind = "function"
            before = strip_angles(masked[stmt_start:paren_open])
            before = re.sub(r"\[\[[^\]]*\]\]", "", before)
            words = IDENT.findall(before)
            name = words[-1] if words else ""
            if name == "operator" or (words and name in KEYWORDS and name != "operator"):
                name = words[-1]
            specifiers = [w for w in words if w in ("inline", "constexpr", "static", "explicit", "virtual", "consteval", "constinit", "friend")]
            params_text = masked[paren_open + 1:paren_close]
            first_param = first_param_type(params_text)
            tail = masked[paren_close + 1:head_end]
            if stopper == "{":
                defined = True
                body_open = head_end
                body_close = match_brace(masked, body_open)
                stmt_end = body_close
            elif stopper == "=":
                # `= default;`, `= delete;`, `= 0;` or a function-typed variable
                stmt_end = masked.find(";", head_end)
            else:
                stmt_end = head_end
            # A constructor's mem-initializer list starts with a single `:` after the params.
            if stopper == "{" and re.search(r"(^|[^:]):([^:]|$)", tail):
                pass  # the head scan already ran to the `{`; the list is in `tail`
            out.append(Decl(kind=kind, name=name, scope=list(scope), first_line=line_of(text, stmt_start),
                            last_line=line_of(text, stmt_end), start=stmt_start, end=stmt_end,
                            body_open=body_open, body_close=body_close, defined=defined,
                            specifiers=specifiers, first_param=first_param, params_text=params_text,
                            paren_open=paren_open, paren_close=paren_close, head_end=head_end,
                            template=is_template, tail=tail))
            i = stmt_end + 1
            continue
        else:
            # A variable: `T name = ...;`, `T name{...};`, `T name;`
            kind = "variable"
            words = IDENT.findall(re.sub(r"\[[^\]]*\]$", "", strip_angles(head).rstrip()))
            name = words[-1] if words else ""
            if stopper == "{":
                body_close = match_brace(masked, head_end)
                stmt_end = masked.find(";", body_close)
            elif stopper == "=":
                # skip a brace-initializer or lambda after `=`
                k = head_end + 1
                while k < n:
                    c = masked[k]
                    if c == "{" or c == "(" or c == "[":
                        k = match_brace(masked, k) + 1
                        continue
                    if c == ";":
                        break
                    k += 1
                stmt_end = k
            else:
                stmt_end = head_end
            specifiers = [w for w in words if w in ("inline", "constexpr", "static")]
        if stmt_end is None or stmt_end < 0:
            break
        out.append(Decl(kind=kind, name=name, scope=list(scope), first_line=line_of(text, stmt_start),
                        last_line=line_of(text, stmt_end), start=stmt_start, end=stmt_end,
                        specifiers=specifiers, template=is_template, defined=defined,
                        body_open=body_open, body_close=body_close))
        i = stmt_end + 1


def declaration_end(masked, d):
    """Where a function's DECLARATION ends: the position of its body's `{`, or of the
    single `:` that opens a constructor's mem-initializer list (`::` is not it)."""
    i = d.paren_close + 1
    while i < d.body_open:
        if masked[i] == ":" and masked[i - 1] != ":" and masked[i + 1] != ":":
            return i
        if masked[i] == "(":
            i = match_brace(masked, i) + 1
            continue
        i += 1
    return d.body_open


def prototype_text(text, masked, d):
    """The prototype a moved definition leaves behind: the declaration from the start of
    its first line to its end, `inline` dropped, ending in `;`."""
    line_start = text.rfind("\n", 0, d.start) + 1
    proto = text[line_start:declaration_end(masked, d)].rstrip()
    head_len = d.paren_open - line_start
    head = re.sub(r"(?<![A-Za-z0-9_])inline[ \t]+", "", proto[:head_len], count=1)
    return head + proto[head_len:] + ";"


def declarations(text):
    """Every declaration of a header, namespace scope and one class level deep."""
    masked, _ = mask(text)
    out = []
    walk_scope(text, masked, 0, len(masked), [], out)
    return out, masked


def code_lines(text):
    """(line number, line) for every line that is not blank, not a comment, not a
    preprocessor line -- what rule n looks past on its way to a declaration is skipped."""
    masked, _ = mask(text)
    lines = text.split("\n")
    mlines = masked.split("\n")
    out = []
    for i, (l, ml) in enumerate(zip(lines, mlines), 1):
        if ml.strip() == "":
            continue
        s = l.strip()
        if re.match(r"^(public|private|protected):\s*$", s):
            continue
        out.append((i, l))
    return out


def pointer_lines_between(lines, after_line, before_line):
    """The `// WL-` lines strictly between two line numbers (1-based), in order."""
    return [k for k in range(after_line + 1, before_line) if POINTER_LINE.match(lines[k - 1])]


def strip_comments_and_blanks(text):
    """The code lines of a file: `//` comments cut (a `//` inside a string literal is not
    one -- the mask tells them apart), block comments removed, preprocessor lines kept,
    blank lines dropped, trailing whitespace dropped."""
    masked, _ = mask(text)
    out = []
    for l, ml in zip(text.split("\n"), masked.split("\n")):
        s = l.strip()
        if s.startswith("#"):
            out.append(s)
            continue
        cut = None
        k = 0
        while k < len(l) - 1:
            if l[k:k + 2] == "//" and ml[k:].strip() == "":
                cut = k
                break
            if l[k:k + 2] == "/*" and ml[k] == " ":
                close = l.find("*/", k + 2)
                if close == -1:
                    cut = k
                    break
                l = l[:k] + " " * (close + 2 - k) + l[close + 2:]
                k = close + 2
                continue
            k += 1
        line = l if cut is None else l[:cut]
        if line.strip() == "":
            continue
        out.append(line.rstrip())
    return "\n".join(out)
