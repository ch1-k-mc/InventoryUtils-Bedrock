// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "ipnext/item/ItemStack.h"

namespace inventory_utils::ipnext::inventory::data {

using item::ItemStack;
using item::ItemType;

class ItemBucket {
public:
    using Entry = std::pair<ItemType, int>;

    [[nodiscard]] int  size() const;
    [[nodiscard]] int  count(ItemType const& element) const;
    [[nodiscard]] bool contains(ItemType const& element) const { return contains(element, 1); }
    [[nodiscard]] bool contains(ItemType const& element, int n) const { return element.isEmpty() || count(element) >= n; }
    [[nodiscard]] bool contains(ItemStack const& stack) const { return contains(stack.itemType, stack.count); }
    [[nodiscard]] bool containsAll(ItemBucket const& another) const;
    [[nodiscard]] bool containsAll(std::vector<ItemStack> const& stacks) const;
    [[nodiscard]] bool isEmpty() const { return mEntries.empty(); }

    [[nodiscard]] int accumulateCount(ItemType const& element) const;

    bool add(ItemType const& element, int n);
    bool add(ItemStack const& stack) { return add(stack.itemType, stack.count); }
    void addAll(std::vector<ItemStack> const& stacks);
    bool addAll(ItemBucket const& another);
    bool remove(ItemType const& element, int n);
    bool remove(ItemStack const& stack) { return remove(stack.itemType, stack.count); }
    void removeAll(std::vector<ItemStack> const& stacks);
    bool removeAll(ItemBucket const& another);
    void clear() { mEntries.clear(); }

    [[nodiscard]] ItemBucket operator-(ItemBucket const& another) const;
    [[nodiscard]] ItemBucket copyAsMutable() const { return *this; }

    [[nodiscard]] std::vector<ItemType> elementSet() const;
    [[nodiscard]] std::vector<Entry> const& entrySet() const { return mEntries; }

    bool operator==(ItemBucket const& other) const;

    [[nodiscard]] std::string toString() const;

private:
    [[nodiscard]] Entry const* find(ItemType const& element) const;
    Entry*                     find(ItemType const& element);

    std::vector<Entry> mEntries;
};

using MutableItemBucket = ItemBucket;

}
