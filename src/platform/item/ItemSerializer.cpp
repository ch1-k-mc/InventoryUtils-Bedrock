// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/item/ItemSerializer.h"

#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/item/SaveContext.h"
#include "mc/world/item/SaveContextFactory.h"

namespace inventory_utils::platform::item {

std::string toSnbt(ItemStack const& stack) {
    if (stack.isNull() || stack.mCount == 0) return {};
    auto ctx = SaveContextFactory::createCloneSaveContext();
    auto tag = stack.save(*ctx);
    return tag ? tag->toSnbt(SnbtFormat::Minimize, 0) : std::string();
}

ItemStack fromSnbt(std::string const& snbt) {
    if (snbt.empty()) return ItemStack();
    auto tag = CompoundTag::fromSnbt(snbt);
    if (!tag) return ItemStack();
    return ItemStack::fromTag(*tag);
}

}
