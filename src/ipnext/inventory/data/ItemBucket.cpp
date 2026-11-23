// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/inventory/data/ItemBucket.h"

#include <algorithm>

#include "ipnext/config/Configs.h"

namespace inventory_utils::ipnext::inventory::data {

ItemBucket::Entry const* ItemBucket::find(ItemType const& element) const {
    for (auto const& e : mEntries) {
        if (element == e.first) return &e;
    }
    return nullptr;
}

ItemBucket::Entry* ItemBucket::find(ItemType const& element) {
    for (auto& e : mEntries) {
        if (element == e.first) return &e;
    }
    return nullptr;
}

int ItemBucket::size() const {
    int n = 0;
    for (auto const& e : mEntries) n += e.second;
    return n;
}

int ItemBucket::count(ItemType const& element) const {
    auto e = find(element);
    return e ? e->second : 0;
}

bool ItemBucket::containsAll(ItemBucket const& another) const {
    return std::all_of(another.mEntries.begin(), another.mEntries.end(), [&](Entry const& e) { return contains(e.first, e.second); });
}

bool ItemBucket::containsAll(std::vector<ItemStack> const& stacks) const {
    return std::all_of(stacks.begin(), stacks.end(), [&](ItemStack const& s) { return contains(s); });
}

int ItemBucket::accumulateCount(ItemType const& element) const {
    if (config::SortSettings::TREAT_NON_STACKABLE_AS_FULL_STACKS.getBooleanValue()) {
        int maxCount = 1;
        for (auto const& e : mEntries) maxCount = std::max(maxCount, e.first.maxCount());
        int sum = 0;
        for (auto const& e : mEntries) {
            if (!(element == e.first)) continue;
            sum += e.first.maxCount() == 1 ? e.second * maxCount : e.second;
        }
        return sum;
    }
    int sum = 0;
    for (auto const& e : mEntries) {
        if (element == e.first) sum += e.second;
    }
    return sum;
}

bool ItemBucket::add(ItemType const& element, int n) {
    if (element.isEmpty() || n <= 0) return false;
    if (auto e = find(element)) e->second += n;
    else mEntries.emplace_back(element, n);
    return true;
}

void ItemBucket::addAll(std::vector<ItemStack> const& stacks) {
    for (auto const& s : stacks) add(s);
}

bool ItemBucket::addAll(ItemBucket const& another) {
    bool any = false;
    for (auto const& e : another.mEntries) any = add(e.first, e.second) || any;
    return any;
}

bool ItemBucket::remove(ItemType const& element, int n) {
    if (element.isEmpty() || n <= 0) return false;
    auto it = std::find_if(mEntries.begin(), mEntries.end(), [&](Entry const& e) { return element == e.first; });
    if (it == mEntries.end() || it->second <= 0) return false;
    it->second -= n;
    if (it->second <= 0) mEntries.erase(it);
    return true;
}

void ItemBucket::removeAll(std::vector<ItemStack> const& stacks) {
    for (auto const& s : stacks) remove(s);
}

bool ItemBucket::removeAll(ItemBucket const& another) {
    bool any = false;
    for (auto const& e : another.mEntries) any = remove(e.first, e.second) || any;
    return any;
}

ItemBucket ItemBucket::operator-(ItemBucket const& another) const {
    ItemBucket b = *this;
    b.removeAll(another);
    return b;
}

std::vector<ItemType> ItemBucket::elementSet() const {
    std::vector<ItemType> out;
    out.reserve(mEntries.size());
    for (auto const& e : mEntries) out.push_back(e.first);
    return out;
}

bool ItemBucket::operator==(ItemBucket const& other) const {
    if (mEntries.size() != other.mEntries.size()) return false;
    for (auto const& e : mEntries) {
        auto o = other.find(e.first);
        if (!o || o->second != e.second) return false;
    }
    return true;
}

std::string ItemBucket::toString() const {
    std::string s = "[";
    for (size_t i = 0; i < mEntries.size(); ++i) {
        if (i) s += ", ";
        s += std::to_string(mEntries[i].second) + "x " + mEntries[i].first.toString();
    }
    return s + "]";
}

}
