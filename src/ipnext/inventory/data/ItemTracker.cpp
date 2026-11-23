// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/inventory/data/ItemTracker.h"

#include <algorithm>

namespace inventory_utils::ipnext::inventory::data {

SubTracker ItemTracker::subTracker() {
    std::vector<int> all(slots.size());
    for (size_t i = 0; i < slots.size(); ++i) all[i] = static_cast<int>(i);
    return {*this, std::move(all)};
}

SubTracker ItemTracker::subTracker(std::vector<int> slotIndices) { return {*this, std::move(slotIndices)}; }

std::string ItemTracker::toString() const {
    std::string s = "cursor: " + cursor.toString() + ", slots: [";
    for (size_t i = 0; i < slots.size(); ++i) {
        if (i) s += ", ";
        s += slots[i].toString();
    }
    return s + "], thrownItems: " + thrownItems.toString();
}

std::vector<ItemStack*> SubTracker::slots() const {
    std::vector<ItemStack*> out;
    out.reserve(mSlotIndices.size());
    for (int i : mSlotIndices) out.push_back(&mMain->slots[i]);
    return out;
}

std::vector<std::pair<int, ItemStack*>> SubTracker::indexedSlots() const {
    std::vector<std::pair<int, ItemStack*>> out;
    out.reserve(mSlotIndices.size());
    for (int i : mSlotIndices) out.emplace_back(i, &mMain->slots[i]);
    return out;
}

SubTracker SubTracker::operator+(SubTracker const& another) const {
    std::vector<int> merged = mSlotIndices;
    for (int i : another.mSlotIndices) {
        if (std::find(merged.begin(), merged.end(), i) == merged.end()) merged.push_back(i);
    }
    return {*mMain, std::move(merged)};
}

ItemBucket collect(ItemTracker const& tracker) {
    ItemBucket b = tracker.thrownItems;
    b.add(tracker.cursor);
    b.addAll(tracker.slots);
    return b;
}

ItemBucket collect(std::vector<ItemStack> const& stacks) {
    ItemBucket b;
    b.addAll(stacks);
    return b;
}

std::vector<ItemStack> filterNotEmpty(std::vector<ItemStack> const& stacks) {
    std::vector<ItemStack> out;
    for (auto const& s : stacks) {
        if (!s.isEmpty()) out.push_back(s);
    }
    return out;
}

std::vector<ItemType> itemTypes(std::vector<ItemStack> const& stacks, bool ignoreDurability) {
    std::vector<ItemType> out;
    for (auto const& s : stacks) {
        if (s.isEmpty()) continue;
        auto t = s.itemType.copy(ignoreDurability);
        if (std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
    }
    return out;
}

std::vector<ItemStack> derefCopy(std::vector<ItemStack*> const& stacks) {
    std::vector<ItemStack> out;
    out.reserve(stacks.size());
    for (auto* s : stacks) out.push_back(*s);
    return out;
}

ItemStat::ItemStat(std::vector<ItemStack> const& items) {
    for (size_t i = 0; i < items.size(); ++i) {
        if (!items[i].isEmpty()) add(static_cast<int>(i), items[i]);
    }
}

ItemStat::ItemStat(std::vector<std::pair<int, ItemStack>> const& indexedItems) {
    for (auto const& [i, s] : indexedItems) {
        if (!s.isEmpty()) add(i, s);
    }
}

void ItemStat::add(int index, ItemStack const& item) {
    auto it = std::find_if(mGroups.begin(), mGroups.end(), [&](GroupEntry const& g) { return item.itemType == g.itemType; });
    if (it == mGroups.end()) {
        mGroups.push_back(GroupEntry{item.itemType});
        it = mGroups.end() - 1;
    }
    it->itemCount += item.count;
    it->slotCount++;
    it->slotIndices.push_back(index);
}

ItemStat::GroupEntry const* ItemStat::group(ItemType const& type) const {
    for (auto const& g : mGroups) {
        if (type == g.itemType) return &g;
    }
    return nullptr;
}

std::vector<ItemType> ItemStat::itemTypes() const {
    std::vector<ItemType> out;
    for (auto const& g : mGroups) out.push_back(g.itemType);
    return out;
}

int ItemStat::totalItemCount() const {
    int n = 0;
    for (auto const& g : mGroups) n += g.itemCount;
    return n;
}

int ItemStat::totalSlotCount() const {
    int n = 0;
    for (auto const& g : mGroups) n += g.slotCount;
    return n;
}

int ItemStat::totalMinSlotCount() const {
    int n = 0;
    for (auto const& g : mGroups) n += g.minSlotCount();
    return n;
}

}
