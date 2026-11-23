// SPDX-License-Identifier: AGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0) and Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (C) masa and contributors; Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <functional>
#include <string>

#include "platform/inventory/ContainerMenu.h"

class ContainerScreenController;

namespace inventory_utils::platform::inventory {

enum class ClickType { PICKUP, QUICK_MOVE, SWAP, CLONE, THROW, QUICK_CRAFT, PICKUP_ALL };

inline constexpr int SLOT_OUTSIDE = -999;

class SlotClicker {
public:
    SlotClicker(ContainerScreenController& screen, ContainerMenu menu) : mScreen(&screen), mMenu(std::move(menu)) {}

    static SlotClicker current();

    [[nodiscard]] bool                 valid() const { return mScreen != nullptr; }
    [[nodiscard]] ContainerMenu const& menu() const { return mMenu; }

    [[nodiscard]] MenuSlot const* getSlotUnderMouse() const;

    bool clickSlot(int slotNum, int mouseButton, ClickType type);

    bool leftClickSlot(int slotNum) { return clickSlot(slotNum, 0, ClickType::PICKUP); }
    bool rightClickSlot(int slotNum) { return clickSlot(slotNum, 1, ClickType::PICKUP); }
    bool shiftClickSlot(int slotNum) { return clickSlot(slotNum, 0, ClickType::QUICK_MOVE); }
    bool dropItem(int slotNum) { return clickSlot(slotNum, 0, ClickType::THROW); }
    bool dropStack(int slotNum) { return clickSlot(slotNum, 1, ClickType::THROW); }
    bool dropItemsFromCursor() { return clickSlot(SLOT_OUTSIDE, 0, ClickType::PICKUP); }
    bool placeFromCursor(int slotNum, int amount);

    using QuickMoveFilter = std::function<bool(MenuSlot const& slot, bool isThrow, ItemStack const& item)>;
    static void setQuickMoveFilter(QuickMoveFilter filter);

    [[nodiscard]] int                sentCount() const { return mSent; }
    [[nodiscard]] std::string const& lastError() const { return mLastError; }

private:
    SlotClicker() = default;

    bool pickup(MenuSlot const& slot, int button);
    bool quickMove(MenuSlot const& slot);
    bool swapWithHotbar(MenuSlot const& slot, int hotbarIndex);
    bool throwItems(MenuSlot const* slot, bool wholeStack);
    bool pickupAll(MenuSlot const& slot);
    bool fail(std::string reason);

    ContainerScreenController* mScreen = nullptr;
    ContainerMenu              mMenu;
    int                        mSent = 0;
    std::string                mLastError;
};

}
