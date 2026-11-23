// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "platform/input/InputHandler.h"
#include "platform/inventory/ContainerMenu.h"

class MinecraftUIRenderContext;
class ScreenView;
class ItemStack;

namespace inventory_utils::ipnext::event {

class LockSlotsHandler : public platform::input::IMouseInputHandler, public platform::input::IKeyboardInputHandler {
public:
    static LockSlotsHandler& get();

    [[nodiscard]] bool                 enabledForeground() const;
    [[nodiscard]] std::vector<int>     lockedInvSlots() const;
    [[nodiscard]] std::set<int> const& storedLockedInvSlots() const { return mStored; }
    [[nodiscard]] bool                 isSlotLocked(int invSlot) const;
    [[nodiscard]] bool                 isMappedSlotLocked(platform::inventory::MenuSlot const& slot) const;

    void setLocked(int invSlot, bool locked);
    void clearAll();

    [[nodiscard]] bool isQuickMoveAllowed(platform::inventory::MenuSlot const& slot, bool isThrow, ::ItemStack const& item) const;

    void onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx);
    void onHudOverlay(ScreenView& view, MinecraftUIRenderContext& ctx);
    void onContainerFrame();

    bool onMouseClick(double mouseX, double mouseY, int buttonCode, bool buttonState) override;
    bool onKeyInput(int keyCode, bool keyState) override;

    void onJoinWorld(std::string const& worldName);
    void load();
    void save();

    [[nodiscard]] bool displayingConfig() const { return mDisplayingConfig; }

private:
    [[nodiscard]] std::filesystem::path file() const;
    void                                updateDisplayingConfig();
    [[nodiscard]] static int            invSlotOf(std::string const& collection, int index);

    std::set<int>    mStored;
    std::vector<int> mCachedSaved;
    std::string      mWorldName;
    bool             mDisplayingConfig = false;
    bool             mClicked          = false;
    int              mMode             = 0;
};

}
