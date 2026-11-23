// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/recipes/CraftingHandler.h"

#include <map>
#include <set>
#include <utility>

namespace inventory_utils::itemscroller::recipes {

namespace {
std::map<std::pair<std::string, int>, CraftingHandler::SlotRange> gGridSlots;
std::set<std::string>                                             gCraftingGuis;
}

void CraftingHandler::clearDefinitions() {
    gGridSlots.clear();
    gCraftingGuis.clear();
}

void CraftingHandler::addCraftingGridDefinition(std::string const& screenName, int outputSlot, SlotRange range) {
    gGridSlots.insert_or_assign({screenName, outputSlot}, range);
    gCraftingGuis.insert(screenName);
}

bool CraftingHandler::isCraftingGui(std::string const& screenName) { return gCraftingGuis.contains(screenName); }

std::optional<CraftingHandler::SlotRange> CraftingHandler::getCraftingGridSlots(platform::inventory::SlotClicker& gui, platform::inventory::MenuSlot const& slot) {
    if (!slot.isOutput) return std::nullopt;
    auto it = gGridSlots.find({gui.menu().screenName(), slot.slotNumber});
    if (it == gGridSlots.end()) return std::nullopt;
    return it->second;
}

platform::inventory::MenuSlot const* CraftingHandler::getFirstCraftingOutputSlotForGui(platform::inventory::SlotClicker& gui) {
    if (!isCraftingGui(gui.menu().screenName())) return nullptr;
    for (auto const& slot : gui.menu().slots()) {
        if (getCraftingGridSlots(gui, slot)) return &slot;
    }
    return nullptr;
}

}
