// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

class ScreenView;
class MinecraftUIRenderContext;

namespace inventory_utils::itemscroller::event {

class RenderEventHandler {
public:
    static RenderEventHandler& instance();

    void onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx);
    [[nodiscard]] int getHoveredRecipeId() const;

private:
    bool calculateRecipePositions(ScreenView& view, int screenWidth, int screenHeight);

    int    mRecipeListX      = 0;
    int    mRecipeListY      = 0;
    int    mRecipesPerColumn = 9;
    int    mColumnWidth      = 0;
    int    mColumns          = 2;
    int    mNumberTextWidth  = 12;
    int    mGapColumn        = 4;
    int    mEntryHeight      = 18;
    double mScale            = 1.0;
    bool   mValid            = false;
};

}
