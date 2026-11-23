// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <string>
#include <vector>

#include "ipnext/inventory/data/ItemTracker.h"
#include "ipnext/item/rule/Rule.h"

namespace inventory_utils::ipnext::inventory::action {

using data::SubTracker;
using item::ItemStack;
using item::ItemType;

enum class PostAction { NONE, GROUP_IN_ROWS, GROUP_IN_COLUMNS, DISTRIBUTE_EVENLY, SHUFFLE, FILL_ONE, REVERSE };
PostAction postActionFromName(std::string const& name);

void restockFrom(SubTracker& self, SubTracker& another);
void moveAllTo(SubTracker& self, SubTracker& another, bool skipEmpty);
void moveAllTo(SubTracker& self, SubTracker& another);
void moveMatchTo(SubTracker& self, SubTracker& another);
void moveFocusMatchTo(SubTracker& self, SubTracker& another, ItemType const& focusType);
void refillStacksTo(SubTracker& self, SubTracker& another);
void moveSingle(SubTracker& self, SubTracker& another);
void moveMatchCraftingTo(SubTracker& self, SubTracker& crafting);
void sort(SubTracker& self, item::rule::Rule& sortingRule, PostAction postAction, bool isRectangular = false, int width = 0, int height = 0);

std::vector<int> distribute(int sum, int listSize);
std::vector<int> distributeMonotonic(int sum, int listSize);
std::vector<int> fillOne(int sum, int max, int listSize);
std::vector<int> pack(int sum, int max);
std::vector<ItemStack> distributeMonotonic(std::vector<ItemStack> const& list);
std::vector<ItemStack> spreadSlot(std::vector<ItemStack> const& list, int emptySlot = 0);
std::vector<ItemStack> spreadItemCount(std::vector<ItemStack> const& list);
std::vector<ItemStack> fillOne(std::vector<ItemStack> const& list);

namespace PostActions {
std::vector<ItemStack> groupInRows(std::vector<ItemStack> const& slots, int width, int height);
std::vector<ItemStack> groupInColumns(std::vector<ItemStack> const& slots, int width, int height);
}

}
