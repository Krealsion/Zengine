# Editor materials setup

Editor, Inventory, Files and Terminal for carrying text, commands and file places. Carry a selection, a Terminal command and a file place into Inventory folders, then bring them back.

```sh
python external-host/demo.py start --setup editor-materials --root demo-runs/editor-materials --build <build> --loom-prefix <installed Loom>
```

## First task

Run `workshop/editor-materials-demo` with phase=prepare (its README names the inputs).

Expect: The carried materials are filed in folders and saved as a toolbox.

More: phase=retrieve in a new root; phase=keyboard with --tui; phase=neovim with --neovim <program>. The story is [carrying from the Editor](https://github.com/Krealsion/Zengine/blob/main/editor/docs/editor.md#carrying-text-commands-and-file-places).

## Reset

**Reset demo**, or `python external-host/demo.py reset --root <root>`, restores the layout and Inventory's selection. It keeps every Inventory entry and every file.

## Limits

- The story writes beat.cpp and notes.txt under the root's workshop/materials
