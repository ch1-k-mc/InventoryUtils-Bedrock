// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/recipes/RecipePattern.h"

#include <chrono>

#include "itemscroller/recipes/CraftingHandler.h"
#include "itemscroller/util/InventoryUtils.h"
#include "platform/item/ItemSerializer.h"

namespace inventory_utils::itemscroller::recipes {

namespace {
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
bool empty(ItemStack const& s) { return util::InventoryUtils::isStackEmpty(s); }
}

void RecipePattern::ensureRecipeSize(int size) {
    if (getRecipeLength() != size) mRecipe.assign(static_cast<size_t>(size), ItemStack());
}

void RecipePattern::clearRecipe() {
    for (auto& s : mRecipe) s = ItemStack();
    mResult         = ItemStack();
    mRecipeSaveTime = -1;
}

void RecipePattern::ensureRecipeSizeAndClearRecipe(int size) {
    ensureRecipeSize(size);
    clearRecipe();
}

void RecipePattern::storeCraftingRecipe(platform::inventory::MenuSlot const& slot, platform::inventory::SlotClicker& gui, bool clearIfEmpty, bool fromKeybind) {
    auto range = CraftingHandler::getCraftingGridSlots(gui, slot);
    if (!range) return;
    auto const& out = gui.menu().getItem(slot);
    if (!empty(out) && isEmpty()) {
        int gridSize = range->getSlotCount();
        int numSlots = static_cast<int>(gui.menu().slots().size());
        ensureRecipeSizeAndClearRecipe(gridSize);
        for (int i = 0, s = range->getFirst(); i < gridSize && s < numSlots; ++i, ++s) {
            auto const& item                = gui.menu().getItem(*gui.menu().getSlot(s));
            mRecipe[static_cast<size_t>(i)] = empty(item) ? ItemStack() : ItemStack(item);
        }
        mResult         = ItemStack(out);
        mRecipeSaveTime = nowMs();
        if (empty(mResult)) clearRecipe();
        util::InventoryUtils::clearFirstCraftingGridOfAllItems(gui);
    } else if (fromKeybind && clearIfEmpty) {
        clearRecipe();
    }
}

bool RecipePattern::isEmpty() const {
    bool allEmpty = true;
    for (auto const& s : mRecipe) {
        if (!empty(s)) allEmpty = false;
    }
    return allEmpty || empty(mResult);
}

int RecipePattern::countRecipeItems() const {
    int n = 0;
    for (auto const& s : mRecipe) {
        if (!empty(s)) ++n;
    }
    return n;
}

bool RecipePattern::isValid() const { return !empty(mResult); }

nlohmann::json RecipePattern::writeToData() const {
    nlohmann::json ingredients = nlohmann::json::array();
    for (auto const& s : mRecipe) ingredients.push_back(platform::item::toSnbt(s));
    return {
        {"Result",      platform::item::toSnbt(mResult)},
        {"Ingredients", ingredients                     },
        {"Length",      getRecipeLength()               },
    };
}

void RecipePattern::readFromData(nlohmann::json const& data) {
    int length = data.value("Length", 9);
    ensureRecipeSizeAndClearRecipe(length);
    if (data.contains("Ingredients") && data["Ingredients"].is_array()) {
        auto const& arr = data["Ingredients"];
        for (int i = 0; i < length && i < static_cast<int>(arr.size()); ++i) {
            if (arr[static_cast<size_t>(i)].is_string()) mRecipe[static_cast<size_t>(i)] = platform::item::fromSnbt(arr[static_cast<size_t>(i)].get<std::string>());
        }
    }
    mResult = platform::item::fromSnbt(data.value("Result", std::string()));
}

}
