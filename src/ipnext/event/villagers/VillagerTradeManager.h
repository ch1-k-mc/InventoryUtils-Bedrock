// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "platform/screen/ScreenLayout.h"

class Actor;
class MerchantRecipe;
class MinecraftUIRenderContext;
class ScreenView;

namespace inventory_utils::ipnext::event::villagers {

struct VillagerTradeData {
    std::string resultItem;
    std::string priceItem1;
    std::string priceItem2;
    std::string resultItemNBT;
    std::string priceItem1NBT;
    std::string priceItem2NBT;
    bool        operator==(VillagerTradeData const&) const = default;
};

using BookmarkMap = std::map<std::string, std::vector<VillagerTradeData>>;

class VillagerTradeManager {
public:
    static VillagerTradeManager& get();

    void install();
    void uninstall();

    void refreshTrader();
    void onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx);
    void onClientTick();
    void onJoinWorld();

    bool onBookmarkKey(bool global, int group);
    bool onDoTradesKey(bool global, int group);

    bool                         toggleBookmark(int index, bool global, int group);
    bool                         startTrades(bool global, int group);
    bool                         startSelect(int index);
    void                         clearBookmarks();
    [[nodiscard]] bool           busy() const { return mJob.has_value(); }
    [[nodiscard]] nlohmann::json debugJson();

private:
    struct Job {
        std::vector<int> indices;
        size_t           at          = 0;
        int              step        = 0;
        int              wait        = 0;
        int              tries       = 0;
        int              iterations  = 0;
        bool             selectOnly  = false;
        double           restoreX    = 0;
        double           restoreY    = 0;
        std::string      expectedId;
        int              lastHeld    = -1;
    };

    Actor*                                       trader() const;
    [[nodiscard]] std::vector<MerchantRecipe*>   offers() const;
    [[nodiscard]] std::string                    profession() const;
    [[nodiscard]] std::string                    uuid() const;
    [[nodiscard]] bool                           isLocalAvailable() const;
    [[nodiscard]] bool                           tradeScreenOpen() const;
    [[nodiscard]] std::vector<VillagerTradeData> const& bookmarks(bool global, int group);
    [[nodiscard]] bool                           has(std::vector<VillagerTradeData> const& list, MerchantRecipe const& r) const;
    [[nodiscard]] int                            hoveredToggle() const;
    bool                                         startJob(std::vector<int> indices, bool selectOnly);
    void                                         stepJob();
    void                                         load();
    void                                         save();

    std::optional<int64_t>              mTraderId;
    void*                               mTradeScreen = nullptr;
    std::array<BookmarkMap, 3>          mGlobal;
    std::array<BookmarkMap, 3>          mLocal;
    std::vector<platform::screen::SlotRect> mToggles;
    std::optional<platform::screen::SlotRect> mScroll;
    std::optional<Job>                  mJob;
    int                                 mTraded = 0;
    bool                                mLoaded = false;
};

}
