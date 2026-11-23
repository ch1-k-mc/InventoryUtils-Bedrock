// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <memory>
#include <string>

#include "ipnext/item/NbtUtils.h"
#include "ipnext/item/rule/Rule.h"

namespace inventory_utils::ipnext::item::rule::parameter {

enum class StringCompare { UNICODE_, IGNORE_CASE, LOCALE };
enum class Strength { PRIMARY, SECONDARY, TERTIARY, IDENTICAL };
enum class NumberOrder { ASCENDING, DESCENDING };
enum class Match { FIRST, LAST };
enum class RequireNbt { REQUIRED, NO_NBT, NOT_REQUIRED };

inline int multiplier(Match m) { return m == Match::FIRST ? 1 : -1; }
int        compareNumber(NumberOrder order, double a, double b);
bool       requireNbtMatch(RequireNbt r, ItemType const& itemType);

class ItemTypeMatcher {
public:
    static std::optional<ItemTypeMatcher> forTag(std::string const& id);
    static std::optional<ItemTypeMatcher> forItem(std::string const& id);
    [[nodiscard]] bool               match(ItemType const& itemType) const;
    [[nodiscard]] std::string const& identifier() const { return mId; }
    [[nodiscard]] std::string        toString() const { return (mIsTag ? "#" : "") + mId; }

private:
    bool        mIsTag = false;
    std::string mId;
};

std::string parseIdentifier(std::string const& s);

Parameter const& reverse();
Parameter const& sub_rule();
Parameter const& match();
Parameter const& blank_string();
Parameter const& string_compare();
Parameter const& locale();
Parameter const& strength();
Parameter const& logical();
Parameter const& number_order();
Parameter const& sub_rule_match();
Parameter const& sub_rule_not_match();
Parameter const& nbt();
Parameter const& allow_extra();
Parameter const& require_nbt();
Parameter const& tag_name();
Parameter const& item_name();
Parameter const& nbt_path();
Parameter const& not_found();
Parameter const& component_id();

struct NativeParameters {
    static Parameter const* find(std::string const& name);
};

}
