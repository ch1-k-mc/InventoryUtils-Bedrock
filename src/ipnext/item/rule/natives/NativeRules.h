// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <functional>
#include <map>
#include <string>

#include "ipnext/item/rule/Rule.h"

namespace inventory_utils::ipnext::item::rule::natives {

struct NativeRules {
    static RulePtr create(std::string const& name);
    static std::vector<std::string> names();
};

}
