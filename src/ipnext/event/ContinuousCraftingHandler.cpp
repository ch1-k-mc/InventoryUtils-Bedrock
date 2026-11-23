// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/event/ContinuousCraftingHandler.h"

#include <algorithm>
#include <map>

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/inventory/AdvancedContainer.h"
#include "ipnext/inventory/ContainerClicker.h"
#include "platform/input/InputHandler.h"
#include "platform/inventory/SlotClicker.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/screen/ScreenTracker.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace inventory_utils::ipnext::event {

namespace {

using namespace config;
using item::ItemStack;
using item::ItemType;

bool method2() { return ModSettings::CONTINUOUS_CRAFTING_METHOD.getIntegerValue() == 2; }

ItemType keyOf(ItemType const& t) { return method2() ? t : t.copy(true); }

int heldCount(inventory::VanillaContainer const& v, ItemType const& type) {
    int n   = 0;
    auto c  = ItemStack::of(v.cursorStack());
    if (!c.isEmpty() && c.itemType == type) n += c.count;
    for (auto const& sl : v.slots()) {
        if (!inventory::isPlayerInventory(sl)) continue;
        auto s = ItemStack::of(v.itemOf(sl));
        if (!s.isEmpty() && s.itemType == type) n += s.count;
    }
    return n;
}

bool shiftDown() { return platform::input::InputHandler::get().isKeyDown(VK_LSHIFT) || platform::input::InputHandler::get().isKeyDown(VK_RSHIFT); }

}

ContinuousCraftingHandler& ContinuousCraftingHandler::get() {
    static ContinuousCraftingHandler instance;
    return instance;
}

void ContinuousCraftingHandler::onClientTick() {
    if (!GuiSettings::CONTINUOUS_CRAFTING_SAVED_VALUE.getBooleanValue()) return;
    auto* screen = platform::screen::ScreenTracker::containerScreen();
    if (!screen) {
        mTrackingScreen = nullptr;
        return;
    }
    platform::item::ScopedItemRegistry registry;
    if (screen != mTrackingScreen) {
        mTrackingScreen = screen;
        init();
    }
    handle();
}

void ContinuousCraftingHandler::init() {
    auto vanilla = inventory::VanillaContainer::current();
    mIsCrafting  = vanilla && inventory::ContainerTypes::getTypes(*vanilla).contains(inventory::ContainerType::CRAFTING);
    mIngredientSlots.clear();
    mStored.clear();
    mOnCraftCount = 0;
    mCrafted      = false;
    if (!mIsCrafting) return;
    for (auto const& s : vanilla->slots()) {
        if (s.collection == "crafting_input_items") mIngredientSlots.push_back(s.slotNumber);
    }
    save();
}

void ContinuousCraftingHandler::save() {
    auto vanilla = inventory::VanillaContainer::current();
    if (!vanilla) return;
    mStored.clear();
    for (int id : mIngredientSlots) mStored.push_back(ItemStack::of(vanilla->itemOf(vanilla->slots()[id])));
}

void ContinuousCraftingHandler::onCrafted() {
    mOnCraftCount = std::min(mOnCraftCount + 2, 2);
    mCrafted      = true;
}

void ContinuousCraftingHandler::handle() {
    if (!mIsCrafting) return;
    auto vanilla = inventory::VanillaContainer::current();
    if (!vanilla) return;
    if (!mCrafted) {
        int  decrease = -1;
        bool allSame  = true, any = false;
        for (size_t i = 0; i < mIngredientSlots.size() && i < mStored.size(); ++i) {
            if (mStored[i].isEmpty()) continue;
            auto now = ItemStack::of(vanilla->itemOf(vanilla->slots()[mIngredientSlots[i]]));
            int  n   = now.isEmpty() ? 0 : (now.itemType == mStored[i].itemType ? now.count : -1);
            if (n < 0) {
                allSame = false;
                break;
            }
            int d = mStored[i].count - n;
            if (decrease < 0) decrease = d;
            if (d != decrease || d <= 0) allSame = false;
            any = true;
        }
        if (any && allSame && decrease > 0 && !mOutput.isEmpty() && heldCount(*vanilla, mOutput.itemType) > mOutputHeld) onCrafted();
    }
    if (mOnCraftCount > 0) --mOnCraftCount;
    if (mSubmitNextCraft > 0) {
        if (--mSubmitNextCraft <= 0) {
            mSubmitNextCraft = 0;
            if (auto const* out = [&]() -> platform::inventory::MenuSlot const* {
                    for (auto const& s : vanilla->slots()) {
                        if (s.collection == "crafting_output_items") return &s;
                    }
                    return nullptr;
                }()) {
                inventory::ContainerClicker::shiftClick(out->slotNumber);
            }
        }
    }
    if (mAfterRefill) {
        if (shiftDown() && isPressing(ModSettings::INCLUDE_HOTBAR_MODIFIER)) mSubmitNextCraft = ModSettings::AUTO_CRAFT_DELAY.getIntegerValue();
        mAfterRefill = false;
    }
    bool cursorBusy = !ItemStack::of(vanilla->cursorStack()).isEmpty();
    if (mCrafted && mOnCraftCount <= 0 && cursorBusy) {
    } else if (mCrafted && mOnCraftCount <= 0) {
        if (autoRefill()) {
            mCrafted         = false;
            mOnCraftCount    = 0;
            mAfterRefill     = true;
            mAutoRefillRetry = 0;
            ++mRefillCount;
        } else if (mAutoRefillRetry < 6) {
            ++mAutoRefillRetry;
        } else {
            mAutoRefillRetry = 0;
            mCrafted         = false;
            mOnCraftCount    = 0;
            mAfterRefill     = false;
        }
    }
    if (!mCrafted) save();
    mOutput = ItemStack();
    for (auto const& sl : vanilla->slots()) {
        if (sl.collection == "crafting_output_items") mOutput = ItemStack::of(vanilla->itemOf(sl));
    }
    mOutputHeld = mOutput.isEmpty() ? 0 : heldCount(*vanilla, mOutput.itemType);
}

bool ContinuousCraftingHandler::autoRefill() {
    auto vanilla = inventory::VanillaContainer::current();
    if (!vanilla) return false;
    std::vector<std::pair<ItemType, std::vector<int>>> typeToSlots;
    for (size_t i = 0; i < mIngredientSlots.size() && i < mStored.size(); ++i) {
        auto const& stored = mStored[i];
        if (stored.isEmpty()) continue;
        auto now = ItemStack::of(vanilla->itemOf(vanilla->slots()[mIngredientSlots[i]]));
        if (!now.isEmpty()) continue;
        auto key = keyOf(stored.itemType);
        auto it  = std::find_if(typeToSlots.begin(), typeToSlots.end(), [&](auto const& e) { return key == e.first; });
        if (it == typeToSlots.end()) typeToSlots.push_back({key, {mIngredientSlots[i]}});
        else it->second.push_back(mIngredientSlots[i]);
    }
    if (typeToSlots.empty()) return false;
    inventory::AdvancedContainer::withTracker(
        [&](inventory::AdvancedContainer::TrackerDsl& dsl) {
            using namespace inventory::AreaTypes;
            auto playerArea = dsl.get(playerStorage() + playerHotbar() + playerOffhand() - lockedSlots());
            auto player     = dsl.asSubTracker(playerArea);
            auto slots      = player.slots();
            std::vector<std::pair<int, std::vector<item::ItemStack*>>> plan;
            for (auto const& [type, list] : typeToSlots) {
                int total = 0;
                for (auto* s : slots) {
                    if (!s->isEmpty() && keyOf(s->itemType) == type) total += s->count;
                }
                std::vector<item::ItemStack*> fed;
                for (int id : list) fed.push_back(&dsl.tracker.slots[id]);
                plan.emplace_back(std::min(total / static_cast<int>(list.size()), type.maxCount()), std::move(fed));
            }
            for (auto* source : slots) {
                for (size_t k = 0; k < typeToSlots.size(); ++k) {
                    if (source->isEmpty() || !(keyOf(source->itemType) == typeToSlots[k].first)) continue;
                    auto& [eachCount, fed] = plan[k];
                    for (auto* destination : fed) {
                        if (destination->count >= eachCount) continue;
                        if (source->isEmpty()) break;
                        source->transferNTo(*destination, eachCount - destination->count);
                    }
                }
            }
        },
        true,
        false
    );
    for (size_t i = 0; i < mIngredientSlots.size() && i < mStored.size(); ++i) {
        if (mStored[i].isEmpty()) continue;
        if (ItemStack::of(vanilla->itemOf(vanilla->slots()[mIngredientSlots[i]])).isEmpty()) return method2();
    }
    return true;
}

}
