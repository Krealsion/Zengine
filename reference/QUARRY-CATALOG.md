<!--
SPDX-License-Identifier: MPL-2.0
Copyright (c) 2026 Joshua DeMoss

This file is Zen-authored archaeology ABOUT reference/, not part of the imported
V1 tree. Everything else in this directory is the imported working copy.
-->

# The `reference/` quarry — legacy capability catalog

**What this is.** A source-addressable index of the pre-Zen V1 game engine kept in
`reference/`, written so a future reader can answer three questions without reading nine
thousand lines of C++:

> Did the old engine contain something relevant to this capability?
> What did it expose, and where do I look?
> Does current Zen/Zengine already answer that, and how?

**What this is not.** Not a specification, not a port plan, not a backlog, and not authority.
Nothing here recommends that Zengine adopt an old class, an old architecture, or an old name.
The governing rule:

> **Catalog what the old engine knew; do not presume the old engine knew how Zen should know it.**

An entry saying a capability is absent from current Zengine is a *fact about coverage*, not a
request for work. Product priority is decided elsewhere.

**Status of the material.** `reference/` is a plain file import of a working tree, frozen at the
Zengine repository seed and unmodified since. It is **not built** by this repository, is
excluded by rule from `doc_links` and `package_vocabulary`, and is not installed
(`docs/contributing/repository-conventions.md`). The catalog below was written by reading the
source, not by building or running it — see [§0.4](#04-the-tree-does-not-build-as-it-stands).

---

## How to use this catalog

- **Looking for a capability** → [§1 Legacy Capability Catalog](#1-legacy-capability-catalog),
  organised by capability rather than by directory.
- **Asking "does Zen already have this?"** → [§2 Current capability coverage](#2-current-capability-coverage).
- **Searching by concept** (`text selection`, `signals`, `config generation`, …) →
  [§3 Quarry index](#3-quarry-index).
- **Asking "is any of this worth taking?"** → [§4 Reusable quarry candidates](#4-reusable-quarry-candidates).
- **Asking "why does Zengine do it differently?"** → [§5 Architectural anti-precedents](#5-architectural-anti-precedents).

Every entry carries the same fields:

```text
Source           exact paths, so a quarrying pass can jump straight to the file
Tags             index terms, matching §3
Classification   one or more of the labels below
What existed     the capability, in Zen-neutral words
Interaction      the maker-/programmer-facing shape: what was callable, in what order
Current Zengine  where the comparable question is answered today, if it is
Traps            what a reader would get wrong, and what does not work in the source
```

**Classification labels.** Multiple labels per entry where truthful; no entry is forced to
carry all of them.

| label | means |
|---|---|
| `REQUIREMENT` | a capability the founder evidently expected a usable engine to have |
| `UX EXPECTATION` | evidence about how a capability should feel, or which operations should be reachable |
| `INTERFACE QUARRY` | a public API worth studying later even where the implementation should not survive |
| `CONCEPTUAL PRECEDENT` | the old code addressed a problem current Zen also addresses |
| `ALGORITHM QUARRY` | the implementation contains logic potentially worth mining |
| `SUPERSEDED` | current Zen/Zengine has a stronger answer |
| `PARTIAL` | current Zen covers part of the old role, not all of it |
| `ABSENT` | the capability appears materially absent from current Zen |
| `ANTI-PRECEDENT` | useful specifically because current Zen chose a different direction deliberately |
| `LOW QUARRY VALUE` | little reason to revisit beyond the historical record |

---

## 0. What is actually in the tree

### 0.1 Shape and size

129 files. About **9,000 lines** of C++ across headers and sources, plus three TrueType/OpenType
fonts, two JSON config files, a Python helper and four Markdown/text documents. Every file
arrived in one commit and none has been edited since.

```text
reference/
  CMakeLists.txt CMakePresets.json .clang-format .clang-tidy   build + lint configuration
  readme.md  vision.md  todo.txt  compile_code.py              documents and one tool
  Resources/
    TTFs/            three fonts + one EULA        (see Traps in §1.4)
    config/          default.json, current.json    the config schema and the live config
    generated/       config.h, config.cpp          checked-in generator OUTPUT
  src/
    kernel.hpp kernel.cpp main.cpp bootstrap.cpp   ← stratum B, see §0.2
    console.cpp advanced_console.cpp               ← stratum B
    zengine/                                       ← stratum A: the V1 engine
      graphics/ graphics_3d/ input/ logic/ types/ ui/
      state_management/ message_bus/
      callback.h zsignal.h timer.* logger.* config_manager.* config_code_gen.cpp
      console.* rttr_wrapper.h
    apps/
      builder/       the visual UI builder            ← the richest single entry
      tests/cookie_clicker/  a demo game
      examples/      four small programs
```

### 0.2 Three strata, not one codebase

Reading `reference/` as a single artifact is the first mistake available. It holds three layers
written at different times against different ideas:

| stratum | files | what it is |
|---|---|---|
| **A — the V1 engine** | `src/zengine/**`, `src/apps/**`, `Resources/**`, `CMakeLists.txt`, `readme.md`, `todo.txt` | the actual older game engine. Almost everything in §1 comes from here. |
| **B — a Zen kernel prototype** | `src/kernel.{hpp,cpp}`, `src/main.cpp`, `src/bootstrap.cpp`, `src/console.cpp`, `src/advanced_console.cpp` | a first sketch of the *substrate* the Loom now owns: a DLL loader, a name→value registry called "senses", a REPL. Not part of the game engine and it does not use it. [§1.20](#120-the-zen-kernel-prototype-stratum-b) |
| **C — an intermediate Zen vision** | `vision.md` | a design document for Zen written *after* the V1 engine and *before* the current architecture. Uses Harness / Shards / Switchboard / Senses / Domain vocabulary. Explicitly proposes migrating three V1 subsystems (Input, Renderer, one Game) as Shards. |

`vision.md` is not the V1 engine's own document — it is a bridge document that treats the V1
engine as the thing being migrated *from*. Its vocabulary is superseded: the Loom owns the
substrate today and none of Harness / Shard / Sense / Domain is current Zen terminology. Read it
as a record of a design position, never as current law.

### 0.3 Generational drift *inside* stratum A

Stratum A is itself not internally consistent — several files were written against an earlier
generation of the same API and never updated. This matters because a reader who takes any one
file as authoritative about "what the old engine looked like" will be wrong:

| file | assumes | actually |
|---|---|---|
| `src/apps/examples/*.cpp` | `src/engine/timer.h`, `Timer::get_current_time()` | header lives at `src/zengine/timer.h`; no such method exists |
| `src/apps/examples/message_bus_example.cpp` | `DataPacket::get_object<T>` returning something dereferenceable | `get_object` is non-template and returns `rttr::variant` |
| `src/zengine/console.h` | a `CodeGenerator` class; `RttrWrapper::invoke_dynamic` returning `DataPacket` | no `CodeGenerator` exists anywhere; `invoke_dynamic` returns `bool` |
| `src/zengine/graphics/sprite.h` | `src/graphics/user_interface/custom_layout.h` | that path does not exist; the file is orphaned and unreferenced |
| `src/zengine/types/function.h` | a type called `AnythingStorage<T>` | the type is `VarStorage<T>`; the file is orphaned and unreferenced |
| `src/apps/builder/main.cpp` | `<utility_states/build_state.h>` | the header is at `apps/builder/build_state.h` |
| `Resources/generated/config.cpp` | `engine/config_manager.h` | the header is at `src/zengine/config_manager.h` |

**`VarStorage` was previously called `AnythingStorage`** and **`DataPacket::get_object` was
previously a template**. Both renames are visible only as breakage.

### 0.4 The tree does not build as it stands

Not attempted, and making it compile is explicitly out of scope. But structural facts observed
while reading are worth recording so nobody re-derives them:

- the top-level `CMakeLists.txt` roots the public include path at
  `${CMAKE_CURRENT_SOURCE_DIR}/game_engine_code/engine/` — **no such directory exists**;
- `src/zengine/CMakeLists.txt` adds `target_include_directories(Zen PUBLIC /types/)` — an
  absolute filesystem root path;
- the codegen custom command invokes a target named `zen_codegen`; the executable actually
  defined is `generate_configs`, and it writes into `${CMAKE_BINARY_DIR}/generated` while the
  library compiles the checked-in `Resources/generated/config.cpp`;
- `Timer::_last_real` is declared `static` in `timer.h` and **never defined** in `timer.cpp`,
  while `Timer::Timer` and `Timer::_accumulate_time` both odr-use it;
- `MessageBus::_singleton` is declared and never defined; `message_bus.h` defines the
  non-inline static `MessageBus::message_listeners` **in a header**;
- `ConfigManager::get<T>` has explicit instantiations for `bool/int/float/double/std::string`
  but the generator emits `int64_t` for integer leaves, so `Config::ui::get_base_padding()`
  has no instantiation to link against;
- `src/advanced_console.cpp` names `zen::Sense`, a type `kernel.hpp` does not declare, and is
  not listed in any `CMakeLists.txt`.

Treat the tree as a **snapshot taken mid-reorganisation**, not as a shipped build.

### 0.5 Authorship weight

Some files carry markers consistent with LLM-assisted authoring — `cookie_clicker_state.cpp`
contains two literal `:contentReference[oaicite:N]{index=N}` artifacts and comments addressed to
the reader in the second person ("your codebase", "your existing UX expectation"); parts of
`advanced_console.cpp` and `build_state.cpp` are phrased the same way. This is recorded because
it changes evidential weight, not quality: such code is weaker evidence of *the founder's own
expectations* than hand-written code is. Where an entry below rests on this material, it says so.

---

## 1. Legacy Capability Catalog

### 1.1 Application lifecycle and game state

**Source** `src/zengine/state_management/game_state.{h,cpp}` ·
`src/zengine/state_management/game_state_manager.{h,cpp}`
**Tags** `state manager` `lifecycle` `main loop` `overlay` `singleton`
**Classification** `REQUIREMENT` · `CONCEPTUAL PRECEDENT` · `ANTI-PRECEDENT`

**What existed.** The engine's headline promise, per `readme.md`: a programmer should be able to
start writing a game without first setting up a window, a renderer and an update cycle. A
singleton `GameStateManager` owned the renderer, the draw list and a **stack** of `GameState`s,
and ran the whole loop itself.

**Interaction.** The entire startup shape was one line — `apps/builder/main.cpp` and
`apps/tests/cookie_clicker/main.cpp` are three lines each:

```cpp
GameStateManager::singleton().initialize_with_state(new BuildState());
```

`initialize_with_state` never returns until `exit()`. Its loop, in order:
`Input::update_input()` → `ConfigManager::reload_if_changed()` → `update()` → `draw()` →
`Input::clean_input()`. A subclass overrode `update()` and `draw(GameGraphics&)`; `exit()`,
`pause()` and `resume()` were virtual with defaults. `push_state` paused the state beneath;
`pop_state` deleted the top and resumed the one below.

Each `GameState` was born owning a root `CustomLayout` sized to the window, and offered
`add_overlay(CustomLayout*)` / `remove_overlay` plus
`add_mouse_callback(Callback<bool,Vector2>, CustomLayout* attachment)` — the callback returning
`true` meaning *consumed*. The manager tried those callbacks first on a left press and fell back
to `root_layout->click_location(mouse)` only if none consumed.

**Current Zengine.** No `GameState` and no engine-owned main loop. A program is a **host** that
loads weaves; lifecycle, replacement and readiness are the Loom's. What a state stack expressed —
"a modal thing on top that gets input first" — is expressed in Workshop by pane ordering plus a
routing priority that reads *four global keys, then the five modes, then a focused pane, then a
live property draft, then commands* (`agents/workshop.md`). Overlay-plus-consuming-callback is
the same shape one layer down.

**Traps.**
- Threading is aspirational: `_update_thread` and `_draw_thread` are members, both
  `std::thread` constructions are commented out, and the class comment describes a
  thread-safety plan that was never implemented.
- `get_current_state()` calls `_game_states.back()` with no empty check.
- The overlay list is raw `CustomLayout*` with no ownership; `DropDown` deletes an overlay it
  registered without removing the registration ([§1.7](#17-ui-the-widget-set)).

---

### 1.2 Window and display

**Source** `src/zengine/graphics/window.{h,cpp}`
**Tags** `window` `display` `resize`
**Classification** `INTERFACE QUARRY` · `SUPERSEDED`

**What existed.** A thin `SDL_Window` wrapper: construct from a name and a `Rectangle`, then
independent `get/set_x`, `get/set_y`, `get/set_width`, `get/set_height`. The window was created
`SDL_WINDOW_RESIZABLE`. The engine created exactly one, inside `Renderer`.

**Interaction.** Position and size are four independent scalar properties, each a read-modify-write
against SDL. There is no notion of a minimum, of an authored size versus a resolved one, or of who
wins when the person and the program disagree.

**Current Zengine.** The SDL skin owns the window and states the ownership rule explicitly: *a
window never shows less than the picture asks for, and is otherwise the person's* — created at
the size its first picture asks for, that size becomes its minimum, and it is grown afterwards
only by a picture that genuinely does not fit (`docs/reference/surface.md`). The measurement
travels the other way as `SurfaceExtent`, published by the medium only when the answer changes.

**Traps.** `Window::set_x` reads the position into a variable named `old_y` and passes it as the
Y argument; `set_y` reads X into `old_y` and passes it as X. Both are copy-paste inversions and
both are wrong. The `Rectangle` constructor argument's *position* is discarded — only width and
height reach `SDL_CreateWindow`.

**Multi-window** appears in `todo.txt` ("Allow for multi-window / BuildState — Build root into its
own window") and in a `renderer.h` comment ("The engine should be able to handle multiple windows
at once, each renderer needs its own window"). It was never built: one `Renderer`, one `Window`,
constructed by the `GameStateManager` singleton at a hard-coded `1820×980`.

---

### 1.3 Drawing

**Source** `src/zengine/graphics/game_graphics.{h,cpp}` · `src/zengine/graphics/renderer.{h,cpp}`
**Tags** `rendering` `draw list` `layers` `clipping` `offset` `painter's order`
**Classification** `REQUIREMENT` · `INTERFACE QUARRY` · `CONCEPTUAL PRECEDENT` · `ANTI-PRECEDENT`

**What existed.** A retained per-frame draw list. Callers never touched SDL: they called
verbs on a `GameGraphics` and each verb pushed a `PriorityDrawable` — a captured lambda plus an
`int layer` and a `float sub_layer`. `Renderer::render_game_graphics` cleared, called
`GameGraphics::draw(SDL_Renderer*)`, and presented. `draw()` stable-sorted the whole list by
`(layer, sub_layer)`, applied each drawable's captured clipping rectangle, invoked it with the
captured offset, then cleared the list.

**Interaction — the whole drawing vocabulary.**

```cpp
draw_rectangle(rect, color, layer=1, sub_layer=1);   fill_rectangle(...);
draw_oval(bounds, color, ...);                       fill_oval(...);
draw_line(start, end, color, ...);
draw_texture(texture, dest, flips, origin, angle, [clipping], ...);   // 4 overloads
draw_text(text, font, font_size, color, max_width, position, ...);
set_clipping(rect); clear_clipping();
set_offset(v); add_offset(v); clear_offset();
set_clear_before_draw(bool);
```

Clipping and offset are **modal**: set once, and every subsequent `add_drawable` captures the
current values. `Color` is literal RGBA (`types/color.h`), never a role.

**Current Zengine.** `SurfaceCanvas` — an extent in **cells** and an ordered list of
`SurfaceLayer`s, each a complete plane of `SurfaceRect`s, `SurfaceLabel`s and
`SurfaceTextRegion`s in painter's order (`docs/reference/surface.md`). Two differences are
structural rather than incidental:
- **role, not colour**: `kFill`/`kAccent`/`kMuted`/`kAlert`, so a terminal picks an SGR *and a
  glyph* while a window picks RGB from one unchanged publisher;
- **publish, don't call**: a weave publishes intent and a replaceable `Skin` paints it. Nothing
  in a weave holds a renderer.

The old two-number `(layer, sub_layer)` sort and the new two-level painter's order answer the
same question and answer it oppositely: sorting made order *global across kinds* — a fact
Zengine measured as a defect and replaced ("a text region belonging to a presentation somebody
had sent to the back still covered a label belonging to the presentation in front").

**Traps.**
- `PriorityDrawable::clipping_rect` is a raw `new Rectangle` freed in the destructor; the struct
  has no copy control.
- `draw_text` opens the font with `TTF_OpenFont`, renders a surface, creates a texture, draws,
  and destroys all three — **inside the per-frame lambda**, for every text draw, every frame.
  There is no font cache and no glyph cache anywhere in the tree.
- `draw_text` throws `std::runtime_error` from inside the draw loop on any SDL failure.
- The oval routines plot individual points in a loop derived from an arc-length estimate;
  `fill_oval` runs an inner scanline loop per step. Correct enough, slow, and not anti-aliased.
- `set_clear_before_draw(false)` exists but the clear colour is hard-coded magenta.

---

### 1.4 Textures, images and fonts

**Source** `src/zengine/graphics/texture.{h,cpp}` · `src/zengine/graphics/texture_manager.{h,cpp}` ·
`src/zengine/graphics/sprite.h` · `Resources/TTFs/`
**Tags** `textures` `images` `resource loading` `fonts` `text measurement` `cache`
**Classification** `REQUIREMENT` · `INTERFACE QUARRY` · `ABSENT` (in current Zengine)

**What existed.** `Texture` wraps `SDL_Texture` and knows its own size. `TextureManager` is a
static registry: a `std::map<std::string, shared_ptr<Texture>>` keyed by path, plus text-texture
creation and text measurement.

**Interaction.**

```cpp
TextureManager::init_ttf();
TextureManager::get_texture("path/to/img.png");                  // cached by path
TextureManager::get_text_texture(text, font_path, size, color);  // NOT cached
TextureManager::get_text_size(text, font_path, size);
TextureManager::get_text_size(text, font_path, size, max_width); // wrapped measurement
TextureManager::unload_textures();
```

Loading is **by path string, lazily, on first request** — there is no asset id, no manifest, no
preload phase, no reference counting beyond `shared_ptr`, and no reload. Image decoding is
`SDL_image`'s `IMG_LoadTexture`; text is `SDL_ttf`'s `TTF_RenderText_Blended_Wrapped`.

**Current Zengine.** No image loading of any kind, no texture cache, no `SDL_image` dependency.
Text is real: the SDL skin embeds JetBrains Mono in the weave's own bytes and measures it once,
publishing the *result* as `SurfaceExtent{text_advance_px, text_line_px}` — because "exactly one
party may measure in a sizing conversation, and for text that party has to be the application"
(`docs/reference/surface.md`). Wrapping is the application's arithmetic against a published
metric, never the medium's own decision.

**Traps.**
- `TextureManager::get_texture` inserts into the map and then returns `_texture_map.end()->second`
  — dereferencing the end iterator. Undefined behaviour on **every cache miss**.
- Two different font-path conventions in one file: `get_text_texture` hard-codes
  `"Resources//TTFs//" + font_path`; `get_text_size` uses `Utility::getResourcePath("TTFs/"…)`.
- `Texture`'s destructor destroys the `SDL_Texture`, and `TextureManager::unload_textures`
  destroys it too, then clears the map — a double destroy for any texture still referenced.
- `sprite.h` is **orphaned**: it includes a path that does not exist, derives from `CustomLayout`
  while duplicating its sizing, and is referenced by nothing. As an *interface* it is still
  readable — set_texture / set_clipping / set_position / set_origin / set_angle / set_scale ×5 /
  flip_x / flip_y — which is a reasonable statement of what a 2-D sprite affordance means.
- ⚠ **Font licensing.** `Resources/TTFs/` holds three faces under terms that are not open.
  The repository already owns this fact — see `THIRD_PARTY_NOTICES.md` and `LICENSING.md`,
  which record the EULA for one and "terms unverified" for the other two. Consult those, not
  this paragraph, before copying a byte out of that directory.

---

### 1.5 Input

**Source** `src/zengine/input/input.{h,cpp}` (the second-largest file in the tree) ·
`src/zengine/callback.h`
**Tags** `input` `key combo` `modifiers` `consumption` `layers` `callbacks` `text input`
`double tap` `rebinding` `mouse` `determinism`
**Classification** `REQUIREMENT` · `UX EXPECTATION` · `INTERFACE QUARRY` · `CONCEPTUAL PRECEDENT` · `PARTIAL`

**What existed.** By far the most *designed* subsystem in the tree, and the one whose ambitions
most closely track questions current Zengine also asks. Two complete APIs live side by side:

1. **Polling** — `is_key_down`, `was_key_pressed_this_frame`, `get_key_pressed_duration`,
   `is_mouse_button_down`, `get_mouse_position/delta/wheel`.
2. **Routed events** — a priority-ordered stack of named layers, each holding handlers that
   return `PASS` or `CONSUME`.

**Interaction — the routed half.**

```cpp
int layer = Input::create_layer("modal", /*priority=*/100);
Input::set_layer_enabled(layer, bool);
Input::destroy_layer(layer);

InputConnection c = Input::on_key(layer, TriggerType::TAPPED, combo,
                                  [](const InputEvent& e){ return InputResult::CONSUME; },
                                  /*min_duration_ms=*/0);
InputConnection m = Input::on_mouse(layer, TriggerType::PRESSED, LEFT, handler);
Input::set_global_filter(fn);          // runs before every layer
// InputConnection is move-only and disconnects in its destructor
```

Supporting vocabulary:

- `KeyCombo{SDL_Scancode key, vector<SDL_Scancode> modifiers}` with **canonical, sorted**
  modifiers. Left and right Shift/Ctrl/Alt/GUI are collapsed into *modifier groups*, and
  `add_custom_modifier(scancode)` promotes any key into its own group — so a maker can make `X`
  behave as a modifier.
- `TriggerType` = `PRESSED · RELEASED · TAPPED · HELD · DOUBLE_TAPPED · REPEAT`, with
  `set_default_input_durations(tap_window_ms, double_tap_duration_ms)`.
- `set_quick_tap_prevention(enabled, buffer_ms)` — a press shorter than a frame is still
  reported to a poller for `buffer_ms` afterwards, so a fast tap is not silently lost.
- `listen_for_key_combo()` — blocks until a non-modifier key is released and returns the combo
  including whatever modifiers were held. This is a **rebinding capture**, i.e. evidence that a
  configurable key map was expected.
- Text input as a *mode*: `start_text_input(Action<const string&> on_text, Action<> on_end)`
  swallows keys, forwards platform text events, and synthesises the sentinels `"\b"` for
  Backspace and `"\0"` for Escape-or-Return, ending the mode on either and on any mouse press.
- Mouse/window services: `warp_mouse`, `warp_mouse_absolute`, `warp_mouse_to_window`,
  `show/hide_cursor`, `is_cursor_visible`, `set/get_relative_mouse_mode`, `get_keyboards`,
  `get_keyboard_name`, `get_keyboard_focus`, `reset_keyboard`.
- `step(uint64_t now_ms)` takes **the engine's authoritative clock as an argument** and from it
  derives the frame's edges, durations and deterministic event list. `route_events()` then walks
  that list.

**Current Zengine.** `zengine-input` is the sole producer of input shapes and the only code that
talks to the platform. Its law is narrower on purpose: *Input reports coherent MOMENTS;
applications interpret GESTURES* (`docs/reference/input.md`). `KeyPressed`/`KeyReleased` carry
the modifiers held at the transition; `TextEntered` carries what the platform's own layout
produced; pointer shapes carry position, delta, space and modifiers. **There is no polling API,
no trigger taxonomy, no layer stack, no consumption protocol and no rebinding capture** — a tap,
a double-tap, a hold and a drag are all application meaning. Consumption exists, but as an
*application's* rule: Workshop's press chain returns a bool that means CONSUMED, with the
routing order written down in one place (`agents/workshop.md`, `QR-2`).

So the old design and the new one disagree about **where interpretation lives**, not about which
facts matter. Both preserve modifiers-at-the-transition; both distinguish the character typed
from the key pressed; both treat consumption as real. What the old engine put in the input
subsystem, Zengine puts in the consumer.

**Traps — and the largest fact about this file.**
- ⚠ **The entire routed half is dead code in this tree.** Nothing calls `Input::step`,
  `route_events`, `on_key`, `on_mouse`, `create_layer`, `set_global_filter` or
  `listen_for_key_combo`. The one mention outside `input.cpp` is a commented-out
  `// Input::on_mouse()` in `build_state.cpp:160`. The game loop calls only `update_input()` and
  `clean_input()`, so `_has_time` is never set, no `InputEvent` is ever produced, and every
  consumer polls. Read the routed API as a **design statement**, not as tried behaviour.
- `get_key_pressed_duration(KeyCombo)` locks `_mutex` and then calls `is_key_down(combo)`, which
  locks it again. `std::mutex` is not recursive — self-deadlock. Same shape in
  `get_key_released_duration`.
- `create_layer` sorts `_layers` by priority and *then* returns `_layers.back().id` — the id of
  whichever layer sorted last, not the one just created.
- `_combo_state()` returns a reference into a `std::vector` that later `emplace_back`s into;
  `route_events` re-sorts `_layers`, invalidating any `InputLayer*`.
- `HELD` is emitted **at release**, carrying the duration — not while the key is down. A
  consumer wanting "is held now" must poll.
- `destroy_layer` does not invalidate outstanding `InputConnection`s.

---

### 1.6 UI: the layout tree

**Source** `src/zengine/ui/custom_layout.{h,cpp}` (the largest engine file) ·
`src/zengine/graphics/enums.h`
**Tags** `layout` `box model` `padding` `margin` `sizing` `positioning` `hit test`
`invalidation` `dirty flags` `parent child`
**Classification** `UX EXPECTATION` · `INTERFACE QUARRY` · `ANTI-PRECEDENT`

**What existed.** A retained, mutable, parent/child tree of `CustomLayout` nodes with a CSS-like
box model. Everything visible in the old UI was a `CustomLayout` or derived from one.

**Interaction — the authored vocabulary** (`graphics/enums.h`):

```text
Layout      CHILD_CONTROLLED | HORIZONTAL | VERTICAL
SizeTo      STATIC | PARENT | PARENT_PERCENT | CHILDREN | CHILDREN_PERCENT | FILL
PositionTo  ABSOLUTE | RELATIVE | LEFT | RIGHT | TOP | BOTTOM | CENTER | PARENT_CONTROLLED
PaddingTo   STATIC | PERCENT
```

Each axis carries a *mode plus an amount*: `set_width(SizeTo::PARENT, 20)` means "the parent's
inside width minus 20"; `PARENT_PERCENT, 0.5` means half of it; `CHILDREN, 8` means "as big as
my children plus 8". `FILL` splits the leftover among siblings that also asked to fill. Position
works the same way, and `PARENT_CONTROLLED` hands the axis to a `HORIZONTAL`/`VERTICAL` parent
that lays children end to end with `set_child_spacing(n)` between them.

Three rectangles per node, and the distinction is the useful part:

```text
get_owned_destination()       everything the node claims, margins included
get_background_destination()  owned minus margins -- what the background paints
get_inside_destination()      background minus padding -- where children live
```

Padding and margin each have per-side mode+amount and a ladder of convenience setters
(`set_padding`, `set_padding_sides`, `set_padding_vertical`, `set_padding_top`, …, 11 overloads
per property). Also: `set_name`/`get_name`, `enable`/`disable`, `set_visible`, `add_child(child)`
and `add_child(child, position)`, `remove_child`, `remove_all_children`, `set_parent`,
`get_children`, `set_background_color`, `set_on_click_callback(std::function<void()>)`,
`click_location(Vector2)`, `find_component(pos, only_callback_registered)`, `reset()`, and a
`Signal on_size_changed`.

**Current Zengine.** The `ui` package answers a deliberately smaller question, and the smallness
is the design: it owns **exactly one distinction — authored versus resolved geometry** —
enforced by a compile-time fence (`docs/reference/ui.md`). An `Element` has an id, a label, a
*context* (whose frame its numbers are read in), an authored `x`/`y` and two authored `Extent`s.
`resolve()` takes a `Viewport`, orders its work by dependency without recursion, emits in
document order, and produces a `Placed` value **cached nowhere** — a resolved rectangle cannot
be stored beside authored intent, and `ui_resolved_geometry_refused` proves the fence fires.

The package's own "what it is not" is the sharpest available comparison to this entry:

> no widget kinds, no stacks, no relational arrangement, and **still no parent/child** … it says
> nothing about containment, ownership, clipping, painting or lifetime.

`CustomLayout` is the union of all of those in one class.

**What the old architecture made easy, stated without recommending the mechanism.** These are
the affordances a reader should weigh; how Zen should express them is an open question, not a
conclusion:
- naming a size **relative to a parent, to children, or to leftover room**, per axis, without
  writing arithmetic;
- a padding/margin box, per side, with a percentage option;
- laying children out in a row or a column by setting one property on the parent;
- centring on either axis with `center()`;
- one call to hit-test a point down to *the deepest node that has a click handler*.

**Traps — and the central architectural finding.**
- ⚠ **Every dirty-flag write is commented out.** `_width_current`, `_height_current`,
  `_x_current`, `_y_current` and the child-position caches exist, are *cleared* everywhere, and
  are **never set**: search for `// _width_current = true;` and its four siblings. The
  invalidation machinery was built and then disabled, so `get_width()`/`get_height()`/`get_x()`/
  `get_y()` recompute the whole dependent subtree on **every call** — and callers call them
  freely (`get_owned_destination` alone calls `get_position()` twice plus both extents). This is
  the single most informative fact in the UI stratum: a pull-based, mutually-recursive layout
  with parent→child and child→parent dependencies was hard enough to invalidate correctly that
  correctness was bought by giving up caching entirely.
- `_child_size_changed()` walks to the parent, which walks to *its* parent, unconditionally.
  `_on_size_changed()` walks down to every child. There is no convergence test — a `TODO` in the
  source says so.
- `get_x()`/`get_y()` for `PARENT_CONTROLLED` call `_parent->_request_child_position_update()`,
  which calls `get_position()` on the parent — a cycle waiting for a shape that closes it.
- `CustomLayout`'s destructor deletes all children *and* removes itself from its parent; children
  are raw pointers with no ownership statement anywhere, and `remove_child` deliberately does not
  delete (a `TODO` proposes an engine-wide option for it).
- `find_component` returns `this` when no child matches, so `click_location` compares the result
  against `this` to decide whether anything was hit.
- `_max_width`/`_min_width` are size *constraints* on `CustomLayout` — and `BaseText::_max_width`
  is a *text wrap width*. Two meanings, one name, in one hierarchy; `Text::set_wrap` writes the
  wrong one ([§1.8](#18-text-and-the-legacy-textbox)).
- `SizeTo::FILL` divides leftover room by the number of filling siblings but adds
  `_child_spacing` only once, before the loop, regardless of sibling count. `get_width`'s FILL
  branch checks the parent's orientation and falls back to the parent's inside width; **`get_height`'s
  does not** — it subtracts sibling heights whatever the parent's layout is.
- `get_height()`'s minimum clamp reads `_min_height + get_margin_left() + get_margin_right() +
  get_padding_left() + get_padding_right()` — the **horizontal** insets, in the height equation.
- `get_width()` returns early for `SizeTo::STATIC` and `get_height()` does not, so a STATIC height
  is subject to the min/max clamps and a STATIC width is not. Two axes, two behaviours.
- `get_padding_*` throws and immediately catches a `std::runtime_error` for an enum case that
  cannot occur — exceptions as control flow, logged and swallowed, returning `0`. Left/right
  percent resolves against the **parent's width** and top/bottom against its height; only
  top/bottom special-case the root, so a root with percent left/right padding dereferences a null
  `_parent`.

---

### 1.7 UI: the widget set

**Source** `src/zengine/ui/button.{h,cpp}` · `src/zengine/ui/drop_down.{h,cpp}` ·
`src/zengine/ui/scroll_view.{h,cpp}` · `src/zengine/ui/engine_config.json`
**Tags** `widgets` `button` `dropdown` `scroll` `hover` `popup` `z order` `clipping`
**Classification** `UX EXPECTATION` · `INTERFACE QUARRY` · `ANTI-PRECEDENT`

**What existed.** Three widgets, all `CustomLayout` subclasses, all built by composing child
layouts rather than by drawing themselves.

**`Button`** — a bordered box (outer layout painted the border colour, inner layout inset by a
margin) around a centred `Text`. API: `set_text`, `get_text`, `set_background_color`,
`set_hovered_bg_color`, `set_auto_hover_color`, `set_border_color`, `set_border(size)`,
`set_text_position(PositionTo, PositionTo)`. Hover is computed in `update()` by testing the
mouse against its own rectangle; with `_auto_hover_color` the hover tint is derived
automatically as "halfway to white".

**`DropDown`** — a display `Button` plus an *ephemeral* option panel, rebuilt from
`set_options(vector<string>)` each time it opens. `get/set_selected_index`, `get_selected_text`,
`Signal on_selection_changed`. Options are `Button`s in a `ScrollView`, with a thin separator
layout between each pair.

**`ScrollView`** — auto-creates an inner `_child_container` on first `add_child`, scrolls by
setting that container's `RELATIVE` y to an offset, clamps the offset to the content overflow,
and clips by calling `game_graphics.set_clipping(background_destination)` around its base draw.
`scroll_to_percent(double)`. Wheel and arrow keys both scroll.

**Current Zengine.** There is no widget set and that is a written standing decision. The
`component` package holds exactly one component, `TextBox`, extracted the day two working tools
genuinely needed the same caret-window-pointer behaviour, and the rule is stated as *extract from
repeated working behaviour, never from a list of widgets* (`docs/reference/component.md`).
`component::Button` was specifically considered and **declined**: "What Create and Delete share
is a label, a bit, a bracket convention and a row — presentation with no invariant to keep"
(`agents/workshop.md`, HD-8). Pressable things in Workshop are drawn in characters —
`[ Create ]` pressable, `( Delete )` not, the same width either way.

Scrolling: Workshop's lists derive their window every paint from `list_window` and store nothing
— "there is no scroll offset, no session field and no scroll gesture" — and every omission is
counted and said out loud.

**Traps.**
- ⚠ `DropDown::_toggle_panel()` on close does `delete _option_panel;` **without** clearing the
  member, removing the overlay from the `GameState`, or removing the mouse callback that
  captures it. The next press dereferences freed memory. The source says so: `// TODO FINISH
  UPGRADING DROPDOWN CURRENTLY USUSABLE`.
- `_hide_panel()` also deletes, so an open→select→close path can double-delete.
- **The popup had to escape the layout tree.** A `DropDown`'s option panel cannot be a child —
  it must paint outside its parent and take input before everything else — so it is registered
  with the `GameState` as an *overlay plus a consuming mouse callback*. This is the tree's
  clearest structural limit: the layout model had no z-order and no floating-content concept,
  so anything floating had to be handed to the application. Current Zengine's answer is a plane
  sequence with the whole depth story written down, and `presentation_order`/`occupied_at` as
  exact inverses (`agents/workshop.md`, WIND-2a).
- Hover has no notion of *topmost*: two overlapping buttons both light up, because each tests the
  pointer against itself independently in `update()`.
- `Button::draw` mutates its own background colour, draws, and restores it — a paint-time state
  change.
- `ScrollView::_manage_input` binds W/S/Up/Down to scrolling whenever the pointer is over it,
  with no focus check — this fights any focused text field. It also calls
  `Input::clear_mouse_wheel()` globally, but only on the path where the pointer *is* over it.
- `_get_children_total_size` sums both axes for every child regardless of orientation.
- `engine_config.json` names a `checkbox` section; no checkbox exists. Read the file as a
  statement of *intended* configurable surface, not of shipped widgets.

---

### 1.8 Text and the legacy `TextBox`

**Source** `src/zengine/ui/text.{h,cpp}` · `src/zengine/ui/text_box.{h,cpp}` ·
`src/zengine/graphics/enums.h` (`TextBoxFilterType`, `DataType`)
**Tags** `text` `text box` `text selection` `caret` `editing` `focus` `filters` `validation`
`signals` `blink`
**Classification** `UX EXPECTATION` · `INTERFACE QUARRY` · `SUPERSEDED`

**What existed.** `Text` is a `CustomLayout` wrapping a `BaseText` child that measures itself
through `TextureManager::get_text_size` and sets its own STATIC size from the result — so a text
node's size is a *measurement*, and layout flows from it. `Text` exposes `set_text`, `set_font`,
`set_font_size`, `set_font_color`, `set_wrap`, and a `Text::Config{font_color, font_size,
font_name}` bundle.

`TextBox` is a `CustomLayout` composing a border layout, a white inner layout with a click
callback that focuses it, and a `Text` child.

**Interaction.**

```cpp
TextBox tb;
tb.set_text("initial");            std::string s = tb.get_text();
tb.set_filter(TextBoxFilterType::DATA_TYPE, DataType::NUMBER);
tb.set_focused(true);              // also entered by clicking the inner layout
tb.on_text_changed;                // Signal, fired on every accepted character
tb.on_text_committed;              // Signal, fired when focus is lost
```

Focus starts an `Input::start_text_input` session; each delivered chunk runs through
`_process_text`, which appends filtered characters or pops the last byte for `"\b"`. The caret is
a literal `"|"` **appended to the rendered string**, toggled by a 500 ms `Timer`.

**What was reachable, and what was not** — this is the entry's real value:

| reachable | not reachable |
|---|---|
| typing | moving the caret at all (no arrows, Home, End, click-to-place) |
| backspace at the end | deleting anywhere but the end; forward delete |
| a blinking caret marker | selection of any kind |
| per-field input filters | clipboard, undo, multiline |
| change-vs-commit as two signals | a value longer than the field (no horizontal window) |
| focus by click; blur commits | max length, placeholder text (both `TODO` in source) |

**Current Zengine.** `component::TextBox` — and `docs/reference/component.md` states the
relationship itself, which is the authoritative comparison:

> The pre-Zen `Zen::TextBox` (`reference/`, archaeology only) is not its ancestor in anything but
> the name: it carried a filter, a focus flag, a blink timer, two signals and a child `Text`
> entity, and it could not move its caret, could not scroll, and erased one **byte** at a time.

Today's component holds the editing state as one value with UTF-8 character boundaries, a caret
and a visible window — the capacity being an argument and never a member, because the Terminal's
row and an Inspector row are different widths in the same running process. It owns **no** policy
and no medium: no commit, no validation, no refusal, no focus, no blink.

⚠ **This surface was extended by the TEXT-0 phase in the same hours this catalog was written**, and
the extension lands exactly in the region the legacy `TextBox` could not reach — selection, the
clipboard operations, a local undo, word moves, and one owner for the editing-key vocabulary. Do
not read an operation list out of this entry: `docs/reference/component.md` is the owner and states
both the current surface and the rule under which each part of it was earned.

So the legacy `TextBox` is best read as evidence for **which pressures were felt**, not for how
they should be answered: a per-field filter, a distinction between "changed" and "committed", and
focus-by-pointer were all wanted early. Where they belong is a separate question — today the
filter/parse/refusal half lives in Workshop's property row, not in the component.

**Traps.**
- The caret is inside the string. `get_text()` is guarded, but anything reading `Text` directly
  while focused sees the `|`.
- `TextBoxFilterType::EMAIL` and `PLUGIN` are declared and have **no branch** in `_process_text`
  — selecting either makes the field silently drop every character typed. `DataType::BOOLEAN`
  likewise never reached.
- Escape and Return are the same gesture: both end the mode, both fire `on_text_committed`.
  There is no cancel.
- `Text::set_text/set_font/set_font_size/set_font_color` are declared `const` and mutate through
  `_base_text`.
- `Text::set_wrap(false)` assigns `_max_width = 0` — which resolves to `CustomLayout::_max_width`
  (a size constraint) rather than `BaseText::_max_width` (the wrap width). See the name-collision
  trap in [§1.6](#16-ui-the-layout-tree).
- `Text::_create_base_text` hard-codes `"Basic-Regular.ttf"`; the config-driven font size is
  commented out beside it.

---

### 1.9 The visual UI builder

**Source** `src/apps/builder/build_state.{h,cpp}` (983 lines — the largest file in the tree) ·
`src/apps/builder/main.cpp`
**Tags** `builder` `inspector` `property editor` `component tree` `selection` `highlight`
`serialization` `config generation` `panes` `schema`
**Classification** `REQUIREMENT` · `UX EXPECTATION` · `INTERFACE QUARRY` · `CONCEPTUAL PRECEDENT` · `PARTIAL`

**What existed.** A self-hosted visual layout editor — a `GameState` whose UI is built from the
same widgets it edits. This is the richest entry in the quarry and the closest legacy ancestor of
Workshop's shape, and it should be the first file a future quarrying pass opens.

**The screen.** A horizontal root splits into a 20 %-wide left column and a build canvas:

```text
left column (vertical)          build canvas
  _create_pane   ScrollView       _build_root   the layout being edited
  _tree_pane     ScrollView       _highlight    a translucent rect over the hovered node
  _details_pane  ScrollView
```

The three left panes are **mutually exclusive by mode**: `enter_creation_mode()` shows create +
tree and hides details; `enter_selection_mode(node)` shows details and hides the other two.

**Interaction.**
- *Create pane* — one button per component kind (`Create Layout`, `Create Button`, `Create Text`,
  `Create TextBox`, `Create ScrollView`), each adding a new child under the current selection and
  then selecting it; plus `Delete Selected`, `Save JSON`, `Load JSON`.
- *Tree pane* — `create_component_tree(node)` recursively renders one clickable `Text` per node,
  indented 16 px per level via a nested container's left margin. Clicking a name selects it.
- *Details pane* — a property inspector built from two row factories:

```cpp
create_details_row(label, TextBox::TextBoxFilter filter, value,
                   Action<const std::string&> on_commit);   // label 40% | filtered TextBox 60%
create_dropdown_row(label, vector<string> options, selected_index,
                   Action<int> on_change);                  // label 40% | DropDown 60%
```

  `create_basic_details` emits ~28 rows for any node: Name, Width Type + Value, Height Type +
  Value, Position X/Y Type + Value, Layout, Child Spacing, four Padding pairs, four Margin pairs,
  and BG Red/Green/Blue/Alpha. Type-specific rows are then appended by `dynamic_cast`:
  Button → Text; Text → Text/Font/Font Size/Wrap/colour; TextBox → Text/Filter Type/Data Type;
  ScrollView → nothing yet.
- *Hover highlight* — `update_highlight()` finds the deepest node under the pointer and moves a
  root-positioned translucent cyan rectangle onto it.
- *Persistence* — `serialize_component` writes every authored property plus a `type` string and a
  `children` array; `deserialize_component` reconstructs by type and recurses.

**Current Zengine.** Workshop's Info panel is the same idea arrived at from a different
direction, and every one of the legacy builder's rough edges has an explicit counterpart:

| the builder did | Workshop does |
|---|---|
| three panes shown/hidden by mode | one body, `info_body_place`, resolved once and asked by the painter, the caret, `refresh_inspector`, both windows and both press handlers |
| a full rebuild of the details pane on every commit (`refresh_details()`) | rows derived per repaint with a live draft explicitly carried across a `SurfaceExtent` (`refocus_keeping_draft`) |
| panes sized by percentage, lists unbounded | `share_body_rows` — max-min fair sharing over one row budget, with every omission counted on its own side |
| a hand-written string list per enum | `zen.DescribeAccepted` → `zen.AcceptedShapes`: the Composer builds a form **from the runtime schema**, and every refusal is the type ladder's own sentence naming the field and its declared kind (`agents/panes.md`, MSG-0) |
| a commit callback per row, wired by hand | a row parses, writes and may be refused **with a reason**; presence and value are two members (`FieldDraft{present, TextBox}`) so `Text present with ""` and `Text absent` are different messages |
| create/delete as buttons that always work | availability is *two reasons, one bit, two owners* — the application refuses a live draft, the document speaks for a missing target, and a control never invents a reason |
| `layout.json` written to the process CWD | three named files with stated promises: what a maker MADE, a desk they NAMED, the desk they were USING (`agents/workshop.md`, WUX-0) |

**Traps — including one that is the strongest single piece of evidence in the quarry.**

- ⚠ **The enum dropdowns are wired to the wrong values, and the cause is the hand-maintained
  parallel list.** `create_dropdown_row` hands back an *index*, and the callback does
  `static_cast<PositionTo>(idx)`. But `PositionTo` is
  `{ABSOLUTE, RELATIVE, LEFT, RIGHT, TOP, BOTTOM, CENTER, PARENT_CONTROLLED}` while the "Position
  X" option list is `{"ABSOLUTE","RELATIVE","LEFT","RIGHT","CENTER","PARENT_CONTROLLED"}` — six
  entries against eight. Choosing `CENTER` (index 4) sets `TOP`; choosing `PARENT_CONTROLLED`
  (index 5) sets `BOTTOM`. The "Position Y" list is wrong in the same way from index 2 onward.
  The source carries the diagnosis two lines above the bug:
  `// TODO VERY IMPORATNT generate this automatically, so we can add values as desired`.
  This is a concrete, reproducible defect caused by a form that knows the *labels* but not the
  *schema* — and it is the clearest legacy support for deriving a form from a runtime type
  description rather than from a hand-written list.
- ⚠ **Type identity by substring.** `serialize_component` writes
  `j["type"] = Utility::demangle(typeid(*component).name())`, and `deserialize_component`
  dispatches with `type.find("Button") != npos`, then `find("Text")`, then `find("TextBox")`.
  `"TextBox"` contains `"Text"` and the `Text` branch is tested first, so a serialised `TextBox`
  is reconstructed as a `Text` and then reads `j["font"]`, which a `TextBox` never wrote —
  `nlohmann` throws. The round trip is broken for one of the five kinds.
- ⚠ **`Utility::demangle` is a stub** — `return mangled_name;` with a `TODO FIXME CROSS PLATFORM`.
  So the `type` string is whatever the compiler's `typeid().name()` yields, which differs between
  MSVC and the Itanium ABI. The substring matching above exists *because* demangling was never
  implemented; the two traps are one causal chain, and the whole persisted format's identity
  depends on a compiler detail.
- ⚠ **Load is a no-op.** `load_from_json` opens and parses the file and then never calls
  `deserialize_component` — the call is commented out with `// TODO do not ovewrite build root`.
  It logs `"Loaded from %s"` regardless. A green that means nothing.
- Drag and drop is declared (`_dragging`, `_dragged_component`, `_drag_start_pos`) and entirely
  unimplemented; left-click selection is commented out, so selection is reachable only through
  the tree pane. `todo.txt` lists "Drag to resize elements, or move them" as wanted.
- `_highlight` is `new`ed once and never deleted unless it has no parent at destruction.
- Save writes to a bare relative `"layout.json"`.
- `BuildState::get_component` is a third copy of the hit-test walk already present in
  `CustomLayout::find_component` and `CookieClickerState::get_component`.

---

### 1.10 Configuration and generated config APIs

**Source** `src/zengine/config_code_gen.cpp` · `src/zengine/config_manager.{h,cpp}` ·
`Resources/config/default.json` · `Resources/config/current.json` ·
`Resources/generated/config.{h,cpp}`
**Tags** `config generation` `codegen` `schema` `json` `hot reload` `defaults` `healing`
**Classification** `REQUIREMENT` · `INTERFACE QUARRY` · `ALGORITHM QUARRY` · `CONCEPTUAL PRECEDENT` · `ABSENT`

**What existed.** The cleanest, most finished code in the whole tree, and the one place where the
old engine's ambition and its execution match. Two halves:

**The runtime half** — `ConfigManager`, a singleton that loads `config/current.json`, copying
`config/default.json` into place if it is missing, then **structurally heals** the live file
against the default: a missing key is filled from the default, a leaf whose JSON *type* differs
from the default's is replaced by the default. The healed tree is flattened into a
`unordered_map<string, json>` keyed by dotted path, and `get<T>("ui.button.default_text")`
reads from that. `has(path)`; `reload()`; `reload_if_changed()`.

**The generation half** — `config_code_gen.cpp`, a standalone program that turns the JSON schema
into a typed C++ API:

```text
--in <file.json>          default: <resources>/config/default.json
--out-dir <dir>           default: <resources>/generated
--root-ns <A::B>          default: Zen::Config
--style get|raw           get_<key>()  |  <key>()
--sort-keys on|off        deterministic alpha  |  preserve JSON order (default)
--fail-on-unsupported     otherwise unsupported leaves are warned and skipped
--dry-run                 print both files instead of writing
--help
```

Object keys become **nested namespaces**; leaf keys become **free functions in the parent
namespace**; leaf types are inferred from the JSON value (`bool` / `int64_t` / `double` /
`std::string`). Keys are mangled to valid C++ identifiers by a real `to_snake` that strips
punctuation, splits camelCase, guards against a leading digit, and appends `_` to any of the 90+
C++ keywords in its own table — warning on stderr whenever a key had to be mangled. Both files
are composed **in memory first**, so a failure writes nothing partial.

The output shape:

```cpp
// Resources/generated/config.h
namespace Zen::Config {
  namespace ui {
    double get_font_size();
    namespace button { std::string get_default_text(); }
  }
}
// Resources/generated/config.cpp
double Zen::Config::ui::get_font_size() {
  return Zen::ConfigManager::instance().get<double>("ui.font_size"); }
```

So a **typo in a config name is a compile error**, while the value itself stays a runtime lookup
that survives a reload. That combination is the point of the design.

**Current Zengine.** No config generator and no `ConfigManager` — and the absence is not an
oversight so much as a different shape of problem. What Zengine has instead:
- **authored JSON files with declared formats and refusals**: the Workshop setup format is
  versioned, its version is the envelope's shape version by `static_assert`, its modes are
  *words* in the file and a closed set, absent intent has exactly one canonical spelling, and an
  unrecognised word refuses the whole candidate naming both what it found and what would have
  worked (`agents/workshop.md`, WIND-2);
- **schema-driven forms at runtime** rather than schema-driven code at build time: the Composer
  reads `zen.AcceptedShapes` and builds a form whose refusals are the type ladder's own
  (`agents/panes.md`);
- **build recipes and load plans as authored files** (`workshop/default-build-recipes.json.in`,
  `workshop/default-load-plan.json`, `docs/reference/load-plan.md`).

The old generator answers "how does a *programmer* name a config value safely"; the current
machinery answers "how does a *maker* author what a run is made of, and what happens when the
file is wrong". Related problems, different questions.

**Traps.**
- `reload_if_changed()` **is** `reload()` — the change detection is commented out with an honest
  diagnosis: file size does not change when one digit replaces another. So the game loop
  re-reads, re-parses, re-heals and re-indexes the entire config file **every frame**.
- Generated integer leaves are `int64_t`; `ConfigManager::get`'s explicit instantiation list has
  no `int64_t`.
- The generated `.cpp` hard-codes `#include "engine/config_manager.h"` — a path that has not
  been correct since the source moved.
- `Resources/generated/` is checked in *and* regenerated into the build directory; the two can
  disagree, and the checked-in copy is the one the library compiles.
- `main` uses `std::ostringstream` without including `<sstream>`.

---

### 1.11 Signals, callbacks and the message bus

**Source** `src/zengine/callback.h` · `src/zengine/zsignal.h` ·
`src/zengine/message_bus/{message.h,data_packet.h,message_bus.h,message_listener.h,socket.h}`
**Tags** `signals` `callbacks` `connectors` `events` `message bus` `broadcast` `serialization`
`rttr`
**Classification** `CONCEPTUAL PRECEDENT` · `INTERFACE QUARRY` · `SUPERSEDED`

**What existed.** Three separate mechanisms for "something happened, tell someone", at three
levels of coupling.

**1. `Callback<Ret, Args...>` / `Action<Args...>`** (`callback.h`) — a thin `std::function`
wrapper with a void specialisation and a C#-inspired `Action<>` alias, so a void callback needs
no `void` in its spelling. Used everywhere: click handlers, text-input handlers, property-commit
handlers.

**2. `Signal`** (`zsignal.h`) — the closest thing to a "connector" in the tree:

```cpp
Signal on_text_changed;
sig.connect(void* key, Action<>&& callback);   // key is the subscriber's identity
sig.disconnect(void* key);                     // removes every callback under that key
sig.emit();  sig();                            // fire all, skipping null keys
```

Live uses: `CustomLayout::on_size_changed`, `TextBox::on_text_changed` /
`on_text_committed`, `DropDown::on_selection_changed`. Note the shape: **subscription is keyed by
an opaque owner pointer**, so a subscriber unsubscribes all of its callbacks at once — a
lifetime-management stance, arrived at without a token type. Signals carry **no arguments**; a
handler closes over what it needs and reads the emitter directly.

**3. `MessageBus`** — a static broadcast bus. `MessageListener`'s constructor registers a queue;
subclasses call `poll_messages()` in their own update. `MessageBus::broadcast(Message)` pushes a
`shared_ptr` into every registered queue and additionally calls any `message_hook(name, fn)`
whose name matches. A `Message` is `{const std::string name, const DataPacket data}`.

**`DataPacket`** is the payload: a vector of `{name, rttr::variant}` with
`attach_object(name, value)` / `get_object(name)`, plus `to_json()` / `from_json()` that encode
each entry under a `"<name>:<type_name>"` key and reflect over class properties via RTTR.

`socket.h` contains an empty `class Socket {};`. Networking never began.

**Current Zengine.** The Loom owns messaging — offices, roles, shapes, admission, tickets,
answers, provenance — and Zengine consumes it as a stranger. The comparison worth carrying:

| legacy | current |
|---|---|
| `broadcast` to every listener queue | addressed sends, with a Ticket answering whether the *authorship* was permitted — and Zengine's Composer deliberately says `SUBMITTED` and nothing stronger, because delivery is one of five things that must go right |
| `Message{name, DataPacket}`, name-matched | `ZEN_SHAPE` types with versions that are part of identity at the admission gate |
| RTTR variants + reflection for payload | declared fields, `zen.DescribeAccepted` / `zen.AcceptedShapes` for discovery, and a type ladder that refuses `1O00` in an Int field without Zengine knowing what an Int is |
| `Signal` keyed by `void*` | no general signal type; a weave publishes shapes, and `SurfaceReady` is the pattern for "republish your state when a new listener says hello" |

**Traps.**
- ⚠ `MessageBus::get_singleton()` returns `MessageBus` **by value**. `message_hook` therefore
  registers into a temporary copy and the hook is discarded; `broadcast` iterates another
  temporary copy's (empty) hook list. The hook half of the bus cannot work as written.
- `MessageBus::_singleton` is declared and never defined; `message_listeners` is defined
  non-inline **in a header**.
- `MessageListener` registers a heap queue in its constructor and never unregisters — every
  destroyed listener leaves a dangling pointer in a static vector that `broadcast` walks.
- `Callback`'s primary template returns `Ret{}` when unset, so an unset callback silently
  produces a default value rather than refusing.
- `Signal::emit` skips null keys with a `// TODO add error logging`; there is no protection
  against a subscriber destroyed without disconnecting.
- `DataPacket::from_json` has its container branch commented out and a "Example; adapt per type"
  fallback for numbers.

---

### 1.12 `VarStorage` — a value that may be a live projection

**Source** `src/zengine/message_bus/var_storage.h` · `src/apps/examples/var_storage_examples.cpp`
**Tags** `var storage` `projection` `binding` `live value` `senses`
**Classification** `CONCEPTUAL PRECEDENT` · `INTERFACE QUARRY` · `ALGORITHM QUARRY`

**What existed.** A small template with an unusually clear intent, documented by a 30-line
comment in the header that is effectively a mini design note. `VarStorage<T>` holds *either* a
constant *or* a way to produce `T` on demand:

```cpp
VarStorage<double> v;
v = 10.0;                                   // 1. a constant
v = ptr_to_double;                           // 2. a pointer, read each time
v = &get_double;                             // 3. a function returning T
v.set(&some_string, [](const std::string& s){ return std::stod(s); });
                                             // 4. an object + a conversion, read each time
double now = v.get_value();                  // or: double(v)
```

The stated purpose: "to be a concept of a pointer, while allowing diverse modularity … by keeping
as much code and specification off of the user." Cases 2–4 mean the *consumer* never learns
whether it is reading a stored value or re-deriving one. The example file even benchmarks the
indirection against calling the conversion directly.

Live use in the engine is one field — `BaseText::_font_size` — with the config-driven binding
(`_font_size.setf(&Zen::Config::ui::font_size)`) present but commented out. That commented line
is the intended shape: **a widget property bound to a generated config accessor, re-read on every
use, so a config reload changes the UI with no propagation code.**

**Current Zengine.** No `VarStorage`, and the underlying question is answered differently in two
places. The `ui` package's fence forbids the *opposite* thing — an authored `Extent` has no field
able to hold a resolved rectangle, so intent cannot be quietly replaced by a cached number
(`docs/reference/ui.md`). And Workshop's introspection panes "derive at every ask and keep
nothing", which is `VarStorage`'s case-3 behaviour raised to a whole pane
(`agents/panes.md`, INTR-1).

The vocabulary comparison worth noting: `vision.md`'s "Senses" — *zero-copy reads of named values
exposed by other Shards*, epoch-protected — and stratum B's `Kernel::provide(path, provider)` are
`VarStorage` scaled up to a process. Same instinct, three sizes.

**Traps.**
- `operator=(convertable_to_T* object)` type-tests with `T(*object)` and then captures a lambda
  returning `T(object)` — constructing from the *pointer*, not the pointee. Case 2 is wrong as
  written.
- `VarStorage` never owns what it points at; the header says so in capitals.
- The default constructor assigns `T()`, so `T` must be default-constructible.
- `get_value()` is non-const.

---

### 1.13 Timing

**Source** `src/zengine/timer.{h,cpp}` · `src/apps/examples/timer_examples.cpp`
**Tags** `timers` `timing` `pause` `time scale` `frame`
**Classification** `REQUIREMENT` · `INTERFACE QUARRY` · `SUPERSEDED`

**What existed.** One static clock read per frame plus per-instance countdown timers.

```cpp
Timer::update_time();          // once per frame, in GameStateManager::update
Timer t(500);                  // a 500-unit delay
if (t.is_time()) { ... }       // true once per elapsed delay; SUBTRACTS the delay, does not reset
t.peek_is_time();              // ask without consuming
t.peek_progress_percentage();  // 0..1 toward the next firing
t.set_time_multiplier(2.0);    // per-timer time scale
t.pause(); t.resume(); t.reset();
```

Two properties are worth naming. `is_time()` **subtracts** rather than zeroing, so a timer that
misses frames still fires the right number of times and does not drift. And every timer has its
own multiplier and pause state, so slow-motion is per-subsystem rather than global. Live uses:
`TextBox`'s 500 ms caret blink, `BuildState`'s 16 ms UI throttle, `CookieClickerState`'s UI
throttle.

**Current Zengine.** A Timer *service* — a loadable weave holding a role, with laws
(`docs/laws/timer-laws.md`, TIMER-01..05), a wire protocol, receipts, a `TimerReady` rule,
continuity across the service's own replacement carried as remaining **durations** rather than
deadlines, and role-addressed beats that a swapped-in successor inherits without asking
(`docs/reference/timer-*.md`). Packages ask for their own pace: `zengine.input.pump` and
`zengine.skin.pump` are both 10 ms and both owned by their package. The authored delay is
normalised by a named rule resolved through the *host's* catalog (`agents/operators.md`).

**Traps.**
- ⚠ `_accumulate_time()` — the only function that advances `_elapsed_time` — is **private and
  never called**. As the code stands, `_elapsed_time` never grows, so `is_time()` never returns
  true and no timer in the tree ever fires. Every `Timer` use in the engine is therefore
  untested-in-place.
- `_last_real` is declared static and never defined ([§0.4](#04-the-tree-does-not-build-as-it-stands)).
- `_last_real` is *static* but assigned per-instance in the constructor, so constructing any
  timer moves every other timer's baseline.
- The delay unit is undocumented: `_accumulate_time` computes milliseconds, `timer_examples.cpp`
  constructs `Timer(1000)` and calls it a second, `message_bus_example.cpp` constructs
  `Timer(.5)`.
- `_automatic_updates` and `_start_time` are defined and never read.
- `Timer::get_current_time()` is called by three example/demo files and **does not exist**.

---

### 1.14 Math, geometry and value types

**Source** `src/zengine/types/{vector2,rectangle,color,line_2d,shape,plane_bounded}.{h,cpp}` ·
`src/zengine/types/function.h` · `src/zengine/logic/math.{h,cpp}`
**Tags** `math` `geometry` `vector` `rectangle` `color` `noise` `lookup table` `hit test`
**Classification** `INTERFACE QUARRY` · `ALGORITHM QUARRY` · `LOW QUARRY VALUE` (mostly)

**What existed.**

`Vector2` / `Vector3` — chainable **mutate-in-place** value types. Every operation returns
`*this`, and `copy()` is how a caller opts out. `vector2_examples.cpp` documents this deliberately
("Use copy in order to preserve data"), including that `b.invert()` changes `b`. Also `set_x/y`,
`add_x/y`, `add`, `multiply` (component-wise), `scale`, `normalize`, `abs`, `negate`, `invert`
(reciprocal, **not** negation), `get_magnitude`, `get_x_int/get_y_int`, `operator<<`, and
non-mutating `+ - *` operators. `Vector3` adds `cross_product`, `dot_product` and `operator==`.

`Rectangle` — position + size as two `Vector2`s, with `contains(Vector2)`, `add`, `copy`,
`deep_copy`, and mutable accessors.

`Color` — plain RGBA `Uint8`.

`Line2D` — precomputes slope, intercept, and domain/range extents from two points; offers
`evaluate(x)`, `check_value_in_domain`, `is_undefined`, and static `shares_domain_and_range`,
`check_lines_parallel`, `get_shared_bounding_box`. A small, self-contained analytic-geometry
helper, and the least broken thing in the `types` directory.

`Math` — `PI`, `PIo2`, `PIo4`, `PIt2`; a 10,001-entry sine/cosine lookup table with
`GenerateTrigLookupTables()` / `FastSin` / `FastCos`; and `getRadiansPointsTo`.

`function.h` — an orphaned 1-D **value-noise generator**: `Noise` lazily extends a random walk
of `(x, y)` points with configurable scale, strength, offset, and minimum-spread/variance floors,
then linearly interpolates between the bracketing points. The generation strategy (extend on
demand, remember the walk, interpolate) is the reusable idea.

**Current Zengine.** No general math or geometry package, and none is needed at the current
scope: `surface/region.hpp` carries the saturating arithmetic the canvas needs, `ui/layout.hpp`
carries `Rect` for resolved geometry only, and colour does not exist as a public concept at all —
elements carry a **role**, and only a medium turns that into ink.

**Traps.**
- ⚠ `GenerateTrigLookupTables()` is **never called**, so `FastSin`/`FastCos` read a zero-filled
  static array and return `0`. Everything in [§1.15](#115-3d) that rotates depends on them.
- ⚠ `Vector3::operator-` computes `o - *this`, not `*this - o`. `Line3D`'s constructor uses
  `end - start`.
- `Rectangle::get_width_int()` and `get_height_int()` return the *position*'s components; all
  four `*_int()` accessors are declared `double`.
- `Rectangle::contains` is inclusive on both edges, so adjacent rectangles both claim a shared
  boundary point.
- `Vector2::normalize()` divides by the magnitude with no zero guard.
- `Shape::connect` never inserts points into `_points`, so its search can never match and the
  function does nothing.
- `PlaneBounded`'s coplanarity test is `dot(point, normal) != 0` — it should test the vector from
  a point *on* the plane, and it uses exact float equality.
- `Noise::get` reads `points[i + 1]` where `i` starts at `size() - 1`.
- `function.h` names `AnythingStorage<T>`, includes two paths that do not exist, and is compiled
  by nothing.

---

### 1.15 3D

**Source** `src/zengine/graphics_3d/{camera,engine_3d}.{h,cpp}` ·
`src/zengine/types/3d/{vector3,line_3d,triangle,sphere}.{h,cpp}`
**Tags** `3d` `camera` `projection` `icosahedron` `mesh`
**Classification** `ALGORITHM QUARRY` · `LOW QUARRY VALUE` · `ABSENT`

**What existed.** A perspective-projection sketch, not a renderer. `Camera` holds a position,
pitch/yaw/roll and an FOV (caching `tan(fov/2)`). `Engine3D::get_screen_coords(point, camera,
screen_size)` translates a world point into camera space, applies three axis rotations, and
divides by the half-width at that depth to yield a screen `Vector2`. `Sphere::generate_points`
carries the 12 golden-ratio vertices of an icosahedron and normalises them onto the sphere.
`Triangle`/`TriangleRef`, `Line3D` as point + direction.

There is **no 3D drawing anywhere** — no rasteriser, no depth buffer, no mesh submission. Nothing
in the engine calls `Engine3D`.

**Current Zengine.** Nothing 3-D, by design at the current scope. `SurfaceCanvas` is a cell grid
with a two-level painter's order and explicitly "no coordinate transform, no opacity, no clipping
tree, no numeric z".

**Traps.** Every rotation goes through `FastSin`/`FastCos`, whose tables are never generated
([§1.14](#114-math-geometry-and-value-types)) — the projection returns the same degenerate answer
for every orientation. `Sphere::generate_points` declares an empty local `triangles` vector,
subdivides *that* (so the loop body never runs), mutates the container it is iterating, clears
`points` inside the loop, and returns the emptied vector. Only the icosahedron vertex table is
worth reading. `Sphere::generate_sphere()` is empty.

---

### 1.16 Resource location and filesystem

**Source** `src/zengine/logic/utils.{h,cpp}`
**Tags** `filesystem` `resource loading` `paths` `demangle`
**Classification** `ALGORITHM QUARRY` · `CONCEPTUAL PRECEDENT` · `PARTIAL`

**What existed.** Three static helpers, one of which is a genuinely reusable idea:

```cpp
Utility::get_resources_path();               // cached; walks UP from the executable
Utility::getResourcePath("TTFs/font.ttf");   // that path, joined
Utility::demangle(typeid(x).name());
```

`get_resources_path` starts at `SDL_GetBasePath()` and walks up to 20 parent directories looking
for a child named `resources`, caching the first hit in a function-local `optional` and throwing
a message naming the search origin if it finds none. That is the right shape for "find the asset
root from an executable that may be in `build/debug/` or in an install prefix", and the failure
message is genuinely diagnostic.

**Current Zengine.** Assets are not discovered at runtime. The graphical skin's typeface is
compiled into the weave's own bytes (`cmake/EmbedBinary.cmake`) and opened from memory — "nothing
is installed, nothing is staged, nothing is discovered at runtime, and no host font is assumed"
(`docs/reference/surface.md`). Workshop's three JSON files are named on the command line with
documented defaults. So the *capability* is present in a stronger form for the one asset that
exists, and the general "find my resources" problem is not currently posed.

**Traps.** The search term is `"resources"` (lowercase) and the directory in the tree is
`Resources` — on a case-sensitive filesystem the walk never matches. `demangle` is a stub that
returns its argument unchanged, with `// TODO FIXME CROSS PLATFORM`; two features depend on it
([§1.9](#19-the-visual-ui-builder)).

---

### 1.17 Logging

**Source** `src/zengine/logger.{h,cpp}`
**Tags** `logging` `debug`
**Classification** `LOW QUARRY VALUE` · `PARTIAL`

**What existed.** `Logger::log(LogLevel::{INFO,WARNING,ERROR}, msg)` — mutex-guarded, timestamped,
written to `zen_log.txt` (opened at static-init time, append mode, flushed every call) and
mirrored to `stdout` or `stderr` by level.

**Current Zengine.** No logger. Diagnostics are `std::fprintf(stderr, ...)` prefixed with the
target's own name (`zengine-skin-sdl: ...`, `zengine-workshop: ...`), and refusals are *values
carrying a reason* rather than log lines — a refused setup candidate names what it found and what
would have worked; a refused property edit is a sentence the maker reads in the row.

**Traps.** A file stream opened during static initialisation, flushed on every call, from a
global. Level formatting is a nested ternary.

---

### 1.18 Serialization

**Source** `src/apps/builder/build_state.cpp` (`serialize_component` / `deserialize_component`,
and the `NLOHMANN_JSON_SERIALIZE_ENUM` block at the top) ·
`src/zengine/message_bus/data_packet.h` (`to_json` / `from_json`)
**Tags** `serialization` `json` `enums` `round trip` `rttr` `type identity`
**Classification** `CONCEPTUAL PRECEDENT` · `ALGORITHM QUARRY` · `ANTI-PRECEDENT`

**What existed.** Two independent JSON encodings.

The **UI tree** encoding names enums by string via `NLOHMANN_JSON_SERIALIZE_ENUM` for `Layout`,
`PositionTo`, `DataType`, `TextBoxFilterType`, `SizeTo` and `PaddingTo` — which is the right
instinct, since it survives a renumber — plus `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` for
`Text::Config` and `TextBox::TextBoxFilter`. Every authored property is written; nothing resolved
is. Children nest.

The **DataPacket** encoding writes each entry under `"<name>:<typename>"` and reflects over class
properties through RTTR, reconstructing by looking the type name up in the RTTR registry.

**Current Zengine.** Wire form is the Loom's: shapes have versions that are part of identity at
the admission gate, and Zengine's own formats state their rules — the setup format's version *is*
the envelope's shape version by `static_assert`, so a version-1 file is refused **on its claim**
before a single row is read, and can therefore never be reported as "a pane row is missing
`place`", a true sentence about a false cause (`agents/workshop.md`). Two absences are enforced
rather than assumed: resolved geometry has **no wire form** at all (`Rect`/`Placed`/`Scene` are
deliberately not shapes), and a `TextBox` "has no wire form, nothing serializes it and nothing
hosts it".

**Traps.** Both encodings identify a type by a **string that a compiler produced**: `typeid`
name for the UI tree, RTTR type name for `DataPacket`. The UI tree then matches that string by
**substring**, which is what breaks `TextBox` round-tripping ([§1.9](#19-the-visual-ui-builder)).
The lesson available here is about identity, not about JSON: an identity that is a prefix of
another identity is not an identity.

---

### 1.19 Runtime code generation and dynamic invocation

**Source** `src/zengine/console.{h,cpp}` · `src/zengine/rttr_wrapper.h`
**Tags** `console` `runtime codegen` `dynamic invocation` `reflection` `rttr` `operators`
`hot reload`
**Classification** `CONCEPTUAL PRECEDENT` · `ANTI-PRECEDENT` · `LOW QUARRY VALUE` (as code)

**What existed.** A sketch — and the most *ambitious* idea in the quarry. `ConsoleInterface::run`
reads lines from stdin and offers two commands:

```text
create_method <class> <method> <code…>   compile source at runtime, then register it
runcommand <name>(<arg>)                 look the name up and invoke it dynamically
```

`create_method` calls `CodeGenerator::generate_and_compile(code, class_name, method_name,
options)` — passing an options JSON carrying a `whitelist`/`blacklist` — then loads a config
naming the produced artifact and registers it through `RttrWrapper`. `RttrWrapper` keeps a
`method_to_class` map, registers `rttr::registration::class_<>(name).method(name, fn)`, and
offers `method_exists(name)` and `invoke_dynamic(name, arg)`.

So: **a running program that compiles new code, registers it under a name, and lets a person
invoke it by name from a console** — plus a whitelist/blacklist notion of what such code may
touch.

**Current Zengine.** The same three concerns exist and are separated:
- **naming and resolving a typed power** is the `operator` package — a catalog, providers, a
  host/consumer seam, and the rule that "one authoring is one live answer" with the host owning
  resolution (`agents/operators.md`, CAT-0 / PROV-0);
- **a person driving a running system by typing** is Workshop's Terminal pane and the Composer,
  where a form is built from the runtime schema and every refusal is the type ladder's own
  (`agents/panes.md`);
- **what a loaded artifact may touch** is the Loom's capability and grant machinery, not a JSON
  allow-list read by a console.

The legacy sketch collapses all three into one class, and none of it works: `CodeGenerator` does
not exist anywhere in the tree, `DataPacket::get_object<T>` is not a template,
`invoke_dynamic` returns `bool` rather than the `DataPacket` the console destructures, and
`console.cpp` is one `#include` line. Read it as **evidence of intent**, which is real and
matters, and not as an implementation.

**Traps.** `RttrWrapper::register_class` registers every method on the *same* type
(`rttr::registration::class_<RttrWrapper>`), so the class name argument does not do what its name
suggests. `invoke_dynamic` unconditionally `get_value<bool>()`s the result.

---

### 1.20 The Zen kernel prototype (stratum B)

**Source** `src/kernel.{hpp,cpp}` · `src/main.cpp` · `src/bootstrap.cpp` · `src/console.cpp` ·
`src/advanced_console.cpp` · `src/CMakeLists.txt`
**Tags** `kernel` `senses` `dynamic loading` `hot reload` `console` `replacement`
**Classification** `CONCEPTUAL PRECEDENT` · `ANTI-PRECEDENT` · `SUPERSEDED`

**What existed.** A ~200-line first sketch of the substrate that the **Loom** owns today.

`zen::Kernel` is a singleton with a `mutex`, a `unordered_map<string, function<any()>>` of
providers, and a list of OS library handles. `load_program(path)` opens the shared library
(`LoadLibraryA` / `dlopen`), resolves the required export `zen_program_entry`, and calls it with
the kernel; the loaded program then calls `provide("TimeDriver.delta_time", …)` to register named
values. `get_sense<T>(path)` looks the name up, invokes the provider, and `any_cast`s to `T*`,
returning `nullptr` on both miss and type mismatch. `dump_senses()` lists names.
`unload_program(name)` closes the handle and — per its own comment — deliberately does **not**
remove the senses it provided ("graceful degradation").

`main.cpp` boots the kernel, loads `bootstrap.dll` and `console.dll`, then runs a REPL.
`console.cpp` provides callable senses by stuffing raw function pointers into the same `any`
channel and runs an interactive `zen>` prompt with `help / senses / load / unload / call / exit`.
`advanced_console.cpp` adds TAB completion over a command list and a `replace <dll>` command
whose whole trick is: load the replacement, then `break` out of the loop so the old console's
`zen_program_entry` returns and the new one takes over.

**Current Zengine / Loom.** Every one of these concerns has a real owner now, and the differences
are the interesting part:

| prototype | today |
|---|---|
| `provide(name, ()->any)`, type-erased, `any_cast<T*>` or null | typed shapes with versioned identity; a type mismatch is a refusal with a sentence, not a null |
| unload leaves stale providers behind on purpose | replacement is a transaction with prepared handles, readiness and continuity — and a released role refuses a second claimant |
| the console is a loaded DLL that seizes the main thread | a Workshop pane, with routing, focus, transcript windowing and completion |
| "replace" = load the new one and return from the old one's entry | prepared replacement, first-breath admission, and role-addressed beats a successor inherits |
| a single global registry keyed by dotted string | offices, roles, catalogs and providers, each with an owner for resolution |

**Traps.** `advanced_console.cpp` names `zen::Sense`, which `kernel.hpp` does not declare, and is
in no `CMakeLists.txt` — it cannot compile. `bootstrap.cpp` and `console.cpp` use
`__declspec(dllexport)` unconditionally, so the tree is Windows-only despite `kernel.cpp`'s
`dlopen` branch. `get_sense<T>` returns `nullptr` for both "absent" and "wrong type" — two
different facts, one answer. Providers returning function pointers cast through `std::any` rely
on the caller guessing the exact signature.

---

### 1.21 Demonstration applications

**Source** `src/apps/tests/cookie_clicker/**` · `src/apps/examples/**`
**Tags** `demo` `example` `tabs` `procedural drawing` `hash`
**Classification** `UX EXPECTATION` · `LOW QUARRY VALUE`

**What existed.** `CookieClickerState` is a complete small game on the UI kit: a two-column
layout, live counters, a big clickable procedurally-drawn cookie, two tabbed scrolling lists of
purchasable upgrades with escalating costs, and `long double` economy arithmetic with `format_number`
/ `format_rate` helpers. Tabs are implemented as "set one scroll view visible, tint the active
tab button". `CookieView::draw` draws a filled oval, a darker rim, and N chip squares placed by a
**deterministic integer hash of the chip index** — so the cookie looks random and is stable across
frames with no RNG.

`vision.md` names this app as the V1 demo target to port ("The cookie clicker app, ported to run
as a Game Shard"), which is why it exists.

The four `examples/` programs are the closest thing to documentation of intended API feel —
`vector2_examples.cpp` in particular is written as annotated expected output, and demonstrates
the mutate-in-place stance deliberately.

**Current Zengine.** `snake` is the worked example, and it is a different *kind* of example on
purpose: "a worked example whose parts are genuinely separate weaves"
(`docs/reference/snake.md`). Its logic header states "no weave, no I/O, no clock, no global
randomness".

**Traps.** Every example includes `src/engine/…` paths and calls `Timer::get_current_time()`;
none of the four can compile against the tree they sit in
([§0.3](#03-generational-drift-inside-stratum-a)). `cookie_clicker_state.cpp` carries the
authorship artifacts noted in [§0.5](#05-authorship-weight). `CookieClickerState::draw` is empty
— everything is drawn through the layout tree.

---

### 1.22 Repository tooling and configuration

**Source** `compile_code.py` · `.clang-format` · `.clang-tidy` · `CMakePresets.json` ·
`CMakeLists.txt` · `readme.md` · `todo.txt`
**Tags** `tooling` `lint` `presets` `sanitizers`
**Classification** `LOW QUARRY VALUE`

**What existed.** `compile_code.py` walks the tree and concatenates every C/C++/CMake/readme file
into one `all_code.txt` with `// File: <path>` separators — a context-packing script, and a
plausible reason the tree circulates as a flat snapshot.

`CMakePresets.json` declares `debug`, `debug-asan` (ASan **and** UBSan) and `release` presets, and
the top-level `CMakeLists.txt` declares `ZEN_ENABLE_WARNINGS`, `ZEN_WARNINGS_AS_ERRORS`,
`ZEN_ENABLE_ASAN`, `ZEN_ENABLE_UBSAN`, `ZEN_ENABLE_LSAN`. **None of the five options is read
anywhere** — declaring them is the whole implementation.

`.clang-tidy` enables `bugprone-*`, `performance-*`, `modernize-*`, `readability-*`,
`portability-*` and `misc-*` with `WarningsAsErrors: ''`. `.clang-format` is LLVM-based, 2-space,
120 columns, **Allman braces** — which the source does not follow (it is uniformly K&R).

`readme.md` is the engine's pitch: "designed to be exceptionally easy to use by adding numerous
tools for the programmer, without taking any abilities away … not a replacement for C++
development, but a powerful addition". `todo.txt` is nine lines and is the most compact statement
of what the founder wanted next: an RNG class, chamfered boxes, icons, licence-checked free
assets, drag-to-resize/move, a config file with profiles, multi-window, and images.

**Current Zengine.** A sanitizer lane that actually runs the full population under ASan+UBSan and
is named as "a second KIND of evidence"; a population contract where a run selecting zero cases
is a failure; a `doc_links` check; a `package_vocabulary` check; and an installed-package witness
built outside the repository (`AGENTS.md`, `agents/verification.md`). The gap between *declaring*
a sanitizer option and *having a lane whose green means something* is the whole distance between
the two trees on this axis.

---

### 1.23 Capabilities absent even in the legacy tree

Recorded so a future reader does not go looking:

| capability | evidence |
|---|---|
| **audio** | no file, no symbol, no dependency, no mention in `readme.md` or `todo.txt` |
| **networking** | `message_bus/socket.h` contains `class Socket {};` and nothing else |
| **physics / collision** | nothing beyond `Rectangle::contains` and `Line2D::shares_domain_and_range` |
| **entity / scene / component system** | none. `CustomLayout` is the only tree; game objects are plain members of a `GameState` |
| **particles** | `graphics/particle.h` declares an empty `class Particle{}` and a `ParticleContructor` struct that nothing constructs |
| **animation / tweening** | none. `engine_config.json` has an `enable_animations` flag with nothing reading it |
| **RNG abstraction** | `std::rand` in two places; "Make a random number generator class" is line 1 of `todo.txt` |
| **automated tests** | none anywhere. `src/apps/tests/` holds the cookie-clicker *game*, not tests |
| **asset pipeline / manifest** | loading is by literal path string at the call site |
| **3D rendering** | projection math only; no rasteriser ([§1.15](#115-3d)) |
| **multi-window** | wanted in `todo.txt` and in a `renderer.h` comment; one window exists |

---

## 2. Current capability coverage

*A requirements aid, not a roadmap. `absent` is a statement about today's Zengine, not a request.
Read against the repository at the time of writing; **the owning documents named in the last column
are authoritative if they disagree with this table**.*

*⚠ One caveat, with a name and a date attached. This table was compiled while the **TEXT-0** phase
was in flight against `component/`, `input/`, `surface/` and `workshop/`, and it landed before this
was filed — `docs/reference/component.md`, `input.md` and `surface.md` were all rewritten in the
same hours, and the rows touching text editing, selection, the clipboard and the editing-key
vocabulary were revised here to match. That is the general case rather than a special one: this
whole column is a projection of somebody else's current truth, it goes stale without anybody
touching this file, and the owner is always the better answer.*

| capability | legacy evidence | current Zen/Zengine | status | owner to consult |
|---|---|---|---|---|
| application lifecycle / state stack | `GameStateManager`, `GameState` | no engine loop; a host loads weaves, the Loom owns lifecycle and replacement | intentionally different | Loom docs; `agents/realization.md` |
| a one-line "just start" entry point | `initialize_with_state(new State)` | a host `main` plus an authored load plan | intentionally different | `docs/reference/load-plan.md` |
| window / display | `Window`, one hard-coded size | SDL skin owns the window; first picture sets the minimum, the person owns the rest | covered (stronger) | `docs/reference/surface.md` |
| multi-window | wanted, never built | not present | absent | — |
| immediate drawing vocabulary | `GameGraphics` verbs + `(layer, sub_layer)` sort | `SurfaceCanvas`: cells, roles, ordered planes, publish-don't-call | superseded | `docs/reference/surface.md` |
| clipping / offset | modal `set_clipping` / `set_offset` | regions own their bounds; a plane covers an earlier plane whole; skin clips | intentionally different | `docs/reference/surface.md` |
| colour | literal RGBA everywhere | semantic roles only; a medium chooses ink *and* glyph | intentionally different | `docs/reference/surface.md` |
| textures / image loading | `TextureManager`, path-keyed cache | none; no `SDL_image` dependency | absent | — |
| sprites (scale/rotate/flip/origin) | `sprite.h`, orphaned | none | absent | — |
| font rendering | `TTF_OpenFont` per draw, no cache | JetBrains Mono embedded in the weave's bytes, measured once | covered (stronger) | `docs/reference/surface.md` |
| text measurement | `get_text_size(text, font, size, max_width)` | `SurfaceExtent` published by the medium; `fit_region` asked by publisher *and* medium | covered (stronger) | `docs/reference/surface.md` |
| input: platform facts | `Input` polling + SDL pumping | `zengine-input` is the sole producer and the only code touching the platform | covered | `docs/reference/input.md` |
| input: modifiers at a transition | `KeyCombo` with canonical modifier groups | `KeyPressed`/`KeyReleased` v2 carry modifiers held at the transition | covered | `docs/reference/input.md` |
| input: typed character vs key | text-input mode with a callback | `TextEntered` — the platform's own layout output | covered (stronger) | `docs/reference/input.md` |
| input: taps, holds, double-taps | `TriggerType` + duration knobs | not spoken at any version — application meaning | intentionally different | `docs/reference/input.md` |
| input: layers, priority, consumption | `create_layer` + `PASS`/`CONSUME` (**never driven**) | consumption is the application's; Workshop's press chain and its priority order are written down | partial | `agents/workshop.md` (QR-2) |
| input: rebinding capture | `listen_for_key_combo()` | none | absent | — |
| input: pointer warp / cursor / relative mode | nine `Input` methods | none | absent | — |
| keyboard focus | a `bool` on `TextBox` | `Panels::keyboard` — a press's memory, resolved fresh at every spend | covered (stronger) | `agents/workshop.md` (MSG-0) |
| authored geometry (mode + amount) | `SizeTo`/`PositionTo`/`PaddingTo` with per-axis amounts | `Extent{mode, amount}`, authored `x`/`y`, a named context frame | partial | `docs/reference/ui.md` |
| resolved geometry, kept separate | not separated — one mutable node held both | a compile-time fence; resolution needs a viewport; result cached nowhere | intentionally different | `docs/reference/ui.md` |
| parent/child containment | `CustomLayout` tree | **none** — an element names what its values are measured against, not what contains it | intentionally different | `docs/reference/ui.md` |
| padding / margin box model | full, per side, with percent | none | absent | — |
| row/column stacking with spacing | `set_vertical`/`set_horizontal` + `set_child_spacing` | none in `ui`; Workshop composes rows itself | absent | — |
| fill / leftover-space distribution | `SizeTo::FILL` | none in `ui`; `share_body_rows` is Workshop's own max-min fair share over rows | partial | `agents/workshop.md` |
| z-order / floating content | none — popups escaped to the `GameState` | an ordered plane sequence; `presentation_order` and `occupied_at` exact inverses | covered (stronger) | `agents/workshop.md` (WIND-2a) |
| hit testing | `find_component`, plus two copies elsewhere | one geometry draws a thing and hits it — a standing rule against a second copy | covered (stronger) | `agents/workshop.md` (HD-3) |
| widget set | Button, DropDown, ScrollView, Text, TextBox | one component (`TextBox`); Button explicitly declined | intentionally different | `docs/reference/component.md` |
| text editing state | append + backspace-at-end, caret as a literal pipe glyph in the string | `component::TextBox` — one state carrying text, caret and a visible window, on character boundaries | superseded | `docs/reference/component.md` |
| text selection / clipboard / undo | none — the legacy box could reach none of the three | present since the TEXT-0 phase, which also routed the clipboard through the Surface vocabulary | covered | `docs/reference/component.md`, `docs/reference/surface.md` |
| word moves, editing-key vocabulary | none | one owner for the editing keys, under the press chain's own bool | covered | `docs/reference/component.md` |
| multiline editing | none | not present | absent (both) | `docs/reference/component.md` |
| per-field input filters | `TextBoxFilterType` (two of five implemented) | a property row parses, writes, and may be refused **with a reason**; the Composer refuses via the type ladder | intentionally different | `agents/panes.md` |
| change vs commit as separate facts | `on_text_changed` / `on_text_committed` | a live draft vs a resting value — fitted with a mark vs windowed with a caret | covered (stronger) | `agents/workshop.md` |
| scrolling a long list | `ScrollView` with a stored offset | derived per paint, nothing stored, omissions counted and said | intentionally different | `agents/workshop.md` |
| a visual editor over a document | `BuildState` — create/tree/details panes | Workshop: objects list, property list, footer controls, one resolved body | covered (different shape) | `agents/workshop.md` |
| a property inspector | hand-written rows + `dynamic_cast` dispatch | rows derived from the document; the Composer builds a form from a runtime schema | covered (stronger) | `agents/panes.md` (MSG-0) |
| a form generated from a schema | wanted (`TODO VERY IMPORATNT`), never built | `zen.DescribeAccepted` → `zen.AcceptedShapes` → typed form, with presence and value as two members | covered | `agents/panes.md` |
| create/delete controls with availability | always-enabled buttons | two reasons, one bit, two owners; a control never invents a refusal | covered (stronger) | `agents/workshop.md` (HD-8) |
| persisting an authored document | `serialize_component` / `deserialize_component` (load disabled) | three named files with stated promises; a versioned, refusing format | covered (stronger) | `agents/workshop.md` (WIND-2, WUX-0) |
| config: a schema with defaults | `default.json` doubles as schema and defaults | authored formats declare their own vocabulary and refuse unknown words | intentionally different | `agents/workshop.md` |
| config: healing a stale file | `structurally_match_and_patch` | a bad candidate is **refused whole**, naming what it found and what would have worked | intentionally different | `agents/workshop.md` |
| config: compile-checked value names | generated nested namespaces + typed getters | none | absent | — |
| config: hot reload | `reload_if_changed` (unconditional; detector disabled) | session/setup files read at start and written at close; live replacement is the Loom's | partial | `agents/workshop.md` |
| build-time code generation | `config_code_gen.cpp` | authored build recipes generate a single-source project | partial (different subject) | `docs/reference/builder.md` |
| signals / connectors | `Signal`, keyed by `void*` | no general signal type; publish shapes, and republish on a peer's hello | intentionally different | `docs/reference/surface.md` (`SurfaceReady`) |
| generic callbacks | `Callback<Ret,Args…>` / `Action<…>` | deliberately avoided in Workshop's controls: "no callback, no command id, no action registry, no `std::function`" | intentionally different | `agents/workshop.md` (HD-8) |
| message bus | `MessageBus::broadcast` to every queue | the Loom's addressed messaging, tickets, answers, provenance | superseded | Loom docs |
| structured message payload | `DataPacket` of RTTR variants | `ZEN_SHAPE` with versioned identity at the admission gate | superseded | Loom docs |
| runtime shape discovery | RTTR reflection | `zen.DescribeAccepted` / `zen.AcceptedShapes` | superseded | `agents/panes.md` |
| value-or-live-projection | `VarStorage<T>` | introspection derives at every ask and keeps nothing; the `ui` fence forbids caching intent | intentionally different | `agents/panes.md` (INTR-1) |
| timers | `Timer` (never advanced in this tree) | a Timer service weave with laws, receipts, continuity across its own replacement | superseded | `docs/laws/timer-laws.md` |
| per-timer pause and time scale | `pause`/`resume`/`set_time_multiplier` | not present as such | absent | — |
| determinism / replay | `Input::step(now_ms)` takes the clock as an argument; `vision.md`'s replay thesis | not a current property | absent | — |
| hot reload of code | `vision.md`'s central claim; stratum B's `replace` | the Loom's replacement, readiness and continuity | covered (different shape) | Loom docs |
| crash containment / sandboxing | `vision.md`'s arenas; a whitelist/blacklist in `console.h` | the Loom's isolation and grant machinery | covered (different shape) | Loom docs |
| resource root discovery | walk up from the executable, cached | the one asset is embedded in the weave's bytes | intentionally different | `docs/reference/surface.md` |
| logging | `Logger` with levels and a file | `fprintf(stderr)` prefixed by target; refusals are values with reasons | intentionally different | — |
| math / geometry types | `Vector2`, `Vector3`, `Rectangle`, `Line2D`, `Math` | only what a package needs, where it needs it | intentionally different | — |
| 3D | projection math only, inert | none | absent (both) | — |
| audio | none | none | absent (both) | — |
| networking | `class Socket {};` | none in Zengine; the Loom has a network **grant** flag, not a transport | absent (both) | Loom `isolation/` |
| entity / scene system | none | none | absent (both) | — |
| particles / animation | stubs and an unread flag | none | absent (both) | — |
| RNG | `std::rand`; wanted in `todo.txt` | deliberately absent from snake's logic; none in the engine | absent (both) | `docs/reference/snake.md` |
| automated tests | none | doctest suites, a population contract, a sanitizer lane, a package witness | covered (stronger) | `agents/verification.md` |

---

## 3. Quarry index

Search terms → where to look. Concept first, legacy noun second.

| term | entry | primary source |
|---|---|---|
| `state manager`, `lifecycle`, `main loop` | [§1.1](#11-application-lifecycle-and-game-state) | `src/zengine/state_management/` |
| `overlay`, `popup`, `modal` | [§1.1](#11-application-lifecycle-and-game-state), [§1.7](#17-ui-the-widget-set) | `game_state.h`, `ui/drop_down.cpp` |
| `window`, `resize`, `multi-window` | [§1.2](#12-window-and-display) | `graphics/window.cpp` |
| `rendering`, `draw list`, `layers`, `painter's order` | [§1.3](#13-drawing) | `graphics/game_graphics.cpp` |
| `clipping`, `offset` | [§1.3](#13-drawing) | `graphics/game_graphics.cpp` |
| `textures`, `images`, `resource loading`, `cache` | [§1.4](#14-textures-images-and-fonts) | `graphics/texture_manager.cpp` |
| `sprite`, `rotation`, `flip`, `origin` | [§1.4](#14-textures-images-and-fonts) | `graphics/sprite.h` (orphaned) |
| `fonts`, `text measurement`, `wrapping` | [§1.4](#14-textures-images-and-fonts), [§1.8](#18-text-and-the-legacy-textbox) | `texture_manager.cpp`, `ui/text.cpp` |
| `input`, `polling`, `mouse`, `wheel` | [§1.5](#15-input) | `input/input.cpp` |
| `key combo`, `modifiers`, `custom modifier` | [§1.5](#15-input) | `input/input.h` |
| `consumption`, `layers`, `priority`, `routing` | [§1.5](#15-input) | `input/input.cpp` (dead code) |
| `double tap`, `hold`, `tap window` | [§1.5](#15-input) | `input/input.cpp` |
| `rebinding`, `key capture` | [§1.5](#15-input) | `Input::listen_for_key_combo` |
| `text input mode` | [§1.5](#15-input), [§1.8](#18-text-and-the-legacy-textbox) | `Input::start_text_input` |
| `layout`, `box model`, `padding`, `margin` | [§1.6](#16-ui-the-layout-tree) | `ui/custom_layout.cpp` |
| `sizing`, `fill`, `percent`, `size to parent` | [§1.6](#16-ui-the-layout-tree) | `graphics/enums.h`, `custom_layout.cpp` |
| `invalidation`, `dirty flags`, `caching` | [§1.6](#16-ui-the-layout-tree) | `custom_layout.cpp` (disabled) |
| `hit test`, `find component` | [§1.6](#16-ui-the-layout-tree) | `CustomLayout::find_component` |
| `button`, `hover` | [§1.7](#17-ui-the-widget-set) | `ui/button.cpp` |
| `dropdown`, `option list` | [§1.7](#17-ui-the-widget-set) | `ui/drop_down.cpp` |
| `scroll`, `scroll view`, `clipped list` | [§1.7](#17-ui-the-widget-set) | `ui/scroll_view.cpp` |
| `text box`, `caret`, `blink`, `focus` | [§1.8](#18-text-and-the-legacy-textbox) | `ui/text_box.cpp` |
| `text selection`, `clipboard`, `undo` | [§1.8](#18-text-and-the-legacy-textbox) | **absent in legacy** — that is the finding |
| `filters`, `validation`, `data type` | [§1.8](#18-text-and-the-legacy-textbox) | `graphics/enums.h`, `text_box.cpp` |
| `builder`, `editor`, `inspector`, `property editor` | [§1.9](#19-the-visual-ui-builder) | `apps/builder/build_state.cpp` |
| `component tree`, `selection`, `highlight` | [§1.9](#19-the-visual-ui-builder) | `build_state.cpp` |
| `schema-driven form`, `generated dropdown` | [§1.9](#19-the-visual-ui-builder), [§1.10](#110-configuration-and-generated-config-apis) | `build_state.cpp:520` TODO |
| `config`, `config generation`, `codegen` | [§1.10](#110-configuration-and-generated-config-apis) | `config_code_gen.cpp` |
| `defaults`, `healing`, `schema patch` | [§1.10](#110-configuration-and-generated-config-apis) | `config_manager.cpp` |
| `hot reload` (config) | [§1.10](#110-configuration-and-generated-config-apis) | `ConfigManager::reload_if_changed` |
| `identifier mangling`, `keyword escape`, `snake case` | [§1.10](#110-configuration-and-generated-config-apis) | `config_code_gen.cpp::to_snake` |
| `signals`, `connectors` | [§1.11](#111-signals-callbacks-and-the-message-bus) | `zsignal.h` |
| `callbacks`, `action` | [§1.11](#111-signals-callbacks-and-the-message-bus) | `callback.h` |
| `message bus`, `broadcast`, `listener` | [§1.11](#111-signals-callbacks-and-the-message-bus) | `message_bus/` |
| `rttr`, `reflection`, `variant payload` | [§1.11](#111-signals-callbacks-and-the-message-bus), [§1.19](#119-runtime-code-generation-and-dynamic-invocation) | `data_packet.h`, `rttr_wrapper.h` |
| `var storage`, `live value`, `projection`, `binding` | [§1.12](#112-varstorage--a-value-that-may-be-a-live-projection) | `message_bus/var_storage.h` |
| `timers`, `pause`, `time scale` | [§1.13](#113-timing) | `zengine/timer.cpp` |
| `math`, `vector`, `rectangle`, `lookup table` | [§1.14](#114-math-geometry-and-value-types) | `types/`, `logic/math.cpp` |
| `noise`, `random walk`, `interpolation` | [§1.14](#114-math-geometry-and-value-types) | `types/function.h` (orphaned) |
| `3d`, `camera`, `projection`, `icosahedron` | [§1.15](#115-3d) | `graphics_3d/`, `types/3d/sphere.cpp` |
| `filesystem`, `resource path`, `demangle` | [§1.16](#116-resource-location-and-filesystem) | `logic/utils.cpp` |
| `logging`, `debug` | [§1.17](#117-logging) | `zengine/logger.cpp` |
| `serialization`, `json`, `enum names`, `round trip` | [§1.18](#118-serialization) | `build_state.cpp`, `data_packet.h` |
| `console`, `repl`, `dynamic invocation`, `runtime codegen` | [§1.19](#119-runtime-code-generation-and-dynamic-invocation) | `zengine/console.h` |
| `kernel`, `senses`, `dynamic loading`, `replace` | [§1.20](#120-the-zen-kernel-prototype-stratum-b) | `src/kernel.cpp`, `src/console.cpp` |
| `tab completion` | [§1.20](#120-the-zen-kernel-prototype-stratum-b) | `src/advanced_console.cpp` |
| `demo`, `tabs`, `procedural drawing`, `stable hash` | [§1.21](#121-demonstration-applications) | `cookie_clicker/cookie_view.cpp` |
| `sanitizers`, `presets`, `lint` | [§1.22](#122-repository-tooling-and-configuration) | `CMakePresets.json`, `.clang-tidy` |
| `determinism`, `replay`, `input log` | [§0.2](#02-three-strata-not-one-codebase), [§1.5](#15-input) | `vision.md`, `Input::step` |
| `audio`, `networking`, `physics`, `entities`, `particles` | [§1.23](#123-capabilities-absent-even-in-the-legacy-tree) | absent |

---

## 4. Reusable quarry candidates

Conservative, and not dismissive. Each names the **kind** of reuse the source could support, if
and when the relevant question is posed. None of this is a proposal.

### Reuse as code (read-and-rewrite; `repository-conventions.md` forbids lift-and-shift)

1. **`config_code_gen.cpp`'s identifier machinery** — `to_snake` plus the C++ keyword table plus
   `warn_if_mangled`. Self-contained, correct, and the exact problem any "turn authored names
   into code" step has. Would need only its output include path changed.
   *(`src/zengine/config_code_gen.cpp`, ~120 lines.)*
2. **`structurally_match_and_patch`** — recursive schema-shaped healing of a JSON document
   against a default. ~25 lines, no dependencies beyond nlohmann. Note that Zengine's current
   posture is *refuse whole* rather than *heal*, so this is quarry for a place where healing is
   the right answer, not a challenge to that posture.
   *(`src/zengine/config_manager.cpp`.)*
3. **The stable per-index hash in `cookie_view.cpp`** — deterministic scatter with no RNG and no
   stored state. Tiny, and the pattern is reusable wherever "looks random, must be stable across
   frames" appears. *(`src/apps/tests/cookie_clicker/cookie_view.cpp::hash01`.)*

### Reuse as algorithm (the idea, re-derived)

4. **The resource-root walk** — walk up N levels from the executable looking for a named
   directory, cache the first hit, and fail with a message naming the search origin.
   *(`src/zengine/logic/utils.cpp::get_resources_path`; fix the case-sensitivity trap.)*
5. **Modifier grouping** — mapping left/right modifier scancodes onto a group id with one
   canonical representative, plus promoting an arbitrary key into a group. This is what makes a
   combo comparable and sortable without enumerating handedness.
   *(`src/zengine/input/input.cpp::init`, `_combo_matches`.)*
6. **Quick-tap prevention** — a press shorter than one frame is still reported to a poller for a
   bounded buffer afterwards. A small, honest answer to a real problem.
   *(`src/zengine/input/input.cpp::is_key_down`.)*
7. **`Timer::is_time()` subtracting the delay rather than resetting** — a missed frame fires the
   right number of times and the schedule does not drift. *(`src/zengine/timer.cpp`.)*
8. **Icosahedron vertex generation** — the 12 golden-ratio vertices, normalised.
   *(`src/zengine/types/3d/sphere.cpp`; the subdivision beside it is broken.)*
9. **Lazily-extended interpolated value noise** — extend the walk on demand, remember it,
   interpolate between brackets. *(`src/zengine/types/function.h::Noise`.)*

### Reuse as API inspiration

10. **The generated config API shape** — object keys → nested namespaces, leaf keys → typed free
    functions, values resolved at runtime through a flat dotted-path map. The property it buys is
    worth naming: *a typo in a config name is a compile error, and the value still survives a
    reload.* *(`Resources/generated/config.h`, `config_code_gen.cpp`.)*
11. **`Input`'s routed surface** — `create_layer(name, priority)` / `on_key(layer, trigger, combo,
    handler, min_duration)` returning an RAII `InputConnection`, with a global pre-filter. Never
    driven, so it is a *design* to study, not behaviour to trust.
    *(`src/zengine/input/input.h`.)*
12. **Two rectangles beyond the owned one** — `owned` ⊃ `background` ⊃ `inside`, so "what I claim",
    "what I paint" and "where my contents live" are three separately-askable questions.
    *(`src/zengine/ui/custom_layout.cpp`.)*
13. **`Signal::connect(void* key, …)` / `disconnect(key)`** — subscription keyed by the
    subscriber's identity, so one call removes everything an owner registered.
    *(`src/zengine/zsignal.h`.)*
14. **`VarStorage<T>`'s four assignment forms** — constant, pointer, function, object+conversion,
    behind one read. *(`src/zengine/message_bus/var_storage.h`; read its header comment, not its
    implementation.)*
15. **The two inspector row factories** — `create_details_row(label, filter, value, on_commit)` and
    `create_dropdown_row(label, options, index, on_change)`. The *signatures* are the useful part:
    a labelled, type-constrained editor with a commit callback is the minimum vocabulary a
    property pane needs. *(`src/apps/builder/build_state.cpp:419-473`.)*

### Reuse as requirement evidence

16. **`todo.txt`** — nine lines naming what the founder wanted next: an RNG class, chamfered
    boxes, icons, licence-checked free assets, drag-to-resize/move, config profiles, multi-window,
    images. The most compact statement of felt gaps in the tree.
17. **`readme.md`'s pitch** — "easy to use … without taking any abilities away", and the specific
    complaint that a programmer should not have to set up a window, a renderer and an update cycle
    before starting.
18. **The `TODO VERY IMPORATNT` at `build_state.cpp:520`** — with the mis-wired dropdowns
    immediately around it, this is the quarry's strongest single piece of evidence for deriving a
    form from a schema rather than from a hand-written list ([§1.9](#19-the-visual-ui-builder)).
19. **`engine_config.json`** — names a `checkbox` section that never existed; a statement of
    *intended* configurable surface.
20. **The legacy `TextBox`'s reachable/unreachable table** ([§1.8](#18-text-and-the-legacy-textbox))
    — evidence for which text pressures were felt early (filter, change-vs-commit, focus-by-click)
    and which were never even approached (caret movement, selection, clipboard, undo).

### Explicitly low value as code

`Logger`, `Color`, `Shape`, `PlaneBounded`, `Line3D`, `Particle`, `Socket`, `rttr_wrapper.h`,
`console.h`, `advanced_console.cpp`, `compile_code.py`, and the `Vector2`/`Vector3` arithmetic —
either trivial, broken, stubbed, or dependent on a library (RTTR) that current Zengine does not use.

---

## 5. Architectural anti-precedents

Places where current Zengine went a different way *and the legacy source shows what the other way
cost*. These are the entries whose value is precisely that they were not repeated.

1. **A mutable tree that holds authored intent and resolved geometry in the same object.**
   `CustomLayout` stores `_size_to_width` (intent) beside `_width` (result) and a `_width_current`
   flag to relate them. The flags were built, then disabled, and the whole subtree recomputes on
   every query ([§1.6](#16-ui-the-layout-tree)). Zengine's `ui` package makes storing a resolved
   rectangle beside authored intent a **compile error**, and proves the fence fires with a
   negative-compilation test. The legacy tree is the measurement that argues for the fence.

2. **Layout, containment, painting, input, lifetime and identity in one base class.** Everything
   in the old UI *is* a `CustomLayout`: it lays out, paints, owns its children's memory, hit-tests,
   holds a click callback, carries a name, and emits a signal. Every widget then inherits all of
   it. Zengine's `ui` package owns exactly one distinction and states five things it is *not*.

3. **Global painter's order across kinds.** `(layer, sub_layer)` sorted one flat list, so a text
   draw at layer 1 covered a rectangle at layer 1 regardless of which presentation each belonged
   to. Zengine's plane sequence replaced exactly this, and `docs/reference/surface.md` names the
   defect it fixed.

4. **A widget set assembled from a list of widgets.** Button, DropDown, ScrollView, Text and
   TextBox arrived because a UI toolkit is expected to have them; `DropDown` shipped in a state its
   own comment calls unusable. Zengine's stated rule is *extract from repeated working behaviour,
   never from a list of widgets*, with `component::Button` declined on the record.

5. **Popups with nowhere to go.** A tree with no z-order forced floating content to be registered
   with the application as an overlay plus a consuming mouse callback — and the resulting
   ownership confusion is the `DropDown` double-delete. Zengine has a written depth story and one
   order helper with an exact inverse.

6. **Three copies of one hit-test walk.** `CustomLayout::find_component`,
   `BuildState::get_component` and `CookieClickerState::get_component` are the same recursion.
   Zengine's HD-3 rule — *the geometry that draws a thing and the geometry that hits it must be the
   same geometry*, with a named function both call — is the direct answer.

7. **A form that knows labels but not the schema.** Hand-maintained enum string lists drifted from
   their enums and silently set the wrong value ([§1.9](#19-the-visual-ui-builder)). The Composer
   derives the form from `zen.AcceptedShapes` and lets the type ladder speak the refusal.

8. **Identity by substring of a compiler-produced string.** `type.find("Text")` matching `TextBox`
   breaks the serialisation round trip, and the root cause is that `demangle` was never written
   ([§1.18](#118-serialization)). An identity that is a prefix of another identity is not an
   identity.

9. **A "loaded" that loads nothing.** `load_from_json` parses the file, never reconstructs
   anything, and logs success ([§1.9](#19-the-visual-ui-builder)). Zengine treats exactly this
   shape as a standing trap — *a green build produced its artifact, a load made a weave live, a
   send was delivered* are each two facts wearing one word (`AGENTS.md`).

10. **A single global registry keyed by a dotted string, where unload leaves entries behind.**
    Stratum B's `Kernel` does this deliberately ("graceful degradation"), and returns `nullptr`
    for both "absent" and "wrong type" ([§1.20](#120-the-zen-kernel-prototype-stratum-b)).

11. **Sanitizer options that are declared and never read.** Five `ZEN_ENABLE_*` options exist in
    `CMakeLists.txt` and nothing consumes them ([§1.22](#122-repository-tooling-and-configuration)).
    Zengine's sanitizer lane is a second *kind* of evidence with a named degradation mode.

12. **No tests at all**, in a tree whose `src/apps/tests/` directory contains a game.

---

## 6. Open questions and uncertainties

Recorded rather than resolved, per the phase's own rule.

- **Which parts of stratum A the founder considers "the old engine" versus scaffolding.** The
  catalog treats `src/zengine/**` and `src/apps/**` as one stratum, but the internal drift
  ([§0.3](#03-generational-drift-inside-stratum-a)) shows at least two API generations inside it.
  A reader who knows the chronology could split it further.
- **Whether the routed `Input` API was ever driven in an earlier revision.** In *this* snapshot it
  is entirely unreferenced ([§1.5](#15-input)). Whether it once worked and was disconnected, or
  was written and never wired, is not answerable from the imported files — the original working
  copy's history would answer it.
- **Whether `Timer` ever advanced.** `_accumulate_time()` is uncalled here
  ([§1.13](#113-timing)), so nothing in this tree can have observed a timer firing; but
  `TextBox`'s blink and two UI throttles are written as if it did.
- **What `CodeGenerator` was.** `console.h` calls it with a whitelist/blacklist options object
  ([§1.19](#119-runtime-code-generation-and-dynamic-invocation)) and no such class exists in the
  import. Whether it was written elsewhere, or was aspirational, is unknown here.
- **Whether `default.json` was intended as the schema or merely as the defaults.** It functions as
  both — `ConfigManager` infers types from it and the generator infers the API from it — and
  nothing says whether that dual role was deliberate or convenient.
- **Which of the three fonts, if any, are safe to reuse.** `THIRD_PARTY_NOTICES.md` records one
  EULA and two unverified provenances; that file is the owner, not this one
  ([§1.4](#14-textures-images-and-fonts)).
- **How much of stratum A is hand-written.** [§0.5](#05-authorship-weight) records the markers
  observed; classifying file by file was not attempted and would change how much of §4's
  "requirement evidence" is genuinely the founder's.

### Research questions a future phase might choose to pose

*Questions, not work items. None of these implies that the answer is "build it".*

- Is there a Zen-shaped answer to **relative sizing** (`SizeTo::PARENT_PERCENT`, `FILL`,
  `CHILDREN`) that does not reintroduce parent/child into `ui`? Workshop's `share_body_rows`
  already solves the fill case for one dimension of one panel; whether that generalises, or should,
  is unexamined.
- The legacy engine put tap/hold/double-tap **in the input subsystem**; Zengine puts them in the
  consumer. Where does a *second* consumer wanting the same gesture put it — and does that answer
  look like the `TextBox` extraction rule?
- `Input::step(now_ms)` derived every event from an injected clock. Is there anything in the
  current input path that would resist the same discipline, and is that a property worth having?
- What would a Zen answer to "a typo in an authored name is a compile error" look like, given that
  authored files are read at runtime and refused with sentences rather than compiled?

---

*Written against the `reference/` tree as imported. Where this document and a current Zengine
document disagree about current Zengine, the current document wins and the line here is stale.*
