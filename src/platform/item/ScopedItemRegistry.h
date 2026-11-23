// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <optional>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/world/item/registry/ItemRegistryManager.h"
#include "mc/world/item/registry/ItemRegistryRef.h"

namespace inventory_utils::platform::item {

class ScopedItemRegistry {
public:
    ScopedItemRegistry() {
        if (auto ci = ll::service::getClientInstance()) init(*ci);
    }
    explicit ScopedItemRegistry(ClientInstance& ci) { init(ci); }
    ScopedItemRegistry(ScopedItemRegistry const&)            = delete;
    ScopedItemRegistry& operator=(ScopedItemRegistry const&) = delete;

private:
    void init(ClientInstance& ci) {
        mRegistry = ci.getItemRegistry();
        mScoped.emplace(mRegistry.mWeakRegistry);
    }

    ItemRegistryRef                                        mRegistry;
    std::optional<ItemRegistryManager::ScopedItemRegistry> mScoped;
};

}
