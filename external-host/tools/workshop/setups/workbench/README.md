# Inspection workbench setup

A ready desk for inspecting typed values: Inventory, Loaded, Compose and three independent Info
views, filled with the workbench toolbox, and a hotkey row whose **Alt+1** captures Input's state
into a new Inventory entry. Choose it to inspect values side by side, or when a test or story needs
a known inspection fixture.

```sh
python external-host/demo.py describe workbench
python external-host/demo.py start --setup workbench --root demo-runs/workbench --build <build> --loom-prefix <installed Loom>
```

`start` returns once the desk, the four entries, the row and its key are usable, and prints this
guide's path. Start it again with the same root to return to it: your work stays as it is.

## First task

1. Press **Alt+1** with the Workshop window focused. Inventory gains `Workbench result`, a fresh
   capture of Input's state shape, one per press. The key works from any pane.
2. Drag the new `Workbench result` into the first Info view: it shows the captured fields as an
   independent copy. The stored `Workbench sample` is unchanged.

Useful next steps, all on the same desk:

- Link `Workbench capture preset` into the second view (select it, `Ctrl`+`Enter`, click the
  view) and drag the sample's `requested_role` field onto its `target_role`.
- Watch `Workbench note` in the third view while the second view saves it.
- Run the whole weaver story: `loom-session run <session> workshop/workbench --input phase=story`
  ([the inspection workbench](https://github.com/Krealsion/Zengine/blob/main/info/docs/info-views.md#the-inspection-workbench)).

## The hotkey

| chord | runs | on | context |
|---|---|---|---|
| Alt+1 | `Workbench capture command` (`InventoryCaptureAdd`, label `Workbench result`) | `zengine.inventory` | the row at the bottom left, `ON row 1` |

Preparation turns the item's binding and the row's context ON, through Inventory's own
configuration door; a toolbox restore alone leaves both OFF. The command runs with the pressing
actor's permission: yours, or the setup guest's, which may capture into Inventory. To disable it,
right-click the tile and choose **Disable item hotkey**, or turn the row's hotkeys OFF. To change
the chord, right-click **Configure command hotkey** (`zengine.inventory alt+2`, say); Reset puts
the declared chord back. It switches the item off before it moves or rebinds it and on again once
it is back in the row with Alt+1, so a command of yours on Alt+2 in another ON view keeps working.
A chord another active view already uses is refused by Inventory: if a command of yours holds
Alt+1 in a view that is ON, Reset reports that refusal, naming the view, instead of ready; your
command keeps Alt+1 and the capture command stays off until a Reset completes.

![Before Reset: the capture command on Alt+2 in an OFF box of its own, top left, beside a command of yours on Alt+2 in its own ON row](images/rebound.png)

![After Reset demo: the capture command back in its row with Alt+1, ON; your row, which the desk no longer seats, keeps Alt+2 and stays ON, and its result `Separate capture` is in Inventory](images/rebound-reset.png)

## Reset and recovery

![Before Reset: the capture command moved into a box of its own, the note in its row](images/moved.png)

![After Reset demo: the capture command back in its row with Alt+1, ON; the note back in Inventory](images/reset.png)

Press **Reset demo**, or run `python external-host/demo.py reset --root <root>`. Reset restores
the layout, the four workbench entries' values, labels and folders, the row -- holding exactly the
capture command, bound to Alt+1 -- and its ON switches, Inventory's selection and the Info and
Compose drafts. A box you made, or a view of yours the command was moved into, stays as it is,
switch included; the command goes back to the row. It keeps every entry you created,
including `Workbench result` captures and saved copies. Save a draft to Inventory before Reset if
you want to keep it. A pane with an operation in flight refuses Reset and the refusal names it;
steps completed before it stay applied.

`python external-host/demo.py status --root <root>` reports the state; `stop` ends Workshop and the
Loom session. A root is reused only while its recorded Loom session is the one answering.

## Limits

- The Info views and Compose are disposable drafts here.
- The row is seated by preparation; moving the command out of it makes Reset put it back, and
  anything else put in the row goes back to main Inventory. Removing the command makes Reset
  recreate it in a fresh row and turn the old one, which still shows the removed tile, OFF.
- Regenerating the packaged toolbox is `workshop/workbench` phase=prepare, in a Workshop without
  workbench entries.
