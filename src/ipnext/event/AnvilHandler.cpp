// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/event/AnvilHandler.h"

#include <algorithm>

#include "mc/client/gui/screens/controllers/AnvilScreenController.h"
#include "mc/safety/RedactableString.h"
#include "mc/world/containers/managers/controllers/AnvilContainerManagerController.h"
#include "mc/world/item/ItemStack.h"

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/event/LockSlotsHandler.h"
#include "ipnext/inventory/ContainerClicker.h"
#include "ipnext/inventory/ContainerTypes.h"
#include "ipnext/inventory/GeneralInventoryActions.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::ipnext::event {

namespace {

using config::GuiSettings::FAST_RENAME_SAVED_VALUE;

bool isAnvilScreen() {
    return platform::screen::ScreenTracker::containerScreenName() == "anvil_screen" && platform::screen::ScreenTracker::containerScreen();
}

AnvilContainerManagerController* anvilManager() {
    if (!isAnvilScreen()) return nullptr;
    auto* screen = static_cast<AnvilScreenController*>(platform::screen::ScreenTracker::containerScreen());
    auto&& mgr = screen->mAnvilContainerManagerController.get();
    return mgr ? &*mgr : nullptr;
}

}

AnvilHandler& AnvilHandler::get() {
    static AnvilHandler instance;
    return instance;
}

std::string AnvilHandler::currentName() const {
    auto* mgr = anvilManager();
    return mgr ? mgr->getItemName().mUnredactedString : std::string();
}

AnvilHandler::Snapshot AnvilHandler::snapshot() const {
    Snapshot s;
    auto     v = inventory::VanillaContainer::current();
    if (!v || !isAnvilScreen()) return s;
    for (auto const& slot : v->slots()) {
        auto const& stack = v->itemOf(slot);
        bool        empty = stack.isNull() || stack.mCount == 0;
        if (slot.collection == "anvil_input_items" && !empty) s.inputs.emplace_back(slot.slotNumber, item::ItemType::of(stack));
        if (slot.collection == "anvil_result_items" && !empty) s.hasResult = true;
    }
    s.name = currentName();
    return s;
}

void AnvilHandler::onClientTick() {
    if (!isAnvilScreen()) {
        mScreen = nullptr;
        mStage  = 0;
        return;
    }
    void* screen = platform::screen::ScreenTracker::containerScreen();
    if (screen != mScreen) {
        mScreen       = screen;
        auto const& v = config::GuiSettings::FAST_RENAME_CHECKBOX_VALUE.getOptionListValue();
        if (v == "CHECKED") FAST_RENAME_SAVED_VALUE.setBooleanValue(true);
        else if (v == "UNCHECKED") FAST_RENAME_SAVED_VALUE.setBooleanValue(false);
        mStage = 0;
        mLast  = snapshot();
        return;
    }
    if (!FAST_RENAME_SAVED_VALUE.getBooleanValue()) {
        mStage = 0;
        mLast  = snapshot();
        return;
    }
    if (mWait > 0) {
        --mWait;
        return;
    }
    try {
        if (mStage == 0) {
            auto cur = snapshot();
            bool firstWasFilled = !mLast.inputs.empty();
            int  firstSlot      = firstWasFilled ? mLast.inputs.front().first : -1;
            bool firstNowEmpty  = firstWasFilled && std::none_of(cur.inputs.begin(), cur.inputs.end(), [&](auto const& p) { return p.first == firstSlot; });
            if (mLast.hasResult && firstNowEmpty) {
                mPending  = mLast;
                mLastText = mLast.name;
                mStage    = 1;
                mWait     = 2;
            }
            mLast = cur;
        } else if (mStage == 1) {
            restore();
            mStage = 2;
            mWait  = 2;
        } else {
            if (auto* mgr = anvilManager()) {
                Bedrock::Safety::RedactableString name(mLastText);
                mgr->setPreviewItemName(name);
                mgr->setItemName(name);
                mgr->updatePreviewItem();
            }
            ++mRestoreCount;
            mStage = 0;
            mLast  = snapshot();
        }
    } catch (std::exception const& e) {
        Log::warn("anvil: {}", e.what());
        mStage = 0;
    }
}

void AnvilHandler::restore() {
    inventory::GeneralInventoryActions::cleanCursor();
    auto v = inventory::VanillaContainer::current();
    if (!v) return;
    for (auto const& [inputSlot, type] : mPending.inputs) {
        auto const* dst = v->clicker().menu().getSlot(inputSlot);
        if (!dst) continue;
        auto const& now = v->itemOf(*dst);
        if (!now.isNull() && now.mCount > 0) continue;
        for (auto const& s : v->slots()) {
            if (s.kind == platform::inventory::InventoryKind::Container) continue;
            if (LockSlotsHandler::get().isMappedSlotLocked(s)) continue;
            auto const& stack = v->itemOf(s);
            if (stack.isNull() || stack.mCount == 0 || !(item::ItemType::of(stack) == type)) continue;
            inventory::ContainerClicker::click(s.slotNumber, 0);
            inventory::ContainerClicker::click(inputSlot, 0);
            inventory::GeneralInventoryActions::cleanCursor();
            break;
        }
    }
}

}
