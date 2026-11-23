// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include <algorithm>
#include <map>

#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"

#include "itemscroller/config/Configs.h"
#include "itemscroller/recipes/CraftingHandler.h"
#include "itemscroller/recipes/RecipeStorage.h"
#include "itemscroller/util/InventoryUtils.h"

namespace inventory_utils::itemscroller::util {

namespace {

using platform::inventory::ClickType;
using recipes::CraftingHandler;
using recipes::RecipePattern;
using recipes::RecipeStorage;

std::vector<std::pair<ItemStack, std::vector<int>>> getSlotsPerItem(std::vector<ItemStack> const& stacks) {
    std::vector<std::pair<ItemStack, std::vector<int>>> out;
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        auto const& s = stacks[static_cast<size_t>(i)];
        if (InventoryUtils::isStackEmpty(s)) continue;
        auto it = std::find_if(out.begin(), out.end(), [&](auto const& e) { return InventoryUtils::areStacksEqual(e.first, s); });
        if (it == out.end()) out.push_back({ItemStack(s), {i}});
        else it->second.push_back(i);
    }
    return out;
}

}

void InventoryUtils::handleRecipeClick(Gui& gui, int hoveredRecipeId, bool isLeftClick, bool isRightClick, bool isPickBlock, bool isShiftDown, bool dropKeyDown) {
    auto& recipes = RecipeStorage::getInstance();
    if (isLeftClick || isRightClick) {
        bool changed = recipes.getSelection() != hoveredRecipeId;
        recipes.changeSelectedRecipe(hoveredRecipeId);
        if (changed) clearFirstCraftingGridOfItems(recipes.getSelectedRecipe(), gui, false);
        else tryMoveItemsToFirstCraftingGrid(recipes.getRecipe(hoveredRecipeId), gui, isShiftDown);

        if (isRightClick) {
            auto const* outputSlot = CraftingHandler::getFirstCraftingOutputSlotForGui(gui);
            if (outputSlot) {
                if (dropKeyDown) {
                    if (isShiftDown) dropStacksUntilEmpty(gui, outputSlot->slotNumber);
                    else gui.dropItem(outputSlot->slotNumber);
                } else {
                    if (isShiftDown) gui.shiftClickSlot(outputSlot->slotNumber);
                    else moveOneSetOfItemsFromSlotToPlayerInventory(gui, *outputSlot);
                }
            }
        }
    } else if (isPickBlock) {
        clearFirstCraftingGridOfAllItems(gui);
    }
}

void InventoryUtils::tryMoveItemsToFirstCraftingGrid(RecipePattern& recipe, Gui& gui, bool fillStacks) {
    if (auto const* out = CraftingHandler::getFirstCraftingOutputSlotForGui(gui)) tryMoveItemsToCraftingGridSlots(recipe, *out, gui, fillStacks);
}

void InventoryUtils::loadRecipeItemsToGridForOutputSlotUnderMouse(RecipePattern& recipe, Gui& gui) {
    auto const* slot = gui.getSlotUnderMouse();
    if (isCraftingSlot(gui, slot) && !isStackEmpty(recipe.getResult())) tryMoveItemsToCraftingGridSlots(recipe, *slot, gui, false);
}

bool InventoryUtils::tryMoveItemsCrafting(Gui& gui, Slot const& slot, bool moveToOtherInventory, bool moveStacks, bool moveEverything) {
    auto&       recipe            = RecipeStorage::getInstance().getSelectedRecipe();
    auto const& stackRecipeOutput = recipe.getResult();
    if (moveToOtherInventory) {
        auto const& out = gui.menu().getItem(slot);
        if (!isStackEmpty(out)) {
            if (areStacksEqual(out, stackRecipeOutput)) {
                if (moveEverything) craftAsManyItemsAsPossible(recipe, slot, gui);
                else if (moveStacks) gui.shiftClickSlot(slot.slotNumber);
                else moveOneSetOfItemsFromSlotToPlayerInventory(gui, slot);
            }
        } else {
            if (auto range = CraftingHandler::getCraftingGridSlots(gui, slot)) clearCraftingGridOfAllItems(gui, range->getFirst(), range->getSlotCount());
        }
    } else if (!isStackEmpty(stackRecipeOutput)) {
        tryMoveItemsToCraftingGridSlots(recipe, slot, gui, moveStacks);
    }
    return false;
}

void InventoryUtils::craftAsManyItemsAsPossible(RecipePattern& recipe, Slot const& slot, Gui& gui) {
    ItemStack result(recipe.getResult());
    int       failSafe = 1024;
    while (failSafe > 0 && !isStackEmpty(gui.menu().getItem(slot)) && areStacksEqual(gui.menu().getItem(slot), result)) {
        gui.shiftClickSlot(slot.slotNumber);
        if (isStackEmpty(gui.menu().getItem(slot)) || !areStacksEqual(gui.menu().getItem(slot), result)) {
            tryMoveItemsToCraftingGridSlots(recipe, slot, gui, true);
        } else {
            break;
        }
        --failSafe;
    }
}

void InventoryUtils::clearFirstCraftingGridOfItems(RecipePattern& recipe, Gui& gui, bool clearNonMatchingOnly) {
    auto const* out = CraftingHandler::getFirstCraftingOutputSlotForGui(gui);
    if (!out) return;
    if (auto range = CraftingHandler::getCraftingGridSlots(gui, *out)) clearCraftingGridOfItems(recipe, gui, range->getFirst(), range->getSlotCount(), clearNonMatchingOnly);
}

void InventoryUtils::clearFirstCraftingGridOfAllItems(Gui& gui) {
    auto const* out = CraftingHandler::getFirstCraftingOutputSlotForGui(gui);
    if (!out) return;
    if (auto range = CraftingHandler::getCraftingGridSlots(gui, *out)) clearCraftingGridOfAllItems(gui, range->getFirst(), range->getSlotCount());
}

bool InventoryUtils::clearCraftingGridOfItems(RecipePattern& recipe, Gui& gui, int first, int rangeSlots, bool clearNonMatchingOnly) {
    int const invSlots  = static_cast<int>(gui.menu().slots().size());
    int const slotCount = std::min(rangeSlots, recipe.getRecipeLength());
    for (int i = 0, slotNum = first; i < slotCount && slotNum < invSlots; ++i, ++slotNum) {
        auto const* slotTmp = gui.menu().getSlot(slotNum);
        if (slotTmp && !isStackEmpty(gui.menu().getItem(*slotTmp))
            && (!clearNonMatchingOnly || !areStacksEqual(recipe.getRecipeItems()[static_cast<size_t>(i)], gui.menu().getItem(*slotTmp)))) {
            gui.shiftClickSlot(slotNum);
            if (!isStackEmpty(gui.menu().getItem(*slotTmp))) gui.dropStack(slotNum);
        }
    }
    return true;
}

bool InventoryUtils::clearCraftingGridOfAllItems(Gui& gui, int first, int rangeSlots) {
    int const invSlots   = static_cast<int>(gui.menu().slots().size());
    bool      clearedAll = true;
    for (int i = 0, slotNum = first; i < rangeSlots && slotNum < invSlots; ++i, ++slotNum) {
        auto const* slotTmp = gui.menu().getSlot(slotNum);
        if (slotTmp && !isStackEmpty(gui.menu().getItem(*slotTmp))) {
            gui.shiftClickSlot(slotNum);
            if (!isStackEmpty(gui.menu().getItem(*slotTmp))) clearedAll = false;
        }
    }
    return clearedAll;
}

bool InventoryUtils::tryMoveItemsToCraftingGridSlots(RecipePattern& recipe, Slot const& slot, Gui& gui, bool fillStacks) {
    int  numSlots = static_cast<int>(gui.menu().slots().size());
    auto range    = CraftingHandler::getCraftingGridSlots(gui, slot);
    if (range && range->getLast() < numSlots && recipe.getRecipeLength() <= range->getSlotCount()) {
        if (!clearCraftingGridOfItems(recipe, gui, range->getFirst(), range->getSlotCount(), true)) return false;
        auto const* slotGridFirst = gui.menu().getSlot(range->getFirst());
        if (!slotGridFirst) return false;
        for (auto const& [ingredientReference, recipeSlots] : getSlotsPerItem(recipe.getRecipeItems())) {
            std::vector<int> targetSlots;
            for (int s : recipeSlots) targetSlots.push_back(s + range->getFirst());
            if (fillStacks) fillCraftingGrid(gui, *slotGridFirst, ingredientReference, targetSlots);
            else moveOneRecipeItemIntoCraftingGrid(gui, *slotGridFirst, ingredientReference, targetSlots);
        }
    }
    return false;
}

void InventoryUtils::fillCraftingGrid(Gui& gui, Slot const& slotGridFirst, ItemStack const& ingredientReference, std::vector<int> const& targetSlots) {
    if (isStackEmpty(ingredientReference)) return;
    int slotNum    = -1;
    int slotReturn = -1;
    while (true) {
        slotNum = getSlotNumberOfLargestMatchingStackFromDifferentInventory(gui, slotGridFirst, ingredientReference);
        if (slotNum < 0) break;
        if (slotReturn == -1) slotReturn = slotNum;
        gui.leftClickSlot(slotNum);
        auto const& cursor = gui.menu().getCarried();
        if (areStacksEqual(ingredientReference, cursor)) {
            int sizeOrig = getStackSize(cursor);
            dragSplitItemsIntoSlots(gui, targetSlots);
            if (!isStackEmpty(gui.menu().getCarried())) {
                if (getStackSize(gui.menu().getCarried()) >= sizeOrig) break;
                gui.leftClickSlot(slotReturn);
                if (!isStackEmpty(gui.menu().getCarried())) {
                    slotReturn = slotNum;
                    gui.leftClickSlot(slotReturn);
                }
            }
        } else {
            break;
        }
        if (!isStackEmpty(gui.menu().getCarried())) break;
    }
    if (slotNum >= 0 && !isStackEmpty(gui.menu().getCarried())) gui.leftClickSlot(slotNum);
}

void InventoryUtils::rightClickCraftOneStack(Gui& gui) {
    auto const* slot = gui.getSlotUnderMouse();
    if (!slot || isStackEmpty(gui.menu().getItem(*slot))) return;
    auto const& cursor0 = gui.menu().getCarried();
    if (!isStackEmpty(cursor0) && !areStacksEqual(gui.menu().getItem(*slot), cursor0)) return;
    int sizeLast = 0;
    while (true) {
        gui.rightClickSlot(slot->slotNumber);
        auto const& cursor = gui.menu().getCarried();
        if (isStackEmpty(cursor) || getStackSize(cursor) <= sizeLast || getStackSize(cursor) >= static_cast<int>(cursor.getMaxStackSize())
            || !areStacksEqual(gui.menu().getItem(*slot), cursor)) {
            break;
        }
        sizeLast = getStackSize(cursor);
    }
}

void InventoryUtils::craftEverythingPossibleWithCurrentRecipe(RecipePattern& recipe, Gui& gui) {
    auto const* slot = CraftingHandler::getFirstCraftingOutputSlotForGui(gui);
    if (!slot || isStackEmpty(recipe.getResult())) return;
    auto range = CraftingHandler::getCraftingGridSlots(gui, *slot);
    if (!range) return;
    if (!clearCraftingGridOfItems(recipe, gui, range->getFirst(), range->getSlotCount(), false)) return;
    tryMoveItemsToCraftingGridSlots(recipe, *slot, gui, true);
    if (!isStackEmpty(gui.menu().getItem(*slot))) craftAsManyItemsAsPossible(recipe, *slot, gui);
}

void InventoryUtils::moveAllCraftingResultsToOtherInventory(RecipePattern& recipe, Gui& gui) {
    if (isStackEmpty(recipe.getResult())) return;
    ItemStack   stackResult(recipe.getResult());
    Slot const* slot = nullptr;
    for (auto const& slotTmp : gui.menu().slots()) {
        if (areStacksEqual(gui.menu().getItem(slotTmp), stackResult) && inventoryExistsAbove(slotTmp, gui)) {
            slot = &slotTmp;
            break;
        }
    }
    if (!slot) return;
    for (int slotNum : getSlotNumbersOfMatchingStacks(gui, *slot, true, stackResult, false, false, false)) gui.shiftClickSlot(slotNum);
}

void InventoryUtils::throwAllCraftingResultsToGround(RecipePattern& recipe, Gui& gui) {
    auto const* slot = CraftingHandler::getFirstCraftingOutputSlotForGui(gui);
    if (slot && !isStackEmpty(recipe.getResult())) dropStacks(gui, recipe.getResult(), slot, false);
}

void InventoryUtils::throwAllNonRecipeItemsToGround(RecipePattern& recipe, Gui& gui) {
    auto const* outputSlot = CraftingHandler::getFirstCraftingOutputSlotForGui(gui);
    if (!outputSlot || isStackEmpty(recipe.getResult())) return;
    auto range = CraftingHandler::getCraftingGridSlots(gui, *outputSlot);
    if (!range) return;
    auto const& recipeItems = recipe.getRecipeItems();
    int const   invSlots    = static_cast<int>(gui.menu().slots().size());
    int const   rangeSlots  = std::min(range->getSlotCount(), static_cast<int>(recipeItems.size()));
    for (int i = 0, slotNum = range->getFirst(); i < rangeSlots && slotNum < invSlots; ++i, ++slotNum) {
        auto const& stack = gui.menu().getItem(*gui.menu().getSlot(slotNum));
        if (!isStackEmpty(stack) && !areStacksEqual(stack, recipeItems[static_cast<size_t>(i)])) {
            ItemStack ref(stack);
            dropAllMatchingStacks(gui, ref);
        }
    }
}

void InventoryUtils::setCraftingGridContentsUsingSwaps(Gui& gui, RecipePattern& recipe, Slot const& outputSlot) {
    auto range = CraftingHandler::getCraftingGridSlots(gui, outputSlot);
    if (!range || isStackEmpty(recipe.getResult())) return;
    auto const&      recipeItems = recipe.getRecipeItems();
    int const        invSlots    = static_cast<int>(gui.menu().slots().size());
    int const        rangeSlots  = std::min(range->getSlotCount(), static_cast<int>(recipeItems.size()));
    std::vector<int> toRemove;
    for (int i = 0, slotNum = range->getFirst(); i < rangeSlots && slotNum < invSlots; ++i, ++slotNum) {
        auto const* craftingTableSlot = gui.menu().getSlot(slotNum);
        auto const& recipeStack       = recipeItems[static_cast<size_t>(i)];
        if (areStacksEqual(recipeStack, gui.menu().getItem(*craftingTableSlot))) continue;
        if (isStackEmpty(recipeStack)) {
            toRemove.push_back(slotNum);
        } else {
            int index = getSlotNumberOfLargestMatchingStackFromDifferentInventory(gui, *craftingTableSlot, recipeStack);
            if (index >= 0) {
                auto const* ingredientSlot = gui.menu().getSlot(index);
                if (ingredientSlot->inventoryId == 0 && ingredientSlot->containerSlot < 9) gui.clickSlot(slotNum, ingredientSlot->containerSlot, ClickType::SWAP);
                else swapSlots(gui, slotNum, index);
            }
        }
    }
    for (int slotNum : toRemove) {
        gui.shiftClickSlot(slotNum);
        if (!isStackEmpty(gui.menu().getItem(*gui.menu().getSlot(slotNum)))) gui.dropStack(slotNum);
    }
}

int InventoryUtils::putSingleItemIntoSlots(Gui& gui, std::vector<int> const& targetSlots, int startIndex) {
    auto const& cursor = gui.menu().getCarried();
    if (isStackEmpty(cursor)) return 0;
    int numSlots = static_cast<int>(gui.menu().slots().size());
    int loops    = std::min(getStackSize(cursor), static_cast<int>(targetSlots.size()) - startIndex);
    int count    = 0;
    for (int i = 0; i < loops; ++i) {
        int slotNum = targetSlots[static_cast<size_t>(startIndex + i)];
        if (slotNum >= numSlots) break;
        gui.rightClickSlot(slotNum);
        ++count;
    }
    return count;
}

void InventoryUtils::moveOneSetOfItemsFromSlotToPlayerInventory(Gui& gui, Slot const& slot) {
    gui.leftClickSlot(slot.slotNumber);
    auto const& cursor = gui.menu().getCarried();
    if (isStackEmpty(cursor)) return;
    ItemStack ref(cursor);
    auto      slots = getSlotNumbersOfMatchingStacks(gui, slot, false, ref, true, true, false);
    if (!moveItemFromCursorToSlots(gui, slots)) moveItemFromCursorToSlots(gui, getSlotNumbersOfEmptySlotsInPlayerInventory(gui, false));
}

void InventoryUtils::moveOneRecipeItemIntoCraftingGrid(Gui& gui, Slot const& slotGridFirst, ItemStack const& ingredientReference, std::vector<int> const& targetSlots) {
    int index     = 0;
    int slotNum   = -1;
    int slotCount = static_cast<int>(targetSlots.size());
    while (index < slotCount) {
        slotNum = getSlotNumberOfSmallestStackFromDifferentInventory(gui, slotGridFirst, ingredientReference, slotCount);
        if (slotNum < 0) break;
        gui.leftClickSlot(slotNum);
        if (areStacksEqual(ingredientReference, gui.menu().getCarried())) {
            int filled = putSingleItemIntoSlots(gui, targetSlots, index);
            index += filled;
            if (filled < 1) break;
        } else {
            break;
        }
    }
    if (slotNum >= 0 && !isStackEmpty(gui.menu().getCarried())) gui.leftClickSlot(slotNum);
}

bool InventoryUtils::moveItemFromCursorToSlots(Gui& gui, std::vector<int> const& slotNumbers) {
    for (int slotNum : slotNumbers) {
        gui.leftClickSlot(slotNum);
        if (isStackEmpty(gui.menu().getCarried())) return true;
    }
    return false;
}

int InventoryUtils::getSlotNumberOfLargestMatchingStackFromDifferentInventory(Gui& gui, Slot const& slotReference, ItemStack const& stackReference) {
    int slotNum = -1, largest = 0;
    for (auto const& slot : gui.menu().slots()) {
        auto const& stack = gui.menu().getItem(slot);
        if (!areSlotsInSameInventory(slot, slotReference) && !slot.isOutput && !isStackEmpty(stack) && areStacksEqual(stackReference, stack)) {
            if (getStackSize(stack) > largest) {
                slotNum = slot.slotNumber;
                largest = getStackSize(stack);
            }
        }
    }
    return slotNum;
}

int InventoryUtils::getSlotNumberOfSmallestStackFromDifferentInventory(Gui& gui, Slot const& slotReference, ItemStack const& stackReference, int idealSize) {
    int slotNumSmallest = -1, slotNumLargest = -1, smallest = 1 << 30, largest = 0;
    for (auto const& slot : gui.menu().slots()) {
        auto const& stack = gui.menu().getItem(slot);
        if (!areSlotsInSameInventory(slot, slotReference) && !slot.isOutput && !isStackEmpty(stack) && areStacksEqual(stackReference, stack)) {
            int size = getStackSize(stack);
            if (size < smallest && size >= idealSize) {
                slotNumSmallest = slot.slotNumber;
                smallest        = size;
            }
            if (size > largest) {
                slotNumLargest = slot.slotNumber;
                largest        = size;
            }
        }
    }
    return slotNumSmallest != -1 ? slotNumSmallest : slotNumLargest;
}

void InventoryUtils::dragSplitItemsIntoSlots(Gui& gui, std::vector<int> const& targetSlots) {
    auto const& cursor0 = gui.menu().getCarried();
    if (isStackEmpty(cursor0)) return;
    if (targetSlots.size() == 1) {
        gui.leftClickSlot(targetSlots.front());
        return;
    }
    ItemStack        ref(cursor0);
    int const        numSlots = static_cast<int>(gui.menu().slots().size());
    std::vector<int> valid;
    for (int slotNum : targetSlots) {
        if (slotNum >= numSlots) break;
        auto const* slot  = gui.menu().getSlot(slotNum);
        auto const& stack = gui.menu().getItem(*slot);
        if (!slot->isOutput && (isStackEmpty(stack) || (areStacksEqual(stack, ref) && getStackSize(stack) < static_cast<int>(ref.getMaxStackSize())))) valid.push_back(slotNum);
        if (static_cast<int>(valid.size()) >= getStackSize(ref)) break;
    }
    if (valid.empty()) return;
    int per = getStackSize(ref) / static_cast<int>(valid.size());
    if (per <= 0) return;
    for (int slotNum : valid) {
        auto const* slot  = gui.menu().getSlot(slotNum);
        auto const& stack = gui.menu().getItem(*slot);
        int         room  = static_cast<int>(ref.getMaxStackSize()) - getStackSize(stack);
        int         n     = std::min(per, room);
        if (n <= 0 || isStackEmpty(gui.menu().getCarried())) continue;
        gui.placeFromCursor(slotNum, n);
    }
}

void InventoryUtils::dropStacksUntilEmpty(Gui& gui, int slotNum) {
    auto const* slot = gui.menu().getSlot(slotNum);
    if (!slot) return;
    int failsafe = 64;
    while (failsafe-- > 0 && !isStackEmpty(gui.menu().getItem(*slot))) gui.dropStack(slotNum);
}

void InventoryUtils::dropStacksWhileHasItem(Gui& gui, int slotNum, ItemStack const& stackReference) {
    auto const* slot = gui.menu().getSlot(slotNum);
    if (!slot) return;
    ItemStack ref(stackReference);
    int       failsafe = 256;
    while (failsafe-- > 0 && areStacksEqual(gui.menu().getItem(*slot), ref)) gui.dropStack(slotNum);
}

void InventoryUtils::swapSlots(Gui& gui, int slotNum, int otherSlot) {
    gui.clickSlot(slotNum, 8, ClickType::SWAP);
    gui.clickSlot(otherSlot, 8, ClickType::SWAP);
    gui.clickSlot(slotNum, 8, ClickType::SWAP);
}

}
