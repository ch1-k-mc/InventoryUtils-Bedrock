// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/util/InventoryUtils.h"

#include <algorithm>

#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"

#include "itemscroller/config/Configs.h"
#include "itemscroller/config/Hotkeys.h"
#include "itemscroller/util/InputUtils.h"

namespace inventory_utils::itemscroller::util {

namespace {

using platform::inventory::ClickType;
using platform::inventory::InventoryKind;
namespace Cfg = config;

std::string itemId(ItemStack const& s) { return s.isNull() || !s.mItem ? std::string() : s.mItem->mFullName->getString(); }

ItemStack copyStack(ItemStack const& s) { return ItemStack(s); }

}

bool InventoryUtils::isStackEmpty(ItemStack const& s) { return s.isNull() || s.mCount == 0; }

int InventoryUtils::getStackSize(ItemStack const& s) { return isStackEmpty(s) ? 0 : static_cast<int>(s.mCount); }

bool InventoryUtils::areStacksEqual(ItemStack const& a, ItemStack const& b) {
    if (isStackEmpty(a) || isStackEmpty(b)) return isStackEmpty(a) && isStackEmpty(b);
    if (a.mItem.get() != b.mItem.get() || a.mAuxValue != b.mAuxValue) return false;
    bool au = a.mUserData != nullptr, bu = b.mUserData != nullptr;
    if (au != bu) return false;
    return !au || a.mUserData->equals(*b.mUserData);
}

bool InventoryUtils::mayPlace(Slot const& slot, ItemStack const&) { return !slot.isOutput; }

int InventoryUtils::slotMaxStackSize(Slot const& slot, ItemStack const& stack) {
    int m = isStackEmpty(stack) ? 64 : static_cast<int>(stack.getMaxStackSize());
    if (slot.kind == InventoryKind::Armor) m = 1;
    return std::min(64, m);
}

bool InventoryUtils::isValidSlot(Slot const* slot, Gui& gui, bool requireItems) {
    return slot != nullptr && (!requireItems || !isStackEmpty(gui.menu().getItem(*slot))) && !Cfg::SLOT_BLACKLIST.contains(slot->collection);
}

bool InventoryUtils::isCraftingSlot(Gui& gui, Slot const* slot) {
    if (!slot) return false;
    auto const& screen = gui.menu().screenName();
    if (screen != "inventory_screen" && screen != "crafting_screen") return false;
    return slot->collection == "crafting_output_items";
}

bool InventoryUtils::inventoryExistsAbove(Slot const& slot, Gui& gui) {
    for (auto const& s : gui.menu().slots()) {
        if (s.y < slot.y && !areSlotsInSameInventory(slot, s)) return true;
    }
    return false;
}

bool InventoryUtils::areSlotsInSameInventory(Slot const& a, Slot const& b, bool treatHotbarAsDifferent) {
    if (a.inventoryId != b.inventoryId) return false;
    if (treatHotbarAsDifferent && a.inventoryId == 0) {
        int i1 = a.containerSlot, i2 = b.containerSlot;
        return i1 == 40 || i2 == 40 || (i1 < 9) == (i2 < 9);
    }
    return true;
}

bool InventoryUtils::canShiftPlaceItems(Gui& gui, Slot const* slot) {
    auto const& cursor = gui.menu().getCarried();
    return slot != nullptr && !isStackEmpty(cursor) && isValidSlot(slot, gui, false) && isStackEmpty(gui.menu().getItem(*slot)) && mayPlace(*slot, cursor);
}

bool InventoryUtils::tryMoveItems(Gui& gui, Slot const* slot, bool scrollingUp) {
    if (!slot || !isStackEmpty(gui.menu().getCarried())) return false;

    bool craftingHandling        = Cfg::Toggles::CRAFTING_FEATURES.getBooleanValue() && isCraftingSlot(gui, slot);
    bool keyActiveMoveEverything = Cfg::Hotkeys::MODIFIER_MOVE_EVERYTHING.getKeybind().isKeybindHeld();
    bool keyActiveMoveMatching   = Cfg::Hotkeys::MODIFIER_MOVE_MATCHING.getKeybind().isKeybindHeld();
    bool keyActiveMoveStacks     = Cfg::Hotkeys::MODIFIER_MOVE_STACK.getKeybind().isKeybindHeld();
    bool nonSingleMove           = keyActiveMoveEverything || keyActiveMoveMatching || keyActiveMoveStacks;
    bool moveToOtherInventory    = scrollingUp;

    if (Cfg::Generic::SLOT_POSITION_AWARE_SCROLL_DIRECTION.getBooleanValue()) {
        bool above           = inventoryExistsAbove(*slot, gui);
        moveToOtherInventory = (above == scrollingUp);
    }
    if ((Cfg::Generic::REVERSE_SCROLL_DIRECTION_SINGLE.getBooleanValue() && !nonSingleMove) || (Cfg::Generic::REVERSE_SCROLL_DIRECTION_STACKS.getBooleanValue() && nonSingleMove)) {
        moveToOtherInventory = !moveToOtherInventory;
    }
    if (!isValidSlot(slot, gui, !craftingHandling)) return false;

    if (craftingHandling) return tryMoveItemsCrafting(gui, *slot, moveToOtherInventory, keyActiveMoveStacks, keyActiveMoveEverything);

    if ((!Cfg::Toggles::SCROLL_SINGLE.getBooleanValue() && !nonSingleMove) || (!Cfg::Toggles::SCROLL_STACKS.getBooleanValue() && keyActiveMoveStacks)
        || (!Cfg::Toggles::SCROLL_MATCHING.getBooleanValue() && keyActiveMoveMatching) || (!Cfg::Toggles::SCROLL_EVERYTHING.getBooleanValue() && keyActiveMoveEverything)) {
        return false;
    }

    if (keyActiveMoveEverything) {
        tryMoveStacks(*slot, gui, false, moveToOtherInventory, false);
    } else if (keyActiveMoveMatching) {
        tryMoveStacks(*slot, gui, true, moveToOtherInventory, false);
        return true;
    } else if (keyActiveMoveStacks) {
        tryMoveStacks(*slot, gui, true, moveToOtherInventory, true);
    } else {
        auto const& stack = gui.menu().getItem(*slot);
        if (moveToOtherInventory) {
            tryMoveSingleItemToOtherInventory(*slot, gui);
        } else if (getStackSize(stack) < slotMaxStackSize(*slot, stack)) {
            tryMoveSingleItemToThisInventory(*slot, gui);
        }
    }
    return false;
}

bool InventoryUtils::dragMoveItems(Gui& gui, MoveAction action, Slot const* hovered, bool isClick) {
    if (!isStackEmpty(gui.menu().getCarried())) {
        stopDragging();
        return false;
    }
    bool cancel = false;
    if (isClick && action != MoveAction::NONE) {
        sSlotNumberLast   = -1;
        sActiveMoveAction = action;
        cancel            = dragMoveFromSlot(gui, hovered, action);
    } else {
        action = sActiveMoveAction;
    }
    if (sActiveMoveAction != MoveAction::NONE && !cancel) dragMoveFromSlot(gui, hovered, action);
    sSlotNumberLast = hovered ? hovered->slotNumber : -1;
    return cancel;
}

void InventoryUtils::stopDragging() {
    sActiveMoveAction = MoveAction::NONE;
    sDraggedSlots.clear();
}

bool InventoryUtils::dragMoveFromSlot(Gui& gui, Slot const* slot, MoveAction action) {
    MoveAmount amount = InputUtils::getMoveAmount(action);
    bool       flag   = slot != nullptr && isValidSlot(slot, gui, true);
    if (flag && slot->slotNumber != sSlotNumberLast && (amount != MoveAmount::MOVE_ONE || !sDraggedSlots.contains(slot->slotNumber))) {
        int n = slot->slotNumber;
        switch (action) {
        case MoveAction::MOVE_TO_OTHER_MOVE_ONE: tryMoveSingleItemToOtherInventory(*slot, gui); break;
        case MoveAction::MOVE_TO_OTHER_LEAVE_ONE: tryMoveAllButOneItemToOtherInventory(*slot, gui); break;
        case MoveAction::MOVE_TO_OTHER_STACKS: gui.shiftClickSlot(n); break;
        case MoveAction::MOVE_TO_OTHER_MATCHING: tryMoveStacks(*slot, gui, true, true, false); break;
        case MoveAction::DROP_ONE: gui.clickSlot(n, 0, ClickType::THROW); break;
        case MoveAction::DROP_LEAVE_ONE:
            gui.leftClickSlot(n);
            gui.rightClickSlot(n);
            gui.dropItemsFromCursor();
            break;
        case MoveAction::DROP_STACKS: gui.clickSlot(n, 1, ClickType::THROW); break;
        case MoveAction::MOVE_DOWN_MOVE_ONE:
        case MoveAction::MOVE_DOWN_LEAVE_ONE:
        case MoveAction::MOVE_DOWN_STACKS:
        case MoveAction::MOVE_DOWN_MATCHING: tryMoveItemsVertically(gui, slot, false, amount); break;
        case MoveAction::MOVE_UP_MOVE_ONE:
        case MoveAction::MOVE_UP_LEAVE_ONE:
        case MoveAction::MOVE_UP_STACKS:
        case MoveAction::MOVE_UP_MATCHING: tryMoveItemsVertically(gui, slot, true, amount); break;
        default: break;
        }
        sDraggedSlots.insert(n);
    }
    return true;
}

void InventoryUtils::dropStacks(Gui& gui, ItemStack const& stackReferenceIn, Slot const* slotReference, bool sameInventory) {
    if (!slotReference || isStackEmpty(stackReferenceIn)) return;
    ItemStack stackReference = copyStack(stackReferenceIn);
    for (auto const& slot : gui.menu().slots()) {
        if (areSlotsInSameInventory(slot, *slotReference) == sameInventory && areStacksEqual(gui.menu().getItem(slot), stackReference)) {
            gui.dropStack(slot.slotNumber);
        }
    }
}

void InventoryUtils::dropAllMatchingStacks(Gui& gui, ItemStack const& stackReferenceIn) {
    if (isStackEmpty(stackReferenceIn)) return;
    ItemStack stackReference = copyStack(stackReferenceIn);
    for (auto const& slot : gui.menu().slots()) {
        if (areStacksEqual(gui.menu().getItem(slot), stackReference)) gui.dropStack(slot.slotNumber);
    }
}

bool InventoryUtils::shiftDropItems(Gui& gui) {
    auto const& cursor = gui.menu().getCarried();
    if (isStackEmpty(cursor) || sSourceIndex < 0) return false;
    int source = gui.menu().findSlot(sSourceCollection, sSourceIndex);
    if (source < 0) return false;
    ItemStack stackReference = copyStack(cursor);
    gui.dropItemsFromCursor();
    dropStacks(gui, stackReference, gui.menu().getSlot(source), true);
    return true;
}

bool InventoryUtils::shiftPlaceItems(Slot const& slot, Gui& gui) {
    gui.leftClickSlot(slot.slotNumber);
    sDraggedSlots.insert(slot.slotNumber);
    tryMoveStacks(slot, gui, true, false, false);
    return true;
}

void InventoryUtils::storeSourceSlotCandidate(Slot const* slot, Gui& gui) {
    if (!slot) return;
    sSourceCandidateCollection = slot->collection;
    sSourceCandidateIndex      = slot->index;
    sStackInCursorLastId       = itemId(gui.menu().getCarried());
}

void InventoryUtils::checkForItemPickup(Gui& gui) {
    auto const& cursor = gui.menu().getCarried();
    if (!isStackEmpty(cursor) && itemId(cursor) != sStackInCursorLastId && sSourceCandidateIndex >= 0) {
        sSourceCollection = sSourceCandidateCollection;
        sSourceIndex      = sSourceCandidateIndex;
    }
}

bool InventoryUtils::tryMoveSingleItemToOtherInventory(Slot const& slot, Gui& gui) {
    auto const& stackOrig = gui.menu().getItem(slot);
    if (!isStackEmpty(gui.menu().getCarried()) || (getStackSize(stackOrig) > 1 && !mayPlace(slot, stackOrig))) return false;
    return clickSlotsToMoveSingleItemByShiftClick(gui, slot.slotNumber);
}

bool InventoryUtils::tryMoveAllButOneItemToOtherInventory(Slot const& slot, Gui& gui) {
    ItemStack stackOrig = copyStack(gui.menu().getItem(slot));
    if (getStackSize(stackOrig) == 1 || getStackSize(stackOrig) > static_cast<int>(stackOrig.getMaxStackSize()) || !mayPlace(slot, stackOrig)) return true;

    gui.rightClickSlot(slot.slotNumber);
    if (isStackEmpty(gui.menu().getCarried())) return false;

    int stackInCursorSizeOrig = getStackSize(gui.menu().getCarried());
    int tempSlotNum           = -1;

    for (auto const& slotTmp : gui.menu().slots()) {
        if (slotTmp.slotNumber != slot.slotNumber && areSlotsInSameInventory(slotTmp, slot, true) && mayPlace(slotTmp, gui.menu().getCarried())) {
            auto const& stackInSlot = gui.menu().getItem(slotTmp);
            if (isStackEmpty(stackInSlot) || areStacksEqual(stackInSlot, gui.menu().getCarried())) {
                gui.rightClickSlot(slotTmp.slotNumber);
                auto const& cursor = gui.menu().getCarried();
                if (isStackEmpty(cursor) || getStackSize(cursor) < stackInCursorSizeOrig) {
                    tempSlotNum = slotTmp.slotNumber;
                    break;
                }
            }
        }
    }
    if (!isStackEmpty(gui.menu().getCarried())) gui.leftClickSlot(slot.slotNumber);

    if (tempSlotNum != -1) {
        gui.shiftClickSlot(slot.slotNumber);
        gui.rightClickSlot(tempSlotNum);
        gui.rightClickSlot(slot.slotNumber);
        if (!isStackEmpty(gui.menu().getCarried())) gui.leftClickSlot(tempSlotNum);
        return true;
    }

    bool treatHotbarAsDifferent = gui.menu().screenName() == "inventory_screen";
    auto slots                  = getSlotNumbersOfEmptySlots(gui, slot, false, treatHotbarAsDifferent, false);
    if (slots.empty()) slots = getSlotNumbersOfMatchingStacks(gui, slot, false, gui.menu().getItem(slot), true, treatHotbarAsDifferent, false);
    if (!slots.empty()) {
        gui.leftClickSlot(slot.slotNumber);
        gui.rightClickSlot(slot.slotNumber);
        for (int slotNum : slots) {
            if (isStackEmpty(gui.menu().getCarried())) return true;
            auto const* slotTmp = gui.menu().getSlot(slotNum);
            if (slotTmp && mayPlace(*slotTmp, gui.menu().getCarried())) gui.leftClickSlot(slotNum);
        }
        if (!isStackEmpty(gui.menu().getCarried())) gui.leftClickSlot(slot.slotNumber);
    }
    return false;
}

bool InventoryUtils::tryMoveSingleItemToThisInventory(Slot const& slot, Gui& gui) {
    ItemStack stackOrig = copyStack(gui.menu().getItem(slot));
    if (!mayPlace(slot, stackOrig)) return false;
    auto const& slots = gui.menu().slots();
    for (int i = static_cast<int>(slots.size()) - 1; i >= 0; --i) {
        auto const& slotTmp  = slots[static_cast<size_t>(i)];
        auto const& stackTmp = gui.menu().getItem(slotTmp);
        if (!areSlotsInSameInventory(slotTmp, slot) && !isStackEmpty(stackTmp) && (getStackSize(stackTmp) == 1 || mayPlace(slotTmp, stackTmp))) {
            if (areStacksEqual(stackTmp, stackOrig)) return clickSlotsToMoveSingleItem(gui, slotTmp.slotNumber, slot.slotNumber);
        }
    }
    return false;
}

void InventoryUtils::tryMoveStacks(Slot const& slot, Gui& gui, bool matchingOnly, bool toOtherInventory, bool firstOnly) {
    ItemStack ref = copyStack(gui.menu().getItem(slot));
    tryMoveStacks(ref, slot, gui, matchingOnly, toOtherInventory, firstOnly);
}

void InventoryUtils::tryMoveStacks(ItemStack const& stackReference, Slot const& slot, Gui& gui, bool matchingOnly, bool toOtherInventory, bool firstOnly) {
    auto const& slots = gui.menu().slots();
    for (int i = static_cast<int>(slots.size()) - 1; i >= 0; --i) {
        auto const& slotTmp = slots[static_cast<size_t>(i)];
        if (slotTmp.slotNumber != slot.slotNumber && areSlotsInSameInventory(slotTmp, slot) == toOtherInventory && !isStackEmpty(gui.menu().getItem(slotTmp))
            && (!matchingOnly || areStacksEqual(stackReference, gui.menu().getItem(slotTmp)))) {
            bool success = shiftClickSlotWithCheck(gui, slotTmp.slotNumber);
            if (!success && Cfg::Toggles::ITEM_MOVING_FALLBACK.getBooleanValue()) clickSlotsToMoveItemsFromSlot(slotTmp, gui, toOtherInventory);
            if (firstOnly) return;
        }
    }
    if (toOtherInventory && !shiftClickSlotWithCheck(gui, slot.slotNumber) && Cfg::Toggles::ITEM_MOVING_FALLBACK.getBooleanValue()) {
        clickSlotsToMoveItemsFromSlot(slot, gui, toOtherInventory);
    }
}

std::vector<int> InventoryUtils::getSlotNumbersOfMatchingStacks(Gui& gui, Slot const& slotReference, bool sameInventory, ItemStack const& stackReference, bool preferPartial, bool treatHotbarAsDifferent, bool reverse) {
    std::vector<int> out;
    auto const&      slots = gui.menu().slots();
    int const        n     = static_cast<int>(slots.size());
    for (int k = 0; k < n; ++k) {
        int         i     = reverse ? n - 1 - k : k;
        auto const& slot  = slots[static_cast<size_t>(i)];
        auto const& stack = gui.menu().getItem(slot);
        if (!isStackEmpty(stack) && areSlotsInSameInventory(slot, slotReference, treatHotbarAsDifferent) == sameInventory && areStacksEqual(stack, stackReference)) {
            if ((getStackSize(stack) < static_cast<int>(stackReference.getMaxStackSize())) == preferPartial) out.insert(out.begin(), slot.slotNumber);
            else out.push_back(slot.slotNumber);
        }
    }
    return out;
}

std::vector<int> InventoryUtils::getSlotNumbersOfMatchingStacks(Gui& gui, ItemStack const& stackReference, bool preferPartial) {
    std::vector<int> out;
    for (auto const& slot : gui.menu().slots()) {
        auto const& stack = gui.menu().getItem(slot);
        if (!isStackEmpty(stack) && areStacksEqual(stack, stackReference)) {
            if ((getStackSize(stack) < static_cast<int>(stackReference.getMaxStackSize())) == preferPartial) out.insert(out.begin(), slot.slotNumber);
            else out.push_back(slot.slotNumber);
        }
    }
    return out;
}

std::vector<int> InventoryUtils::getSlotNumbersOfEmptySlots(Gui& gui, Slot const& slotReference, bool sameInventory, bool treatHotbarAsDifferent, bool reverse) {
    std::vector<int> out;
    auto const&      slots = gui.menu().slots();
    int const        n     = static_cast<int>(slots.size());
    for (int k = 0; k < n; ++k) {
        auto const& slot = slots[static_cast<size_t>(reverse ? n - 1 - k : k)];
        if (isStackEmpty(gui.menu().getItem(slot)) && !slot.isOutput && areSlotsInSameInventory(slot, slotReference, treatHotbarAsDifferent) == sameInventory) {
            out.push_back(slot.slotNumber);
        }
    }
    return out;
}

std::vector<int> InventoryUtils::getSlotNumbersOfEmptySlotsInPlayerInventory(Gui& gui, bool reverse) {
    std::vector<int> out;
    auto const&      slots = gui.menu().slots();
    int const        n     = static_cast<int>(slots.size());
    for (int k = 0; k < n; ++k) {
        auto const& slot = slots[static_cast<size_t>(reverse ? n - 1 - k : k)];
        if (slot.inventoryId == 0 && isStackEmpty(gui.menu().getItem(slot))) out.push_back(slot.slotNumber);
    }
    return out;
}

void InventoryUtils::clickSlotsToMoveItemsFromSlot(Slot const& slotFrom, Gui& gui, bool toOtherInventory) {
    gui.leftClickSlot(slotFrom.slotNumber);
    if (isStackEmpty(gui.menu().getCarried())) return;
    for (auto const& slotDst : gui.menu().slots()) {
        auto const& stackDst = gui.menu().getItem(slotDst);
        if (areSlotsInSameInventory(slotDst, slotFrom) != toOtherInventory && (isStackEmpty(stackDst) || areStacksEqual(stackDst, gui.menu().getCarried()))) {
            gui.leftClickSlot(slotDst.slotNumber);
        }
        if (isStackEmpty(gui.menu().getCarried())) return;
    }
    if (!isStackEmpty(gui.menu().getCarried())) gui.leftClickSlot(slotFrom.slotNumber);
}

bool InventoryUtils::clickSlotsToMoveSingleItem(Gui& gui, int slotFrom, int slotTo) {
    auto const* from = gui.menu().getSlot(slotFrom);
    if (!from) return false;
    auto const& stack = gui.menu().getItem(*from);
    if (isStackEmpty(stack)) return false;
    if (getStackSize(stack) > 1) gui.rightClickSlot(slotFrom);
    else gui.leftClickSlot(slotFrom);
    gui.rightClickSlot(slotTo);
    if (!isStackEmpty(gui.menu().getCarried())) gui.leftClickSlot(slotFrom);
    return true;
}

bool InventoryUtils::clickSlotsToMoveSingleItemByShiftClick(Gui& gui, int slotFrom) {
    auto const* slot = gui.menu().getSlot(slotFrom);
    if (!slot) return false;
    auto const& stack = gui.menu().getItem(*slot);
    if (isStackEmpty(stack)) return false;
    if (getStackSize(stack) > 1) {
        gui.leftClickSlot(slotFrom);
        if (!isStackEmpty(gui.menu().getItem(*slot))) {
            gui.leftClickSlot(slotFrom);
            return false;
        }
        gui.rightClickSlot(slotFrom);
    }
    gui.shiftClickSlot(slotFrom);
    if (!isStackEmpty(gui.menu().getCarried())) gui.leftClickSlot(slotFrom);
    return true;
}

bool InventoryUtils::shiftClickSlotWithCheck(Gui& gui, int slotNum) {
    auto const* slot = gui.menu().getSlot(slotNum);
    if (!slot || isStackEmpty(gui.menu().getItem(*slot))) return false;
    int sizeOrig = getStackSize(gui.menu().getItem(*slot));
    gui.shiftClickSlot(slotNum);
    auto const& after = gui.menu().getItem(*slot);
    return isStackEmpty(after) || getStackSize(after) != sizeOrig;
}

bool InventoryUtils::tryMoveItemsVertically(Gui& gui, Slot const* slot, bool moveUp, MoveAmount amount) {
    if (!slot || !isStackEmpty(gui.menu().getCarried())) return false;
    auto slots = getVerticallyFurthestSuitableSlotsForStackInSlot(gui, *slot, moveUp);
    if (slots.empty()) return false;
    if (amount == MoveAmount::FULL_STACKS) moveStackToSlots(gui, *slot, slots, false);
    else if (amount == MoveAmount::MOVE_ONE) moveOneItemToFirstValidSlot(gui, *slot, slots);
    else if (amount == MoveAmount::LEAVE_ONE) moveStackToSlots(gui, *slot, slots, true);
    else if (amount == MoveAmount::ALL_MATCHING) moveMatchingStacksToSlots(gui, *slot, moveUp);
    return true;
}

void InventoryUtils::moveMatchingStacksToSlots(Gui& gui, Slot const& slot, bool moveUp) {
    ItemStack ref           = copyStack(gui.menu().getItem(slot));
    auto      matchingSlots = getSlotNumbersOfMatchingStacks(gui, slot, true, ref, true, true, false);
    auto      targetSlots   = getSlotNumbersOfEmptySlots(gui, slot, false, true, false);
    auto      sameEmpty     = getSlotNumbersOfEmptySlots(gui, slot, true, true, false);
    targetSlots.insert(targetSlots.end(), sameEmpty.begin(), sameEmpty.end());
    targetSlots.insert(targetSlots.end(), matchingSlots.begin(), matchingSlots.end());
    sortVertically(gui, matchingSlots, !moveUp);
    sortVertically(gui, targetSlots, moveUp);
    for (int srcSlotNum : matchingSlots) {
        auto const* srcSlot  = gui.menu().getSlot(srcSlotNum);
        auto const* lastSlot = srcSlot ? moveStackToSlots(gui, *srcSlot, targetSlots, false) : nullptr;
        if (!lastSlot || lastSlot->slotNumber == srcSlot->slotNumber || (lastSlot->y > srcSlot->y) == moveUp) return;
    }
}

Slot const* InventoryUtils::moveStackToSlots(Gui& gui, Slot const& slotFrom, std::vector<int> const& slotsTo, bool leaveOne) {
    Slot const* lastSlot = nullptr;
    if (isStackEmpty(gui.menu().getItem(slotFrom))) return nullptr;
    gui.leftClickSlot(slotFrom.slotNumber);
    if (leaveOne) gui.rightClickSlot(slotFrom.slotNumber);
    for (int slotNum : slotsTo) {
        if (isStackEmpty(gui.menu().getCarried())) break;
        auto const* dst = gui.menu().getSlot(slotNum);
        if (!dst) continue;
        auto const& dstStack = gui.menu().getItem(*dst);
        if (mayPlace(*dst, gui.menu().getCarried()) && (isStackEmpty(dstStack) || areStacksEqual(dstStack, gui.menu().getCarried()))) {
            gui.leftClickSlot(slotNum);
            lastSlot = dst;
        }
    }
    if (!isStackEmpty(gui.menu().getCarried())) gui.leftClickSlot(slotFrom.slotNumber);
    return lastSlot;
}

void InventoryUtils::moveOneItemToFirstValidSlot(Gui& gui, Slot const& slotFrom, std::vector<int> const& slotsTo) {
    gui.rightClickSlot(slotFrom.slotNumber);
    if (isStackEmpty(gui.menu().getCarried())) return;
    int sizeOrig = getStackSize(gui.menu().getCarried());
    for (int slotNum : slotsTo) {
        gui.rightClickSlot(slotNum);
        auto const& cursor = gui.menu().getCarried();
        if (isStackEmpty(cursor) || getStackSize(cursor) != sizeOrig) break;
    }
    if (!isStackEmpty(gui.menu().getCarried())) gui.leftClickSlot(slotFrom.slotNumber);
}

std::vector<int> InventoryUtils::getVerticallyFurthestSuitableSlotsForStackInSlot(Gui& gui, Slot const& slotIn, bool above) {
    std::vector<int> out;
    auto const&      stackSlot = gui.menu().getItem(slotIn);
    if (isStackEmpty(stackSlot)) return out;
    for (auto const& slotTmp : gui.menu().slots()) {
        if (slotTmp.slotNumber != slotIn.slotNumber && slotTmp.y != slotIn.y && above == (slotTmp.y < slotIn.y)) {
            auto const& stackTmp = gui.menu().getItem(slotTmp);
            if ((isStackEmpty(stackTmp) && mayPlace(slotTmp, stackSlot)) || (areStacksEqual(stackTmp, stackSlot) && slotMaxStackSize(slotTmp, stackTmp) > getStackSize(stackTmp))) {
                out.push_back(slotTmp.slotNumber);
            }
        }
    }
    sortVertically(gui, out, above);
    return out;
}

void InventoryUtils::sortVertically(Gui& gui, std::vector<int>& slotNumbers, bool topToBottom) {
    std::stable_sort(slotNumbers.begin(), slotNumbers.end(), [&](int a, int b) {
        if (a == b) return false;
        auto const* s1 = gui.menu().getSlot(a);
        auto const* s2 = gui.menu().getSlot(b);
        if (s1->y == s2->y) return (s1->slotNumber < s2->slotNumber) == topToBottom;
        return (s1->y < s2->y) == topToBottom;
    });
}

void InventoryUtils::tryClearCursor(Gui& gui) {
    auto const& cursor = gui.menu().getCarried();
    if (isStackEmpty(cursor)) return;
    auto emptySlots = getSlotNumbersOfEmptySlotsInPlayerInventory(gui, false);
    if (!emptySlots.empty()) {
        gui.leftClickSlot(emptySlots.front());
    } else {
        ItemStack ref           = copyStack(cursor);
        auto      matchingSlots = getSlotNumbersOfMatchingStacks(gui, ref, true);
        for (int slotNum : matchingSlots) {
            auto const* slot = gui.menu().getSlot(slotNum);
            if (!slot) break;
            auto const& stackSlot = gui.menu().getItem(*slot);
            if (!areStacksEqual(stackSlot, gui.menu().getCarried()) || getStackSize(stackSlot) >= static_cast<int>(ref.getMaxStackSize())) break;
            if (slot->inventoryId == 0) gui.leftClickSlot(slotNum);
        }
    }
    if (!isStackEmpty(gui.menu().getCarried())) gui.dropItemsFromCursor();
}

}
