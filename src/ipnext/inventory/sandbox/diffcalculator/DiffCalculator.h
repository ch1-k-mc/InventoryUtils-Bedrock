// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <stdexcept>
#include <string>

#include "ipnext/inventory/sandbox/ContainerSandbox.h"

namespace inventory_utils::ipnext::inventory::sandbox::diffcalculator {

class NoRoomException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct DiffCalculator {
    static void apply(ContainerSandbox& sandbox, ItemTracker const& goalTracker);
};

namespace util {
int  calcRank(int n, int g);
int  clickCountLowerBound(int n, int g);
int  clickCountUpperBound(int n, int g);
int  clickCountSingleSlotToLess(int from, int to);
bool canRight(int from, int to);
}

}
