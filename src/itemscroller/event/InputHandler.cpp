// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/event/InputHandler.h"

#include <Windows.h>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"

#include "itemscroller/config/Configs.h"
#include <algorithm>

#include "itemscroller/event/KeybindCallbacks.h"
#include "itemscroller/event/RenderEventHandler.h"
#include "itemscroller/recipes/RecipeStorage.h"
#include "itemscroller/util/InputUtils.h"
#include "itemscroller/util/InventoryUtils.h"
#include "platform/input/KeyCodes.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::itemscroller::event {

namespace {

using util::InputUtils;
using util::InventoryUtils;
using util::MoveAction;
namespace Cfg = config;

LocalPlayer* player() {
    auto ci = ll::service::getClientInstance();
    return ci ? ci->getLocalPlayer() : nullptr;
}

bool isUsableContainerScreen() {
    auto& name = platform::screen::ScreenTracker::containerScreenName();
    if (!platform::screen::ScreenTracker::containerScreen() || Cfg::GUI_BLACKLIST.contains(name)) return false;
    auto* p = player();
    return p && (!p->isCreative() || Cfg::Generic::ENABLE_IN_CREATIVE.getBooleanValue());
}

bool isShiftDown() {
    auto& in = platform::input::InputHandler::get();
    return in.isKeyDown(VK_LSHIFT) || in.isKeyDown(VK_RSHIFT);
}

}

InputHandler& InputHandler::getInstance() {
    static InputHandler instance;
    return instance;
}

bool InputHandler::onKeyInput(int keyCode, bool keyState) {
    if (InputUtils::isRecipeViewOpen() && keyState) {
        auto& recipes        = recipes::RecipeStorage::getInstance();
        int   index          = -1;
        int   oldIndex       = recipes.getSelection();
        int   recipesPerPage = recipes.getRecipeCountPerPage();
        int   change         = isShiftDown() ? recipesPerPage : recipesPerPage / 2;
        if (keyCode >= VK_NUMPAD1 && keyCode <= VK_NUMPAD9) index = std::clamp(keyCode - VK_NUMPAD1, 0, 8);
        else if (keyCode == VK_UP && oldIndex > 0) index = oldIndex - 1;
        else if (keyCode == VK_DOWN && oldIndex < recipes.getTotalRecipeCount() - 1) index = oldIndex + 1;
        else if (keyCode == VK_LEFT && oldIndex >= change) index = oldIndex - change;
        else if (keyCode == VK_RIGHT && oldIndex < recipes.getTotalRecipeCount() - change) index = oldIndex + change;
        if (index >= 0) {
            recipes.changeSelectedRecipe(index);
            return true;
        }
    }
    return handleInput(keyCode, keyState, 0);
}

bool InputHandler::onMouseScroll(double, double, double amount) { return handleInput(platform::input::KeyCodes::UNKNOWN, false, amount); }

bool InputHandler::onMouseClick(double, double, int buttonCode, bool buttonState) { return handleInput(buttonCode, buttonState, 0); }

bool InputHandler::handleInput(int keyCode, bool keyState, double dWheel) {
    if (!player()) return false;

    MoveAction action = InventoryUtils::getActiveMoveAction();
    if (action != MoveAction::NONE && !InputUtils::isActionKeyActive(action)) InventoryUtils::stopDragging();

    bool cancel = false;
    if (!KeybindCallbacks::getInstance().functionalityEnabled() || !isUsableContainerScreen()) return false;

    auto gui = platform::inventory::SlotClicker::current();
    if (!gui.valid()) return false;

    bool const isAttack          = InputUtils::isAttack(keyCode);
    bool const isUse             = InputUtils::isUse(keyCode);
    bool const isPickBlock       = InputUtils::isPickBlock(keyCode);
    bool const isAttackUseOrPick = isAttack || isUse || isPickBlock;

    auto& recipes = recipes::RecipeStorage::getInstance();
    if (dWheel != 0) {
        if (InputUtils::isRecipeViewOpen()) {
            recipes.scrollSelection(dWheel < 0);
            cancel = true;
        } else {
            cancel = InventoryUtils::tryMoveItems(gui, gui.getSlotUnderMouse(), dWheel > 0);
        }
    } else {
        auto const* slot  = gui.getSlotUnderMouse();
        bool const  shift = isShiftDown();
        if (keyState && isAttackUseOrPick) {
            int hoveredRecipeId = RenderEventHandler::instance().getHoveredRecipeId();
            if (hoveredRecipeId >= 0) {
                bool dropKeyDown = platform::input::InputHandler::get().isKeyDown('Q');
                InventoryUtils::handleRecipeClick(gui, hoveredRecipeId, isAttack, isUse, isPickBlock, shift, dropKeyDown);
                return true;
            }
            if (isPickBlock && InputUtils::isRecipeViewOpen() && InventoryUtils::isCraftingSlot(gui, slot)) {
                recipes.storeCraftingRecipeToCurrentSelection(*slot, gui, true, false);
                return true;
            }
        }
        InventoryUtils::checkForItemPickup(gui);
        if (keyState && (isAttack || isUse)) InventoryUtils::storeSourceSlotCandidate(slot, gui);

        if (Cfg::Toggles::RIGHT_CLICK_CRAFT_STACK.getBooleanValue() && isUse && keyState && InventoryUtils::isCraftingSlot(gui, slot)) {
            InventoryUtils::rightClickCraftOneStack(gui);
        } else if (Cfg::Toggles::SHIFT_PLACE_ITEMS.getBooleanValue() && isAttack && shift && InventoryUtils::canShiftPlaceItems(gui, slot) && slot) {
            cancel |= InventoryUtils::shiftPlaceItems(*slot, gui);
        } else if (Cfg::Toggles::SHIFT_DROP_ITEMS.getBooleanValue() && isAttack && shift && !slot && !InventoryUtils::isStackEmpty(gui.menu().getCarried())) {
            cancel |= InventoryUtils::shiftDropItems(gui);
        }
    }
    return cancel;
}

void InputHandler::onMouseMove(double, double) {
}

void InputHandler::onContainerFrame() {
    MoveAction action = InventoryUtils::getActiveMoveAction();
    if (action == MoveAction::NONE) return;
    if (!KeybindCallbacks::getInstance().functionalityEnabled() || !isUsableContainerScreen()) return;
    platform::item::ScopedItemRegistry registry;
    auto                               gui = platform::inventory::SlotClicker::current();
    if (!gui.valid()) return;
    if (InputUtils::isActionKeyActive(action)) InventoryUtils::dragMoveItems(gui, action, gui.getSlotUnderMouse(), false);
    else InventoryUtils::stopDragging();
}

}
