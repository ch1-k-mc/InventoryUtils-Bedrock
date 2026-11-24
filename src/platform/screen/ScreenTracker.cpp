// SPDX-License-Identifier: AGPL-3.0-only
#include "gui/GuiManager.h"
#include "platform/input/InputHandler.h"
#include "platform/screen/ScreenTracker.h"

#include <array>
#include <string_view>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/gui/screens/controllers/HudScreenController.h"
#include "mc/world/containers/managers/controllers/HudContainerManagerController.h"

#include "mc/world/containers/controllers/ContainerController.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/containers/models/ContainerModel.h"

#include "platform/inventory/InventorySession.h"

namespace inventory_utils::platform::screen {

namespace {

constexpr std::array<std::string_view, 26> kContainerScreens{
    "inventory_screen",     "crafting_screen",        "chest_screen",         "small_chest_screen",
    "large_chest_screen",   "barrel_screen",          "shulker_box_screen",   "ender_chest_screen",
    "furnace_screen",       "blast_furnace_screen",   "smoker_screen",        "anvil_screen",
    "brewing_stand_screen", "stonecutter_screen",     "smithing_table_screen", "grindstone_screen",
    "loom_screen",          "cartography_screen",     "beacon_screen",        "enchanting_screen",
    "hopper_screen",        "dispenser_screen",       "dropper_screen",       "trade2_screen",        "horse_screen",
    "trade_screen",
};

ll::event::ListenerPtr          gListener;
ll::event::ListenerPtr          gAfterListener;
std::vector<std::function<void(ScreenView&, MinecraftUIRenderContext&)>> gOverlayRenderers;
std::vector<std::function<void(ScreenView&, MinecraftUIRenderContext&)>> gHudRenderers;
std::weak_ptr<ScreenController> gController;
std::weak_ptr<ScreenController> gHudController;
std::string                     gScreen;
std::set<std::string>           gSeen;
std::vector<std::function<void()>> gFrameListeners;

void record(ll::event::BeforeUIRenderEvent& e, std::string const& name);
void logCollectionsOnce(std::string const& name);

void onBeforeUiRender(ll::event::BeforeUIRenderEvent& e) {
    std::string name = e.screenView().getScreenName();
    record(e, name);
    if (inventory::InventorySession::onBeforeRender(name, e.screenView())) e.cancel();
}

void onAfterUiRender(ll::event::AfterUIRenderEvent& e) {
    input::InputHandler::get().checkFocus();
    if (inventory::InventorySession::busy()) return;
    std::string name = e.screenView().getScreenName();
    if (name == "hud_screen") {
        if (ScreenTracker::isGuiOpen()) return;
        for (auto& r : gHudRenderers) r(e.screenView(), e.uiRenderContext());
        return;
    }
    if (gOverlayRenderers.empty()) return;
    if (name.empty() || name != gScreen || !ScreenTracker::containerScreen()) return;
    for (auto& r : gOverlayRenderers) r(e.screenView(), e.uiRenderContext());
}

void record(ll::event::BeforeUIRenderEvent& e, std::string const& name) {
    if (gSeen.insert(name).second) {
        ll::mod::NativeMod::current()->getLogger().info("First seen screen: {}", name);
    }
    if (name == "hud_screen") gHudController = e.screenView().mController;
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    std::string top = ci->getTopScreenName();
    if (!ScreenTracker::isContainerScreenName(name)) {
        if (!gScreen.empty() && top != gScreen) {
            gScreen.clear();
            gController.reset();
        }
        return;
    }
    if (top != name) return;
    gScreen     = name;
    gController = e.screenView().mController;
    logCollectionsOnce(name);
    for (auto& l : gFrameListeners) l();
}

void logCollectionsOnce(std::string const& name) {
    static std::set<std::string> logged;
    if (logged.contains(name)) return;
    auto* mgr = ScreenTracker::containerManager();
    if (!mgr) return;
    logged.insert(name);
    std::string list;
    for (auto const& [collection, ctrl] : *mgr->mContainers) {
        int size = 0;
        if (ctrl) {
            if (auto model = ctrl->mContainerModel.lock()) size = model->getContainerSize();
        }
        if (!list.empty()) list += ", ";
        list += collection + "(" + std::to_string(size) + ")";
    }
    ll::mod::NativeMod::current()->getLogger().info("Collection {}: {}", name, list);
}

}

void ScreenTracker::install() {
    if (gListener) return;
    gListener      = ll::event::EventBus::getInstance().emplaceListener<ll::event::BeforeUIRenderEvent>(onBeforeUiRender);
    gAfterListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>(onAfterUiRender, ll::event::EventPriority::High);
}

void ScreenTracker::addContainerFrameListener(std::function<void()> listener) { gFrameListeners.push_back(std::move(listener)); }

void ScreenTracker::addHudOverlayRenderer(std::function<void(ScreenView&, MinecraftUIRenderContext&)> renderer) {
    gHudRenderers.push_back(std::move(renderer));
}

void ScreenTracker::addContainerOverlayRenderer(std::function<void(ScreenView&, MinecraftUIRenderContext&)> renderer) {
    gOverlayRenderers.push_back(std::move(renderer));
}

void ScreenTracker::uninstall() {
    gFrameListeners.clear();
    if (gListener) ll::event::EventBus::getInstance().removeListener(gListener);
    if (gAfterListener) ll::event::EventBus::getInstance().removeListener(gAfterListener);
    gListener.reset();
    gAfterListener.reset();
    gOverlayRenderers.clear();
    gHudRenderers.clear();
    gController.reset();
    gHudController.reset();
    gScreen.clear();
}

ContainerScreenController* ScreenTracker::containerScreen() {
    if (gScreen.empty()) return nullptr;
    auto ci = ll::service::getClientInstance();
    if (!ci || ci->getTopScreenName() != gScreen) return nullptr;
    auto c = gController.lock();
    return c ? static_cast<ContainerScreenController*>(c.get()) : nullptr;
}

ContainerManagerController* ScreenTracker::containerManager() {
    auto* csc = containerScreen();
    if (!csc) return nullptr;
    auto&& mgr = csc->mContainerManagerController.get();
    return mgr ? &*mgr : nullptr;
}

ContainerManagerController* ScreenTracker::hudManager() {
    auto c = gHudController.lock();
    if (!c) return nullptr;
    auto&& mgr = static_cast<HudScreenController*>(c.get())->mHudScreenManagerController.get();
    return mgr ? static_cast<ContainerManagerController*>(&*mgr) : nullptr;
}

std::string const& ScreenTracker::containerScreenName() { return gScreen; }

std::set<std::string> const& ScreenTracker::seenScreens() { return gSeen; }

bool ScreenTracker::isGuiOpen() {
    if (gui::GuiManager::get().isOpen()) return true;
    auto ci = ll::service::getClientInstance();
    if (!ci) return false;
    std::string top = ci->getTopScreenName();
    return !(top.empty() || top == "hud_screen" || top == "hud_crosshair_screen" || top == "in_game_play_screen");
}

bool ScreenTracker::isContainerScreenName(std::string const& name) {
    for (auto s : kContainerScreens) {
        if (name == s) return true;
    }
    return false;
}

}
