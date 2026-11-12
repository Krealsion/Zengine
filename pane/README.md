# Pane

**The contract between a pane and the host that arranges it: the shapes they exchange, and the
helpers a pane may draw its rows and picture and seat itself with.**

A pane is a weave that offers itself to a host, draws in an area of the window the host places, and
hears its keys, presses and questions from the host and the window. Every shape that crosses
between the two is here, and so are the helpers a pane may use to draw and to be seated -- none of
which a pane must use. Workshop is one host of it; a pane built against the installed package links
`zengine::pane` and needs nothing of Workshop's.

## What is in this folder

| | |
|---|---|
| `vocabulary.hpp` | the shapes of the seam: the offer, the keys, a declared row, the reveal, the quit, a menu's ask |
| `presenter_vocabulary.hpp` | the presenter's seam: a granted menu, its showing and its close |
| `operation.hpp`, `carry.hpp` | an owner operation a gesture approves and the act classes it names, and what a pane carries out of itself |
| `settings.hpp`, `settings_taken.hpp` | the settings a pane declares and is handed, and a facet that takes them |
| `document.hpp` | a pane's manual, asked of the pane |
| `menu.hpp`, `escape.hpp` | a requester's record of its menu, and Escape's default |
| `drawing.hpp`, `rows.hpp`, `rows_block.hpp` | rows drawn for a window's room, and the area's name |
| `picture.hpp`, `canvas_drawing.hpp`, `canvas_text.hpp` | a picture of parts, drawn and measured |
| `text.hpp`, `text_fit.hpp`, `parts.hpp`, `causes.hpp` | text cut to a room, named parts, what a press may cause |
| `facets.hpp`, `seat.hpp` | the facets a pane may be built on, and a pane seated on them |
| `CMakeLists.txt` | the build: `zengine-pane-vocabulary`, exported as `zengine::pane`, header-only |

The protocol's reference is [Workshop's panes](../workshop/docs/workshop-panes.md). Working on it:
[AGENTS.md](AGENTS.md).
