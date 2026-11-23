// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/recipes/RecipeStorage.h"

#include <filesystem>
#include <fstream>

#include "ll/api/mod/NativeMod.h"

#include "itemscroller/config/Configs.h"
#include "platform/item/ScopedItemRegistry.h"

namespace inventory_utils::itemscroller::recipes {

namespace {

std::string sanitize(std::string const& s) {
    std::string out;
    for (char c : s) {
        bool bad = c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || static_cast<unsigned char>(c) < 32;
        out += bad ? '_' : c;
    }
    return out.empty() ? std::string("unknown") : out;
}

}

RecipeStorage& RecipeStorage::getInstance() {
    static RecipeStorage instance;
    return instance;
}

void RecipeStorage::reset(bool isLogout) {
    if (isLogout) {
        for (int i = 0; i < getTotalRecipeCount(); ++i) clearRecipe(i);
    }
}

void RecipeStorage::changeSelectedRecipe(int index) {
    if (index >= 0 && index < getTotalRecipeCount()) {
        mSelected = index;
        mDirty    = true;
    }
}

void RecipeStorage::scrollSelection(bool forward) { changeSelectedRecipe(mSelected + (forward ? 1 : -1)); }

RecipePattern& RecipeStorage::getRecipe(int index) {
    if (index >= 0 && index < getTotalRecipeCount()) return mRecipes[static_cast<size_t>(index)];
    return mRecipes[0];
}

void RecipeStorage::storeCraftingRecipeToCurrentSelection(platform::inventory::MenuSlot const& slot, platform::inventory::SlotClicker& gui, bool clearIfEmpty, bool fromKeybind) {
    getSelectedRecipe().storeCraftingRecipe(slot, gui, clearIfEmpty, fromKeybind);
    mDirty = true;
}

void RecipeStorage::clearRecipe(int index) {
    getRecipe(index).clearRecipe();
    mDirty = true;
}

bool RecipeStorage::isEmpty() const {
    for (auto const& r : mRecipes) {
        if (!r.isEmpty()) return false;
    }
    return true;
}

std::string RecipeStorage::getFileName() const {
    if (!config::Generic::SCROLL_CRAFT_RECIPE_FILE_GLOBAL.getBooleanValue()) return "recipes_" + sanitize(mWorldName) + ".json";
    return "recipes.json";
}

void RecipeStorage::readFromDisk() {
    if (!config::Generic::SCROLL_CRAFT_STORE_RECIPES_TO_FILE.getBooleanValue()) return;
    auto path = ll::mod::NativeMod::current()->getConfigDir() / getFileName();
    if (!std::filesystem::exists(path)) return;
    std::ifstream in(path, std::ios::binary);
    auto          data = nlohmann::json::parse(in, nullptr, false);
    if (!data.is_object() || !data.contains("Recipes") || !data["Recipes"].is_array()) return;
    platform::item::ScopedItemRegistry registry;
    for (auto& r : mRecipes) r.clearRecipe();
    for (auto const& tag : data["Recipes"]) {
        int index = tag.value("RecipeIndex", -1);
        if (index >= 0 && index < getTotalRecipeCount()) mRecipes[static_cast<size_t>(index)].readFromData(tag);
    }
    changeSelectedRecipe(data.value("Selected", 0));
    mDirty = false;
}

void RecipeStorage::writeToDisk() {
    if (!mDirty || !config::Generic::SCROLL_CRAFT_STORE_RECIPES_TO_FILE.getBooleanValue()) return;
    platform::item::ScopedItemRegistry registry;
    nlohmann::json                     recipes = nlohmann::json::array();
    for (int i = 0; i < getTotalRecipeCount(); ++i) {
        auto const& r = mRecipes[static_cast<size_t>(i)];
        if (!r.isValid()) continue;
        auto tag           = r.writeToData();
        tag["RecipeIndex"] = i;
        recipes.push_back(tag);
    }
    nlohmann::json data{{"Recipes", recipes}, {"Selected", mSelected}};
    auto           dir = ll::mod::NativeMod::current()->getConfigDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::ofstream out(dir / getFileName(), std::ios::binary | std::ios::trunc);
    out << data.dump(2);
    mDirty = false;
}

}
