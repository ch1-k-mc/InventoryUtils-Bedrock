// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/event/LockSlotsHandler.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/item/ItemStack.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "platform/input/KeyCodes.h"
#include "platform/inventory/SlotClicker.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/render/Gui2d.h"
#include "platform/screen/ScreenLayout.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::ipnext::event {

namespace {

using namespace config;
using platform::render::Gui2d;
namespace KC = platform::input::KeyCodes;

constexpr char const* OVERLAY_TEXTURE = "inventoryprofilesnext/textures/gui/overlay_new.png";

struct SpriteRect {
    int u, v;
};
SpriteRect backgroundSprite() { return {40, 8}; }
SpriteRect foregroundSprite() { return {40 + 32, 8 + 32 * (LockedSlotsSettings::LOCKED_SLOTS_FOREGROUND_STYLE.getIntegerValue() - 1)}; }
SpriteRect configSprite() { return {40 - 32, 8}; }

void drawCenteredSprite(Gui2d& g, SpriteRect s, int cx, int cy) { g.blitPixels(OVERLAY_TEXTURE, cx - 16, cy - 16, s.u, s.v, 32, 32, 256, 256); }

uint32_t withAlphaScale(uint32_t argb, float scale) {
    uint32_t a = static_cast<uint32_t>(static_cast<float>((argb >> 24) & 0xFF) * scale);
    return (a << 24) | (argb & 0x00FFFFFF);
}

std::string sanitized(std::string s) {
    for (auto& c : s) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*' || static_cast<unsigned char>(c) < 0x20) c = '_';
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    return s;
}

platform::inventory::MenuSlot const* hoveredSlot(platform::inventory::SlotClicker const& clicker) { return clicker.getSlotUnderMouse(); }

}

LockSlotsHandler& LockSlotsHandler::get() {
    static LockSlotsHandler instance;
    return instance;
}

bool LockSlotsHandler::enabledForeground() const {
    return Features::ENABLE_LOCK_SLOTS.getBooleanValue() && !isPressing(LockedSlotsSettings::LOCK_SLOTS_QUICK_DISABLE);
}

std::vector<int> LockSlotsHandler::lockedInvSlots() const {
    if (!enabledForeground()) return {};
    return {mStored.begin(), mStored.end()};
}

bool LockSlotsHandler::isSlotLocked(int invSlot) const { return enabledForeground() && mStored.contains(invSlot); }

int LockSlotsHandler::invSlotOf(std::string const& collection, int index) {
    if (collection == "inventory_items") return 9 + index;
    if (collection == "hotbar_items") return index;
    if (collection == "offhand_items") return 40;
    if (collection == "armor_items") return 36 + index;
    return -1;
}

bool LockSlotsHandler::isMappedSlotLocked(platform::inventory::MenuSlot const& slot) const {
    if (slot.kind == platform::inventory::InventoryKind::Container) return false;
    return isSlotLocked(slot.containerSlot);
}

void LockSlotsHandler::setLocked(int invSlot, bool locked) {
    if (locked) mStored.insert(invSlot);
    else mStored.erase(invSlot);
}

void LockSlotsHandler::clearAll() { mStored.clear(); }

bool LockSlotsHandler::isQuickMoveAllowed(platform::inventory::MenuSlot const& slot, bool isThrow, ::ItemStack const& item) const {
    if (!isMappedSlotLocked(slot)) return true;
    if (LockedSlotsSettings::LOCK_SLOTS_DISABLE_USER_INTERACTION.getBooleanValue() || LockedSlotsSettings::LOCKED_SLOTS_DISABLE_QUICK_MOVE_THROW.getBooleanValue()) return false;
    if (isThrow && slot.kind == platform::inventory::InventoryKind::Hotbar && LockedSlotsSettings::LOCKED_SLOTS_DISABLE_THROW_FOR_NON_STACKABLE.getBooleanValue()) {
        return !item.isNull() && item.getMaxStackSize() > 1;
    }
    return true;
}

std::filesystem::path LockSlotsHandler::file() const {
    std::string dir = LockedSlotsSettings::ENABLE_LOCK_SLOTS_PER_SERVER.getBooleanValue() ? sanitized(mWorldName) : std::string();
    auto        d   = configDir() / std::filesystem::u8path(dir);
    std::error_code ec;
    std::filesystem::create_directories(d, ec);
    return d / "lockSlots.txt";
}

void LockSlotsHandler::onJoinWorld(std::string const& worldName) {
    mWorldName = worldName;
    load();
}

void LockSlotsHandler::load() {
    mCachedSaved.clear();
    mStored.clear();
    std::ifstream in(file(), std::ios::binary);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        try {
            size_t pos = 0;
            int    v   = std::stoi(line, &pos);
            mStored.insert(v);
        } catch (...) {
        }
    }
    mCachedSaved.assign(mStored.begin(), mStored.end());
}

void LockSlotsHandler::save() {
    std::vector<int> sorted(mStored.begin(), mStored.end());
    if (sorted == mCachedSaved) return;
    mCachedSaved = sorted;
    std::ofstream out(file(), std::ios::binary | std::ios::trunc);
    for (size_t i = 0; i < sorted.size(); ++i) out << (i ? "\n" : "") << sorted[i];
    if (!out) Log::error("Failed to write lockSlots.txt");
}

void LockSlotsHandler::updateDisplayingConfig() {
    bool was          = mDisplayingConfig;
    mDisplayingConfig = enabledForeground() && platform::screen::ScreenTracker::containerScreen()
                     && (isPressing(LockedSlotsSettings::LOCK_SLOTS_SWITCH_CONFIG_MODIFIER) || isPressing(LockedSlotsSettings::LOCK_SLOTS_QUICK_CONFIG_KEY));
    if (was && !mDisplayingConfig) save();
}

bool LockSlotsHandler::onMouseClick(double, double, int buttonCode, bool buttonState) {
    if (!enabledForeground() || !platform::screen::ScreenTracker::containerScreen()) return false;
    updateDisplayingConfig();
    bool currentClicked = (mDisplayingConfig && isPressing(LockedSlotsSettings::LOCK_SLOTS_CONFIG_KEY)) || isPressing(LockedSlotsSettings::LOCK_SLOTS_QUICK_CONFIG_KEY);
    if (currentClicked != mClicked) {
        if (!currentClicked) {
            mClicked = false;
            save();
            return true;
        }
        auto clicker = platform::inventory::SlotClicker::current();
        if (auto const* s = clicker.valid() ? hoveredSlot(clicker) : nullptr) {
            int inv = s->kind == platform::inventory::InventoryKind::Container ? -1 : s->containerSlot;
            if (inv >= 0) {
                mClicked = true;
                mMode    = mStored.contains(inv) ? 1 : 0;
                setLocked(inv, mMode == 0);
                return true;
            }
        }
    }
    if (!buttonState) return false;
    if (buttonCode != KC::MOUSE_LEFT && buttonCode != KC::MOUSE_RIGHT) return false;
    auto clicker = platform::inventory::SlotClicker::current();
    if (!clicker.valid()) return false;
    auto const* s = hoveredSlot(clicker);
    if (!s || !isMappedSlotLocked(*s)) return false;
    if (LockedSlotsSettings::LOCK_SLOTS_DISABLE_USER_INTERACTION.getBooleanValue()) return true;
    bool shift = platform::input::InputHandler::get().isKeyDown(VK_LSHIFT) || platform::input::InputHandler::get().isKeyDown(VK_RSHIFT);
    if (shift) {
        platform::item::ScopedItemRegistry registry;
        return !isQuickMoveAllowed(*s, false, clicker.menu().getItem(*s));
    }
    return false;
}

bool LockSlotsHandler::onKeyInput(int keyCode, bool keyState) {
    if (!enabledForeground()) return false;
    if (platform::screen::ScreenTracker::containerScreen()) updateDisplayingConfig();
    if (!keyState || keyCode != 'Q') return false;
    platform::item::ScopedItemRegistry registry;
    if (platform::screen::ScreenTracker::containerScreen()) {
        auto clicker = platform::inventory::SlotClicker::current();
        if (!clicker.valid()) return false;
        auto const* s = hoveredSlot(clicker);
        if (!s || !isMappedSlotLocked(*s)) return false;
        return !isQuickMoveAllowed(*s, true, clicker.menu().getItem(*s));
    }
    if (platform::screen::ScreenTracker::isGuiOpen()) return false;
    auto ci = ll::service::getClientInstance();
    auto* player = ci ? ci->getLocalPlayer() : nullptr;
    if (!player) return false;
    int selected = player->getSelectedItemSlot();
    if (!isSlotLocked(selected)) return false;
    platform::inventory::MenuSlot hb;
    hb.kind          = platform::inventory::InventoryKind::Hotbar;
    hb.containerSlot = selected;
    return !isQuickMoveAllowed(hb, true, player->getSelectedItem());
}

void LockSlotsHandler::onContainerFrame() {
    if (!enabledForeground()) {
        mClicked = false;
        return;
    }
    updateDisplayingConfig();
    if (!mClicked) return;
    auto clicker = platform::inventory::SlotClicker::current();
    if (!clicker.valid()) return;
    if (auto const* s = hoveredSlot(clicker)) {
        if (s->kind == platform::inventory::InventoryKind::Container) return;
        int inv = s->containerSlot;
        if ((mMode == 0) == !mStored.contains(inv)) setLocked(inv, mMode == 0);
    }
}

void LockSlotsHandler::onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx) {
    if (!Features::ENABLE_LOCK_SLOTS.getBooleanValue()) return;
    bool showForeground = enabledForeground() && LockedSlotsSettings::SHOW_LOCKED_SLOTS_FOREGROUND.getBooleanValue();
    bool showBackground = LockedSlotsSettings::SHOW_LOCKED_SLOTS_BACKGROUND.getBooleanValue();
    if (!showForeground && !showBackground && !mDisplayingConfig) return;
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    auto  rects = platform::screen::collectSlotRects(view);
    Gui2d g(ctx, *ci);
    auto  bg = withAlphaScale(static_cast<uint32_t>(LockedSlotsSettings::SHOW_LOCKED_SLOTS_BG_COLOR.getIntegerValue()), 1.0f / 3.0f);
    for (auto const& [key, r] : rects) {
        int inv = invSlotOf(key.first, key.second);
        if (inv < 0) continue;
        int  x = r.x + 1, y = r.y + 1;
        bool locked = mStored.contains(inv);
        if (mDisplayingConfig) {
            if (locked) g.fill(x, y, x + 16, y + 16, bg);
            else drawCenteredSprite(g, configSprite(), x + 8, y + 8);
            continue;
        }
        if (!locked) continue;
        if (showBackground) g.fill(x, y, x + 16, y + 16, bg);
        if (showForeground) drawCenteredSprite(g, foregroundSprite(), x + 8, y + 8);
    }
    g.flush();
}

void LockSlotsHandler::onHudOverlay(ScreenView&, MinecraftUIRenderContext& ctx) {
    if (!enabledForeground() || !LockedSlotsSettings::ALSO_SHOW_LOCKED_SLOTS_IN_HOTBAR.getBooleanValue() || mStored.empty()) return;
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    auto* player = ci->getLocalPlayer();
    if (!player) return;
    Gui2d    g(ctx, *ci);
    int      selected = player->getSelectedItemSlot();
    uint32_t color    = static_cast<uint32_t>(LockedSlotsSettings::SHOW_LOCKED_SLOTS_HOTBAR_COLOR.getIntegerValue());
    int      i        = g.screenWidth() / 2;
    for (int j = 0; j <= 8; ++j) {
        if (!mStored.contains(j)) continue;
        int x = i - 90 + j * 20 + 2;
        int y = g.screenHeight() - 16 - 3;
        if (j == selected) {
            g.fill(x - 4, y - 4, x + 20, y, color);
            g.fill(x - 4, y, x, y + 16, color);
            g.fill(x - 4, y + 16, x + 20, y + 20, color);
            g.fill(x + 16, y, x + 20, y + 16, color);
        } else if (j - selected == -1) {
            g.fill(x - 2, y - 2, x + 16, y, color);
            g.fill(x - 2, y + 16, x + 16, y + 18, color);
            g.fill(x - 2, y, x, y + 16, color);
        } else if (j - selected == 1) {
            g.fill(x, y - 2, x + 16, y, color);
            g.fill(x, y + 16, x + 16, y + 18, color);
            g.fill(x + 16, y - 2, x + 18, y + 18, color);
        } else {
            g.fill(x - 2, y - 2, x + 16, y, color);
            g.fill(x - 2, y + 16, x + 16, y + 18, color);
            g.fill(x - 2, y, x, y + 16, color);
            g.fill(x + 16, y - 2, x + 18, y + 18, color);
        }
        if (LockedSlotsSettings::SHOW_LOCKED_SLOTS_FOREGROUND.getBooleanValue()) drawCenteredSprite(g, foregroundSprite(), x + 8, y + 8);
    }
    g.flush();
}

}
