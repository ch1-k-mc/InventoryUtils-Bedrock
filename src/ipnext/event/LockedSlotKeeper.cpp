// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/event/LockedSlotKeeper.h"

#include <vector>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/event/LockSlotsHandler.h"
#include "platform/inventory/InventorySession.h"
#include "platform/inventory/SlotClicker.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::ipnext::event {

namespace {

using namespace config;

LocalPlayer* localPlayer() {
    auto ci = ll::service::getClientInstance();
    return ci ? ci->getLocalPlayer() : nullptr;
}

bool emptyAt(LocalPlayer& p, int inv) {
    auto const& s = p.getInventory().getItem(inv);
    return s.isNull() || s.mCount == 0;
}

bool active() {
    return Features::ENABLE_LOCK_SLOTS.getBooleanValue() && !LockedSlotsSettings::LOCKED_SLOTS_ALLOW_PICKUP_INTO_EMPTY.getBooleanValue();
}

}

LockedSlotKeeper& LockedSlotKeeper::get() {
    static LockedSlotKeeper instance;
    return instance;
}

void LockedSlotKeeper::onJoinWorld() {
    mTicksAfterJoin = 0;
    mWorldJoined    = true;
}

void LockedSlotKeeper::init() {
    if (mProcessing) return;
    mEmptyLocked.clear();
    mEmptyNonLocked.clear();
    auto* p = localPlayer();
    if (!p || p->isCreative()) return;
    auto locked = LockSlotsHandler::get().lockedInvSlots();
    std::set<int> lockedSet(locked.begin(), locked.end());
    for (int inv = 0; inv <= 35; ++inv) {
        if (!emptyAt(*p, inv)) continue;
        if (lockedSet.contains(inv)) mEmptyLocked.insert(inv);
        else mEmptyNonLocked.insert(inv);
    }
}

void LockedSlotKeeper::onClientTick() {
    if (!active() || mProcessing || platform::inventory::InventorySession::busy()) return;
    auto* p = localPlayer();
    if (!p) return;
    platform::item::ScopedItemRegistry registry;
    if (mWorldJoined) {
        bool inventoryEmpty = true;
        for (int inv = 0; inv <= 35 && inventoryEmpty; ++inv) inventoryEmpty = emptyAt(*p, inv);
        if (mTicksAfterJoin > LockedSlotsSettings::LOCKED_SLOTS_DELAY_KEEPER_REINIT_TICKS.getIntegerValue() || !inventoryEmpty) {
            init();
            mWorldJoined    = false;
            mTicksAfterJoin = 0;
            return;
        }
        ++mTicksAfterJoin;
    }
    if (platform::screen::ScreenTracker::isGuiOpen()) {
        mScreenOpening = true;
        return;
    }
    if (mScreenOpening) {
        mScreenOpening = false;
        init();
        return;
    }
    if (p->isCreative()) return;
    std::vector<std::pair<int, int>> moves;
    std::vector<int>                 freeSlots(mEmptyNonLocked.begin(), mEmptyNonLocked.end());
    for (int inv : mEmptyLocked) {
        if (emptyAt(*p, inv)) continue;
        int target = -1;
        while (!freeSlots.empty()) {
            int t = freeSlots.front();
            freeSlots.erase(freeSlots.begin());
            if (emptyAt(*p, t)) {
                target = t;
                break;
            }
        }
        moves.emplace_back(inv, target);
    }
    if (moves.empty()) return;
    mProcessing  = true;
    bool request = platform::inventory::InventorySession::request(
        [moves](ContainerScreenController& csc, ContainerManagerController& mgr) {
            auto menu = platform::inventory::ContainerMenu::fromManager(mgr, "inventory_screen", &csc);
            platform::inventory::SlotClicker clicker(csc, menu);
            auto slotOf = [&](int inv) {
                for (auto const& s : clicker.menu().slots()) {
                    if (s.kind != platform::inventory::InventoryKind::Container && s.containerSlot == inv) return s.slotNumber;
                }
                return -1;
            };
            for (auto const& [from, to] : moves) {
                int fromId = slotOf(from);
                if (fromId < 0) continue;
                if (to < 0) {
                    clicker.leftClickSlot(fromId);
                    clicker.dropItemsFromCursor();
                    continue;
                }
                int toId = slotOf(to);
                if (toId < 0) continue;
                if (from >= 0 && from <= 8) {
                    clicker.clickSlot(toId, from, platform::inventory::ClickType::SWAP);
                } else {
                    clicker.leftClickSlot(fromId);
                    clicker.leftClickSlot(toId);
                }
            }
        },
        platform::inventory::InventorySession::Hide::Invisible,
        [this, n = static_cast<int>(moves.size())](bool, std::string const&) {
            mMoved += n;
            mProcessing = false;
            auto keep = mEmptyLocked;
            init();
            for (int v : keep) {
                if (LockSlotsHandler::get().isSlotLocked(v)) mEmptyLocked.insert(v);
            }
            mScreenOpening = false;
        }
    );
    if (!request) mProcessing = false;
    else Log::debug("locked slot keeper: moving {} stacks out of locked slots", moves.size());
}

}
