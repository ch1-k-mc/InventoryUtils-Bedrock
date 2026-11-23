// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/item/rule/parameter/NativeParameters.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/Item.h"

#include "ipnext/Log.h"
#include "ipnext/parser/RuleParser.h"

namespace inventory_utils::ipnext::item::rule::parameter {

namespace {

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

struct IdentifierArgumentType : ArgumentType {
    std::string toString(std::any const& v) const override { return *std::any_cast<std::string>(&v); }
    std::optional<std::any> parse(std::string const& a) const override {
        auto id = parseIdentifier(a);
        if (id.empty()) return std::nullopt;
        return std::any(id);
    }
};

struct StringArgumentType : ArgumentType {
    std::string             toString(std::any const& v) const override { return *std::any_cast<std::string>(&v); }
    std::optional<std::any> parse(std::string const& a) const override { return std::any(a); }
};

struct BooleanArgumentType : ArgumentType {
    std::string toString(std::any const& v) const override { return *std::any_cast<bool>(&v) ? "true" : "false"; }
    std::optional<std::any> parse(std::string const& a) const override {
        if (a == "true") return std::any(true);
        if (a == "false") return std::any(false);
        return std::nullopt;
    }
};

template <class E, size_t N>
struct EnumArgumentType : ArgumentType {
    std::array<char const*, N> names;
    explicit EnumArgumentType(std::array<char const*, N> n) : names(n) {}
    std::string toString(std::any const& v) const override { return lower(names[static_cast<size_t>(*std::any_cast<E>(&v))]); }
    std::optional<std::any> parse(std::string const& a) const override {
        auto u = upper(a);
        for (size_t i = 0; i < N; ++i) {
            if (u == names[i]) return std::any(static_cast<E>(i));
        }
        return std::nullopt;
    }
};

struct NbtArgumentType : ArgumentType {
    std::string toString(std::any const& v) const override {
        auto const& t = *std::any_cast<NbtUtils::TagPtr>(&v);
        return t ? t->toSnbt(SnbtFormat::Minimize, 0) : std::string("{}");
    }
    std::optional<std::any> parse(std::string const& a) const override { return std::any(NbtUtils::parseNbtOrEmpty(a)); }
};

struct RuleArgumentType : ArgumentType {
    std::string             toString(std::any const&) const override { return "[rule]"; }
    std::optional<std::any> parse(std::string const& a) const override { return std::any(parser::TemporaryRuleParser::parse(a)); }
};

struct TagNameArgumentType : ArgumentType {
    std::string toString(std::any const& v) const override { return std::any_cast<ItemTypeMatcher>(&v)->toString(); }
    std::optional<std::any> parse(std::string const& a) const override {
        auto m = ItemTypeMatcher::forTag(a);
        if (!m) return std::nullopt;
        return std::any(*m);
    }
};

struct ItemNameArgumentType : ArgumentType {
    std::string toString(std::any const& v) const override { return std::any_cast<ItemTypeMatcher>(&v)->toString(); }
    std::optional<std::any> parse(std::string const& a) const override {
        auto m = ItemTypeMatcher::forItem(a);
        if (!m) return std::nullopt;
        return std::any(*m);
    }
};

struct NbtPathArgumentType : ArgumentType {
    std::string toString(std::any const& v) const override { return std::any_cast<NbtUtils::NbtPath>(&v)->toString(); }
    std::optional<std::any> parse(std::string const& a) const override {
        auto p = NbtUtils::NbtPath::of(a);
        if (!p) {
            Log::warn("Invalid nbt path {}", a);
            return std::nullopt;
        }
        return std::any(*p);
    }
};

IdentifierArgumentType const any_identifier;
StringArgumentType const     any_string;
BooleanArgumentType const    type_boolean;
NbtArgumentType const        any_nbt;
RuleArgumentType const       any_rule;
TagNameArgumentType const    any_tag_name;
ItemNameArgumentType const   any_item_name;
NbtPathArgumentType const    any_nbt_path;

EnumArgumentType<Match, 2> const         enum_match({"FIRST", "LAST"});
EnumArgumentType<StringCompare, 3> const enum_string_compare({"UNICODE", "IGNORE_CASE", "LOCALE"});
EnumArgumentType<Strength, 4> const      enum_strength({"PRIMARY", "SECONDARY", "TERTIARY", "IDENTICAL"});
EnumArgumentType<NumberOrder, 2> const   enum_number_order({"ASCENDING", "DESCENDING"});
EnumArgumentType<RequireNbt, 3> const    enum_require_nbt({"REQUIRED", "NO_NBT", "NOT_REQUIRED"});

std::map<std::string, Parameter const*>& registry() {
    static std::map<std::string, Parameter const*> m;
    return m;
}

Parameter const& make(Parameter& storage, char const* name, ArgumentType const& type) {
    if (storage.argumentType == nullptr) {
        storage.name         = name;
        storage.argumentType = &type;
        registry()[name]     = &storage;
    }
    return storage;
}

#define IU_PARAM(NAME, TYPE)                    \
    Parameter const& NAME() {                   \
        static Parameter p;                     \
        return make(p, #NAME, TYPE);            \
    }

}

IU_PARAM(reverse, type_boolean)
IU_PARAM(sub_rule, any_rule)
IU_PARAM(match, enum_match)
IU_PARAM(blank_string, enum_match)
IU_PARAM(string_compare, enum_string_compare)
IU_PARAM(locale, any_string)
IU_PARAM(strength, enum_strength)
IU_PARAM(logical, type_boolean)
IU_PARAM(number_order, enum_number_order)
IU_PARAM(sub_rule_match, any_rule)
IU_PARAM(sub_rule_not_match, any_rule)
IU_PARAM(nbt, any_nbt)
IU_PARAM(allow_extra, type_boolean)
IU_PARAM(require_nbt, enum_require_nbt)
IU_PARAM(tag_name, any_tag_name)
IU_PARAM(item_name, any_item_name)
IU_PARAM(nbt_path, any_nbt_path)
IU_PARAM(not_found, enum_match)
IU_PARAM(component_id, any_identifier)

#undef IU_PARAM

Parameter const* NativeParameters::find(std::string const& name) {
    static bool const loaded = [] {
        reverse(), sub_rule(), match(), blank_string(), string_compare(), locale(), strength(), logical(), number_order(), sub_rule_match(),
            sub_rule_not_match(), nbt(), allow_extra(), require_nbt(), tag_name(), item_name(), nbt_path(), not_found(), component_id();
        return true;
    }();
    (void)loaded;
    auto it = registry().find(name);
    return it == registry().end() ? nullptr : it->second;
}

int compareNumber(NumberOrder order, double a, double b) {
    int c = a < b ? -1 : (b < a ? 1 : 0);
    return order == NumberOrder::ASCENDING ? c : -c;
}

bool requireNbtMatch(RequireNbt r, ItemType const& itemType) {
    switch (r) {
    case RequireNbt::REQUIRED:
        return !itemType.tagIsEmpty();
    case RequireNbt::NO_NBT:
        return itemType.tagIsEmpty();
    default:
        return true;
    }
}

std::string parseIdentifier(std::string const& s) {
    std::string ns = "minecraft", path = s;
    if (auto p = s.find(':'); p != std::string::npos) {
        ns   = s.substr(0, p);
        path = s.substr(p + 1);
    }
    auto validNs   = [](char c) { return std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.'; };
    auto validPath = [&](char c) { return validNs(c) || c == '/'; };
    if (path.empty() || !std::all_of(ns.begin(), ns.end(), validNs) || !std::all_of(path.begin(), path.end(), validPath)) return {};
    return ns + ":" + path;
}

std::optional<ItemTypeMatcher> ItemTypeMatcher::forTag(std::string const& id) {
    auto identifier = parseIdentifier(id);
    if (identifier.empty()) return std::nullopt;
    ItemTypeMatcher m;
    m.mIsTag = true;
    m.mId    = identifier;
    return m;
}

std::optional<ItemTypeMatcher> ItemTypeMatcher::forItem(std::string const& id) {
    auto identifier = parseIdentifier(id);
    if (identifier.empty()) return std::nullopt;
    ItemTypeMatcher m;
    m.mId = identifier;
    return m;
}

bool ItemTypeMatcher::match(ItemType const& itemType) const {
    if (itemType.isEmpty()) return false;
    if (!mIsTag) return itemType.itemId() == mId;
    auto const* item = itemType.vanillaStack().mItem.get();
    return item && item->hasTag(HashedString(mId));
}

}
