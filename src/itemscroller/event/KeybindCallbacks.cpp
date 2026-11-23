// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/event/KeybindCallbacks.h"

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"

#include "itemscroller/config/Configs.h"
#include "itemscroller/config/Hotkeys.h"
#include "itemscroller/util/InputUtils.h"
#include "itemscroller/recipes/CraftingHandler.h"
#include "itemscroller/recipes/RecipeStorage.h"
#include "itemscroller/util/InventoryUtils.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/screen/ScreenTracker.h"
#include "platform/util/Messages.h"

namespace inventory_utils::itemscroller::event {

namespace {

using malilib::hotkeys::KeyAction;
using malilib::hotkeys::KeybindMulti;
using util::InputUtils;
using util::InventoryUtils;
using util::MoveAction;
namespace Cfg = config;
namespace H   = config::Hotkeys;

void showToggle(char const* name, bool on) {
    platform::util::chat(std::string("Item Scroller: ") + name + " " + (on ? "\xC2\xA7" "aON" : "\xC2\xA7" "cOFF") + "\xC2\xA7" "r");
}

}

KeybindCallbacks& KeybindCallbacks::getInstance() {
    static KeybindCallbacks instance;
    return instance;
}

void KeybindCallbacks::setCallbacks() {
    for (auto* hotkey : H::HOTKEY_LIST()) {
        hotkey->getKeybind().setCallback([this](KeyAction action, KeybindMulti& key) { return onKeyAction(action, key); });
    }
    H::MASS_CRAFT_TOGGLE.getKeybind().setCallback([](KeyAction, KeybindMulti&) {
        Cfg::Generic::MASS_CRAFT_HOLD.toggleBooleanValue();
        showToggle("massCraftHold", Cfg::Generic::MASS_CRAFT_HOLD.getBooleanValue());
        Cfg::saveToFile();
        return true;
    });
}

void KeybindCallbacks::clearCallbacks() {
    for (auto* hotkey : H::HOTKEY_LIST()) hotkey->getKeybind().setCallback({});
}

bool KeybindCallbacks::functionalityEnabled() const { return Cfg::Generic::MOD_MAIN_TOGGLE.getBooleanValue(); }

bool KeybindCallbacks::onKeyAction(KeyAction, KeybindMulti& key) {
    auto ci = ll::service::getClientInstance();
    if (!ci || !ci->getLocalPlayer()) return false;

    if (&key == &H::TOGGLE_MOD_ON_OFF.getKeybind()) {
        Cfg::Generic::MOD_MAIN_TOGGLE.toggleBooleanValue();
        platform::util::chat(functionalityEnabled() ? "Item Scroller: \xC2\xA7" "aON\xC2\xA7" "r" : "Item Scroller: \xC2\xA7" "cOFF\xC2\xA7" "r");
        Cfg::saveToFile();
        return true;
    }

    if (!functionalityEnabled() || !platform::screen::ScreenTracker::containerScreen()
        || Cfg::GUI_BLACKLIST.contains(platform::screen::ScreenTracker::containerScreenName())) {
        return false;
    }
    if (ci->getLocalPlayer()->isCreative() && !Cfg::Generic::ENABLE_IN_CREATIVE.getBooleanValue()) return false;

    auto gui = platform::inventory::SlotClicker::current();
    if (!gui.valid()) return false;
    auto const* slot       = gui.getSlotUnderMouse();
    MoveAction  moveAction = InputUtils::getDragMoveAction(key);

    if (slot) {
        if (moveAction != MoveAction::NONE) return InventoryUtils::dragMoveItems(gui, moveAction, slot, true);
        if (&key == &H::KEY_MOVE_EVERYTHING.getKeybind()) {
            InventoryUtils::tryMoveStacks(*slot, gui, false, true, false);
            return true;
        }
        if (&key == &H::DROP_ALL_MATCHING.getKeybind()) {
            auto const& item = gui.menu().getItem(*slot);
            if (Cfg::Toggles::DROP_MATCHING.getBooleanValue() && !InventoryUtils::isStackEmpty(item)) {
                ItemStack ref(item);
                InventoryUtils::dropStacks(gui, ref, slot, true);
                return true;
            }
        }
    }

    if (&key == &H::SLOT_DEBUG.getKeybind()) {
        if (slot) {
            auto const& item = gui.menu().getItem(*slot);
            platform::util::chat(
                "slot: slotNumber: " + std::to_string(slot->slotNumber) + ", collection: " + slot->collection + "[" + std::to_string(slot->index)
                + "], containerSlot: " + std::to_string(slot->containerSlot) + ", stack: "
                + (InventoryUtils::isStackEmpty(item) ? std::string("<empty>") : item.mItem->mFullName->getString() + " x" + std::to_string(item.mCount))
                + ", numSlots: " + std::to_string(gui.menu().slots().size())
            );
        } else {
            platform::util::chat("GUI: " + gui.menu().screenName());
        }
        return true;
    }
    auto& recipes = recipes::RecipeStorage::getInstance();
    if (&key == &H::CRAFT_EVERYTHING.getKeybind()) {
        InventoryUtils::craftEverythingPossibleWithCurrentRecipe(recipes.getSelectedRecipe(), gui);
        return true;
    }
    if (&key == &H::THROW_CRAFT_RESULTS.getKeybind()) {
        InventoryUtils::throwAllCraftingResultsToGround(recipes.getSelectedRecipe(), gui);
        return true;
    }
    if (&key == &H::MOVE_CRAFT_RESULTS.getKeybind()) {
        InventoryUtils::moveAllCraftingResultsToOtherInventory(recipes.getSelectedRecipe(), gui);
        return true;
    }
    if (&key == &H::STORE_RECIPE.getKeybind()) {
        if (InputUtils::isRecipeViewOpen() && InventoryUtils::isCraftingSlot(gui, slot)) {
            recipes.storeCraftingRecipeToCurrentSelection(*slot, gui, true, true);
            return true;
        }
    }
    return false;
}

void KeybindCallbacks::onClientTick() {
    if (!functionalityEnabled()) return;
    auto ci = ll::service::getClientInstance();
    if (!ci || !ci->getLocalPlayer() || (ci->getLocalPlayer()->isCreative() && !Cfg::Generic::ENABLE_IN_CREATIVE.getBooleanValue())) return;
    if (!platform::screen::ScreenTracker::containerScreen() || Cfg::GUI_BLACKLIST.contains(platform::screen::ScreenTracker::containerScreenName())) {
        mMassCraftTicker = 0;
        return;
    }
    if (!(H::MASS_CRAFT.getKeybind().isKeybindHeld() || Cfg::Generic::MASS_CRAFT_HOLD.getBooleanValue())) return;
    if (++mMassCraftTicker < Cfg::Generic::MASS_CRAFT_INTERVAL.getIntegerValue()) return;
    mMassCraftTicker = 0;

    platform::item::ScopedItemRegistry registry;
    auto                               gui = platform::inventory::SlotClicker::current();
    if (!gui.valid()) return;
    auto const* outputSlot = recipes::CraftingHandler::getFirstCraftingOutputSlotForGui(gui);
    if (!outputSlot) return;
    auto  range  = recipes::CraftingHandler::getCraftingGridSlots(gui, *outputSlot);
    auto& recipe = recipes::RecipeStorage::getInstance().getSelectedRecipe();
    int   limit  = Cfg::Generic::MASS_CRAFT_ITERATIONS.getIntegerValue();
    if (InventoryUtils::isStackEmpty(recipe.getResult()) || !range) return;
    if (range->getSlotCount() < recipe.countRecipeItems()) return;

    if (Cfg::Generic::MASS_CRAFT_SWAPS.getBooleanValue()) {
        for (int i = 0; i < limit; ++i) {
            InventoryUtils::tryClearCursor(gui);
            InventoryUtils::throwAllCraftingResultsToGround(recipe, gui);
            InventoryUtils::throwAllNonRecipeItemsToGround(recipe, gui);
            InventoryUtils::setCraftingGridContentsUsingSwaps(gui, recipe, *outputSlot);
            if (!InventoryUtils::areStacksEqual(gui.menu().getItem(*outputSlot), recipe.getResult())) break;
            gui.shiftClickSlot(outputSlot->slotNumber);
        }
    } else {
        int failsafe = 0;
        while (++failsafe < limit) {
            InventoryUtils::tryClearCursor(gui);
            InventoryUtils::throwAllCraftingResultsToGround(recipe, gui);
            InventoryUtils::throwAllNonRecipeItemsToGround(recipe, gui);
            InventoryUtils::tryMoveItemsToFirstCraftingGrid(recipe, gui, true);
            if (!InventoryUtils::areStacksEqual(gui.menu().getItem(*outputSlot), recipe.getResult())) break;
            InventoryUtils::dropStacksWhileHasItem(gui, outputSlot->slotNumber, recipe.getResult());
        }
    }
}

}
