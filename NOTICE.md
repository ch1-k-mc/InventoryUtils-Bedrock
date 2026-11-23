# Notice

**Inventory Utils** is a client mod for Minecraft: Bedrock Edition on LeviLamina. It contains an **unofficial port** of the Java Edition mods below. Their names belong to their respective projects. This project is not affiliated with their authors, or with Mojang or Microsoft. Please do not contact the original authors about this mod.

## License

The mod as a whole is distributed under the **GNU Affero General Public License v3.0 only** ([LICENSE](LICENSE)). Individual source files keep the license of the work they are based on, shown by the `SPDX-License-Identifier` at the top of each file:

- Files based on Item Scroller or malilib: **LGPL-3.0-only**
- Files based on Inventory Profiles Next or libIPN: **AGPL-3.0-or-later**
- Files that combine both, and this project's own files: **AGPL-3.0-only**

When redistributing, include or indicate where to obtain the complete source code, this NOTICE, `LICENSE` and `licenses/`.

## Original works

### Item Scroller

- Authors: masa (maruohon), with contributions from Sakura-Ryoko and others
- Source: [sakura-ryoko/itemscroller](https://github.com/sakura-ryoko/itemscroller), branch `26.3`, commit `9d67667d69598eb52d9ebad1d6c52f89e0630854`
- License: **GNU Lesser General Public License v3.0** ([licenses/LGPL-3.0.txt](licenses/LGPL-3.0.txt), written as additional permissions on the GNU GPL v3, [licenses/GPL-3.0.txt](licenses/GPL-3.0.txt))
- Changes: rewritten from Java to C++. Java Edition screens, slots and clicks were replaced with their Bedrock equivalents (container screen controllers and LeviLamina events). Sorting and villager trading were merged into the Inventory Profiles Next side.

### malilib

- Source: [sakura-ryoko/malilib](https://github.com/sakura-ryoko/malilib), branch `26.3`, commit `b043d04c21009a9ac1d19078e56c501d63d56ec8`
- License: **GNU Lesser General Public License v3.0**

### Inventory Profiles Next (IPN)

- Copyright (c) 2019-2020 jsnimda, Copyright (c) 2021-2022 Plamen K. Kosseff, and contributors
- Source: [Inventory-Profiles-Next-Mod/IPN](https://codeberg.org/Inventory-Profiles-Next-Mod/IPN), branch `all-in-one`, commit `bb3acc6f6ccbbf367fa5818bf8101e1247c68dae`
- License: **GNU Affero General Public License v3.0 or later**
- Changes: rewritten from Kotlin / Java to C++, with a hand-written parser instead of ANTLR-generated code. Java Edition screens, slots and clicks were replaced with their Bedrock equivalents. Wheel and drag moves were merged into the Item Scroller side.

### libIPN

- Source: [Inventory-Profiles-Next-Mod/libIPN](https://codeberg.org/Inventory-Profiles-Next-Mod/libIPN), branch `master`, commit `89a2efa477c842e1317faee4368e69795d7637fa`
- License: **GNU Affero General Public License v3.0 or later**

## How the LGPL parts are combined

The parts based on Item Scroller and malilib stay under the LGPL v3 with their copyright notices; no permission is removed from them. They are combined with the AGPL parts as permitted by the LGPL v3 and by section 13 of the GNU GPL v3 / AGPL v3. The complete corresponding source of the whole mod is published in this repository.

## Bundled assets from the original works

Language files (`en_us` and other locales):

- `assets/itemscroller/lang/`
- `assets/malilib/lang/`
- `assets/libipn/lang/`
- `assets/inventoryprofilesnext/lang/`

Images:

- `assets/malilib/textures/gui/gui_widgets.png` (malilib)
- `assets/libipn/textures/gui/widgets.png` (libIPN)
- `assets/inventoryprofilesnext/textures/gui/gui_buttons.png` (IPN)
- `assets/inventoryprofilesnext/textures/gui/overlay_new.png` (IPN)

Translations added for the Bedrock port are new, including the Japanese translation of Item Scroller and the Chinese translations of text added by this port. `assets/inventoryutils/lang/` is written for this port.

## Crafting Tools

Crafting Tools provides features equivalent to Crafting Tweaks (TwelveIterations / BlayTheNinth, All Rights Reserved). It is this project's own design and implementation, written from the public feature description. It contains no code, language files or images from Crafting Tweaks, and is not affiliated with or endorsed by its authors.

## Icon

The icon is original to this project.

## Dependencies

- [LeviLamina](https://github.com/LiteLDev/LeviLamina) (LGPL-3.0), linked at runtime. Not included in this repository.
