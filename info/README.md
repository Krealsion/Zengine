# Info

**A Workshop pane for looking closely at things: the properties of a pane on your desk, and
typed values kept in Inventory. Several Info views can stay open at once, and nothing you do
elsewhere replaces what one shows.**

The default view shows the properties of the pane you pick with `inspect in Info` in the Pane
Manager, where you can change them. More views each hold one value, such as a captured sample, an
unfinished command or a stored note: edit it, save it or a copy, watch it for changes, ask a
sample's source for a fresh one, or fill one of its fields by dragging a field from another view.

## Pages

- [`zengine.info` -- Info](docs/zengine.info.md): the manual -- every command of the properties,
  a property draft and a value view, what each says, and what the pane shows
- [Independent Info views and the inspection workbench](docs/info-views.md): one story through
  several views at once, on a ready-made set of values to try it with

![Info wearing its ? at its top right, and the Manual showing its manual](docs/images/zengine.info-manual.png)

## What is in this folder

| | |
|---|---|
| `pane.cpp` | the pane itself, built as a weave Workshop loads (`zengine-info-pane`) |
| `property_view.hpp` | the default view's pane properties: the pane inspected, its rows, and a property you are changing |
| `value_view.hpp` | one value view: the value it holds, your unsaved edits, and what it is waiting for |
| `value_watch.hpp` | a value view's watch: Workshop's permission to keep reading a linked entry, and how it ends |
| `value_picture.hpp` | how a value view is drawn: its rows, buttons and field list, fitted to the room it has |
| `vocabulary.hpp` | the names it is known by (`zengine.info`), the actions your keymap file can move, and what it keeps when reloaded |
| `CMakeLists.txt` | how the pane is built |
| `docs/` | these pages |

## The three parts of a value view

| part | owns | does not own |
|---|---|---|
| `value_view.hpp` | every press, key and answer that reaches the view; the value and your edits; saves, refreshes, links, samples and field pickups, and what each is waiting for | the watch's requests; the layout of its rows |
| `value_watch.hpp` | the watch's requests to Workshop and the permission they hold: asking, reading again on a change, and ending it, here and at Workshop | the value: what it reads goes back to the view, which decides whether your draft takes it |
| `value_picture.hpp` | the rows, buttons and field list, laid out from what the view says it shows, and what a press on each place means | which buttons can act, and anything the view holds |

The view passes the watch its pane name and the message it is answering, and takes back what the
watch has to say; it gives the picture a description of itself and takes back the rows to publish.

Working on it: [AGENTS.md](AGENTS.md).
