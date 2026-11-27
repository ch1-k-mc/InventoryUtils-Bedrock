# Changelog

## 0.1.2

- Fixed: the hotbar favorites panel reacted to keys and clicks outside the inventory (for example `A` while typing in chat).
- Fixed: hotkeys no longer fire while you type in a text field (chat, search boxes, signs, anvil).
- Fixed: after pressing `E` while an item was being swapped in the background, the inventory could stop opening.
- In a local world, auto refill and profile switching swap items without opening a hidden inventory, so the game no longer pauses for a moment, and the inventory no longer stops opening after switching profiles.

## 0.1.1

- Updated the package description and README: Crafting Tools is described as an independent implementation of features equivalent to Crafting Tweaks.
- Chat messages of the `/invutils` command and log messages are now in English.
- With JustEnoughItems that supports it, the JEI side panel starts below the hotbar favorites panel.

## 0.1.0 — first release

Initial release for Minecraft: Bedrock Edition 1.26.60 / LeviLamina 26.60.x (client).

- Item Scroller port: wheel, drag and key-combination moves, drops, recipe memory and crafting helpers. Works in Creative with `enableInCreative` (on by default)
- Inventory Profiles Next port: sorting, move all, slot locks, auto refill, profiles, continuous crafting, Fast Rename and villager trade favorites
- Crafting Tools, written independently with features equivalent to Crafting Tweaks: rotate, balance, spread, clear, compress and decompress on the crafting table
- Hotbar favorites, without requiring JEI. In Creative, items are provided even if you do not have them. Items with data can be restored with their NBT only in local worlds
- One settings screen (`R+C` or `/invutils gui`) with tabs for each feature set
- `/invutils` command and config files
- English, Japanese and Chinese (Simplified / Traditional)
- Known limitations on Bedrock: Inventory Profiles Next's boolean options with a toggle key are plain booleans, and colors are edited in a `#AARRGGBB` text field
