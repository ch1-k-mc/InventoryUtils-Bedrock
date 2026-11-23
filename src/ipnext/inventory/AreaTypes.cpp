// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/inventory/AreaTypes.h"

#include <algorithm>
#include <map>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"

#include "ipnext/event/LockSlotsHandler.h"

namespace inventory_utils::ipnext::inventory {

namespace {

std::vector<int> distinctConcat(std::vector<int> a, std::vector<int> const& b) {
    for (int i : b) {
        if (std::find(a.begin(), a.end(), i) == a.end()) a.push_back(i);
    }
    return a;
}

std::vector<int> minusDistinct(std::vector<int> const& a, std::vector<int> const& b) {
    std::vector<int> out;
    for (int i : a) {
        if (std::find(b.begin(), b.end(), i) != b.end()) continue;
        if (std::find(out.begin(), out.end(), i) == out.end()) out.push_back(i);
    }
    return out;
}

bool findRectangular(std::vector<ItemArea::Point> const& points, int& width, int& height, std::vector<int>& order) {
    if (points.empty()) return false;
    std::map<int, std::vector<int>> byY;
    for (size_t i = 0; i < points.size(); ++i) byY[points[i].y].push_back(static_cast<int>(i));
    int total = static_cast<int>(points.size());
    int h     = static_cast<int>(byY.size());
    if (total % h != 0) return false;
    int w = total / h;
    if (w == 1 || h == 1) return false;
    for (auto& [y, list] : byY) {
        if (static_cast<int>(list.size()) != w) return false;
        std::stable_sort(list.begin(), list.end(), [&](int a, int b) { return points[a].x < points[b].x; });
    }
    auto const& first = byY.begin()->second;
    for (int i = 0; i < w; ++i) {
        for (auto const& [y, list] : byY) {
            if (points[list[i]].x != points[first[i]].x) return false;
        }
    }
    width  = w;
    height = h;
    order.clear();
    for (auto const& [y, list] : byY) order.insert(order.end(), list.begin(), list.end());
    return true;
}

int selectedHotbarSlot() {
    auto ci = ll::service::getClientInstance();
    if (!ci) return 0;
    auto* player = ci->getLocalPlayer();
    return player ? player->getSelectedItemSlot() : 0;
}

}

ItemArea::ItemArea(std::vector<Point> fromSlotLocations, std::vector<int> slotIndices, bool orderSensitive)
: mFrom(std::move(fromSlotLocations)),
  mSlotIndices(std::move(slotIndices)),
  mOrderSensitive(orderSensitive) {
    checkRectangular();
}

ItemArea ItemArea::of(std::vector<MenuSlot> const& slots, std::vector<int> slotIndices, bool orderSensitive) {
    std::vector<Point> points;
    points.reserve(slots.size());
    for (auto const& s : slots) points.push_back({s.x, s.y});
    return ItemArea(std::move(points), std::move(slotIndices), orderSensitive);
}

void ItemArea::checkRectangular() {
    mIsRectangular = false;
    mWidth = mHeight = 0;
    if (mOrderSensitive) return;
    std::vector<Point> pts;
    for (int i : mSlotIndices) pts.push_back(mFrom[i]);
    int              w = 0, h = 0;
    std::vector<int> order;
    if (findRectangular(pts, w, h, order)) {
        mIsRectangular = true;
        mWidth         = w;
        mHeight        = h;
        std::vector<int> reordered(mSlotIndices.size());
        for (size_t newIndex = 0; newIndex < order.size(); ++newIndex) reordered[newIndex] = mSlotIndices[order[newIndex]];
        mSlotIndices = std::move(reordered);
    } else {
        int total = static_cast<int>(mSlotIndices.size());
        if (total % 9 == 0) {
            mIsRectangular = true;
            mWidth         = 9;
            mHeight        = total / 9;
        }
    }
}

ItemArea ItemArea::operator+(ItemArea const& other) const {
    return ItemArea(mFrom, distinctConcat(mSlotIndices, other.mSlotIndices), mOrderSensitive || other.mOrderSensitive);
}

ItemArea ItemArea::operator-(ItemArea const& other) const {
    return ItemArea(mFrom, minusDistinct(mSlotIndices, other.mSlotIndices), mOrderSensitive || other.mOrderSensitive);
}

AreaType AreaType::operator+(AreaType const& other) const {
    auto a = *this, b = other;
    return AreaType([a, b](VanillaContainer const& c) { return a.getItemArea(c) + b.getItemArea(c); });
}

AreaType AreaType::operator-(AreaType const& other) const {
    auto a = *this, b = other;
    return AreaType([a, b](VanillaContainer const& c) { return a.getItemArea(c) - b.getItemArea(c); });
}

AreaType AreaType::inSlots(std::function<std::vector<MenuSlot const*>(VanillaContainer const&)> elements, bool orderSensitive) {
    return AreaType([elements, orderSensitive](VanillaContainer const& c) {
        auto             set   = elements(c);
        auto const&      slots = c.slots();
        std::vector<int> indices;
        for (size_t i = 0; i < slots.size(); ++i) {
            for (auto const* e : set) {
                if (e && e->slotNumber == slots[i].slotNumber) {
                    indices.push_back(static_cast<int>(i));
                    break;
                }
            }
        }
        return ItemArea::of(slots, std::move(indices), orderSensitive);
    });
}

AreaType AreaType::match(std::function<bool(MenuSlot const&)> predicate, bool orderSensitive) {
    return AreaType([predicate, orderSensitive](VanillaContainer const& c) {
        auto const&      slots = c.slots();
        std::vector<int> indices;
        for (size_t i = 0; i < slots.size(); ++i) {
            if (predicate(slots[i])) indices.push_back(static_cast<int>(i));
        }
        return ItemArea::of(slots, std::move(indices), orderSensitive);
    });
}

AreaType AreaType::playerInvSlots(std::function<std::vector<int>()> invSlots, bool) {
    return AreaType([invSlots](VanillaContainer const& c) {
        auto const&        slots = c.slots();
        std::map<int, int> slotIndexOfInvSlot;
        for (size_t i = 0; i < slots.size(); ++i) {
            if (isPlayerInventory(slots[i])) slotIndexOfInvSlot[slots[i].containerSlot] = static_cast<int>(i);
        }
        std::vector<int> indices;
        for (int inv : invSlots()) {
            if (auto it = slotIndexOfInvSlot.find(inv); it != slotIndexOfInvSlot.end()) indices.push_back(it->second);
        }
        return ItemArea::of(slots, std::move(indices));
    });
}

namespace AreaTypes {

namespace {

std::vector<int> range(int from, int to) {
    std::vector<int> v;
    for (int i = from; i <= to; ++i) v.push_back(i);
    return v;
}

ItemArea storageArea(VanillaContainer const& c, bool sortableOnly) {
    auto             types = ContainerTypes::getTypes(c);
    auto const&      slots = c.slots();
    std::vector<int> indices;
    bool             ok = types.contains(ContainerType::SORTABLE_STORAGE) || (!sortableOnly && types.contains(ContainerType::NO_SORTING_STORAGE));
    if (ok) {
        bool isHorse = types.contains(ContainerType::HORSE_STORAGE);
        for (size_t i = 0; i < slots.size(); ++i) {
            auto const& s = slots[i];
            if (isPlayerInventory(s)) continue;
            if (s.isOutput) continue;
            if (isHorse && s.containerSlot >= 0 && s.containerSlot <= 1) continue;
            indices.push_back(static_cast<int>(i));
        }
    }
    return ItemArea::of(slots, std::move(indices));
}

}

AreaType const& focusedSlot() {
    static AreaType const a = AreaType::inSlots([](VanillaContainer const& c) {
        std::vector<MenuSlot const*> v;
        if (auto* s = c.focusedSlot()) v.push_back(s);
        return v;
    });
    return a;
}
AreaType const& playerStorage() {
    static AreaType const a = AreaType::playerInvSlots([] { return range(9, 35); });
    return a;
}
AreaType const& playerHotbar() {
    static AreaType const a = AreaType::playerInvSlots([] { return range(0, 8); });
    return a;
}
AreaType const& playerMainhand() {
    static AreaType const a = AreaType::playerInvSlots([] { return std::vector<int>{selectedHotbarSlot()}; }, true);
    return a;
}
AreaType const& playerOffhand() {
    static AreaType const a = AreaType::playerInvSlots([] { return std::vector<int>{40}; }, true);
    return a;
}
AreaType const& playerHands() {
    static AreaType const a = playerMainhand() + playerOffhand();
    return a;
}
AreaType const& itemStorage() {
    static AreaType const a([](VanillaContainer const& c) { return storageArea(c, false); });
    return a;
}
AreaType const& sortableItemStorage() {
    static AreaType const a([](VanillaContainer const& c) { return storageArea(c, true); });
    return a;
}
AreaType const& lockedSlots() {
    static AreaType const a = AreaType::playerInvSlots([] { return event::LockSlotsHandler::get().lockedInvSlots(); });
    return a;
}
AreaType const& craftingIngredient() {
    static AreaType const a = AreaType::match([](MenuSlot const& s) { return s.collection == "crafting_input_items"; });
    return a;
}
AreaType disabled(VanillaContainer const&) {
    return AreaType::match([](MenuSlot const&) { return false; });
}

}

}
