// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <string>
#include <vector>

#include "ipnext/item/ItemType.h"

namespace inventory_utils::ipnext::item {

struct ItemStack {
    ItemType itemType;
    int      count = 0;

    ItemStack() = default;
    ItemStack(ItemType type, int n) : itemType(std::move(type)), count(n) {}

    static ItemStack EMPTY() { return {}; }
    static ItemStack of(VanillaItemStack const& stack);

    [[nodiscard]] bool overstacked() const { return count > itemType.maxCount(); }

    bool operator==(ItemStack const& o) const {
        if (isEmpty() && o.isEmpty()) return true;
        return itemType == o.itemType && count == o.count;
    }

    [[nodiscard]] bool isEmpty() const { return itemType.isEmpty() || count <= 0; }
    [[nodiscard]] bool isFull() const { return count >= itemType.maxCount(); }
    [[nodiscard]] bool isNotFull() const { return !isEmpty() && !isFull(); }
    [[nodiscard]] int  room() const { return itemType.maxCount() - count; }
    [[nodiscard]] bool stackableWith(ItemStack const& b) const {
        return (itemType.maxCount() > 1 && itemType == b.itemType) || isEmpty() || b.isEmpty();
    }

    void setEmpty() {
        itemType = ItemType();
        count    = 0;
    }
    void swapWith(ItemStack& another) {
        std::swap(itemType, another.itemType);
        std::swap(count, another.count);
    }
    void transferTo(ItemStack& another) { transferNTo(another, count); }
    void transferOneTo(ItemStack& another) { transferNTo(another, 1); }
    void transferNTo(ItemStack& another, int n);
    void splitHalfTo(ItemStack& cursor) { transferNTo(cursor, count - count / 2); }

    [[nodiscard]] ItemStack copyAsMutable() const { return *this; }
    [[nodiscard]] std::string toString() const { return std::to_string(count) + "x " + itemType.toString(); }

private:
    void normalize() {
        if (itemType.isEmpty() || count <= 0) setEmpty();
    }
};

using MutableItemStack = ItemStack;

}
