// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <optional>
#include <string>

#include "platform/inventory/SlotClicker.h"

namespace inventory_utils::itemscroller::recipes {

class CraftingHandler {
public:
    struct SlotRange {
        int first = 0;
        int last  = -1;
        SlotRange(int start, int numSlots) : first(start), last(start + numSlots - 1) {}
        [[nodiscard]] int  getFirst() const { return first; }
        [[nodiscard]] int  getLast() const { return last; }
        [[nodiscard]] int  getSlotCount() const { return last - first + 1; }
        [[nodiscard]] bool contains(int slot) const { return slot >= first && slot <= last; }
    };

    static void clearDefinitions();
    static void addCraftingGridDefinition(std::string const& screenName, int outputSlot, SlotRange range);
    static bool isCraftingGui(std::string const& screenName);

    static std::optional<SlotRange> getCraftingGridSlots(platform::inventory::SlotClicker& gui, platform::inventory::MenuSlot const& slot);
    static platform::inventory::MenuSlot const* getFirstCraftingOutputSlotForGui(platform::inventory::SlotClicker& gui);
};

}
