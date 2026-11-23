// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "ipnext/inventory/data/ItemTracker.h"

namespace inventory_utils::ipnext::inventory::sandbox {

using data::ItemTracker;

struct SandboxClick {
    int clickIndex = 0;
    int slotIndex  = 0;
    int button     = 0;
};

class ContainerSandbox {
public:
    explicit ContainerSandbox(ItemTracker items) : mItems(std::move(items)) {}

    [[nodiscard]] ItemTracker const& items() const { return mItems; }

    void leftClick(int slotIndex);
    void rightClick(int slotIndex);
    void leftClickOutside();
    void rightClickOutside();

    [[nodiscard]] int                              clickCount() const { return static_cast<int>(mClicks.size()); }
    [[nodiscard]] std::vector<SandboxClick> const& clicks() const { return mClicks; }

private:
    void addClick(int slotIndex, int button) { mClicks.push_back({clickCount(), slotIndex, button}); }

    ItemTracker               mItems;
    std::vector<SandboxClick> mClicks;
};

class ItemPlanner {
public:
    explicit ItemPlanner(ItemTracker items) : mInnerSandbox(std::move(items)) {}

    void sandbox(std::function<void(ContainerSandbox&)> const& action);
    void tracker(std::function<void(ItemTracker&)> const& action);

    [[nodiscard]] std::vector<SandboxClick> clicks();

private:
    void                              innerSync();
    [[nodiscard]] ItemTracker const& itemTracker() const { return mTrackingItems ? *mTrackingItems : mInnerSandbox.items(); }

    ContainerSandbox           mInnerSandbox;
    std::optional<ItemTracker> mTrackingItems;
};

}
