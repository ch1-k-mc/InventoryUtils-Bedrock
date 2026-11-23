// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/inventory/SlotClicker.h"

#include <algorithm>

#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"

#include "platform/inventory/TransferAmount.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::platform::inventory {

namespace {

SlotData toSlotData(MenuSlot const& s) { return SlotData(s.collection, s.index); }

SlotData cursorSlot() { return SlotData("cursor_items", 0); }

SlotClicker::QuickMoveFilter gQuickMoveFilter;

bool isEmpty(ItemStack const& s) { return s.isNull() || s.mCount == 0; }

int count(ItemStack const& s) { return isEmpty(s) ? 0 : static_cast<int>(s.mCount); }

}

SlotClicker SlotClicker::current() {
    auto* csc = screen::ScreenTracker::containerScreen();
    auto* mgr = screen::ScreenTracker::containerManager();
    if (!csc || !mgr) return SlotClicker();
    return SlotClicker(*csc, ContainerMenu::fromManager(*mgr, screen::ScreenTracker::containerScreenName(), csc));
}

bool SlotClicker::placeFromCursor(int slotNum, int amount) {
    if (!mScreen) return fail("no_screen");
    auto const* slot = mMenu.getSlot(slotNum);
    if (!slot || slot->isOutput || amount <= 0) return fail("bad_slot");
    bool ok = mMenu.manager().handlePlaceAmount(cursorSlot(), amount, toSlotData(*slot));
    if (ok) ++mSent;
    return ok || fail("rejected");
}

MenuSlot const* SlotClicker::getSlotUnderMouse() const {
    if (!mScreen) return nullptr;
    SlotData const& hovered = mScreen->mHoveredSlotData;
    std::string     name    = hovered.mCollectionName.get();
    if (name.empty()) return nullptr;
    return mMenu.getSlot(mMenu.findSlot(name, hovered.mCollectionIndex));
}

bool SlotClicker::fail(std::string reason) {
    mLastError = std::move(reason);
    return false;
}

void SlotClicker::setQuickMoveFilter(QuickMoveFilter filter) { gQuickMoveFilter = std::move(filter); }

bool SlotClicker::clickSlot(int slotNum, int mouseButton, ClickType type) {
    if (!mScreen) return fail("no_screen");
    MenuSlot const* slot = mMenu.getSlot(slotNum);
    if (slot && gQuickMoveFilter && (type == ClickType::QUICK_MOVE || type == ClickType::THROW)) {
        if (!gQuickMoveFilter(*slot, type == ClickType::THROW, mMenu.getItem(*slot))) return fail("locked");
    }
    switch (type) {
    case ClickType::PICKUP:
        if (slotNum == SLOT_OUTSIDE) return throwItems(nullptr, mouseButton == 0);
        if (!slot) return fail("bad_slot");
        return pickup(*slot, mouseButton);
    case ClickType::QUICK_MOVE:
        if (!slot) return fail("bad_slot");
        return quickMove(*slot);
    case ClickType::SWAP:
        if (!slot) return fail("bad_slot");
        return swapWithHotbar(*slot, mouseButton);
    case ClickType::THROW:
        if (!slot) return fail("bad_slot");
        return throwItems(slot, mouseButton == 1);
    case ClickType::PICKUP_ALL:
        if (!slot) return fail("bad_slot");
        return pickupAll(*slot);
    case ClickType::QUICK_CRAFT:
        return fail("quick_craft_not_supported");
    case ClickType::CLONE:
    default: return fail("unsupported");
    }
}

bool SlotClicker::pickup(MenuSlot const& slot, int button) {
    auto& mgr     = mMenu.manager();
    auto& carried = mMenu.getCarried();
    auto& inSlot  = mMenu.getItem(slot);
    bool  ok      = false;
    if (isEmpty(carried)) {
        if (isEmpty(inSlot)) return fail("nothing");
        if (button == 1 && !slot.isOutput) ok = mgr.handleTakeHalf(cursorSlot(), toSlotData(slot));
        else ok = mgr.handleTakeAll(cursorSlot(), toSlotData(slot));
    } else if (slot.isOutput) {
        if (isEmpty(inSlot) || !carried.isStackable(inSlot)) return fail("output_mismatch");
        if (count(carried) + count(inSlot) > carried.getMaxStackSize()) return fail("output_full");
        ok = mgr.handleTakeAll(cursorSlot(), toSlotData(slot));
    } else if (isEmpty(inSlot) || carried.isStackable(inSlot)) {
        if (button == 1) {
            ok = mgr.handlePlaceOne(cursorSlot(), toSlotData(slot));
        } else {
            int room = isEmpty(inSlot) ? count(carried) : std::max(0, inSlot.getMaxStackSize() - count(inSlot));
            int n    = std::min(room, count(carried));
            if (n <= 0) return fail("slot_full");
            ok = mgr.handlePlaceAmount(cursorSlot(), n, toSlotData(slot));
        }
    } else {
        ok = mgr.handleSwap(cursorSlot(), toSlotData(slot));
    }
    if (ok) ++mSent;
    return ok || fail("rejected");
}

bool SlotClicker::quickMove(MenuSlot const& slot) {
    auto& inSlot = mMenu.getItem(slot);
    if (isEmpty(inSlot)) return fail("nothing");
    int amount = slot.isOutput ? 64 * 36 : count(inSlot);
    mScreen->_handleAutoPlace(amount, slot.collection, slot.index);
    ++mSent;
    return true;
}

bool SlotClicker::swapWithHotbar(MenuSlot const& slot, int hotbarIndex) {
    auto&       mgr = mMenu.manager();
    std::string targetCollection = hotbarIndex == 40 ? "offhand_items" : "hotbar_items";
    int         targetIndex      = hotbarIndex == 40 ? 0 : hotbarIndex;
    if (targetCollection == slot.collection && targetIndex == slot.index) return fail("same_slot");
    MenuSlot target;
    target.collection = targetCollection;
    target.index      = targetIndex;
    auto& a           = mMenu.getItem(slot);
    auto& b           = mMenu.getItem(target);
    bool  ok          = false;
    if (isEmpty(a) && isEmpty(b)) return fail("nothing");
    if (isEmpty(a)) ok = mgr.handleTakeAll(toSlotData(slot), SlotData(targetCollection, targetIndex));
    else if (isEmpty(b)) ok = mgr.handleTakeAll(SlotData(targetCollection, targetIndex), toSlotData(slot));
    else ok = mgr.handleSwap(toSlotData(slot), SlotData(targetCollection, targetIndex));
    if (ok) ++mSent;
    return ok || fail("rejected");
}

bool SlotClicker::throwItems(MenuSlot const* slot, bool wholeStack) {
    auto&    mgr = mMenu.manager();
    SlotData src = slot ? toSlotData(*slot) : cursorSlot();
    auto&    item = slot ? mMenu.getItem(*slot) : mMenu.getCarried();
    if (isEmpty(item)) return fail("nothing");
    int  n  = wholeStack ? count(item) : 1;
    bool ok = mgr.handleDrop(src, makeTransferAmount(ItemTransferAmount::ItemTransferAmountTag::RequestAmount, n));
    if (ok) ++mSent;
    return ok || fail("rejected");
}

bool SlotClicker::pickupAll(MenuSlot const& slot) {
    (void)slot;
    auto&            mgr     = mMenu.manager();
    ItemStack const& carried = mMenu.getCarried();
    if (isEmpty(carried)) return fail("nothing");
    int  max  = carried.getMaxStackSize();
    bool any  = false;
    for (int pass = 0; pass < 2 && count(mMenu.getCarried()) < max; ++pass) {
        for (auto const& s : mMenu.slots()) {
            if (count(mMenu.getCarried()) >= max) break;
            if (s.isOutput) continue;
            ItemStack const& in = mMenu.getItem(s);
            if (isEmpty(in) || !in.isStackable(mMenu.getCarried())) continue;
            if ((pass == 0) == (count(in) >= in.getMaxStackSize())) continue;
            int n = std::min(count(in), max - count(mMenu.getCarried()));
            if (n > 0 && mgr.handleTakeAmount(cursorSlot(), n, toSlotData(s))) {
                ++mSent;
                any = true;
            }
        }
    }
    return any || fail("nothing");
}

}
