// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/inventory/AdvancedContainer.h"

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/inventory/ContainerClicker.h"
#include "ipnext/inventory/sandbox/diffcalculator/DiffCalculator.h"
#include "platform/inventory/SlotClicker.h"
#include "platform/util/Messages.h"

namespace inventory_utils::ipnext::inventory {

namespace {

ItemTracker snapshot(VanillaContainer const& v) {
    std::vector<item::ItemStack> slots;
    slots.reserve(v.slots().size());
    for (auto const& s : v.slots()) slots.push_back(item::ItemStack::of(v.itemOf(s)));
    return ItemTracker(item::ItemStack::of(v.cursorStack()), std::move(slots));
}

void handleException(std::exception const& e) {
    if (dynamic_cast<sandbox::diffcalculator::NoRoomException const*>(&e)) {
        platform::util::chat(e.what());
    } else {
        Log::warn("AdvancedContainer: {}", e.what());
        platform::util::chat(e.what());
    }
}

bool canInsert(MenuSlot const& slot) { return !slot.isOutput && slot.kind != platform::inventory::InventoryKind::Armor; }

}

AdvancedContainer::AdvancedContainer(VanillaContainer vanilla) : mVanilla(std::move(vanilla)), mPlanner(snapshot(mVanilla)) {}

void AdvancedContainer::sandbox(std::function<void(SandboxDsl&)> const& block) {
    try {
        mPlanner.sandbox([&](sandbox::ContainerSandbox& sb) {
            SandboxDsl dsl(*this, sb);
            block(dsl);
        });
    } catch (std::exception const& e) {
        handleException(e);
    }
}

void AdvancedContainer::tracker(std::function<void(TrackerDsl&)> const& block) {
    try {
        mPlanner.tracker([&](ItemTracker& t) {
            TrackerDsl dsl(*this, t);
            block(dsl);
        });
    } catch (std::exception const& e) {
        handleException(e);
    }
}

std::vector<std::pair<int, int>> AdvancedContainer::slotIdClicks() {
    try {
        auto const&                      slots = mVanilla.slots();
        std::vector<std::pair<int, int>> out;
        for (auto const& c : mPlanner.clicks()) {
            int id = c.slotIndex < 0 ? platform::inventory::SLOT_OUTSIDE : slots.at(static_cast<size_t>(c.slotIndex)).slotNumber;
            out.emplace_back(id, c.button);
        }
        return out;
    } catch (std::exception const& e) {
        handleException(e);
        return {};
    }
}

void AdvancedContainer::arrange(bool instant) {
    int interval = 0;
    if (!instant && config::ModSettings::ADD_INTERVAL_BETWEEN_CLICKS.getBooleanValue()) interval = config::ModSettings::INTERVAL_BETWEEN_CLICKS_MS.getIntegerValue();
    ContainerClicker::executeClicks(slotIdClicks(), interval);
}

void AdvancedContainer::run(bool instant, std::function<void(AdvancedContainer&)> const& block) {
    auto vanilla = VanillaContainer::current();
    if (!vanilla) return;
    AdvancedContainer c(std::move(*vanilla));
    block(c);
    c.arrange(instant);
}

void AdvancedContainer::withTracker(std::function<void(TrackerDsl&)> const& block, bool instant, bool cleanCursor) {
    run(instant, [&](AdvancedContainer& c) {
        if (cleanCursor && !config::Debugs::FORCE_NO_CLEAN_CURSOR.getBooleanValue()) c.cleanCursor();
        c.tracker(block);
    });
}

void AdvancedContainer::cleanCursor() {
    sandbox([this](SandboxDsl& dsl) {
        using namespace AreaTypes;
        auto& sb = dsl.sandbox;
        if (sb.items().cursor.isEmpty()) return;
        cursorPutTo(sb, dsl.get(focusedSlot() - lockedSlots()), false);
        if (sb.items().cursor.isEmpty()) return;
        cursorPutTo(sb, dsl.get(playerHands() + playerHotbar() + playerStorage() - lockedSlots()), true);
        if (sb.items().cursor.isEmpty()) return;
        cursorPutTo(sb, dsl.get(playerStorage() + playerHotbar() + playerOffhand() - lockedSlots()), false);
        if (sb.items().cursor.isEmpty()) return;
        cursorPutTo(sb, dsl.get(itemStorage()), true);
        if (sb.items().cursor.isEmpty()) return;
        cursorPutTo(sb, dsl.get(itemStorage()), false);
    });
}

void AdvancedContainer::cursorPutTo(sandbox::ContainerSandbox& sb, ItemArea const& destination, bool skipEmpty) {
    auto const& slots = mVanilla.slots();
    if (sb.items().cursor.isEmpty()) return;
    for (int slotIndex : destination.slotIndices()) {
        auto const& slotItem = sb.items().slots[slotIndex];
        if (skipEmpty && slotItem.isEmpty()) continue;
        if (!canInsert(slots[slotIndex])) continue;
        if (sb.items().cursor.stackableWith(slotItem) && !slotItem.isFull()) sb.leftClick(slotIndex);
        if (sb.items().cursor.isEmpty()) return;
    }
}

}
