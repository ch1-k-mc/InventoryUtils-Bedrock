// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <set>
#include <string>
#include <vector>

#include "itemscroller/util/MoveAction.h"
#include "platform/inventory/SlotClicker.h"

class ItemStack;

namespace inventory_utils::itemscroller::recipes {
class RecipePattern;
}

namespace inventory_utils::itemscroller::util {

using Gui  = platform::inventory::SlotClicker;
using Slot = platform::inventory::MenuSlot;

class InventoryUtils {
public:
    static bool isCraftingSlot(Gui& gui, Slot const* slot);
    static bool canShiftPlaceItems(Gui& gui, Slot const* hovered);

    static bool tryMoveItems(Gui& gui, Slot const* hovered, bool scrollingUp);
    static bool dragMoveItems(Gui& gui, MoveAction action, Slot const* hovered, bool isClick);
    static void stopDragging();
    [[nodiscard]] static MoveAction getActiveMoveAction() { return sActiveMoveAction; }
    static void resetLastSlotNumber() { sSlotNumberLast = -1; }

    static void dropStacks(Gui& gui, ItemStack const& stackReference, Slot const* slotReference, bool sameInventory);
    static void dropAllMatchingStacks(Gui& gui, ItemStack const& stackReference);
    static bool shiftDropItems(Gui& gui);
    static bool shiftPlaceItems(Slot const& slot, Gui& gui);
    static void storeSourceSlotCandidate(Slot const* slot, Gui& gui);
    static void checkForItemPickup(Gui& gui);

    static void tryMoveStacks(Slot const& slot, Gui& gui, bool matchingOnly, bool toOtherInventory, bool firstOnly);
    static bool tryMoveItemsVertically(Gui& gui, Slot const* slot, bool moveUp, MoveAmount amount);
    static void tryClearCursor(Gui& gui);

    static bool areStacksEqual(ItemStack const& a, ItemStack const& b);
    static bool isStackEmpty(ItemStack const& s);
    static int  getStackSize(ItemStack const& s);

    static void handleRecipeClick(Gui& gui, int hoveredRecipeId, bool isLeftClick, bool isRightClick, bool isPickBlock, bool isShiftDown, bool dropKeyDown);
    static void tryMoveItemsToFirstCraftingGrid(recipes::RecipePattern& recipe, Gui& gui, bool fillStacks);
    static void loadRecipeItemsToGridForOutputSlotUnderMouse(recipes::RecipePattern& recipe, Gui& gui);
    static void clearFirstCraftingGridOfItems(recipes::RecipePattern& recipe, Gui& gui, bool clearNonMatchingOnly);
    static void clearFirstCraftingGridOfAllItems(Gui& gui);
    static void rightClickCraftOneStack(Gui& gui);
    static void craftEverythingPossibleWithCurrentRecipe(recipes::RecipePattern& recipe, Gui& gui);
    static void moveAllCraftingResultsToOtherInventory(recipes::RecipePattern& recipe, Gui& gui);
    static void throwAllCraftingResultsToGround(recipes::RecipePattern& recipe, Gui& gui);
    static void throwAllNonRecipeItemsToGround(recipes::RecipePattern& recipe, Gui& gui);
    static void setCraftingGridContentsUsingSwaps(Gui& gui, recipes::RecipePattern& recipe, Slot const& outputSlot);
    static void moveOneSetOfItemsFromSlotToPlayerInventory(Gui& gui, Slot const& slot);
    static void dropStacksUntilEmpty(Gui& gui, int slotNum);
    static void dropStacksWhileHasItem(Gui& gui, int slotNum, ItemStack const& stackReference);
    static void swapSlots(Gui& gui, int slotNum, int otherSlot);

private:
    static bool tryMoveItemsCrafting(Gui& gui, Slot const& slot, bool moveToOtherInventory, bool moveStacks, bool moveEverything);
    static void craftAsManyItemsAsPossible(recipes::RecipePattern& recipe, Slot const& slot, Gui& gui);
    static bool clearCraftingGridOfItems(recipes::RecipePattern& recipe, Gui& gui, int first, int count, bool clearNonMatchingOnly);
    static bool clearCraftingGridOfAllItems(Gui& gui, int first, int count);
    static bool tryMoveItemsToCraftingGridSlots(recipes::RecipePattern& recipe, Slot const& slot, Gui& gui, bool fillStacks);
    static void fillCraftingGrid(Gui& gui, Slot const& slotGridFirst, ItemStack const& ingredientReference, std::vector<int> const& targetSlots);
    static int  putSingleItemIntoSlots(Gui& gui, std::vector<int> const& targetSlots, int startIndex);
    static void moveOneRecipeItemIntoCraftingGrid(Gui& gui, Slot const& slotGridFirst, ItemStack const& ingredientReference, std::vector<int> const& targetSlots);
    static bool moveItemFromCursorToSlots(Gui& gui, std::vector<int> const& slotNumbers);
    static int  getSlotNumberOfLargestMatchingStackFromDifferentInventory(Gui& gui, Slot const& slotReference, ItemStack const& stackReference);
    static int  getSlotNumberOfSmallestStackFromDifferentInventory(Gui& gui, Slot const& slotReference, ItemStack const& stackReference, int idealSize);
    static void dragSplitItemsIntoSlots(Gui& gui, std::vector<int> const& targetSlots);

    static bool isValidSlot(Slot const* slot, Gui& gui, bool requireItems);
    static bool inventoryExistsAbove(Slot const& slot, Gui& gui);
    static bool dragMoveFromSlot(Gui& gui, Slot const* slot, MoveAction action);

    static bool tryMoveSingleItemToOtherInventory(Slot const& slot, Gui& gui);
    static bool tryMoveAllButOneItemToOtherInventory(Slot const& slot, Gui& gui);
    static bool tryMoveSingleItemToThisInventory(Slot const& slot, Gui& gui);
    static void tryMoveStacks(ItemStack const& stackReference, Slot const& slot, Gui& gui, bool matchingOnly, bool toOtherInventory, bool firstOnly);

    static std::vector<int> getSlotNumbersOfMatchingStacks(Gui& gui, Slot const& slotReference, bool sameInventory, ItemStack const& stackReference, bool preferPartial, bool treatHotbarAsDifferent, bool reverse);
    static std::vector<int> getSlotNumbersOfMatchingStacks(Gui& gui, ItemStack const& stackReference, bool preferPartial);
    static std::vector<int> getSlotNumbersOfEmptySlots(Gui& gui, Slot const& slotReference, bool sameInventory, bool treatHotbarAsDifferent, bool reverse);
    static std::vector<int> getSlotNumbersOfEmptySlotsInPlayerInventory(Gui& gui, bool reverse);
    static bool             areSlotsInSameInventory(Slot const& a, Slot const& b, bool treatHotbarAsDifferent = false);

    static void clickSlotsToMoveItemsFromSlot(Slot const& slotFrom, Gui& gui, bool toOtherInventory);
    static bool clickSlotsToMoveSingleItem(Gui& gui, int slotFrom, int slotTo);
    static bool clickSlotsToMoveSingleItemByShiftClick(Gui& gui, int slotFrom);
    static bool shiftClickSlotWithCheck(Gui& gui, int slotNum);

    static void        moveMatchingStacksToSlots(Gui& gui, Slot const& slot, bool moveUp);
    static Slot const* moveStackToSlots(Gui& gui, Slot const& slotFrom, std::vector<int> const& slotsTo, bool leaveOne);
    static void        moveOneItemToFirstValidSlot(Gui& gui, Slot const& slotFrom, std::vector<int> const& slotsTo);
    static std::vector<int> getVerticallyFurthestSuitableSlotsForStackInSlot(Gui& gui, Slot const& slotIn, bool above);
    static void             sortVertically(Gui& gui, std::vector<int>& slotNumbers, bool topToBottom);

    static bool mayPlace(Slot const& slot, ItemStack const& stack);
    static int  slotMaxStackSize(Slot const& slot, ItemStack const& stack);

    static inline std::set<int> sDraggedSlots;
    static inline MoveAction    sActiveMoveAction = MoveAction::NONE;
    static inline int           sSlotNumberLast   = -1;
    static inline std::string sSourceCandidateCollection;
    static inline int         sSourceCandidateIndex = -1;
    static inline std::string sSourceCollection;
    static inline int         sSourceIndex = -1;
    static inline std::string sStackInCursorLastId;
};

}
