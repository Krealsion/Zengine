# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Steps the Workshop tools share: the link's own session, this session's inventory row, a
picture fetched and checked chunk by chunk, and key chords spelled as Workshop's Input owner
spells them (zengine/input/vocabulary.hpp: scancodes are SDL's, modifiers shift=1 ctrl=2 alt=4)."""

import json

from loom_session.values import BOOL, BYTES, FLOAT, INT, TEXT

ZERO = {INT: 0, FLOAT: 0.0, TEXT: "", BOOL: False, BYTES: b""}

LETTERS = dict((chr(ord("a") + i), 4 + i) for i in range(26))
DIGITS = dict((str((i + 1) % 10), 30 + i) for i in range(10))
NAMED = {"enter": 40, "return": 40, "escape": 41, "esc": 41, "backspace": 42, "tab": 43,
         "space": 44, "right": 79, "left": 80, "down": 81, "up": 82, "home": 74, "end": 77,
         "delete": 76}
NAMED.update(dict(("f%d" % (i + 1), 58 + i) for i in range(12)))
# Punctuation by SDL scancode (zengine/input/vocabulary.hpp `scan::kMinus` .. `scan::kSlash`),
# spelled by the character or its name; "+" separates a chord, so a plus is "shift+=".
NAMED.update({"-": 45, "minus": 45, "=": 46, "equals": 46, "[": 47, "]": 48, ";": 51, "'": 52,
              "`": 53, ",": 54, "comma": 54, ".": 55, "period": 55, "/": 56, "slash": 56})
MODIFIERS = {"shift": 1, "ctrl": 2, "control": 2, "alt": 4}
# zengine/input/vocabulary.hpp space::: the two terminal skins report cells, the SDL skin pixels.
SPACE_CELLS = 1
SPACE_PIXELS = 2


def chord(spelling):
    """``(scancode, modifiers)`` for a chord such as ``ctrl+p`` or ``down``."""
    parts = [p.strip().lower() for p in spelling.split("+") if p.strip()]
    if not parts:
        raise ValueError("an empty chord")
    mods = 0
    for p in parts[:-1]:
        if p not in MODIFIERS:
            raise ValueError("'%s' is not a modifier (shift, ctrl, alt)" % p)
        mods |= MODIFIERS[p]
    key = parts[-1]
    code = LETTERS.get(key) or DIGITS.get(key) or NAMED.get(key)
    if code is None:
        raise ValueError("'%s' is not a key this tool can spell" % key)
    return code, mods


def moment(ctx, kind, **given):
    """One WHOLE ``InjectedEvent`` of ``kind``: Workshop's gate refuses a moment that leaves out a
    field its shape declares. The field list is the host's own descriptor, never restated here;
    a field this moment does not use is zero, the default zengine/input/vocabulary.hpp declares
    for every one of them."""
    shape = ctx.conn.schema("InjectedEvent", 1)
    out = {}
    for f in shape.fields:
        if f.name == "kind":
            out[f.name] = kind
        elif f.name in given:
            out[f.name] = given.pop(f.name)
        elif f.type.kind in ZERO:
            out[f.name] = ZERO[f.type.kind]
        else:
            raise ValueError("InjectedEvent v1 declares '%s', which has no zero this tool can "
                             "supply" % f.name)
    if given:
        raise ValueError("InjectedEvent v1 declares no field %s" % ", ".join(sorted(given)))
    return out


def chord_moments(ctx, spelling, text="", repeat=1):
    """The moments of one chord, pressed and released `repeat` time(s) -- never held, so the
    batch bound on keys held at once never sees more than one -- and optionally the text typed
    after the last press. `repeat` is a maker's hand pressing the same key again, not the
    platform's own key-repeat (which holds and never releases between): this pane's own list
    navigation (row up/down) answers to the same press-release pair a single tap sends, however
    many times it is sent."""
    if repeat < 1:
        raise ValueError("repeat must press the chord at least once")
    code, mods = chord(spelling)
    events = []
    for _ in range(repeat):
        events.append(moment(ctx, "KeyPressed", scancode=code, modifiers=mods))
        events.append(moment(ctx, "KeyReleased", scancode=code, modifiers=mods))
    if text:
        events.append(moment(ctx, "TextEntered", text=text))
    return events


def clear_moments(ctx):
    """Ctrl+A, then Backspace, pressed and released once each -- ``component/text_box.hpp``'s
    own ``select_all`` followed by its own ``backspace``, which the same file documents as
    erasing the SELECTION whole when one is active ("while text is selected, erase the
    SELECTION"), never one character beside it. WHY NOT A BACKSPACE COUNT, the tool's own
    earlier order: a fixed number of Backspaces only clears a default no longer than the count,
    and only from a caret already at the field's end -- wrong the moment a suggested default
    runs longer, or a click has already placed the caret mid-field (`click` before `clear` in
    this tool's own order does exactly that).

    WHY THE BACKSPACE IS NOT LEFT TO `TextBox::type` ALONE. `type`'s own "WHILE TEXT IS
    SELECTED, TYPING REPLACES IT" only fires when a caller's own text is non-empty -- this
    tool's caller skips the `TextEntered` moment entirely when `text` is empty (there is no
    such thing as typing nothing), so a selection with no typing after it would otherwise
    survive untouched and the committing chord would submit whatever was already there. The
    Backspace here does not depend on whether text follows: it erases the selection either way,
    so an empty replacement and a non-empty one are the same two-step act (select, erase) with
    typing as a true optional third step, not a hidden precondition of erasure.

    Four moments, not the 48 the old count could reach -- a click, the text and the committing
    chord all fit beside it in one injected batch with room held in reserve besides.

    WHAT THIS DOES NOT COVER. A field this pane never hands a `TextBox` -- none exists in this
    package today -- would not answer to Ctrl+A or Backspace: `zengine.input` still admits the
    moments (they are ordinary key events, not a request naming their target), so nothing here
    refuses them, and this tool cannot see, from a picture alone, whether the pane it reached
    consumed them or dropped them. `inspect_capture.py`'s own `changed` check is what a caller
    keeps that honest with -- and asserting the FIELD'S OWN RESULTING TEXT, not merely that the
    picture changed, is `workshop/verify_recipe.py`'s job once a recipe is written from it."""
    select_code, select_mods = chord("ctrl+a")
    erase_code, erase_mods = chord("backspace")
    return [moment(ctx, "KeyPressed", scancode=select_code, modifiers=select_mods),
            moment(ctx, "KeyReleased", scancode=select_code, modifiers=select_mods),
            moment(ctx, "KeyPressed", scancode=erase_code, modifiers=erase_mods),
            moment(ctx, "KeyReleased", scancode=erase_code, modifiers=erase_mods)]


def point(spelling):
    """``(x, y, space)`` for a point spelled ``"126,42"`` (pixels -- the SDL skin's own unit) or
    ``"10,3c"`` (cells -- the two terminal skins'). Empty means an empty point, for callers that
    make one optional."""
    if not spelling:
        raise ValueError("an empty point")
    cells = spelling.endswith(("c", "C"))
    body = spelling[:-1] if cells else spelling
    parts = body.split(",")
    if len(parts) != 2:
        raise ValueError("'%s' is not a point ('x,y' pixels, or 'x,yc' cells)" % spelling)
    try:
        x, y = int(parts[0].strip()), int(parts[1].strip())
    except ValueError:
        raise ValueError("'%s' is not a point ('x,y' pixels, or 'x,yc' cells)" % spelling)
    return x, y, (SPACE_CELLS if cells else SPACE_PIXELS)


BUTTONS = {"left": 1, "middle": 2, "right": 3, "1": 1, "2": 2, "3": 3}


def button_of(word):
    """``zengine/input/vocabulary.hpp``'s own numbering (1 left, 2 middle, 3 right) for a
    spelling such as ``"right"``; empty means the default, left."""
    if not word:
        return 1
    key = word.strip().lower()
    if key not in BUTTONS:
        raise ValueError("'%s' is not a button (left, middle, right)" % word)
    return BUTTONS[key]


def click_moments(ctx, spelling, button="left"):
    """The moments of one click at ``spelling`` (see :func:`point`) with ``button`` (see
    :func:`button_of`) -- pressed, released, at the same position, the same way a hand reports
    one: down and up do not drift. `right` is this pane's own second route to its context menu
    wherever the menu declares no key (`workshop/pane_menu.hpp`; a line open for typing keeps
    every ordinary letter, `M` included, so the menu moves to the pointer there on purpose).

    A CLICK THAT MAY OPEN A MENU SHOULD OFTEN BE THE WHOLE BATCH, NOT FOLLOWED BY A CHORD.
    `workshop/weave_handlers.cpp` counts every `KeyPressed` as a new gesture (`++gestures_`,
    unconditional, before any menu-specific dispatch runs), and a menu a click opened stays
    eligible to grant only while `gestures_` has not moved past the moment the click itself was
    dispatched (`workshop/weave_external.cpp`: `refuse("late -- the maker acted since that
    gesture, or it was already spent")`). A batch that appends ANY key after the click -- even
    one this pane binds to nothing, sent only to force a settle and a fresh picture -- is
    itself a later gesture, and can invalidate the very menu the click was sent to open before
    this tool ever asks for the picture that would show it. `inspect_capture.py`'s own `chord`
    input is optional for exactly this reason: a caller testing a click's own menu-opening
    effect leaves it empty, settles on the click alone, and reads or acts on the result in a
    separate, later run."""
    x, y, space = point(spelling)
    b = button_of(button)
    return [moment(ctx, "PointerButton", button=b, pressed=True, x=x, y=y, space=space),
            moment(ctx, "PointerButton", button=b, pressed=False, x=x, y=y, space=space)]


def link_session(ctx, link):
    """The link's own word about the far session it holds, and as whom; the run fails unless it
    is admitted."""
    s = ctx.ask("loom.link." + link, "loom.link.StatusRequested", {})
    ctx.check(s["state"] == "admitted", "the link '%s' is %s%s" % (
        link, s["state"], (": " + s["detail"]) if s["detail"] else ""))
    return s


def own_row(ctx, link, status):
    """The guest door's inventory, and the row that IS this link's far session -- matched by the
    far session the link holds, never by being the last admitted row."""
    inv = ctx.ask("zengine.guests", "GuestConnectionsRequested", {}, via=link)
    mine = [r for r in inv["rows"] if r["session"] == status["session"]]
    ctx.check(mine, "zengine.guests lists %d connection(s) and none is this link's far session %d"
              % (len(inv["rows"]), status["session"]))
    row = mine[0]
    ctx.check(row["state"] == "admitted" and row["established"] == status["established_name"],
              "zengine.guests lists this session as %s '%s', not admitted as '%s'"
              % (row["state"], row["established"], status["established_name"]))
    ctx.produce("connections.json", json.dumps(inv.fields, indent=1).encode("utf-8"))
    return inv, row


def fetch_picture(ctx, link, captured):
    """Every byte of one retained picture, each chunk checked to continue it."""
    total = captured["bytes"]
    data = bytearray()
    while len(data) < total:
        c = ctx.ask("zengine.skin", "SurfaceCaptureChunkRequested",
                    {"capture": captured["capture"], "offset": len(data)}, via=link)
        piece = c["data"]
        ctx.check(c["capture"] == captured["capture"] and c["offset"] == len(data) and
                  c["total"] == total and piece and len(data) + len(piece) <= total,
                  "zengine.skin sent a chunk of capture %d at %d (%d of %d bytes) where capture "
                  "%d at %d of %d was asked for" % (c["capture"], c["offset"], len(piece),
                                                    c["total"], captured["capture"], len(data),
                                                    total))
        data += piece
    return bytes(data)


def check_picture(ctx, captured, data):
    """A picture is what its format says it is, whole."""
    ctx.check(captured["ok"], "zengine.skin refused the picture: %s" % captured.get("refusal"))
    ctx.check(len(data) == captured["bytes"], "the picture is %d bytes, not the %d described"
              % (len(data), captured["bytes"]))
    if captured["format"] == "image/bmp":
        ctx.check(data[:2] == b"BM" and int.from_bytes(data[2:6], "little") == len(data),
                  "the picture does not read as the whole BMP it claims to be")
        return "bmp"
    ctx.check(captured["format"] == "text/cells", "an unknown picture format '%s'"
              % captured["format"])
    return "cells.txt"


def picture(ctx, link, name, after_frame=-1, pending=None):
    """Ask for a picture (or collect one already asked for), fetch it whole and keep it as
    ``<name>.<bmp|cells.txt>``. Returns ``(captured, bytes)``."""
    captured = pending.wait(60.0) if pending is not None else ctx.ask(
        "zengine.skin", "SurfaceCaptureRequested", {"after_frame": after_frame}, via=link)
    data = fetch_picture(ctx, link, captured) if captured["ok"] else b""
    ext = check_picture(ctx, captured, data)
    ctx.produce("%s.%s" % (name, ext), data)
    return captured, data


def png_of(bmp, crop=None):
    """A PNG of an uncompressed 24/32-bit BMP picture (what the SDL skin hands back), standard
    library only, optionally cropped to [x0, y0, x1, y1] in the picture's pixels: small enough to
    keep as evidence or put in a document, where the BMP is megabytes."""
    import struct
    import zlib
    offset = struct.unpack_from("<I", bmp, 10)[0]
    width, height = struct.unpack_from("<ii", bmp, 18)
    bpp = struct.unpack_from("<H", bmp, 28)[0]
    if bpp not in (24, 32):
        raise ValueError("a %d-bit picture is not one this encoder reads" % bpp)
    top_down, height = height < 0, abs(height)
    stride, step = ((width * bpp + 31) // 32) * 4, bpp // 8
    x0, y0, x1, y1 = crop or (0, 0, width, height)
    x0, x1 = max(0, min(x0, width)), max(0, min(x1, width))
    y0, y1 = max(0, min(y0, height)), max(0, min(y1, height))
    raw = bytearray()
    for y in range(y0, y1):
        at = offset + (y if top_down else height - 1 - y) * stride
        row = bmp[at + x0 * step: at + x1 * step]
        out = bytearray((x1 - x0) * 3)
        out[0::3], out[1::3], out[2::3] = row[2::step], row[1::step], row[0::step]
        raw += b"\x00" + out

    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", x1 - x0, y1 - y0, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b""))
