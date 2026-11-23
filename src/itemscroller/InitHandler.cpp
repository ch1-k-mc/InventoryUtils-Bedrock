// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/InitHandler.h"

#include "itemscroller/config/Configs.h"
#include "itemscroller/config/Hotkeys.h"
#include "itemscroller/event/InputHandler.h"
#include "itemscroller/event/KeybindCallbacks.h"
#include "itemscroller/event/RenderEventHandler.h"
#include "itemscroller/event/WorldLoadListener.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "platform/config/ConfigRegistry.h"
#include "platform/input/InputHandler.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::itemscroller {

namespace {
ll::event::ListenerPtr gTickListener;
using malilib::hotkeys::KeybindMulti;
namespace Hotkeys = config::Hotkeys;
}

void InitHandler::registerModHandlers() {
    platform::config::ConfigRegistry::get().add({
        "itemscroller",
        [] { return config::categories(); },
        [] { config::loadFromFile(); },
        [] { config::saveToFile(); },
    });
    config::loadFromFile();

    auto& input = platform::input::InputHandler::get();
    input.addKeybindProvider([](std::vector<KeybindMulti*>& out) {
        for (auto* h : Hotkeys::HOTKEY_LIST()) out.push_back(&h->getKeybind());
    });
    input.registerKeyboardInputHandler(&event::InputHandler::getInstance());
    input.registerMouseInputHandler(&event::InputHandler::getInstance());
    platform::screen::ScreenTracker::addContainerFrameListener([] { event::InputHandler::getInstance().onContainerFrame(); });

    event::KeybindCallbacks::getInstance().setCallbacks();

    platform::screen::ScreenTracker::addContainerOverlayRenderer([](ScreenView& view, MinecraftUIRenderContext& ctx) {
        event::RenderEventHandler::instance().onContainerOverlay(view, ctx);
    });
    gTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientLevelTickEvent>([](ll::event::ClientLevelTickEvent&) {
        event::KeybindCallbacks::getInstance().onClientTick();
    });
    event::WorldLoadListener::install();
}

void InitHandler::unregisterModHandlers() {
    event::WorldLoadListener::uninstall();
    if (gTickListener) ll::event::EventBus::getInstance().removeListener(gTickListener);
    gTickListener.reset();
    event::KeybindCallbacks::getInstance().clearCallbacks();
    auto& input = platform::input::InputHandler::get();
    input.unregisterKeyboardInputHandler(&event::InputHandler::getInstance());
    input.unregisterMouseInputHandler(&event::InputHandler::getInstance());
}

}
