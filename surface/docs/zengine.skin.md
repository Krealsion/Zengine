# zengine.skin -- the Skin

**Manual.** The Skin: the weave that owns the screen, answering to the office `zengine.skin` (an
office is the name a weave answers to). It takes the pictures and lines other weaves publish and
paints them, in a terminal or in a window. Two Skins ship, in three libraries, and a load plan
loads one library into the office: `zengine-skin-tui-classic` paints a terminal;
`zengine-skin-tui-block` is that same terminal Skin built as a second library, so a running Skin
has another library to be replaced by, and it looks no different; and `zengine-skin-sdl`, built
unless the build sets `ZENGINE_SDL_SKIN=OFF`, paints a window. Everything on this page is true of
all three libraries unless it names one.

## About

No other weave touches the terminal or a window: a weave publishes what it wants drawn, in
[the Surface vocabulary](surface.md), and the Skin holding the office paints it. A Skin claims its
medium when it is loaded and gives it back when it is unloaded. The office holds one Skin at a time:
loading a second into it is refused, and replacing the one there (Loom's swap) unloads it before
its successor claims the medium. Workshop's default plan loads `zengine-skin-tui-classic`, and its
graphical plan `zengine-skin-sdl` ([the two shipped plans](../../workshop/docs/load-plans.md#the-two-shipped-plans)).

**The two terminal libraries are the same Skin**, so a terminal Skin replaced while running has
another library to be replaced by; they paint the same bytes, and neither is a look of its own.
Each takes the terminal's output side: the alternate screen, a hidden cursor, pointer reporting
where the pointer arrives in band (not on a Windows console, which hands the pointer to the Input
weave's reader), and on Windows the console's VT processing and UTF-8 output; it puts every one
back when it is unloaded. With no terminal (redirected, piped, a suite) it claims nothing and
still writes each frame to its output. Rows 1 and 2 are the `status` and `score` lines; the
picture starts at row 3.

**The SDL Skin is a window**, opened by the first picture. It sets labels in a small bitmap face
and text regions in a real typeface, JetBrains Mono, compiled in
([its provenance](../fonts/PROVENANCE.md)), since at that size a bitmap `a`, `e`, `o` and `c`
differ by a pixel. When SDL will not start, the Skin still loads, takes every message, and says
why on stderr in SDL's words: `zengine-skin-sdl: SDL_Init(SDL_INIT_VIDEO) failed: <SDL's reason>`.
The window is also an ear: its events are the SDL Input reader's, and the Skin takes none.

A Skin paints canvases and lines, and nothing else: the snake's board is drawn by
[the Graphics package](../../graphics/docs/graphics.md)'s terminal window.

## Use

Publish your picture as a [`SurfaceCanvas`](#surfacecanvas) and your line as
[`SurfaceText`](#surfacetext); the Skin paints the last of each. When you hear
[`SurfaceReady`](#follow), publish your line and your picture again: a Skin that just took the
office has painted nothing. Size your picture to the last [`SurfaceExtent`](#follow) you heard, or
keep your own size and let the Skin clip it. On a paste, ask
[`ClipboardTextRequested`](#clipboardtextrequested); to see what was presented, ask
[`SurfaceCaptureRequested`](#surfacecapturerequested). Address the office, never a weave's id: a
replaced Skin is another weave. Any weave may also ask the office what a shape it accepts does
([`ShapeDocumentRequested`](../../manual/docs/manual.md#an-office-answers-for-a-shape)).

Who may send: in Workshop, a weave its plan loads (Workshop admits every artifact its plan names,
`workshop/admission.hpp`). Workshop itself publishes the canvas, its line and each copy, and sends
`ClipboardTextRequested` and `SurfacePlacementRemembered` to the office (`workshop/grant.cpp`).
Workshop's Terminal may send `SurfaceText` to the office and nothing else of these. A guest row
with the `capture` power may ask for captures and their chunks (`workshop/guests.cpp`).

## Commands

### `SurfaceCanvas`

`SurfaceCanvas` v9 is a whole picture: an extent in canvas pixels and its layers, back to front.

- **Takes:** `width` and `height`, in canvas pixels; `layers`, each its rects, then its labels,
  then its text regions ([the drawing model](surface.md)).
- **Answers:** nothing; it paints, counts a frame, then says where the window is and how much room
  it has, when either changed ([Follow](#follow)). A terminal Skin floors the picture to whole cells
  and paints each rectangle as its role's colour and glyph (accent `*`, muted `.`, alert `!`,
  guide `+`, fill and any role it does not know `#`, a ground a space) and each label and text row
  as its own characters in its role's colour; the first picture clears the screen from row 3 down,
  and a picture shorter than the last erases the rows it no longer covers. The SDL Skin draws
  the picture one canvas pixel to a window pixel, sets each label byte in its bitmap face (printable
  ASCII; any other byte is a visible box, never dropped), and sets each text region in its typeface,
  or in cells when the typeface did not open. Its window is created at the first picture's size,
  which becomes its smallest: a weaver may drag it larger, and only a picture that does not fit
  grows it, within its display, so a picture sized to the room the Skin reported never moves it.
  It is centred on its display until someone moves it, and kept whole on the display afterwards.
- **Refuses:** nothing. A picture larger than the room is clipped; a canvas with no layers is a
  picture of nothing and clears the last one.
- **Example:** Workshop publishes its whole screen as one canvas after every change.

### `SurfaceText`

`SurfaceText` v1 is one line of plain text for a named slot: `status` or `score`.

- **Takes:** `slot`, `status` or `score`; `text`, plain text.
- **Answers:** nothing; it counts a line. A terminal Skin writes `status` on row 1 and `score` on
  row 2, over what stood there. The SDL Skin puts both in the window's title,
  `zengine [sdl skin] | <status> | <score>`, and draws `score` in the picture's top-right corner, in
  the alert role, over everything; an empty `score` draws nothing there.
- **Refuses:** nothing; a slot it has no place for is dropped.
- **Example:** `mail.publish(zengine::surface::SurfaceText{zengine::surface::kSlotStatus, "saved"});`

### `ClipboardTextRequested`

`ClipboardTextRequested` v1 asks what the platform clipboard holds now, because a weaver pressed
paste: it is the one way the system clipboard's text reaches the bus, and nothing reads it
otherwise.

- **Takes:** nothing.
- **Answers:** `ClipboardText` v1, to the asker alone. The SDL Skin reads the platform clipboard:
  `readable` true, and `text` empty when the clipboard holds no text. A terminal Skin answers
  `readable` false: no terminal can say, and the asker pastes what this process last copied
  ([`ClipboardCopy`](#clipboardcopy)). The SDL Skin whose SDL did not start answers the same.
- **Refuses:** nothing.
- **Example:** Workshop's paste: `mail.send_to_role(zengine::surface::kSkinRole, zengine::surface::ClipboardTextRequested{}, corr);`,
  settling the answer as any ask.

### `SurfacePlacementRemembered`

`SurfacePlacementRemembered` v1 offers back where the window was: a want, not an instruction.

- **Takes:** `x` and `y`, the normal window's top-left as a [`SurfacePlacement`](#follow) said
  it; `maximized`.
- **Answers:** nothing directly; what follows is the next `SurfacePlacement` and `SurfaceExtent`,
  never an echo. The SDL Skin judges the place against the displays there now: a window wholly on a
  display goes where it was; any other is moved in until the whole window, its frame included, is on
  the display it overlaps most; with no display to judge by, nothing moves. A remembered maximize
  waits for the next picture to size the window, then lands when the medium is next serviced. From
  then on the window is no longer centred. An offer that arrives before the window exists does
  nothing, and a terminal Skin does nothing with any.
- **Refuses:** nothing. A maximize the platform will not make is said once on stderr,
  `zengine-skin-sdl: SDL_MaximizeWindow failed: <SDL's reason>`, and not tried again.
- **Example:** Workshop sends the placement its session file kept, once, when it restores a session.

### `PumpSurface`

`PumpSurface` v1 asks the Skin to service its medium now: the same work its own beat does
([`TimerFired`](#timerfired)), for a suite or a host with no Timer.

- **Takes:** nothing.
- **Answers:** nothing; it counts a pump, then says where the window is and how much room it has,
  when either changed. A terminal Skin has nothing to service. The SDL Skin lets the window answer
  its operating system and takes no event out of the queue; when so many stand unread that nothing
  can be taking them, it says so once on stderr, as `zengine-skin-sdl: 1200 events are queued for
  this window and nothing is taking them.`, and names what to load: `zengine-input-sdl` as
  `zengine.input`.
- **Refuses:** nothing.
- **Example:** `bus.send_to_role(zengine::surface::kSkinRole, loom::Message(loom::to_value(zengine::surface::PumpSurface{})));`

### `SurfaceCaptureRequested`

`SurfaceCaptureRequested` v1 asks for a picture of what the medium presented, now or once a later
frame has been painted. A picture proves what was shown at one frame, and nothing about work still
pending: ordering it after an input is the injecting host's (a
[fence](https://github.com/Krealsion/Loom/blob/main/docs/reference/messaging.md#fences-when-what-one-send-set-in-motion-has-been-dispatched),
or a link ask's `settle`).

- **Takes:** `after_frame`: -1 for now; a frame number at or past the Skin's count waits for the
  picture that passes it, whoever paints it.
- **Answers:** `SurfaceCaptured` v1, to the asker alone: `ok`, the picture's number (`capture`), the
  `frame` it shows, `width` and `height`, `cell_px`, `format` and its size in `bytes`. The Skin keeps
  this one picture, replacing the last. The SDL Skin draws the last picture again, reads it back and
  presents it, so what you get is what the window shows: `image/bmp`, 24-bit, in window pixels,
  `cell_px` `kCanvasCellPx`. A terminal Skin answers the cells it painted: `text/cells`, one row of
  `width` bytes a line, `cell_px` 0.
- **Refuses:** `ok` false, in `refusal`: `a capture is already waiting for frame 4; one waits at a
  time`; `this delivery cannot defer its answer`; `this medium has no picture to read back right
  now`, before any picture or with no window; `the picture is <n> bytes, over the <m> this Skin
  retains`, `m` being `kMaxCaptureBytes`: a picture too large is refused, never cut.
- **Example:** a guest with the `capture` power asks `{after_frame: -1}`, then fetches the bytes.

### `SurfaceCaptureChunkRequested`

`SurfaceCaptureChunkRequested` v1 asks for the bytes of the picture the Skin keeps, from an offset.

- **Takes:** `capture`, the number `SurfaceCaptured` gave; `offset`, the first byte wanted.
- **Answers:** `SurfaceCaptureChunk` v1, to the asker alone: `capture`, `offset`, the picture's
  `total` and up to `kCaptureChunkBytes` of `data`; the last chunk is the one whose offset and data
  reach `total`.
- **Refuses:** with `zen.Refused`: `no picture is retained; ask for a capture first`; `capture 2 is
  gone; the retained one is 3`; `offset 90000 is outside a picture of 86454 bytes`.
- **Example:** ask `{capture: 3, offset: 0}`, then from each chunk's end until `total`.

## Hears

### `ClipboardCopy`

`ClipboardCopy` v1, from whoever hosted a copy: a weaver copied this text. The SDL Skin puts it on
the platform clipboard, saying on stderr when SDL will not. A terminal Skin writes the terminal's
set-clipboard sequence (OSC 52), which a terminal may honour or ignore, and claims nothing.

### `TimerReady`

`TimerReady` v1, from the Timer: a Timer is there now, so the Skin asks it for its beat again.

### `TimerFired`

`TimerFired` v1, from the Timer: the Skin's beat, `zengine.skin.pump`, every `kPumpBeatMs`. Each
does what [`PumpSurface`](#pumpsurface) does; a firing of another id is ignored.

## Follow

Each Skin publishes, from `zengine.skin`: `SurfaceReady` v1 once per incarnation, when it is
activated, or when nothing activated it, on the first message about its medium that reaches it;
`SurfaceExtent` v4 when the room changes and only then, never zeroes (a terminal's cells times
`kCanvasCellPx`, less `kTuiReservedRows` rows, with text a cell; the window's pixels, its face's
advance and line, and `cell_px` `kCanvasCellPx`); with no answer (no terminal, no window yet) it
says nothing. The SDL Skin also publishes `SurfacePlacement` v1 when the window moves or is
maximized, before the extent, always the normal window's top-left; a terminal Skin never does. On
activation and on `TimerReady` a Skin sends `StartRoleTimer` v1 to `zengine.timer`:
`zengine.skin.pump`, repeating, to the office, so a replacement takes over the beat.

## See also

- [The Surface package](surface.md): what a weave publishes, the roles, the layers and the two kinds
  of text, and the arithmetic every Skin shares.
- [Pointer spaces](../../workshop/docs/pointer-spaces.md): where a pointer lands on a picture.
- [Input](../../input/docs/input.md): the reader that owns the window's events, and the close box.
- [Load plans](../../workshop/docs/load-plans.md): how a plan puts a Skin in the office.
- [A weave's manual](../../manual/docs/manual.md).
