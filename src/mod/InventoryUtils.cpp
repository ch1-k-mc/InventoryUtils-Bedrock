// SPDX-License-Identifier: AGPL-3.0-only
#include "mod/InventoryUtils.h"

#include "ll/api/mod/RegisterHelper.h"

#include "command/InvUtilsCommand.h"
#include "craftingtools/CraftingTools.h"
#include "favorites/HotbarFavorites.h"
#include "gui/GuiHost.h"
#include "gui/GuiManager.h"
#include "ipnext/event/villagers/VillagerTradeManager.h"
#include "ipnext/InitHandler.h"
#include "itemscroller/InitHandler.h"
#include "malilib/hotkeys/KeybindMulti.h"
#include "platform/input/InputHandler.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils {

InventoryUtils& InventoryUtils::getInstance() {
    static InventoryUtils instance;
    return instance;
}

bool InventoryUtils::load() {
    getSelf().getLogger().info("Inventory Utils: loading");
    command::install();
    gui::GuiManager::installHooks();
    ipnext::event::villagers::VillagerTradeManager::get().install();
    return true;
}

bool InventoryUtils::enable() {
    platform::screen::ScreenTracker::install();
    malilib::hotkeys::KeybindMulti::setGuiOpenQuery([] { return platform::screen::ScreenTracker::isGuiOpen(); });
    platform::input::InputHandler::get().install();
    itemscroller::InitHandler::registerModHandlers();
    ipnext::InitHandler::registerModHandlers();
    craftingtools::install();
    favorites::HotbarFavorites::get().install();
    gui::host::install();
    getSelf().getLogger().info("Inventory Utils: enabled");
    return true;
}

bool InventoryUtils::disable() {
    gui::host::uninstall();
    favorites::HotbarFavorites::get().uninstall();
    craftingtools::uninstall();
    ipnext::InitHandler::unregisterModHandlers();
    itemscroller::InitHandler::unregisterModHandlers();
    platform::input::InputHandler::get().uninstall();
    platform::screen::ScreenTracker::uninstall();
    getSelf().getLogger().info("Inventory Utils: disabled");
    return true;
}

}

LL_REGISTER_MOD(inventory_utils::InventoryUtils, inventory_utils::InventoryUtils::getInstance());
