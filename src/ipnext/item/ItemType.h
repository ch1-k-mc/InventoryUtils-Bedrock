// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "mc/world/item/ItemStack.h"

class CompoundTag;

namespace inventory_utils::ipnext::item {

using VanillaItemStack = ::ItemStack;

struct EnchantmentEntry {
    int id    = 0;
    int level = 0;
    bool operator==(EnchantmentEntry const&) const = default;
};

struct PotionEffect {
    std::string effect;
    int         amplifier = 0;
    int         duration  = 0;
    bool operator==(PotionEffect const&) const = default;
    [[nodiscard]] int compareTo(PotionEffect const& o) const {
        if (int c = effect.compare(o.effect); c != 0) return c < 0 ? -1 : 1;
        if (amplifier != o.amplifier) return o.amplifier < amplifier ? -1 : 1;
        if (duration != o.duration) return o.duration < duration ? -1 : 1;
        return 0;
    }
};

class ItemType {
public:
    struct Data;

    ItemType() = default;

    static ItemType of(VanillaItemStack const& stack);

    [[nodiscard]] bool isEmpty() const { return mData == nullptr; }

    bool ignoreDurability = false;

    [[nodiscard]] ItemType copy(bool ignoreDurability_) const {
        ItemType t      = *this;
        t.ignoreDurability = ignoreDurability_;
        return t;
    }

    bool operator==(ItemType const& other) const;
    [[nodiscard]] size_t hashCode() const;

    [[nodiscard]] std::string const& itemId() const;
    [[nodiscard]] std::string        namespace_() const;
    [[nodiscard]] int                aux() const;
    [[nodiscard]] int                maxCount() const;
    [[nodiscard]] std::string const& tagSnbt() const;
    [[nodiscard]] bool               tagIsEmpty() const;
    [[nodiscard]] CompoundTag const* tag() const;
    [[nodiscard]] bool               isDamageable() const;
    [[nodiscard]] int                damage() const;
    [[nodiscard]] int                maxDamage() const;
    [[nodiscard]] int                durability() const;
    [[nodiscard]] int                rawId() const;
    [[nodiscard]] bool               hasCustomName() const;
    [[nodiscard]] std::string const& customName() const;
    [[nodiscard]] std::string const& displayName() const;
    [[nodiscard]] std::string const& translationKey() const;
    [[nodiscard]] std::string const& translatedName() const;
    [[nodiscard]] std::string const& potionName() const;
    [[nodiscard]] std::vector<PotionEffect> const& comparablePotionEffects() const;
    [[nodiscard]] std::vector<EnchantmentEntry> const& enchantments() const;
    [[nodiscard]] std::vector<EnchantmentEntry> const& storedEnchantments() const;
    [[nodiscard]] double             enchantmentsScore() const;
    [[nodiscard]] bool               isShulker() const;
    [[nodiscard]] bool               isEmptyShulker() const;
    [[nodiscard]] bool               isBundle() const;
    [[nodiscard]] bool               isBundleAndHasItems() const;
    [[nodiscard]] int                containedItemCount() const;
    [[nodiscard]] int searchTabIndex() const;
    [[nodiscard]] int groupIndex() const;

    [[nodiscard]] VanillaItemStack const& vanillaStack() const;
    [[nodiscard]] VanillaItemStack        vanillaStackWithCount(int count) const;

    [[nodiscard]] std::string toString() const;

    static void clearCreativeIndexCache();

private:
    std::shared_ptr<Data const> mData;
};

struct ItemTypeHash {
    size_t operator()(ItemType const& t) const { return t.hashCode(); }
};

}
