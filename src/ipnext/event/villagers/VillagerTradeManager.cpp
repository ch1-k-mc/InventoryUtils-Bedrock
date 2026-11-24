// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/event/villagers/VillagerTradeManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/trading/MerchantRecipe.h"
#include "mc/world/item/trading/MerchantRecipeList.h"

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/inventory/ContainerClicker.h"
#include "ipnext/inventory/ContainerTypes.h"
#include "ipnext/item/ItemType.h"
#include "malilib/hotkeys/KeybindMulti.h"
#include "platform/input/GameInput.h"
#include "platform/input/InputHandler.h"
#include "platform/render/Gui2d.h"
#include "platform/screen/ScreenTracker.h"
#include "platform/util/Messages.h"

namespace inventory_utils::ipnext::event::villagers {

namespace {

using json = nlohmann::json;

constexpr char const* WANDERING = "___+++___ wandering ___+++___";

std::array<std::string, 3> const GLOBAL_KEYS{"globalBookmarks", "globalBookmarks1", "globalBookmarks2"};
std::array<std::string, 3> const LOCAL_KEYS{"localBookmarks", "localBookmarks1", "localBookmarks2"};

item::ItemType typeOf(ItemInstance const& inst) {
    if (inst.isNull() || inst.mCount == 0) return {};
    ::ItemStack s;
    static_cast<ItemStackBase&>(s) = inst;
    return item::ItemType::of(s);
}

std::string idOf(item::ItemType const& t) {
    if (t.isEmpty()) return {};
    return t.aux() != 0 ? t.itemId() + "@" + std::to_string(t.aux()) : t.itemId();
}

VillagerTradeData dataOf(MerchantRecipe const& r) {
    auto a = typeOf(*r.mBuyA), b = typeOf(*r.mBuyB), s = typeOf(*r.mSell);
    return {idOf(s), idOf(a), idOf(b), s.isEmpty() ? "" : s.tagSnbt(), a.isEmpty() ? "" : a.tagSnbt(), b.isEmpty() ? "" : b.tagSnbt()};
}

bool isDisabled(MerchantRecipe const& r) { return r.mMaxUses > 0 && r.mUses >= r.mMaxUses; }

uint32_t overlayColor(int argb) {
    uint32_t c = static_cast<uint32_t>(argb);
    uint32_t a = (c >> 24) * 3 / 5;
    return (a << 24) | (c & 0x00FFFFFF);
}

json toJson(VillagerTradeData const& d) {
    json j{{"resultItem", d.resultItem}, {"priceItem1", d.priceItem1}};
    if (!d.priceItem2.empty()) j["priceItem2"] = d.priceItem2;
    if (!d.resultItemNBT.empty()) j["resultItemNBT"] = d.resultItemNBT;
    if (!d.priceItem1NBT.empty()) j["priceItem1NBT"] = d.priceItem1NBT;
    if (!d.priceItem2NBT.empty()) j["priceItem2NBT"] = d.priceItem2NBT;
    return j;
}

VillagerTradeData fromJson(json const& j) {
    auto str = [&](char const* k) { return j.contains(k) && j[k].is_string() ? j[k].get<std::string>() : std::string(); };
    return {str("resultItem"), str("priceItem1"), str("priceItem2"), str("resultItemNBT"), str("priceItem1NBT"), str("priceItem2NBT")};
}

std::filesystem::path file() { return config::configDir() / "villager-trading-config-v2.json"; }

std::vector<VillagerTradeData> const EMPTY;

}

VillagerTradeManager& VillagerTradeManager::get() {
    static VillagerTradeManager instance;
    return instance;
}

void VillagerTradeManager::install() {}
void VillagerTradeManager::uninstall() { mJob.reset(); }

void VillagerTradeManager::refreshTrader() {
    if (!tradeScreenOpen()) {
        mTradeScreen = nullptr;
        return;
    }
    void* screen = platform::screen::ScreenTracker::containerScreen();
    if (screen == mTradeScreen && trader()) return;
    mTradeScreen = screen;
    mTraderId.reset();
    auto ci = ll::service::getClientInstance();
    if (!ci || !ci->getLevel()) return;
    auto* player = ci->getLocalPlayer();
    if (!player) return;
    Vec3   pos  = player->getPosition();
    float  best = 16.0f * 16.0f;
    Actor* pick = nullptr;
    for (Actor* a : ci->getLevel()->getRuntimeActorList()) {
        if (!a || a == player) continue;
        auto* list = a->getTradeOffers();
        if (!list || list->mRecipeList->empty()) continue;
        float d = a->getPosition().distanceToSqr(pos);
        if (d < best) {
            best = d;
            pick = a;
        }
    }
    if (pick) mTraderId = pick->getOrCreateUniqueID().rawID;
}

void VillagerTradeManager::onJoinWorld() {
    mTraderId.reset();
    mJob.reset();
    load();
}

Actor* VillagerTradeManager::trader() const {
    if (!mTraderId) return nullptr;
    auto ci = ll::service::getClientInstance();
    if (!ci || !ci->getLevel()) return nullptr;
    ActorUniqueID id;
    id.rawID = *mTraderId;
    return ci->getLevel()->fetchEntity(id, false);
}

std::vector<MerchantRecipe*> VillagerTradeManager::offers() const {
    std::vector<MerchantRecipe*> out;
    auto*                        a = trader();
    if (!a) return out;
    auto* list = a->getTradeOffers();
    if (!list) return out;
    for (auto& r : *list->mRecipeList) out.push_back(&r);
    return out;
}

std::string VillagerTradeManager::profession() const {
    auto* a = trader();
    if (!a) return {};
    auto const& type = a->getTypeName();
    if (type.find("wandering_trader") != std::string::npos) return WANDERING;
    if (type.find("villager") != std::string::npos) return type + ":" + std::to_string(a->getVariant());
    return {};
}

std::string VillagerTradeManager::uuid() const { return mTraderId ? std::to_string(*mTraderId) : std::string(); }

bool VillagerTradeManager::isLocalAvailable() const {
    auto* a = trader();
    return a && a->getTypeName().find("villager") != std::string::npos;
}

bool VillagerTradeManager::tradeScreenOpen() const {
    auto const& name = platform::screen::ScreenTracker::containerScreenName();
    return (name == "trade_screen" || name == "trade2_screen") && platform::screen::ScreenTracker::containerScreen();
}

std::vector<VillagerTradeData> const& VillagerTradeManager::bookmarks(bool global, int group) {
    if (!mLoaded) load();
    if (group < 0 || group > 2) return EMPTY;
    std::string key = global ? profession() : uuid();
    if (global && key.empty()) key = uuid();
    auto& map = global ? mGlobal[group] : mLocal[group];
    auto  it  = map.find(key);
    return it == map.end() ? EMPTY : it->second;
}

bool VillagerTradeManager::has(std::vector<VillagerTradeData> const& list, MerchantRecipe const& r) const {
    auto d = dataOf(r);
    return std::find(list.begin(), list.end(), d) != list.end();
}

int VillagerTradeManager::hoveredToggle() const {
    double mx = platform::input::InputHandler::get().mouseX(), my = platform::input::InputHandler::get().mouseY();
    if (mScroll && (my < mScroll->y || my >= mScroll->y + mScroll->h)) return -1;
    for (size_t i = 0; i < mToggles.size(); ++i) {
        auto const& t = mToggles[i];
        if (mx >= t.x && mx < t.x + t.w && my >= t.y && my < t.y + t.h) return static_cast<int>(i);
    }
    return -1;
}

void VillagerTradeManager::onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx) {
    refreshTrader();
    if (!tradeScreenOpen()) return;
    mToggles = platform::screen::collectNamedRects(view, "trade_toggle");
    auto scroll = platform::screen::collectNamedRects(view, "trade_scroll_panel");
    mScroll     = scroll.empty() ? std::nullopt : std::optional(scroll.front());
    if (!config::Features::VILLAGER_TRADING_ENABLE.getBooleanValue()) return;
    auto list = offers();
    if (list.empty()) return;
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    using namespace config::ModSettings;
    struct Group {
        bool                           on;
        malilib::config::ConfigInteger const* global;
        malilib::config::ConfigInteger const* local;
    };
    std::array<Group, 3> const groups{{
        {true, &VILLAGER_TRADING_GLOBAL_COLOR, &VILLAGER_TRADING_LOCAL_COLOR},
        {VILLAGER_TRADING_GROUP_1.getBooleanValue(), &VILLAGER_TRADING_GLOBAL_COLOR1, &VILLAGER_TRADING_LOCAL_COLOR1},
        {VILLAGER_TRADING_GROUP_2.getBooleanValue(), &VILLAGER_TRADING_GLOBAL_COLOR2, &VILLAGER_TRADING_LOCAL_COLOR2},
    }};
    platform::render::Gui2d g(ctx, *ci);
    size_t n = std::min(list.size(), mToggles.size());
    for (size_t i = 0; i < n; ++i) {
        std::vector<uint32_t> colors;
        for (int k = 0; k < 3; ++k) {
            if (!groups[k].on) continue;
            if (has(bookmarks(true, k), *list[i])) colors.push_back(overlayColor(groups[k].global->getIntegerValue()));
            if (isLocalAvailable() && has(bookmarks(false, k), *list[i])) colors.push_back(overlayColor(groups[k].local->getIntegerValue()));
        }
        if (colors.empty()) continue;
        auto t  = mToggles[i];
        int  y0 = t.y + 1, y1 = t.y + t.h - 1;
        if (mScroll) {
            y0 = std::max(y0, mScroll->y);
            y1 = std::min(y1, mScroll->y + mScroll->h);
        }
        if (y1 <= y0) continue;
        int width = t.w - 2, part = width / static_cast<int>(colors.size()), extra = width - part * static_cast<int>(colors.size());
        int x     = t.x + 1;
        for (auto c : colors) {
            int w = part + extra;
            extra = 0;
            g.fill(x, y0, x + w, y1, c);
            x += w;
        }
    }
    g.flush();
}

bool VillagerTradeManager::onBookmarkKey(bool global, int group) {
    refreshTrader();
    if (!config::Features::VILLAGER_TRADING_ENABLE.getBooleanValue() || !tradeScreenOpen()) return false;
    if (!global && !isLocalAvailable()) return false;
    int i = hoveredToggle();
    if (i < 0) return false;
    return toggleBookmark(i, global, group);
}

bool VillagerTradeManager::toggleBookmark(int index, bool global, int group) {
    auto list = offers();
    if (index < 0 || index >= static_cast<int>(list.size()) || group < 0 || group > 2) return false;
    if (!mLoaded) load();
    std::string key = global ? profession() : uuid();
    if (global && key.empty()) key = uuid();
    if (key.empty()) return false;
    auto& vec = (global ? mGlobal[group] : mLocal[group])[key];
    auto  d   = dataOf(*list[index]);
    auto  it  = std::find(vec.begin(), vec.end(), d);
    if (it == vec.end()) vec.push_back(d);
    else vec.erase(it);
    save();
    return true;
}

bool VillagerTradeManager::onDoTradesKey(bool global, int group) {
    refreshTrader();
    if (!config::Features::VILLAGER_TRADING_ENABLE.getBooleanValue() || !tradeScreenOpen()) return false;
    if (!global && !isLocalAvailable()) return false;
    return startTrades(global, group);
}

bool VillagerTradeManager::startTrades(bool global, int group) {
    auto const&      marks = bookmarks(global, group);
    auto             list  = offers();
    std::vector<int> indices;
    for (size_t i = 0; i < list.size() && i < mToggles.size(); ++i) {
        if (!isDisabled(*list[i]) && has(marks, *list[i])) indices.push_back(static_cast<int>(i));
    }
    if (indices.empty()) return false;
    return startJob(std::move(indices), false);
}

bool VillagerTradeManager::startSelect(int index) { return startJob({index}, true); }

void VillagerTradeManager::clearBookmarks() {
    for (auto& m : mGlobal) m.clear();
    for (auto& m : mLocal) m.clear();
    mLoaded = true;
    save();
}

bool VillagerTradeManager::startJob(std::vector<int> indices, bool selectOnly) {
    if (mJob || !tradeScreenOpen()) return false;
    Job job;
    job.indices    = std::move(indices);
    job.selectOnly = selectOnly;
    job.restoreX   = platform::input::InputHandler::get().mouseX();
    job.restoreY   = platform::input::InputHandler::get().mouseY();
    mJob           = std::move(job);
    return true;
}

void VillagerTradeManager::onClientTick() {
    if (!mJob) return;
    if (!tradeScreenOpen()) {
        mJob.reset();
        return;
    }
    try {
        stepJob();
    } catch (std::exception const& e) {
        Log::warn("villager trade: {}", e.what());
        mJob.reset();
    }
}

namespace {
enum Step { SCROLL = 0, MOVE, PRESS, RELEASE, RESTORE, WAIT_RESULT, TRADE, NEXT };
}

void VillagerTradeManager::stepJob() {
    auto& job = *mJob;
    if (job.wait > 0) {
        --job.wait;
        return;
    }
    if (job.at >= job.indices.size()) {
        mJob.reset();
        return;
    }
    int  index = job.indices[job.at];
    auto list  = offers();
    auto resultSlot = [&]() -> std::pair<int, item::ItemType> {
        auto v = inventory::VanillaContainer::current();
        if (!v) return {-1, {}};
        for (auto const& s : v->slots()) {
            if (s.collection == "trade2_result_item") return {s.slotNumber, item::ItemType::of(v->itemOf(s))};
        }
        return {-1, {}};
    };
    switch (job.step) {
    case SCROLL: {
        if (index < 0 || index >= static_cast<int>(mToggles.size()) || index >= static_cast<int>(list.size())) {
            job.step = NEXT;
            return;
        }
        auto const& t  = mToggles[index];
        double      cy = t.y + t.h / 2.0;
        if (mScroll && (cy < mScroll->y + 4 || cy > mScroll->y + mScroll->h - 4)) {
            if (++job.tries > 40) {
                job.step = NEXT;
                return;
            }
            double sx = mScroll->x + mScroll->w / 2.0, sy = mScroll->y + mScroll->h / 2.0;
            platform::input::GameInput::move(sx, sy);
            platform::input::GameInput::wheel(cy < mScroll->y ? 120 : -120, sx, sy);
            job.wait = 2;
            return;
        }
        job.tries      = 0;
        job.expectedId = idOf(typeOf(*list[index]->mSell));
        job.step       = MOVE;
        [[fallthrough]];
    }
    case MOVE: {
        auto const& t = mToggles[index];
        platform::input::GameInput::move(t.x + t.w / 2.0, t.y + t.h / 2.0);
        job.step = PRESS;
        return;
    }
    case PRESS: {
        auto const& t = mToggles[index];
        platform::input::GameInput::button(true, t.x + t.w / 2.0, t.y + t.h / 2.0);
        job.step = RELEASE;
        return;
    }
    case RELEASE: {
        auto const& t = mToggles[index];
        platform::input::GameInput::button(false, t.x + t.w / 2.0, t.y + t.h / 2.0);
        job.step = RESTORE;
        return;
    }
    case RESTORE:
        platform::input::GameInput::move(job.restoreX, job.restoreY);
        job.step  = job.selectOnly ? NEXT : WAIT_RESULT;
        job.tries = 0;
        return;
    case WAIT_RESULT: {
        auto [slot, type] = resultSlot();
        if (slot >= 0 && !type.isEmpty() && idOf(type) == job.expectedId) {
            job.step       = TRADE;
            job.iterations = 0;
            job.lastHeld   = -1;
            return;
        }
        if (++job.tries > 20) job.step = NEXT;
        return;
    }
    case TRADE: {
        auto [slot, type] = resultSlot();
        if (slot < 0 || type.isEmpty() || idOf(type) != job.expectedId || ++job.iterations > 64) {
            job.step = NEXT;
            return;
        }
        int held = 0;
        if (auto v = inventory::VanillaContainer::current()) {
            for (auto const& s : v->slots()) {
                if (s.kind == platform::inventory::InventoryKind::Container) continue;
                auto t = item::ItemType::of(v->itemOf(s));
                if (!t.isEmpty() && idOf(t) == job.expectedId) held += static_cast<int>(v->itemOf(s).mCount);
            }
        }
        if (job.lastHeld >= 0 && held <= job.lastHeld) {
            job.step = NEXT;
            return;
        }
        job.lastHeld = held;
        inventory::ContainerClicker::shiftClick(slot);
        ++mTraded;
        job.wait = 2;
        return;
    }
    default:
        ++job.at;
        job.step  = SCROLL;
        job.tries = 0;
        return;
    }
}

void VillagerTradeManager::load() {
    mLoaded = true;
    for (auto& m : mGlobal) m.clear();
    for (auto& m : mLocal) m.clear();
    std::ifstream in(file(), std::ios::binary);
    if (!in) return;
    auto root = json::parse(in, nullptr, false);
    if (!root.is_object()) {
        Log::warn("Could not read villager-trading-config-v2.json (broken JSON)");
        return;
    }
    auto readMap = [&](std::string const& key, BookmarkMap& out) {
        if (!root.contains(key) || !root[key].is_object()) return;
        for (auto& [k, arr] : root[key].items()) {
            if (!arr.is_array()) continue;
            auto& vec = out[k];
            for (auto& e : arr) {
                if (e.is_object()) vec.push_back(fromJson(e));
            }
        }
    };
    for (int g = 0; g < 3; ++g) {
        readMap(GLOBAL_KEYS[g], mGlobal[g]);
        readMap(LOCAL_KEYS[g], mLocal[g]);
    }
}

void VillagerTradeManager::save() {
    json root = json::object();
    auto put  = [&](std::string const& key, BookmarkMap const& m) {
        json o = json::object();
        for (auto const& [k, vec] : m) {
            json arr = json::array();
            for (auto const& d : vec) arr.push_back(toJson(d));
            o[k] = arr;
        }
        root[key] = o;
    };
    for (int g = 0; g < 3; ++g) {
        put(GLOBAL_KEYS[g], mGlobal[g]);
        put(LOCAL_KEYS[g], mLocal[g]);
    }
    std::error_code ec;
    std::filesystem::create_directories(file().parent_path(), ec);
    std::ofstream out(file(), std::ios::binary | std::ios::trunc);
    out << root.dump(2);
}

json VillagerTradeManager::debugJson() {
    refreshTrader();
    json offersJson = json::array();
    auto list       = offers();
    for (size_t i = 0; i < list.size(); ++i) {
        auto d = dataOf(*list[i]);
        offersJson.push_back({
            {"i", i},
            {"buyA", d.priceItem1},
            {"buyACount", static_cast<int>(list[i]->mBuyA->mCount)},
            {"buyB", d.priceItem2},
            {"sell", d.resultItem},
            {"sellCount", static_cast<int>(list[i]->mSell->mCount)},
            {"uses", list[i]->mUses},
            {"maxUses", list[i]->mMaxUses},
            {"tier", list[i]->mTier},
            {"global", has(bookmarks(true, 0), *list[i])},
            {"local", has(bookmarks(false, 0), *list[i])},
        });
    }
    json toggles = json::array();
    for (auto const& t : mToggles) toggles.push_back({t.x, t.y, t.w, t.h});
    return {
        {"screen", platform::screen::ScreenTracker::containerScreenName()},
        {"open", tradeScreenOpen()},
        {"trader", mTraderId ? json(uuid()) : json(nullptr)},
        {"profession", profession()},
        {"localAvailable", isLocalAvailable()},
        {"offers", offersJson},
        {"toggles", toggles},
        {"scroll", mScroll ? json{mScroll->x, mScroll->y, mScroll->w, mScroll->h} : json(nullptr)},
        {"hovered", hoveredToggle()},
        {"busy", busy()},
        {"traded", mTraded},
        {"pressedKeys", malilib::hotkeys::KeybindMulti::getActiveKeysString()},
        {"triggeredCount", malilib::hotkeys::KeybindMulti::getTriggeredCount()},
    };
}

}
