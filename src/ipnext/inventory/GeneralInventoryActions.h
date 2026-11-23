// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <string>

#include "ipnext/item/rule/Rule.h"

namespace inventory_utils::ipnext::inventory::GeneralInventoryActions {

void doSort(bool forcePlayer = false);
void doSortInColumns(bool forcePlayer = false);
void doSortInRows(bool forcePlayer = false);
void doMoveMatch();
void doMoveMatch(bool toPlayer);
void doThrowMatch();
void cleanCursor();

item::rule::RulePtr ruleFor(std::string const& sortingMethodIndividual, std::string const& customContent);
item::rule::RulePtr ruleForGlobal(std::string const& sortingMethod);

}
