# Organized workbench setup

Inventory browsing folders beside three Info views, for the organized workbench toolbox. File workbench entries in named folders and retrieve them through the folder path.

```sh
python external-host/demo.py start --setup folders --root demo-runs/folders --build <build> --loom-prefix <installed Loom>
```

## First task

Run `workshop/workbench` with phase=restore and variant=organized, then phase=retrieve.

Expect: The sample is retrieved through its folder path into the first Info view.

More: phase=folders and phase=menus in the same tool. The story is [the organized workbench](https://github.com/Krealsion/Zengine/blob/main/inventory/docs/inventory-folders.md#the-organized-workbench).

## Reset

**Reset demo**, or `python external-host/demo.py reset --root <root>`, restores the layout, Inventory's selection and folder (Root) and the three Info views. It keeps every Inventory entry, folder and view.

## Limits

- The organized toolbox is restored by its story, not by preparation
