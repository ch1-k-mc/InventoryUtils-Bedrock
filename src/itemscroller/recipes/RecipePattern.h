// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <vector>

#include "nlohmann/json.hpp"

#include "mc/world/item/ItemStack.h"
#include "platform/inventory/SlotClicker.h"

namespace inventory_utils::itemscroller::recipes {

class RecipePattern {
public:
    RecipePattern() { ensureRecipeSizeAndClearRecipe(9); }

    void ensureRecipeSize(int size);
    void clearRecipe();
    void ensureRecipeSizeAndClearRecipe(int size);

    void storeCraftingRecipe(platform::inventory::MenuSlot const& slot, platform::inventory::SlotClicker& gui, bool clearIfEmpty, bool fromKeybind);

    [[nodiscard]] ItemStack const&              getResult() const { return mResult; }
    [[nodiscard]] int                           getRecipeLength() const { return static_cast<int>(mRecipe.size()); }
    [[nodiscard]] std::vector<ItemStack> const& getRecipeItems() const { return mRecipe; }
    [[nodiscard]] bool                          isEmpty() const;
    [[nodiscard]] int                           countRecipeItems() const;
    [[nodiscard]] bool                          isValid() const;

    [[nodiscard]] nlohmann::json writeToData() const;
    void                         readFromData(nlohmann::json const& data);

private:
    ItemStack              mResult;
    std::vector<ItemStack> mRecipe;
    long long              mRecipeSaveTime = -1;
};

}
