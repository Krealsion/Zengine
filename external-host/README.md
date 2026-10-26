# Workshop from another host

**Lets a program on another Loom host (an external Loom host, or ELH), such as an agent's own
session, connect to a Workshop you are running: press its keys, take pictures of what it shows,
read its desk as text and act through its panes, as a guest you named and only with the powers
you gave it.**

The two run on one machine, joined by a local socket, and Workshop shows who is connected in
its Connections pane. The same tools prepare [ready-to-use setups](../workshop/docs/demo-setups.md):
a dedicated Workshop laid out for one task, with a Reset demo button.

## Pages

- [Drive Workshop from another host](docs/external-host.md): say who may connect, link a host,
  and run the tool packages against Workshop
- [Author and check a recipe through an ELH](docs/elh-recipe-journey.md): write a new build
  recipe through Workshop's Files pane from another host's session, then check it

## Manuals

- [`zengine.guests`](docs/zengine.guests.md), the guest door: who is connected, a guest's own row,
  and where Workshop's plan is
- [`zengine.connections`](docs/zengine.connections.md), the Connections pane
- [`zengine.demo`](docs/zengine.demo.md), a setup's Demo pane and the office its demo service
  attaches to

## What is in this folder

| | |
|---|---|
| `tools/workshop/` | the `workshop` tools a Loom session runs against a running Workshop, by message alone |
| `tools/workshop/setups/` | the ready-to-use setups, one folder each |
| `tools/workshop-machine/` | the `workshop-machine` tools, which also reach this machine: a build tree, a checkout, project files, a setup's files |
| `tools/desk/` | the `desk` tools by message: the guide marked, and the controls that may cause a message |
| `tools/desk-machine/` | the `desk-machine` tools: Workshop's desk written out as text files |
| `demo.py` | starts, resets and stops a ready-to-use setup |
| `vocabulary_weave.hpp` | the message shapes the other host loads so it can speak to Workshop (`zengine-guest-vocabulary`) |
| `connections-pane/` | the Connections pane: the other hosts connected to this Workshop |
| `demo-control/` | the Demo pane a setup shows, with its Reset demo button |
| `docs/` | these pages |

Where an installed Zengine carries loadable weaves, `zengine-guest-vocabulary` is among them, and
both tool packages are under `share/zengine/loom-tools/`.

Working on it: [AGENTS.md](AGENTS.md).
