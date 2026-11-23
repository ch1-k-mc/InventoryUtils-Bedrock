// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/item/ItemStack.h"

#include <algorithm>

namespace inventory_utils::ipnext::item {

ItemStack ItemStack::of(VanillaItemStack const& stack) {
    if (stack.isNull() || stack.mCount == 0) return {};
    return {ItemType::of(stack), static_cast<int>(stack.mCount)};
}

void ItemStack::transferNTo(ItemStack& another, int n) {
    if (!stackableWith(another)) return;
    if (overstacked() && !another.isEmpty()) return;
    if (isEmpty()) return;
    if (another.isEmpty()) {
        another.itemType = itemType;
        another.count    = 0;
    }
    int transferable = overstacked() ? count : std::max(0, std::min(n, std::min(count, another.room())));
    if (transferable == 0) return;
    count -= transferable;
    another.count += transferable;
    normalize();
    another.normalize();
}

}
