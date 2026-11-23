// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "platform/inventory/SlotClicker.h"

namespace inventory_utils::ipnext::inventory {

using platform::inventory::MenuSlot;

enum class ContainerType {
    PLAYER,
    CREATIVE,
    PURE_BACKPACK,
    TEMP_SLOTS,
    NO_SORTING_STORAGE,
    SORTABLE_STORAGE,
    HORSE_STORAGE,
    CRAFTING,
    STONECUTTER,
    TRADER,
    ANVIL,
    RECTANGULAR,
    WIDTH_9,
    HEIGHT_3,
    CUSTOM,
};

using ContainerTypeSet = std::set<ContainerType>;

class VanillaContainer {
public:
    static std::optional<VanillaContainer> current();

    [[nodiscard]] std::vector<MenuSlot> const& slots() const { return mClicker.menu().slots(); }
    [[nodiscard]] std::string const&           screenName() const { return mClicker.menu().screenName(); }
    [[nodiscard]] MenuSlot const*              focusedSlot() const { return mClicker.getSlotUnderMouse(); }
    [[nodiscard]] ::ItemStack const&           cursorStack() const { return mClicker.menu().getCarried(); }
    [[nodiscard]] ::ItemStack const&           itemOf(MenuSlot const& slot) const { return mClicker.menu().getItem(slot); }
    [[nodiscard]] platform::inventory::SlotClicker& clicker() { return mClicker; }

private:
    explicit VanillaContainer(platform::inventory::SlotClicker clicker) : mClicker(std::move(clicker)) {}
    platform::inventory::SlotClicker mClicker;
};

inline bool isPlayerInventory(MenuSlot const& s) { return s.kind != platform::inventory::InventoryKind::Container; }

namespace ContainerTypes {
ContainerTypeSet getTypes(VanillaContainer const& container);
ContainerTypeSet getTypes(std::string const& screenName, bool hasSlots = true);
}

inline bool contains(ContainerTypeSet const& s, ContainerType t) { return s.contains(t); }
inline bool containsAny(ContainerTypeSet const& s, std::initializer_list<ContainerType> ts) {
    for (auto t : ts) {
        if (s.contains(t)) return true;
    }
    return false;
}

}
