// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <string>
#include <vector>

#include "itemscroller/recipes/RecipePattern.h"

namespace inventory_utils::itemscroller::recipes {

class RecipeStorage {
public:
    static RecipeStorage& getInstance();

    void reset(bool isLogout);

    [[nodiscard]] int getSelection() const { return mSelected; }
    void              changeSelectedRecipe(int index);
    void              scrollSelection(bool forward);
    [[nodiscard]] int getFirstVisibleRecipeId() const { return getCurrentRecipePage() * getRecipeCountPerPage(); }
    [[nodiscard]] int getTotalRecipeCount() const { return static_cast<int>(mRecipes.size()); }
    [[nodiscard]] int getRecipeCountPerPage() const { return MAX_RECIPES; }
    [[nodiscard]] int getCurrentRecipePage() const { return getSelection() / getRecipeCountPerPage(); }

    RecipePattern& getRecipe(int index);
    RecipePattern& getSelectedRecipe() { return getRecipe(getSelection()); }

    void storeCraftingRecipeToCurrentSelection(platform::inventory::MenuSlot const& slot, platform::inventory::SlotClicker& gui, bool clearIfEmpty, bool fromKeybind);
    void clearRecipe(int index);

    void setWorldName(std::string name) { mWorldName = std::move(name); }
    void readFromDisk();
    void writeToDisk();

private:
    static constexpr int MAX_PAGES   = 8;
    static constexpr int MAX_RECIPES = 18;

    RecipeStorage() : mRecipes(MAX_RECIPES * MAX_PAGES) {}

    [[nodiscard]] bool        isEmpty() const;
    [[nodiscard]] std::string getFileName() const;

    std::vector<RecipePattern> mRecipes;
    int                        mSelected = 0;
    bool                       mDirty    = false;
    std::string                mWorldName;
};

}
