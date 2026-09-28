# Value inspection setup

Inventory, Info and a Reset button with nine captured Input structures. Inspect and edit a stored structured value in Info without changing its source.

```sh
python external-host/demo.py start --setup values --root demo-runs/values --build <build> --loom-prefix <installed Loom>
```

## First task

Drag `Demo input 1` from Inventory into Info, edit a scalar, then Save copy.

Expect: Info shows the edited copy; Inventory gains a new entry and `Demo input 1` is unchanged.

More: Fetch the original again in Info and compare; Press Reset demo and repeat with a fresh copy. The story is [demo setups](../../../../../docs/workshop/demo-setups.md#run-and-repeat-the-stories).

## Reset

**Reset demo**, or `python external-host/demo.py reset --root <root>`, restores the layout, the nine Demo input entries' values and labels, Inventory's selection and the Info/Compose drafts on the desk. It keeps every entry you created, including saved copies and commands; submitted commands are not undone.

## Limits

- Info and Compose drafts are disposable here: Reset discards them
