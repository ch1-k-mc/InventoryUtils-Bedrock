// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/inventory/sandbox/ContainerSandbox.h"

#include <stdexcept>

#include "ipnext/inventory/sandbox/diffcalculator/DiffCalculator.h"

namespace inventory_utils::ipnext::inventory::sandbox {

void ContainerSandbox::leftClick(int slotIndex) {
    auto& cursor = mItems.cursor;
    auto& target = mItems.slots[slotIndex];
    if (cursor.isEmpty() || target.isEmpty() || !cursor.stackableWith(target)) cursor.swapWith(target);
    else cursor.transferTo(target);
    addClick(slotIndex, 0);
}

void ContainerSandbox::rightClick(int slotIndex) {
    auto& cursor = mItems.cursor;
    auto& target = mItems.slots[slotIndex];
    if (cursor.isEmpty()) target.splitHalfTo(cursor);
    else if (cursor.stackableWith(target)) cursor.transferOneTo(target);
    else cursor.swapWith(target);
    addClick(slotIndex, 1);
}

void ContainerSandbox::leftClickOutside() {
    mItems.thrownItems.add(mItems.cursor);
    mItems.cursor.setEmpty();
    addClick(-999, 0);
}

void ContainerSandbox::rightClickOutside() {
    item::ItemStack dummy;
    mItems.cursor.transferOneTo(dummy);
    mItems.thrownItems.add(dummy);
    addClick(-999, 1);
}

void ItemPlanner::innerSync() {
    if (!mTrackingItems) return;
    diffcalculator::DiffCalculator::apply(mInnerSandbox, *mTrackingItems);
    if (!(mInnerSandbox.items() == *mTrackingItems)) throw std::runtime_error("ContainerSandbox actual result not same as goal");
    mTrackingItems.reset();
}

void ItemPlanner::sandbox(std::function<void(ContainerSandbox&)> const& action) {
    innerSync();
    action(mInnerSandbox);
}

void ItemPlanner::tracker(std::function<void(ItemTracker&)> const& action) {
    int         syncId = mInnerSandbox.clickCount();
    ItemTracker before = itemTracker();
    ItemTracker after  = before.copyAsMutable();
    action(after);
    if (syncId != mInnerSandbox.clickCount()) throw std::runtime_error("ContainerSandbox out of sync");
    if (!(data::collect(before) == data::collect(after))) throw std::runtime_error("Unequal before and after item counts");
    mTrackingItems = std::move(after);
}

std::vector<SandboxClick> ItemPlanner::clicks() {
    innerSync();
    return mInnerSandbox.clicks();
}

}
