# Usage

[日本語](USAGE.ja.md) | [简体中文](USAGE.zh-CN.md) | [繁體中文](USAGE.zh-TW.md)

## Keys

`A+B` means hold `A` and press `B`. These are the defaults; all hotkeys can be changed in the settings screen.

### Item Scroller: moving (in a container screen)

| Keys | Action |
|---|---|
| Wheel on a stack | Move one item to the other side (up / down changes direction) |
| `Shift` + wheel | Move the whole stack |
| `Alt` + wheel | Move all items of the same type |
| `Alt+Shift` + wheel | Move all items |
| `Shift` + left-drag | Move the stacks you drag over |
| `Shift` + right-drag | Move the stacks, leaving one item |
| `Ctrl` + left-drag | Move one item from each stack |
| `Alt` + left-drag | Move all items of the same type as those dragged over |
| `Alt+Shift` + left-click | Move all items to the other side |
| `Q` + left-drag | Drop one item from each stack |
| `Shift+Q` + left-drag | Drop whole stacks |
| `Shift+Q` + right-drag | Drop stacks, leaving one item |
| `W` / `S` + left-click | Move one item toward the top / bottom of the screen |
| `Shift+W` / `Shift+S` + left-click | Move the stack up / down |
| `Alt+W` / `Alt+S` + left-click | Move all of the same type up / down |
| `W` / `S` + right-click | Move up / down, leaving one item |
| `Ctrl+Shift+Q` on a stack | Drop all items of the same type |
| `Shift` + left-click outside, holding items | Drop all items matching the held item |
| `Shift` + click an empty slot, holding items | Place items of the same type |

### Item Scroller: crafting (crafting table)

| Keys | Action |
|---|---|
| Hold `A` + middle-click the result slot | Remember the recipe in the grid |
| Hold `A` | Show remembered recipes |
| Click a recipe in the list | Select it and fill the grid |
| `Ctrl+C` | Craft as much as possible once with the selected recipe |
| Hold `Ctrl+Alt+C` | Mass-craft and throw out the results |
| `Ctrl+M` | Move crafted items to the inventory (or the other side) |
| `Ctrl+T` | Throw crafted items on the ground |
| Right-click the result slot | Craft one stack |
| Wheel on the result slot | Craft and move |

### Inventory Profiles Next: sorting and moving

| Keys | Action |
|---|---|
| `R` | Sort the inventory or container under the mouse (hotbar excluded by default) |
| `Alt+R` | Sort including the hotbar |
| Sort buttons (top right) | Sort normally, in columns or in rows |
| Wheel on a sort button | Change the sort order |
| Move-all button / `R+T` | Move matching items to the other side |
| `Shift` + move-all | Move everything |
| `Ctrl` + move-all | Move only the type under the mouse |
| `CapsLock` + move-all | Only top up stacks on the other side |
| `R+M` | Drop matching items (`Shift`: everything) |

### Inventory Profiles Next: locks, refill and profiles

| Keys | Action |
|---|---|
| `Alt` + left-click a slot | Lock / unlock the slot |
| Hold `Alt` while using a tool | Temporarily pause tool refill |
| `Ctrl` + left-click a slot (inventory) | Toggle auto refill for that slot |
| `Ctrl+R` (HUD) | Toggle auto refill for the current hotbar slot |
| `R+P` (inventory) | Save current equipment as a profile |
| `R+1` to `R+0` (HUD) | Apply the profile with that number |
| `R+Z` / `R+X` (HUD) | Next / previous profile |
| `R+H` (HUD) | Return to the saved inventory arrangement |
| `R+Y` | Reload rules, profiles and locks |

### Inventory Profiles Next: stations and trading

| Keys | Action |
|---|---|
| Continuous crafting checkbox | Refill the same layout as materials run out (crafting table, stonecutter) |
| Fast Rename checkbox (anvil) | Rename items repeatedly |
| `B` / `L` on a trade | Mark as a global / this-villager favorite |
| `T` | Run global favorite trades |
| `Shift+T` | Run this villager's favorite trades |

### Crafting Tools (crafting table only)

Three buttons appear left of the grid. **The hotkeys are unassigned by default**; assign them in the settings screen.

| Control | Action |
|---|---|
| Rotate / `Shift` + rotate | Shift the outer ring clockwise / counterclockwise |
| Balance | Divide items evenly among slots of the same type |
| `Shift` + balance | With a single item type, spread it over all slots |
| Clear | Return the grid to the inventory |
| `Shift` + clear | Clear and drop what does not fit |
| Compress / compress all (hotkey) | Lay out the 9-item (or 4-item) block recipe for the item under the mouse |
| Decompress / decompress all (hotkey) | Lay out the recipe that turns one block into items |

### Hotbar favorites

A panel appears at the top left of container screens (below the JEI panel when JEI is installed). JEI is not required.

| Control | Action |
|---|---|
| `+` button, or the `saveHotbar` hotkey (unassigned by default) | Save the current hotbar as a row |
| Left-click a row (Creative) | Fill the hotbar with the saved items, even if you do not have them |
| Left-click a row (Survival) | Swap in matching items from your inventory |
| `A` while hovering a row | Delete the row |

## Settings screen

Open it with `R+C` or `/invutils gui [itemscroller|ipnext|craftingtools|favorites]`. Mod tabs are at the top, categories on the left and options on the right. It reopens on the last tab, and values are saved when it closes.

- Search box, section index, and Reset for each option
- Number options: slider, or press `T` to type a value
- Hotkeys: click the button, press the keys, then release them all. `Esc` clears the binding. The icon on the left opens advanced settings (right-click resets them)
- Colors: a swatch and a `#AARRGGBB` text field

## Commands

| Command | Description |
|---|---|
| `/invutils gui [tab]` | Open the settings screen |
| `/invutils config list [category]` | List categories, or a category's options and values |
| `/invutils config get <category> <name>` | Show an option |
| `/invutils config set <category> <name> <value>` | Change an option (hotkeys: `LEFT_CONTROL,C`) |
| `/invutils config reset <category> <name>` | Reset an option |
| `/invutils save` / `reload` | Write / reload the config files |

## Config

Files are in `mods/InventoryUtils/config/`.

| File | Contents |
|---|---|
| `itemscroller.json` | Item Scroller settings |
| `recipes_<world>.json` | Remembered recipes |
| `inventoryprofilesnext/inventoryprofiles.json` | Inventory Profiles Next settings |
| `inventoryprofilesnext/rules-v2.*.txt` | Sorting rules |
| `inventoryprofilesnext/<world>/lockSlots.txt` | Slot locks |
| `inventoryprofilesnext/<world>/profiles-V2.txt` | Profiles |
| `inventoryprofilesnext/villager-trading-config-v2.json` | Trade favorites |
| `craftingtools.json` | Crafting Tools settings (`mode`: `BUTTONS_AND_KEYS` / `KEYS_ONLY` / `DISABLED`, `showButtons`, hotkeys) |
| `hotbarfavorites.json` | Hotbar favorites settings |
| `hotbar-favorites.json` | Saved hotbars |
