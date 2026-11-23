// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/InitHandler.h"

#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/event/AnvilHandler.h"
#include "ipnext/event/AutoRefillHandler.h"
#include "ipnext/event/ContinuousCraftingHandler.h"
#include "ipnext/event/LockSlotsHandler.h"
#include "ipnext/event/LockedSlotKeeper.h"
#include "ipnext/event/ProfileSwitchHandler.h"
#include "ipnext/event/StoneCutterCraftingHandler.h"
#include "ipnext/event/villagers/VillagerTradeManager.h"
#include "ipnext/gui/inject/SortingButtonCollectionWidget.h"
#include "ipnext/inventory/ContainerClicker.h"
#include "ipnext/inventory/ContainerTypes.h"
#include "ipnext/inventory/GeneralInventoryActions.h"
#include "ipnext/parser/RuleLoader.h"
#include "gui/GuiHost.h"
#include "platform/config/ConfigRegistry.h"
#include "platform/i18n/I18n.h"
#include "platform/input/InputHandler.h"
#include "platform/inventory/SlotClicker.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::ipnext::InitHandler {

namespace {

using malilib::hotkeys::KeyAction;
using malilib::hotkeys::KeybindMulti;

std::vector<ll::event::ListenerPtr> gListeners;

void bind(malilib::config::ConfigHotkey& hotkey, void (*action)()) {
    hotkey.getKeybind().setCallback([action](KeyAction, KeybindMulti&) {
        if (!inventory::VanillaContainer::current()) return false;
        try {
            action();
        } catch (std::exception const& e) {
            Log::warn("hotkey: {}", e.what());
        }
        return true;
    });
}

}

void reloadCustomConfigs() {
    parser::RuleLoader::reload(true);
    event::ProfileSwitchHandler::get().load(true);
    event::LockSlotsHandler::get().load();
    event::AutoRefillHandler::get().loadDisabled();
}

void registerModHandlers() {
    platform::config::ConfigRegistry::get().add({
        "ipnext",
        [] { return config::categories(); },
        [] { config::loadFromFile(); },
        [] { config::saveToFile(); },
    });
    config::loadFromFile();
    {
        auto const& v = config::GuiSettings::CONTINUOUS_CRAFTING_CHECKBOX_VALUE.getOptionListValue();
        if (v == "CHECKED") config::GuiSettings::CONTINUOUS_CRAFTING_SAVED_VALUE.setBooleanValue(true);
        else if (v == "UNCHECKED") config::GuiSettings::CONTINUOUS_CRAFTING_SAVED_VALUE.setBooleanValue(false);
    }

    auto& input = platform::input::InputHandler::get();
    input.addKeybindProvider([](std::vector<KeybindMulti*>& out) {
        for (auto* h : config::Hotkeys::HOTKEY_LIST()) out.push_back(&h->getKeybind());
    });
    input.updateUsedKeys();

    using namespace inventory::GeneralInventoryActions;
    namespace H = config::Hotkeys;
    bind(H::SORT_INVENTORY, [] { doSort(); });
    bind(H::SORT_INVENTORY_IN_COLUMNS, [] { doSortInColumns(); });
    bind(H::SORT_INVENTORY_IN_ROWS, [] { doSortInRows(); });
    bind(H::MOVE_ALL_ITEMS, [] { doMoveMatch(); });
    bind(H::THROW_ALL_ITEMS, [] { doThrowMatch(); });
    bind(config::Debugs::CLEAN_CURSOR, [] { cleanCursor(); });
    H::RELOAD_CUSTOM_CONFIGS.getKeybind().setCallback([](KeyAction, KeybindMulti&) {
        reloadCustomConfigs();
        return true;
    });
    H::OPEN_CONFIG_MENU.getKeybind().setCallback([](KeyAction, KeybindMulti&) {
        ::inventory_utils::gui::host::openConfig();
        return true;
    });

    auto& profilesHandler = event::ProfileSwitchHandler::get();
    (void)profilesHandler;
    H::APPLY_PROFILE.getKeybind().setCallback([](KeyAction, KeybindMulti&) { return event::ProfileSwitchHandler::get().onApplyProfile(); });
    H::NEXT_PROFILE.getKeybind().setCallback([](KeyAction, KeybindMulti&) { return event::ProfileSwitchHandler::get().onNextProfile(); });
    H::PREV_PROFILE.getKeybind().setCallback([](KeyAction, KeybindMulti&) { return event::ProfileSwitchHandler::get().onPrevProfile(); });
    for (int i = 0; i < 10; ++i) {
        H::PROFILE_KEYS[i].getKeybind().setCallback([i](KeyAction, KeybindMulti&) { return event::ProfileSwitchHandler::get().onQuickProfile(i); });
    }
    H::SAVE_AS_PROFILE.getKeybind().setCallback([](KeyAction, KeybindMulti&) {
        if (platform::screen::ScreenTracker::containerScreenName() != "inventory_screen") return false;
        return event::ProfileSwitchHandler::get().onSaveAsProfile();
    });

    platform::screen::ScreenTracker::addContainerFrameListener([] { inventory::ContainerClicker::onContainerFrame(); });

    platform::i18n::addNamespace("inventoryprofilesnext");
    platform::screen::ScreenTracker::addContainerOverlayRenderer([](ScreenView& view, MinecraftUIRenderContext& ctx) {
        gui::inject::SortingButtonCollectionWidget::get().onContainerOverlay(view, ctx);
    });
    input.registerMouseInputHandler(&gui::inject::SortingButtonCollectionWidget::get());

    auto& locks = event::LockSlotsHandler::get();
    input.registerMouseInputHandler(&locks, true);
    input.registerKeyboardInputHandler(&locks, true);
    platform::screen::ScreenTracker::addContainerFrameListener([] { event::LockSlotsHandler::get().onContainerFrame(); });
    platform::screen::ScreenTracker::addContainerOverlayRenderer([](ScreenView& view, MinecraftUIRenderContext& ctx) {
        event::LockSlotsHandler::get().onContainerOverlay(view, ctx);
    });
    platform::screen::ScreenTracker::addHudOverlayRenderer([](ScreenView& view, MinecraftUIRenderContext& ctx) { event::LockSlotsHandler::get().onHudOverlay(view, ctx); });
    auto& refill = event::AutoRefillHandler::get();
    input.registerMouseInputHandler(&refill, true);
    platform::screen::ScreenTracker::addContainerFrameListener([] { event::AutoRefillHandler::get().onContainerFrame(); });
    platform::screen::ScreenTracker::addContainerOverlayRenderer([](ScreenView& view, MinecraftUIRenderContext& ctx) {
        event::AutoRefillHandler::get().onContainerOverlay(view, ctx);
    });
    platform::screen::ScreenTracker::addHudOverlayRenderer([](ScreenView& view, MinecraftUIRenderContext& ctx) { event::AutoRefillHandler::get().onHudOverlay(view, ctx); });
    H::AUTO_REFILL_GAME_TOGGLE_FOR_SLOT.getKeybind().setCallback([](KeyAction, KeybindMulti&) { return event::AutoRefillHandler::get().onGameToggleKey(); });
    gListeners.push_back(ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientLevelTickEvent>([](ll::event::ClientLevelTickEvent&) {
        event::LockedSlotKeeper::get().onClientTick();
        event::AutoRefillHandler::get().onClientTick();
        event::ProfileSwitchHandler::get().onClientTick();
        event::ContinuousCraftingHandler::get().onClientTick();
        event::villagers::VillagerTradeManager::get().onClientTick();
        event::AnvilHandler::get().onClientTick();
        event::StoneCutterCraftingHandler::get().onClientTick();
    }));
    input.registerMouseInputHandler(&event::StoneCutterCraftingHandler::get(), true);
    platform::screen::ScreenTracker::addContainerOverlayRenderer([](ScreenView& view, MinecraftUIRenderContext& ctx) {
        event::StoneCutterCraftingHandler::get().onContainerOverlay(view, ctx);
    });
    platform::screen::ScreenTracker::addContainerOverlayRenderer([](ScreenView& view, MinecraftUIRenderContext& ctx) {
        event::villagers::VillagerTradeManager::get().onContainerOverlay(view, ctx);
    });
    for (int g = 0; g < 3; ++g) {
        H::GLOBAL_BOOKMARK_TRADE[g].getKeybind().setCallback([g](KeyAction, KeybindMulti&) { return event::villagers::VillagerTradeManager::get().onBookmarkKey(true, g); });
        H::LOCAL_BOOKMARK_TRADE[g].getKeybind().setCallback([g](KeyAction, KeybindMulti&) { return event::villagers::VillagerTradeManager::get().onBookmarkKey(false, g); });
        H::DO_GLOBAL_TRADE[g].getKeybind().setCallback([g](KeyAction, KeybindMulti&) { return event::villagers::VillagerTradeManager::get().onDoTradesKey(true, g); });
        H::DO_LOCAL_TRADE[g].getKeybind().setCallback([g](KeyAction, KeybindMulti&) { return event::villagers::VillagerTradeManager::get().onDoTradesKey(false, g); });
    }
    platform::inventory::SlotClicker::setQuickMoveFilter([](platform::inventory::MenuSlot const& slot, bool isThrow, ::ItemStack const& item) {
        return event::LockSlotsHandler::get().isQuickMoveAllowed(slot, isThrow, item);
    });

    gListeners.push_back(ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientJoinLevelEvent>([](ll::event::ClientJoinLevelEvent&) {
        parser::RuleLoader::load();
        auto        ci   = ll::service::getClientInstance();
        std::string name = ci && ci->getLevel() ? ci->getLevel()->getLevelData().mLevelName.get() : std::string();
        event::LockSlotsHandler::get().onJoinWorld(name);
        event::LockedSlotKeeper::get().onJoinWorld();
        event::AutoRefillHandler::get().onJoinWorld(name);
        event::ProfileSwitchHandler::get().onJoinWorld(name);
        event::villagers::VillagerTradeManager::get().onJoinWorld();
    }));
    parser::RuleLoader::load();
}

void unregisterModHandlers() {
    for (auto& l : gListeners) {
        if (l) ll::event::EventBus::getInstance().removeListener(l);
    }
    gListeners.clear();
    for (auto* h : config::Hotkeys::HOTKEY_LIST()) h->getKeybind().setCallback({});
    platform::input::InputHandler::get().unregisterMouseInputHandler(&gui::inject::SortingButtonCollectionWidget::get());
    platform::input::InputHandler::get().unregisterMouseInputHandler(&event::LockSlotsHandler::get());
    platform::input::InputHandler::get().unregisterMouseInputHandler(&event::AutoRefillHandler::get());
    platform::input::InputHandler::get().unregisterMouseInputHandler(&event::StoneCutterCraftingHandler::get());
    platform::input::InputHandler::get().unregisterKeyboardInputHandler(&event::LockSlotsHandler::get());
    platform::inventory::SlotClicker::setQuickMoveFilter({});
    event::villagers::VillagerTradeManager::get().uninstall();
}

}
