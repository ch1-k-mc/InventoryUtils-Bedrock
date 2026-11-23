// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/item/ItemType.h"

#include <climits>
#include <format>
#include <functional>
#include <mutex>
#include <unordered_map>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/deps/nbt/ByteTag.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/Int64Tag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/ShortTag.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/alchemy/Potion.h"
#include "mc/world/item/enchanting/Enchant.h"
#include "mc/world/item/registry/CreativeGroupInfo.h"
#include "mc/world/item/registry/CreativeItemEntry.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryRef.h"

namespace inventory_utils::ipnext::item {

struct ItemType::Data {
    VanillaItemStack              stack;
    std::string                   itemId;
    int                           aux        = 0;
    int                           maxCount   = 64;
    std::string                   tagSnbt;
    std::string                   tagNoDamageSnbt;
    bool                          damageable = false;
    int                           damage     = 0;
    int                           maxDamage  = 0;
    int                           rawId      = 0;
    bool                          hasCustomName = false;
    std::string                   customName;
    std::string                   displayName;
    std::string                   translationKey;
    std::string                   translatedName;
    std::string                   potionName;
    std::vector<PotionEffect>     potionEffects;
    std::vector<EnchantmentEntry> enchantments;
    double                        enchantmentsScore = 0;
    bool                          shulker  = false;
    bool                          bundle   = false;
    bool                          hasItems = false;
    int                           itemsCount = 0;
};

namespace {

using SharedTypes::CreativeItemCategory;

std::string snbtOf(CompoundTag const* tag) {
    if (!tag || tag->empty()) return {};
    return tag->toSnbt(SnbtFormat::Minimize, 0);
}

int tagAsInt(Tag const& t) {
    switch (t.getId()) {
    case Tag::Byte:
        return static_cast<ByteTag const&>(t).data;
    case Tag::Short:
        return static_cast<ShortTag const&>(t).data;
    case Tag::Int:
        return static_cast<IntTag const&>(t).data;
    case Tag::Int64:
        return static_cast<int>(static_cast<Int64Tag const&>(t).data);
    default:
        return 0;
    }
}

std::vector<EnchantmentEntry> readEnchantments(CompoundTag const* tag) {
    std::vector<EnchantmentEntry> out;
    if (!tag || !tag->contains("ench", Tag::List)) return out;
    auto const& list = static_cast<ListTag const&>((*tag)["ench"].get());
    for (auto const& e : list) {
        if (!e || e->getId() != Tag::Compound) continue;
        auto const&      c = static_cast<CompoundTag const&>(*e);
        EnchantmentEntry en;
        if (c.contains("id")) en.id = tagAsInt(c["id"].get());
        if (c.contains("lvl")) en.level = tagAsInt(c["lvl"].get());
        out.push_back(en);
    }
    return out;
}

int enchantMaxLevel(int id) {
    auto& list = Enchant::mEnchants();
    if (id < 0 || id >= static_cast<int>(list.size()) || !list[id]) return 1;
    int m = list[id]->getMaxLevel();
    return m > 0 ? m : 1;
}

bool isCurse(int id) { return id == 27 || id == 28; }

bool isPotionLike(std::string const& id) {
    return id == "minecraft:potion" || id == "minecraft:splash_potion" || id == "minecraft:lingering_potion" || id == "minecraft:arrow";
}

struct CreativeIndex {
    std::unordered_map<std::string, std::pair<int, int>> byIdAux;
    std::unordered_map<std::string, std::pair<int, int>> byId;
    bool                                                 built = false;
};

std::mutex    gCreativeMutex;
CreativeIndex gCreative;

constexpr CreativeItemCategory kCategoryOrder[] = {
    CreativeItemCategory::Construction,
    CreativeItemCategory::Nature,
    CreativeItemCategory::Equipment,
    CreativeItemCategory::Items,
};

void buildCreativeIndex() {
    if (gCreative.built) return;
    gCreative.built = true;
    auto ci         = ll::service::getClientInstance();
    if (!ci) return;
    ItemRegistryRef reg      = ci->getItemRegistry();
    auto            creative = reg.getCreativeItemRegistry();
    if (!creative) return;
    auto const&       items  = *creative->mCreativeItems;
    auto const&       groups = *creative->mCreativeGroups;
    std::vector<bool> used(items.size(), false);
    int               order = 0;
    auto              add   = [&](size_t index, int category) {
        auto const& inst = *items[index].mItemInstance;
        auto const* it   = inst.mItem.get();
        if (!it) return;
        std::string id  = it->mFullName->getString();
        std::string key = id + ":" + std::to_string(inst.mAuxValue);
        gCreative.byIdAux.try_emplace(key, order, category);
        gCreative.byId.try_emplace(id, order, category);
        ++order;
    };
    int categoryIndex = 0;
    for (auto category : kCategoryOrder) {
        for (auto const& group : groups) {
            if (group.mCategory != category) continue;
            for (uint index : *group.mItemIndexes) {
                if (index >= items.size() || used[index]) continue;
                used[index] = true;
                add(index, categoryIndex);
            }
        }
        ++categoryIndex;
    }
    for (size_t i = 0; i < items.size(); ++i) {
        if (!used[i]) add(i, INT_MAX);
    }
}

std::pair<int, int> creativeIndexOf(std::string const& id, int aux) {
    std::lock_guard lock(gCreativeMutex);
    buildCreativeIndex();
    if (auto it = gCreative.byIdAux.find(id + ":" + std::to_string(aux)); it != gCreative.byIdAux.end()) return it->second;
    if (auto it = gCreative.byId.find(id); it != gCreative.byId.end()) return it->second;
    return {-1, INT_MAX};
}

ItemType::Data const& emptyData() {
    static ItemType::Data d;
    return d;
}

}

void ItemType::clearCreativeIndexCache() {
    std::lock_guard lock(gCreativeMutex);
    gCreative = CreativeIndex{};
}

ItemType ItemType::of(VanillaItemStack const& stack) {
    ItemType t;
    if (stack.isNull() || stack.mCount == 0) return t;
    auto const* it = stack.mItem.get();
    if (!it) return t;
    auto d   = std::make_shared<Data>();
    d->stack = stack;
    d->stack.set(1);
    d->itemId   = it->mFullName->getString();
    d->aux      = stack.mAuxValue;
    d->maxCount = std::max<int>(1, stack.getMaxStackSize());
    CompoundTag const* tag = stack.mUserData.get();
    d->tagSnbt             = snbtOf(tag);
    if (tag && tag->contains("Damage")) {
        auto copy = tag->clone();
        copy->erase("Damage");
        d->tagNoDamageSnbt = snbtOf(copy.get());
    } else {
        d->tagNoDamageSnbt = d->tagSnbt;
    }
    d->damageable     = stack.isDamageableItem();
    d->damage         = stack.getDamageValue();
    d->maxDamage      = it->getMaxDamage();
    d->rawId          = stack.getId();
    d->customName     = stack.getCustomName();
    d->hasCustomName  = !d->customName.empty();
    d->displayName    = stack.getHoverName();
    d->translationKey = stack.getDescriptionId();
    d->translatedName = stack.getName();
    if (isPotionLike(d->itemId)) {
        int potionId = d->itemId == "minecraft:arrow" ? d->aux - 1 : d->aux;
        auto& potions = Potion::mPotionsById();
        if (potionId >= 0 && potionId < static_cast<int>(potions.size()) && potions[potionId]) {
            auto const& p = *potions[potionId];
            d->potionName = p.mNameId;
            for (auto const& e : *p.mEffects) {
                d->potionEffects.push_back({std::format("{:03}", static_cast<unsigned>(e.mId)), e.mAmplifier, static_cast<int>(e.mDuration->mValue)});
            }
        }
    }
    d->enchantments = readEnchantments(tag);
    for (auto const& e : d->enchantments) {
        d->enchantmentsScore += isCurse(e.id) ? -0.001 : static_cast<double>(e.level) / enchantMaxLevel(e.id);
    }
    d->shulker  = d->itemId.find("shulker_box") != std::string::npos;
    d->bundle   = d->itemId.size() >= 6 && d->itemId.compare(d->itemId.size() - 6, 6, "bundle") == 0;
    d->hasItems = tag && (tag->contains("Items") || tag->contains("storage_item_component_content"));
    if (tag && tag->contains("Items", Tag::List)) d->itemsCount = static_cast<int>(static_cast<ListTag const&>((*tag)["Items"].get()).size());
    t.mData     = std::move(d);
    return t;
}

bool ItemType::operator==(ItemType const& other) const {
    if (isEmpty() || other.isEmpty()) return isEmpty() && other.isEmpty();
    if (mData == other.mData) return true;
    if (mData->itemId != other.mData->itemId || mData->aux != other.mData->aux) return false;
    if (!ignoreDurability || !mData->damageable) return mData->tagSnbt == other.mData->tagSnbt;
    return mData->tagNoDamageSnbt == other.mData->tagNoDamageSnbt;
}

size_t ItemType::hashCode() const {
    if (isEmpty()) return 0;
    return std::hash<std::string>{}(mData->itemId) * 31 + static_cast<size_t>(mData->aux);
}

#define IU_D (mData ? *mData : emptyData())

std::string const& ItemType::itemId() const { return IU_D.itemId; }
std::string        ItemType::namespace_() const {
    auto const& id = itemId();
    auto        p  = id.find(':');
    return p == std::string::npos ? std::string("minecraft") : id.substr(0, p);
}
int                ItemType::aux() const { return IU_D.aux; }
int                ItemType::maxCount() const { return mData ? mData->maxCount : 64; }
std::string const& ItemType::tagSnbt() const { return IU_D.tagSnbt; }
bool               ItemType::tagIsEmpty() const { return IU_D.tagSnbt.empty(); }
CompoundTag const* ItemType::tag() const { return mData ? mData->stack.mUserData.get() : nullptr; }
bool               ItemType::isDamageable() const { return IU_D.damageable; }
int                ItemType::damage() const { return IU_D.damage; }
int                ItemType::maxDamage() const { return IU_D.maxDamage; }
int                ItemType::durability() const { return isDamageable() ? maxDamage() - damage() : -1; }
int                ItemType::rawId() const { return IU_D.rawId; }
bool               ItemType::hasCustomName() const { return IU_D.hasCustomName; }
std::string const& ItemType::customName() const {
    static std::string const empty;
    return hasCustomName() ? IU_D.displayName : empty;
}
std::string const& ItemType::displayName() const { return IU_D.displayName; }
std::string const& ItemType::translationKey() const { return IU_D.translationKey; }
std::string const& ItemType::translatedName() const { return IU_D.translatedName; }
std::string const& ItemType::potionName() const { return IU_D.potionName; }
std::vector<PotionEffect> const&     ItemType::comparablePotionEffects() const { return IU_D.potionEffects; }
std::vector<EnchantmentEntry> const& ItemType::enchantments() const { return IU_D.enchantments; }
std::vector<EnchantmentEntry> const& ItemType::storedEnchantments() const {
    static std::vector<EnchantmentEntry> const empty;
    return itemId() == "minecraft:enchanted_book" ? IU_D.enchantments : empty;
}
double ItemType::enchantmentsScore() const { return IU_D.enchantmentsScore; }
bool   ItemType::isShulker() const { return IU_D.shulker; }
bool   ItemType::isEmptyShulker() const { return IU_D.shulker && !IU_D.hasItems; }
bool   ItemType::isBundle() const { return IU_D.bundle; }
bool   ItemType::isBundleAndHasItems() const { return IU_D.bundle && IU_D.hasItems; }
int    ItemType::containedItemCount() const { return IU_D.itemsCount; }

int ItemType::searchTabIndex() const {
    if (isEmpty()) return -1;
    return creativeIndexOf(mData->itemId, mData->aux).first;
}
int ItemType::groupIndex() const {
    if (isEmpty()) return INT_MAX;
    return creativeIndexOf(mData->itemId, mData->aux).second;
}

VanillaItemStack const& ItemType::vanillaStack() const {
    static VanillaItemStack const empty;
    return mData ? mData->stack : empty;
}

VanillaItemStack ItemType::vanillaStackWithCount(int count) const {
    VanillaItemStack s = vanillaStack();
    if (!isEmpty()) s.set(count);
    return s;
}

std::string ItemType::toString() const {
    if (isEmpty()) return "minecraft:air";
    return mData->itemId + (mData->aux ? "@" + std::to_string(mData->aux) : std::string()) + mData->tagSnbt;
}

#undef IU_D

}
