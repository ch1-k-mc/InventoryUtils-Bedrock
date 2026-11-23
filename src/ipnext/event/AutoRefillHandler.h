// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "ipnext/item/ItemStack.h"
#include "platform/input/InputHandler.h"

class MinecraftUIRenderContext;
class ScreenView;

namespace inventory_utils::ipnext::event {

class AutoRefillHandler : public platform::input::IMouseInputHandler {
public:
    static AutoRefillHandler& get();

    void onClientTick();
    void onJoinWorld(std::string const& worldName);
    void init();

    [[nodiscard]] std::set<int> const& disabledSlots() const { return mDisabledSlots; }
    void                               toggleDisableRefillForSlot(int index);
    void                               loadDisabled();
    void                               saveDisabled();

    bool onGameToggleKey();
    bool onMouseClick(double mouseX, double mouseY, int buttonCode, bool buttonState) override;
    void onContainerFrame();

    void onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx);
    void onHudOverlay(ScreenView& view, MinecraftUIRenderContext& ctx);

    [[nodiscard]] std::string const& lastAction() const { return mLastAction; }
    [[nodiscard]] int                handledCount() const { return mHandledCount; }

    void blackListChanged();

    void markProfileSwapped(int invSlot) { mProfilesSwapped.insert(invSlot); }

    struct Monitor;

private:
    AutoRefillHandler();
    [[nodiscard]] std::filesystem::path disabledFile() const;

    std::vector<Monitor*> mMonitors;
    std::set<int>         mDisabledSlots;
    std::vector<int>      mCachedDisabled;
    std::set<std::string> mBlacklist;
    std::set<int>         mProfilesSwapped;
    std::string           mWorldName;
    std::string           mLastAction;
    int                   mHandledCount = 0;
    bool                  mScreenOpening = false;
    int                   mTicksAfterUp  = 16;
    bool                  mClicked       = false;
    int                   mMode          = 0;

    friend struct Monitor;
};

}
