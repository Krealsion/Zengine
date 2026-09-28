# Command presets setup

Inventory, Loaded, Info and Compose for partial command presets. Save an incomplete command as a preset, reopen it and finish it from a typed record.

```sh
python external-host/demo.py start --setup presets --root demo-runs/presets --build <build> --loom-prefix <installed Loom>
```

## First task

Run `workshop/inventory-preset-demo` with a fresh label.

Expect: A blank template, a complete command and an incomplete preset are stored; both editors reset.

More: Run `workshop/inventory-slots-demo` for portable rows and hotkeys. The story is [portable inventory toolboxes](../../../../../docs/workshop/inventory-slots.md#reusable-live-demonstration).

## Reset

**Reset demo**, or `python external-host/demo.py reset --root <root>`, restores the layout, the nine Demo input entries' values and labels, Inventory's selection and the Info/Compose drafts on the desk. It keeps every entry you created, including saved copies and commands; submitted commands are not undone.

## Limits

- Each slots story adds three portable views, within Inventory's twelve-view bound
