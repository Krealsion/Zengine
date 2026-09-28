# Command reuse setup

Inventory, Loaded and Compose with captured values to fill commands from. Fill a command from an Inventory reference, store it and submit it explicitly.

```sh
python external-host/demo.py start --setup commands --root demo-runs/commands --build <build> --loom-prefix <installed Loom>
```

## First task

Run `workshop/inventory-compose-demo` with a fresh label, or pick a weave in Loaded and fill a Compose field from Inventory.

Expect: A stored command appears in Inventory and runs once when you submit it.

More: Drag the stored command back into Compose and submit it again. The story is [Inventory to Compose](https://github.com/Krealsion/Zengine/blob/main/docs/workshop/inventory-compose.md).

## Reset

**Reset demo**, or `python external-host/demo.py reset --root <root>`, restores the layout, the nine Demo input entries' values and labels, Inventory's selection and the Info/Compose drafts on the desk. It keeps every entry you created, including saved copies and commands; submitted commands are not undone.

## Limits

- Commands you store are kept by Reset; submitted commands are not undone
