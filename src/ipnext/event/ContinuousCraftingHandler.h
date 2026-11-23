// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <vector>

#include "ipnext/item/ItemStack.h"

namespace inventory_utils::ipnext::event {

class ContinuousCraftingHandler {
public:
    static ContinuousCraftingHandler& get();

    void onClientTick();

    [[nodiscard]] int refillCount() const { return mRefillCount; }

private:
    void init();
    void handle();
    void onCrafted();
    bool autoRefill();
    void save();

    void*                        mTrackingScreen = nullptr;
    bool                         mIsCrafting     = false;
    std::vector<int>             mIngredientSlots;
    std::vector<item::ItemStack> mStored;
    int                          mOnCraftCount   = 0;
    bool                         mCrafted        = false;
    bool                         mAfterRefill    = false;
    int                          mAutoRefillRetry = 0;
    int                          mSubmitNextCraft = 0;
    int                          mRefillCount     = 0;
    item::ItemStack              mOutput;
    int                          mOutputHeld      = 0;
};

}
