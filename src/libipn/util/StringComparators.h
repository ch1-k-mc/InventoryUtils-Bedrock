// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/libIPN (master @89a2efa, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <functional>
#include <memory>
#include <string>

namespace inventory_utils::libipn::util {

using U16Comparator = std::function<int(std::u16string const&, std::u16string const&)>;

std::u16string toU16(std::string const& utf8);

int naturalOrder(std::u16string const& a, std::u16string const& b);
int caseInsensitiveOrder(std::u16string const& a, std::u16string const& b);

enum class CollatorStrength { PRIMARY, SECONDARY, TERTIARY, IDENTICAL };
U16Comparator makeCollator(std::string const& localeTag, CollatorStrength strength);

U16Comparator makeLogicalStringComparator(U16Comparator textComparator);

}
