# Views and the View Builder

**A view is a small panel described as data rather than written in C++: number fields, buttons
and labels placed in whole pixels, the field each label shows, and the message each button sends.
A weaver makes one by hand in the View Builder, a pane beside Flow, and runs it in a pane of its
own.**

A running view shows what it is told and says what its buttons mean; [Flow](../flow/docs/flow.md)
composes what happens next. The two meet only at message shapes, which a weaver drags from one
pane to the other. Workshop runs the views; this folder has no headers for other programs.

## Pages

- [Make a small panel by hand in the View Builder](docs/view-builder.md): the guide to making a
  view, running it, and joining it to Flow by dragging its shapes.
- [Described views](docs/view.md): the reference beneath the guide: a view's description and its
  file format, the view host, and the renderer that draws a view.

## What is in this folder

| | |
|---|---|
| `description.hpp` | a view as data, its saved file format, and the rules every view meets |
| `view.hpp` | the renderer: draws one running view in its pane and sends its buttons' messages |
| `host.hpp` | the view host: runs each view as a participant of its own |
| `vocabulary.hpp` | what may be asked of the view host: run a view, change it, stop it |
| `view-builder/` | the View Builder, the pane where a view is made by hand |
| `CMakeLists.txt` | how the folder builds |
| `docs/` | these pages and their pictures |

Working on it: [AGENTS.md](AGENTS.md).
