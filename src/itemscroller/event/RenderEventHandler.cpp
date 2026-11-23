// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/event/RenderEventHandler.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/world/item/ItemStack.h"

#include "itemscroller/config/Configs.h"
#include "itemscroller/recipes/RecipeStorage.h"
#include "itemscroller/util/InputUtils.h"
#include "itemscroller/util/InventoryUtils.h"
#include "platform/input/InputHandler.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/render/Gui2d.h"
#include "platform/screen/ScreenLayout.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::itemscroller::event {

namespace {

using platform::render::Gui2d;
using recipes::RecipePattern;
using recipes::RecipeStorage;
using util::InputUtils;
using util::InventoryUtils;

constexpr int kStackBase = 16;

void renderStackAt(Gui2d& g, ItemStack const& stack, int x, int y, double scale, bool border) {
    int w = static_cast<int>(kStackBase * scale);
    if (border) g.outline(x - 1, y - 1, w + 2, w + 2, 0xFFFFFFFF);
    g.fill(x, y, x + w, y + w, 0x20FFFFFF);
    if (!InventoryUtils::isStackEmpty(stack)) g.renderItem(stack, x, y, static_cast<float>(scale));
}

void renderTooltip(Gui2d& g, ItemStack const& stack, int mouseX, int mouseY) {
    if (InventoryUtils::isStackEmpty(stack)) return;
    std::string name = stack.getName();
    int         w    = g.stringWidth(name) + 6;
    int         x    = mouseX + 12;
    int         y    = mouseY - 12;
    g.fill(x - 3, y - 3, x + w, y + Gui2d::LINE_HEIGHT + 3, 0xF0100010);
    g.outline(x - 3, y - 3, w + 3, Gui2d::LINE_HEIGHT + 6, 0x505000FF);
    g.drawString(name, x, y, 0xFFFFFFFF, true);
}

}

RenderEventHandler& RenderEventHandler::instance() {
    static RenderEventHandler instance;
    return instance;
}

bool RenderEventHandler::calculateRecipePositions(ScreenView& view, int screenWidth, int screenHeight) {
    auto&     recipes       = RecipeStorage::getInstance();
    int const gapHorizontal = 2;
    int const gapVertical   = 2;
    mRecipesPerColumn       = 9;
    mColumns                = static_cast<int>(std::ceil(static_cast<double>(recipes.getRecipeCountPerPage()) / mRecipesPerColumn));
    mNumberTextWidth        = 12;
    mGapColumn              = 4;
    auto area = platform::screen::getGuiArea(view, platform::screen::ScreenTracker::containerScreenName(), screenWidth, screenHeight);
    if (!area) return false;
    int usableHeight = screenHeight;
    int usableWidth  = area->left;
    double gapScaleVertical             = 1.0 - static_cast<double>(gapVertical) / (kStackBase + gapVertical);
    int    maxStackDimensionsVertical   = static_cast<int>((usableHeight / (mRecipesPerColumn + 1.2)) * gapScaleVertical);
    double gapScaleHorizontal           = 1.0 - static_cast<double>(gapHorizontal) / (kStackBase + gapHorizontal);
    int    maxStackDimensionsHorizontal = static_cast<int>(((usableWidth - (mColumns * (mNumberTextWidth + mGapColumn))) / (mColumns + 3 + 0.8)) * gapScaleHorizontal);
    int    stackDimensions              = std::min(maxStackDimensionsVertical, maxStackDimensionsHorizontal);
    if (stackDimensions <= 0) return false;
    mScale       = static_cast<double>(stackDimensions) / kStackBase;
    mEntryHeight = kStackBase + gapVertical;
    mRecipeListX = usableWidth - static_cast<int>((mColumns * (kStackBase + mNumberTextWidth + mGapColumn) + gapHorizontal) * mScale);
    mRecipeListY = static_cast<int>(mEntryHeight * mScale);
    mColumnWidth = kStackBase + mNumberTextWidth + mGapColumn;
    return true;
}

void RenderEventHandler::onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx) {
    mValid = false;
    if (!InputUtils::isRecipeViewOpen()) return;
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    platform::item::ScopedItemRegistry registry;
    Gui2d                              g(ctx, *ci);
    if (!calculateRecipePositions(view, g.screenWidth(), g.screenHeight())) return;
    mValid = true;

    auto&     recipes      = RecipeStorage::getInstance();
    int const first        = recipes.getFirstVisibleRecipeId();
    int const countPerPage = recipes.getRecipeCountPerPage();
    int const lastOnPage   = first + countPerPage - 1;
    auto      tx           = [&](double x) { return mRecipeListX + static_cast<int>(x * mScale); };
    auto      ty           = [&](double y) { return mRecipeListY + static_cast<int>(y * mScale); };

    std::string page = "Page " + std::to_string(first / countPerPage + 1) + " / " + std::to_string(recipes.getTotalRecipeCount() / countPerPage);
    g.drawString(page, tx(16), ty(-12), 0xC0C0C0C0, false, static_cast<float>(mScale));

    for (int i = 0, recipeId = first; recipeId <= lastOnPage; ++i, ++recipeId) {
        auto const& stack    = recipes.getRecipe(recipeId).getResult();
        bool        selected = recipeId == recipes.getSelection();
        int         row      = i % mRecipesPerColumn;
        int         column   = i / mRecipesPerColumn;
        int         x        = column * mColumnWidth + mGapColumn + mNumberTextWidth;
        int         y        = row * mEntryHeight;
        renderStackAt(g, stack, tx(x), ty(y), mScale, selected);
        std::string idx    = std::to_string(recipeId + 1);
        float       tscale = 0.75f * static_cast<float>(mScale);
        int         nx     = tx(x) - g.stringWidth(idx, tscale) - static_cast<int>(2 * mScale);
        int         ny     = ty(row * mEntryHeight + mEntryHeight / 2.0 - Gui2d::LINE_HEIGHT / 2.0);
        g.drawString(idx, nx, ny, 0xFFC0C0C0, false, tscale);
    }

    int const hovered = getHoveredRecipeId();
    if (config::Generic::CRAFTING_RENDER_RECIPE_ITEMS.getBooleanValue()) {
        RecipePattern& recipe = hovered >= 0 ? recipes.getRecipe(hovered) : recipes.getSelectedRecipe();
        auto const&    items  = recipe.getRecipeItems();
        int const      dims   = static_cast<int>(std::ceil(std::sqrt(std::min(recipe.getRecipeLength(), 9))));
        int const      x      = -3 * 17 + 2;
        int const      y      = 3 * mEntryHeight;
        for (int i = 0, row = 0; row < dims; ++row) {
            for (int col = 0; col < dims && i < static_cast<int>(items.size()); ++col, ++i) {
                renderStackAt(g, items[static_cast<size_t>(i)], tx(x + col * 17), ty(y + row * 17), mScale, false);
            }
        }
    }
    if (hovered >= 0) {
        auto& in = platform::input::InputHandler::get();
        renderTooltip(g, recipes.getRecipe(hovered).getResult(), static_cast<int>(in.mouseX()), static_cast<int>(in.mouseY()));
    }
}

int RenderEventHandler::getHoveredRecipeId() const {
    if (!mValid || !InputUtils::isRecipeViewOpen()) return -1;
    auto&     in              = platform::input::InputHandler::get();
    int const mouseX          = static_cast<int>(in.mouseX());
    int const mouseY          = static_cast<int>(in.mouseY());
    int const stackDimensions = static_cast<int>(kStackBase * mScale);
    for (int column = 0; column < mColumns; ++column) {
        int startX = mRecipeListX + static_cast<int>((column * mColumnWidth + mGapColumn + mNumberTextWidth) * mScale);
        if (mouseX < startX || mouseX > startX + stackDimensions) continue;
        for (int row = 0; row < mRecipesPerColumn; ++row) {
            int startY = mRecipeListY + static_cast<int>(row * mEntryHeight * mScale);
            if (mouseY >= startY && mouseY <= startY + stackDimensions) {
                return RecipeStorage::getInstance().getFirstVisibleRecipeId() + column * mRecipesPerColumn + row;
            }
        }
    }
    return -1;
}

}
