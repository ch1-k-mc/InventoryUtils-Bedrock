// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/inventory/LegacySwap.h"

#include "ll/api/service/Bedrock.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/world/ContainerID.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/inventory/transaction/InventoryAction.h"
#include "mc/world/inventory/transaction/InventorySource.h"
#include "mc/world/inventory/transaction/InventorySourceType.h"
#include "mc/world/inventory/transaction/InventoryTransaction.h"
#include "mc/world/inventory/transaction/InventoryTransactionItemGroup.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/NetworkItemStackDescriptor.h"
#include "mc/world/level/Level.h"

namespace inventory_utils::platform::inventory::legacy {

namespace {

struct Ref {
    int container = 0;
    int slot      = 0;
};

bool toRef(LocalPlayer& p, int inv, Ref& out) {
    if (inv >= 0 && inv < p.getInventory().getContainerSize() && inv < 36) out = {0, inv};
    else if (inv >= 36 && inv <= 39) out = {120, inv - 36};
    else if (inv == 40) out = {119, 0};
    else return false;
    return true;
}

ItemStack getRef(LocalPlayer& p, Ref r) {
    switch (r.container) {
    case 119: return p.getOffhandSlot();
    case 120: return p.getArmor(static_cast<SharedTypes::Legacy::ArmorSlot>(r.slot));
    default: return p.getInventory().getItem(r.slot);
    }
}

void setRef(LocalPlayer& p, Ref r, ItemStack const& item) {
    switch (r.container) {
    case 119: p.setOffhandSlot(item); break;
    case 120: p.setArmor(static_cast<SharedTypes::Legacy::ArmorSlot>(r.slot), item); break;
    default: p.getInventory().setItem(r.slot, item); break;
    }
}

InventorySource source(int container) {
    InventorySource src{};
    src.mType        = InventorySourceType::ContainerInventory;
    src.mContainerId = static_cast<ContainerID>(container);
    return src;
}

LocalPlayer* player() {
    auto ci = ll::service::getClientInstance();
    return ci ? ci->getLocalPlayer() : nullptr;
}

}

bool available() {
    auto level = ll::service::getLevel();
    return player() && level && !level->isClientSide();
}

::ItemStack itemAt(int inv) {
    auto* p = player();
    Ref   r;
    if (!p || !toRef(*p, inv, r)) return ItemStack::EMPTY_ITEM();
    return getRef(*p, r);
}

bool swapSlots(int a, int b) {
    auto* p = player();
    Ref   ra, rb;
    if (!p || a == b || !toRef(*p, a, ra) || !toRef(*p, b, rb)) return false;
    ItemStack const itemA = getRef(*p, ra);
    ItemStack const itemB = getRef(*p, rb);
    InventoryTransaction tx{};
    tx.addAction(InventoryAction(source(ra.container), static_cast<uint>(ra.slot), itemA, itemB));
    tx.addAction(InventoryAction(source(rb.container), static_cast<uint>(rb.slot), itemB, itemA));
    setRef(*p, ra, itemB);
    setRef(*p, rb, itemA);
    p->sendInventoryTransaction(tx);
    return true;
}

}
