// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <functional>
#include <vector>

#include "ipnext/inventory/ContainerTypes.h"

namespace inventory_utils::ipnext::inventory {

class ItemArea {
public:
    struct Point {
        int x = 0, y = 0;
    };

    ItemArea() = default;
    ItemArea(std::vector<Point> fromSlotLocations, std::vector<int> slotIndices, bool orderSensitive = false);
    static ItemArea of(std::vector<MenuSlot> const& slots, std::vector<int> slotIndices, bool orderSensitive = false);

    [[nodiscard]] bool                    isEmpty() const { return mSlotIndices.empty(); }
    [[nodiscard]] std::vector<int> const& slotIndices() const { return mSlotIndices; }
    [[nodiscard]] bool                    isRectangular() const { return mIsRectangular; }
    [[nodiscard]] int                     width() const { return mWidth; }
    [[nodiscard]] int                     height() const { return mHeight; }
    [[nodiscard]] bool                    orderSensitive() const { return mOrderSensitive; }

    ItemArea operator+(ItemArea const& other) const;
    ItemArea operator-(ItemArea const& other) const;

private:
    void checkRectangular();

    std::vector<Point> mFrom;
    std::vector<int>   mSlotIndices;
    bool               mOrderSensitive = false;
    bool               mIsRectangular  = false;
    int                mWidth = 0, mHeight = 0;
};

class AreaType {
public:
    using Fn = std::function<ItemArea(VanillaContainer const&)>;
    AreaType() = default;
    explicit AreaType(Fn fn) : mFn(std::move(fn)) {}

    [[nodiscard]] ItemArea getItemArea(VanillaContainer const& c) const { return mFn ? mFn(c) : ItemArea(); }

    AreaType operator+(AreaType const& other) const;
    AreaType operator-(AreaType const& other) const;

    static AreaType inSlots(std::function<std::vector<MenuSlot const*>(VanillaContainer const&)> elements, bool orderSensitive = false);
    static AreaType match(std::function<bool(MenuSlot const&)> predicate, bool orderSensitive = false);
    static AreaType playerInvSlots(std::function<std::vector<int>()> invSlots, bool orderSensitive = false);

private:
    Fn mFn;
};

namespace AreaTypes {
AreaType const& focusedSlot();
AreaType const& playerStorage();
AreaType const& playerHotbar();
AreaType const& playerMainhand();
AreaType const& playerOffhand();
AreaType const& playerHands();
AreaType const& itemStorage();
AreaType const& sortableItemStorage();
AreaType const& lockedSlots();
AreaType const& craftingIngredient();
AreaType disabled(VanillaContainer const& container);
}

}
