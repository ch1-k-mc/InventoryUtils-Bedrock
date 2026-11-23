// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <vector>

#include "ipnext/item/ItemType.h"
#include "platform/input/InputHandler.h"
#include "platform/screen/ScreenLayout.h"

class MinecraftUIRenderContext;
class ScreenView;

namespace inventory_utils::ipnext::event {

class StoneCutterCraftingHandler : public platform::input::IMouseInputHandler {
public:
    static StoneCutterCraftingHandler& get();

    void onClientTick();
    void onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx);
    bool onMouseClick(double mouseX, double mouseY, int buttonCode, bool buttonState) override;

    [[nodiscard]] int refillCount() const { return mRefillCount; }
    [[nodiscard]] int reselectCount() const { return mReselectCount; }
    [[nodiscard]] int selectedIndex() const { return mSelected; }

private:
    struct State {
        item::ItemType input;
        int            inputCount = 0;
        item::ItemType result;
        int            held = 0;
    };
    State read() const;

    enum Stage { WATCH = 0, AFTER_REFILL, MOVE, PRESS, RELEASE, RESTORE, AFTER_SELECT };

    void*                                   mScreen = nullptr;
    State                                   mLast;
    int                                     mStage = WATCH;
    int                                     mWait  = 0;
    int                                     mTries = 0;
    int                                     mReselects = 0;
    int                                     mGuardTicks = 0;
    item::ItemType                          mExpectedResult;
    std::vector<platform::screen::SlotRect> mButtons;
    int                                     mSelected = -1;
    double                                  mRestoreX = 0, mRestoreY = 0;
    int                                     mRefillCount   = 0;
    int                                     mReselectCount = 0;
};

}
