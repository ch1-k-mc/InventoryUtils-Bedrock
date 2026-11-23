// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <map>
#include <string>
#include <vector>

#include "ipnext/inventory/data/ItemBucket.h"

namespace inventory_utils::ipnext::inventory::data {

class SubTracker;

class ItemTracker {
public:
    ItemStack              cursor;
    std::vector<ItemStack> slots;
    ItemBucket             thrownItems;

    ItemTracker() = default;
    ItemTracker(ItemStack c, std::vector<ItemStack> s, ItemBucket t = {}) : cursor(std::move(c)), slots(std::move(s)), thrownItems(std::move(t)) {}

    [[nodiscard]] ItemTracker copyAsMutable() const { return *this; }
    [[nodiscard]] SubTracker  subTracker();
    [[nodiscard]] SubTracker  subTracker(std::vector<int> slotIndices);

    bool operator==(ItemTracker const& o) const { return cursor == o.cursor && slots == o.slots && thrownItems == o.thrownItems; }

    [[nodiscard]] std::string toString() const;
};

using MutableItemTracker = ItemTracker;

class SubTracker {
public:
    SubTracker(ItemTracker& main, std::vector<int> slotIndices) : mMain(&main), mSlotIndices(std::move(slotIndices)) {}

    [[nodiscard]] ItemTracker&            mainTracker() const { return *mMain; }
    [[nodiscard]] std::vector<int> const& slotIndices() const { return mSlotIndices; }
    [[nodiscard]] std::vector<ItemStack*> slots() const;
    [[nodiscard]] std::vector<std::pair<int, ItemStack*>> indexedSlots() const;

    [[nodiscard]] SubTracker operator+(SubTracker const& another) const;

private:
    ItemTracker*     mMain;
    std::vector<int> mSlotIndices;
};

using MutableSubTracker = SubTracker;

ItemBucket collect(ItemTracker const& tracker);
ItemBucket collect(std::vector<ItemStack> const& stacks);
std::vector<ItemStack> filterNotEmpty(std::vector<ItemStack> const& stacks);
std::vector<ItemType> itemTypes(std::vector<ItemStack> const& stacks, bool ignoreDurability = false);
std::vector<ItemStack> derefCopy(std::vector<ItemStack*> const& stacks);

class ItemStat {
public:
    struct GroupEntry {
        ItemType         itemType;
        int              itemCount = 0;
        int              slotCount = 0;
        std::vector<int> slotIndices;
        [[nodiscard]] int minSlotCount() const {
            int m = itemType.maxCount();
            return (itemCount + m - 1) / m;
        }
    };

    explicit ItemStat(std::vector<ItemStack> const& items);
    explicit ItemStat(std::vector<std::pair<int, ItemStack>> const& indexedItems);

    [[nodiscard]] std::vector<GroupEntry> const& groupEntries() const { return mGroups; }
    [[nodiscard]] GroupEntry const*              group(ItemType const& type) const;
    [[nodiscard]] std::vector<ItemType>          itemTypes() const;
    [[nodiscard]] int                            totalItemCount() const;
    [[nodiscard]] int                            totalSlotCount() const;
    [[nodiscard]] int                            totalMinSlotCount() const;
    [[nodiscard]] int                            totalMaxSlotCount() const { return totalItemCount(); }

private:
    void                    add(int index, ItemStack const& item);
    std::vector<GroupEntry> mGroups;
};

}
