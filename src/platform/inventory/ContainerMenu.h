// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <map>
#include <string>
#include <vector>

#include "mc/world/item/ItemStack.h"

class ContainerManagerController;
class ContainerScreenController;

namespace inventory_utils::platform::inventory {

enum class InventoryKind {
    Container,
    Player,
    Hotbar,
    Offhand,
    Armor,
};

struct MenuSlot {
    int           slotNumber = -1;
    std::string   collection;
    int           index = 0;
    InventoryKind kind  = InventoryKind::Container;
    int  inventoryId = 0;
    int  containerSlot = 0;
    bool isOutput      = false;
    int  x = 0, y = 0;
};

class ContainerMenu {
public:
    static ContainerMenu fromManager(ContainerManagerController& mgr, std::string const& screenName, ContainerScreenController* screen = nullptr);

    [[nodiscard]] std::vector<MenuSlot> const& slots() const { return mSlots; }
    [[nodiscard]] MenuSlot const*              getSlot(int slotNumber) const;
    [[nodiscard]] int                          findSlot(std::string const& collection, int index) const;
    [[nodiscard]] std::string const&           screenName() const { return mScreen; }

    [[nodiscard]] ItemStack const& getItem(MenuSlot const& slot) const;
    [[nodiscard]] ItemStack const& getCarried() const;

    [[nodiscard]] ContainerManagerController& manager() const { return *mManager; }

private:
    ContainerManagerController*        mManager = nullptr;
    ContainerScreenController*         mScreenController = nullptr;
    std::string                        mScreen;
    std::vector<MenuSlot>              mSlots;
    mutable std::map<int, ItemStack>   mOutputCache;
};

}
