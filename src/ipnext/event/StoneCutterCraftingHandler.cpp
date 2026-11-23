// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/event/StoneCutterCraftingHandler.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "mc/world/item/ItemStack.h"

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/event/LockSlotsHandler.h"
#include "ipnext/inventory/ContainerClicker.h"
#include "ipnext/inventory/ContainerTypes.h"
#include "platform/input/GameInput.h"
#include "platform/input/KeyCodes.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::ipnext::event {

namespace {

bool isStonecutter() {
    return platform::screen::ScreenTracker::containerScreenName() == "stonecutter_screen" && platform::screen::ScreenTracker::containerScreen();
}

bool shiftDown() { return platform::input::InputHandler::get().isKeyDown(VK_LSHIFT) || platform::input::InputHandler::get().isKeyDown(VK_RSHIFT); }

void shiftClickResult() {
    auto v = inventory::VanillaContainer::current();
    if (!v) return;
    for (auto const& slot : v->slots()) {
        if (slot.collection == "stonecutter_result_items") inventory::ContainerClicker::shiftClick(slot.slotNumber);
    }
}

}

StoneCutterCraftingHandler& StoneCutterCraftingHandler::get() {
    static StoneCutterCraftingHandler instance;
    return instance;
}

StoneCutterCraftingHandler::State StoneCutterCraftingHandler::read() const {
    State s;
    auto  v = inventory::VanillaContainer::current();
    if (!v) return s;
    for (auto const& slot : v->slots()) {
        auto const& stack = v->itemOf(slot);
        if (stack.isNull() || stack.mCount == 0) continue;
        if (slot.collection == "stonecutter_input_items") {
            s.input      = item::ItemType::of(stack);
            s.inputCount = stack.mCount;
        } else if (slot.collection == "stonecutter_result_items") {
            s.result = item::ItemType::of(stack);
        }
    }
    if (!s.input.isEmpty()) {
        for (auto const& slot : v->slots()) {
            if (slot.kind == platform::inventory::InventoryKind::Container) continue;
            auto const& stack = v->itemOf(slot);
            if (!stack.isNull() && stack.mCount > 0 && item::ItemType::of(stack) == s.input) s.held += stack.mCount;
        }
    }
    return s;
}

void StoneCutterCraftingHandler::onContainerOverlay(ScreenView& view, MinecraftUIRenderContext&) {
    if (!isStonecutter()) return;
    mButtons = platform::screen::collectNamedRects(view, "stone_button");
}

bool StoneCutterCraftingHandler::onMouseClick(double mx, double my, int buttonCode, bool buttonState) {
    if (!buttonState || buttonCode != platform::input::KeyCodes::MOUSE_LEFT || !isStonecutter()) return false;
    for (size_t i = 0; i < mButtons.size(); ++i) {
        auto const& b = mButtons[i];
        if (mx >= b.x && mx < b.x + b.w && my >= b.y && my < b.y + b.h) {
            if (static_cast<int>(i) != mSelected) mGuardTicks = 0;
            mSelected = static_cast<int>(i);
            break;
        }
    }
    return false;
}

void StoneCutterCraftingHandler::onClientTick() {
    if (!isStonecutter()) {
        mScreen = nullptr;
        mStage  = WATCH;
        return;
    }
    void* screen = platform::screen::ScreenTracker::containerScreen();
    if (screen != mScreen) {
        mScreen   = screen;
        mStage    = WATCH;
        mSelected = -1;
        mLast     = read();
        return;
    }
    if (!config::GuiSettings::CONTINUOUS_CRAFTING_SAVED_VALUE.getBooleanValue()) {
        mStage = WATCH;
        mLast  = read();
        return;
    }
    if (mWait > 0) {
        --mWait;
        return;
    }
    try {
        auto cur = read();
        switch (mStage) {
        case WATCH: {
            if (mGuardTicks > 0) {
                --mGuardTicks;
                if (!cur.input.isEmpty() && !cur.result.isEmpty() && !(cur.result == mExpectedResult) && mReselects < 3) {
                    ++mReselects;
                    mStage = AFTER_REFILL;
                    mTries = 0;
                    return;
                }
            }
            if (!mLast.input.isEmpty() && cur.input.isEmpty() && !mLast.result.isEmpty()) {
                int  heldNow = 0;
                int  source  = -1;
                auto v       = inventory::VanillaContainer::current();
                for (auto const& slot : v->slots()) {
                    if (slot.kind == platform::inventory::InventoryKind::Container) continue;
                    auto const& stack = v->itemOf(slot);
                    if (stack.isNull() || stack.mCount == 0 || !(item::ItemType::of(stack) == mLast.input)) continue;
                    heldNow += stack.mCount;
                    if (source < 0 && !LockSlotsHandler::get().isMappedSlotLocked(slot)) source = slot.slotNumber;
                }
                if (heldNow <= mLast.held && source >= 0) {
                    inventory::ContainerClicker::shiftClick(source);
                    ++mRefillCount;
                    mExpectedResult = mLast.result;
                    mStage          = AFTER_REFILL;
                    mTries          = 0;
                    mReselects      = 0;
                    mWait           = 6;
                }
            }
            break;
        }
        case AFTER_REFILL:
            if (!cur.result.isEmpty() && cur.result == mExpectedResult) {
                mStage = AFTER_SELECT;
                mWait  = 6;
                return;
            }
            if (mSelected < 0 || mSelected >= static_cast<int>(mButtons.size()) || cur.input.isEmpty()) {
                if (++mTries > 10) mStage = WATCH;
                return;
            }
            mRestoreX = platform::input::InputHandler::get().mouseX();
            mRestoreY = platform::input::InputHandler::get().mouseY();
            mStage    = MOVE;
            [[fallthrough]];
        case MOVE: {
            auto const& b = mButtons[mSelected];
            platform::input::GameInput::move(b.x + b.w / 2.0, b.y + b.h / 2.0);
            mStage = PRESS;
            return;
        }
        case PRESS: {
            auto const& b = mButtons[mSelected];
            platform::input::GameInput::button(true, b.x + b.w / 2.0, b.y + b.h / 2.0);
            mStage = RELEASE;
            return;
        }
        case RELEASE: {
            auto const& b = mButtons[mSelected];
            platform::input::GameInput::button(false, b.x + b.w / 2.0, b.y + b.h / 2.0);
            mStage = RESTORE;
            return;
        }
        case RESTORE:
            platform::input::GameInput::move(mRestoreX, mRestoreY);
            ++mReselectCount;
            mStage = AFTER_SELECT;
            mWait  = 6;
            return;
        case AFTER_SELECT:
            if (!cur.input.isEmpty() && !cur.result.isEmpty() && !(cur.result == mExpectedResult) && ++mReselects <= 2) {
                mStage = AFTER_REFILL;
                mTries = 0;
                return;
            }
            if (!cur.result.isEmpty() && cur.result == mExpectedResult && shiftDown() && config::isPressing(config::ModSettings::INCLUDE_HOTBAR_MODIFIER)) {
                shiftClickResult();
                mWait = 2;
            }
            mStage      = WATCH;
            mGuardTicks = 40;
            break;
        default:
            mStage = WATCH;
            break;
        }
        mLast = read();
    } catch (std::exception const& e) {
        Log::warn("stonecutter: {}", e.what());
        mStage = WATCH;
    }
}

}
