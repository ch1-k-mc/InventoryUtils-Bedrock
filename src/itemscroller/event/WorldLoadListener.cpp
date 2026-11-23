// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/event/WorldLoadListener.h"

#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"

#include "itemscroller/recipes/RecipeStorage.h"

namespace inventory_utils::itemscroller::event {

namespace {
std::vector<ll::event::ListenerPtr> gListeners;
}

void WorldLoadListener::install() {
    auto& bus = ll::event::EventBus::getInstance();
    gListeners.push_back(bus.emplaceListener<ll::event::ClientJoinLevelEvent>([](ll::event::ClientJoinLevelEvent&) {
        auto        ci   = ll::service::getClientInstance();
        std::string name = ci && ci->getLevel() ? ci->getLevel()->getLevelData().mLevelName.get() : std::string();
        auto&       rs   = recipes::RecipeStorage::getInstance();
        rs.reset(true);
        rs.setWorldName(name);
        rs.readFromDisk();
    }));
    gListeners.push_back(bus.emplaceListener<ll::event::ClientExitLevelEvent>([](ll::event::ClientExitLevelEvent&) {
        auto& rs = recipes::RecipeStorage::getInstance();
        rs.writeToDisk();
        rs.reset(true);
    }));
}

void WorldLoadListener::uninstall() {
    for (auto& l : gListeners) {
        if (l) ll::event::EventBus::getInstance().removeListener(l);
    }
    gListeners.clear();
}

}
