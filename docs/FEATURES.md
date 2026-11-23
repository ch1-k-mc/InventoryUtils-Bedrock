# Features

[日本語](FEATURES.ja.md) | [简体中文](FEATURES.zh-CN.md) | [繁體中文](FEATURES.zh-TW.md)

## Item Scroller

- Move items with the wheel: one, a stack, all of a type, or everything
- Drag to move or drop stacks (whole, leave one, one at a time, same type)
- `W` / `S` to move items toward the top / bottom of the screen
- Drop all of a type; place with `Shift`
- Remember recipes, per world, and craft from the list
- Mass crafting, moving or throwing results, one-stack crafting from the result slot
- Works in Creative (`enableInCreative`, on by default; added for Bedrock)

## Inventory Profiles Next

- **Sorting**: normal, columns or rows, with selectable sort orders and rule files
- **Moving**: move all, matching only, the type under the mouse, or top up; drop matching items
- **Slot locks**: locked slots are skipped by sorting and moving, with a lock mark in screens and on the hotbar
- **Auto refill**: replaces used-up stacks and nearly broken tools, can be toggled per slot
- **Profiles**: save and switch equipment and hotbar layouts
- **Continuous crafting**: crafting table and stonecutter
- **Anvil**: Fast Rename
- **Villager trading**: global and per-villager favorites, run with a key

## Crafting Tools

Written independently from the public feature descriptions. Crafting table only.

- Rotate, balance, spread, clear, force clear
- Compress and decompress with 9-item / 4-item recipes
- Buttons left of the grid; the mode can be `BUTTONS_AND_KEYS`, `KEYS_ONLY` or `DISABLED`

## Hotbar favorites

- Save whole hotbars and recall them from a panel in container screens. JEI is not required
- In Creative, items are provided even if you do not have them
- In Survival, only matching items from your inventory are swapped in
- Items with data (such as filled chests) can be restored with their NBT **only in local worlds**, not on external servers

## Settings screen

One screen (`R+C` or `/invutils gui`) with tabs for Item Scroller, Inventory Profiles Next, Crafting Tools and Hotbar Favorites. Includes search, section index, sliders, hotkey capture and per-option reset.

## Differences from Java Edition

- Overlapping features are merged: sorting and villager trading use Inventory Profiles Next; wheel and drag moves and crafting helpers use Item Scroller.
- One key (`R+C`) opens all settings.
- Inventory Profiles Next's boolean options with a toggle key are plain booleans here.
- Colors are edited in a `#AARRGGBB` text field with a swatch; there is no color picker.

## Known issues

- Client only. Keyboard and mouse with the PC classic UI.
- Items with custom NBT that are not in the creative list cannot be restored on external servers unless you already have them.
- Fast mouse drags may miss slots.
- Auto refill notifications have no sound.
- Not every feature has been tested on external servers, and Realms has not been tested.
