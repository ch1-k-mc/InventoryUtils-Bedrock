// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/inventory/ContainerClicker.h"

#include <chrono>
#include <deque>
#include <optional>

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "platform/inventory/SlotClicker.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::ipnext::inventory::ContainerClicker {

using platform::inventory::ClickType;
using platform::inventory::SlotClicker;

namespace {

struct PendingClick {
    int       slotId;
    int       button;
    ClickType type;
};

struct Queue {
    std::deque<PendingClick>              clicks;
    void*                                 screen = nullptr;
    std::chrono::milliseconds             interval{0};
    std::chrono::steady_clock::time_point next;
    int                                   highlight = -1;
};

std::optional<Queue> gQueue;
int                  gLastCount  = 0;
int                  gLastFailed = 0;
bool                 gDryRun     = false;
std::vector<std::pair<int, int>> gLastClicks;

bool sendOne(SlotClicker& clicker, PendingClick const& c) {
    bool ok = clicker.clickSlot(c.slotId, c.button, c.type);
    if (!ok) {
        ++gLastFailed;
        Log::trace("Click failed: slot {} button {} ({})", c.slotId, c.button, clicker.lastError());
    }
    return ok;
}

void genericClick(int slotId, int button, ClickType type) {
    auto clicker = SlotClicker::current();
    if (!clicker.valid()) return;
    Log::trace("Clicking: on slot: {} with button: {}", slotId, button);
    sendOne(clicker, {slotId, button, type});
}

void execute(std::vector<PendingClick> const& clicks, int interval) {
    gLastCount  = static_cast<int>(clicks.size());
    gLastFailed = 0;
    gLastClicks.clear();
    for (auto const& c : clicks) gLastClicks.emplace_back(c.slotId, c.type == ClickType::THROW ? 100 + c.button : c.button);
    if (gDryRun) return;
    if (interval == 0) {
        auto clicker = SlotClicker::current();
        if (!clicker.valid()) return;
        for (auto const& c : clicks) sendOne(clicker, c);
        return;
    }
    Queue q;
    q.clicks.assign(clicks.begin(), clicks.end());
    q.screen   = platform::screen::ScreenTracker::containerScreen();
    q.interval = std::chrono::milliseconds(interval);
    q.next     = std::chrono::steady_clock::now();
    gQueue     = std::move(q);
}

}

void leftClick(int slotId) { click(slotId, 0); }
void rightClick(int slotId) { click(slotId, 1); }
void shiftClick(int slotId) { genericClick(slotId, 0, ClickType::QUICK_MOVE); }
void qClick(int slotId) { genericClick(slotId, 1, ClickType::THROW); }
void click(int slotId, int button) { genericClick(slotId, button, ClickType::PICKUP); }

void executeClicks(std::vector<std::pair<int, int>> const& clicks, int interval) {
    int l = 0, r = 0;
    for (auto const& c : clicks) (c.second == 0 ? l : r)++;
    Log::debug("Click count total {}. {} left. {} right. Time = {}s", clicks.size(), l, r, static_cast<double>(clicks.size()) * interval / 1000.0);
    std::vector<PendingClick> list;
    for (auto const& [slot, button] : clicks) list.push_back({slot, button, ClickType::PICKUP});
    execute(list, interval);
}

void executeQClicks(std::vector<int> const& slotIds, int interval) {
    std::vector<PendingClick> list;
    for (int id : slotIds) list.push_back({id, 1, ClickType::THROW});
    execute(list, interval);
}

bool busy() { return gQueue.has_value(); }

void onContainerFrame() {
    if (!gQueue) return;
    auto& q = *gQueue;
    if (platform::screen::ScreenTracker::containerScreen() != q.screen) {
        Log::debug("Click cancelled due to container changed");
        gQueue.reset();
        return;
    }
    auto now = std::chrono::steady_clock::now();
    if (now < q.next) return;
    if (q.clicks.empty()) {
        gQueue.reset();
        return;
    }
    auto clicker = SlotClicker::current();
    if (!clicker.valid()) {
        gQueue.reset();
        return;
    }
    auto c = q.clicks.front();
    q.clicks.pop_front();
    q.highlight = c.slotId;
    sendOne(clicker, c);
    q.next = now + q.interval;
}

void postScreenRender(ScreenView&, MinecraftUIRenderContext&) {}

int                                     lastClickCount() { return gLastCount; }
std::vector<std::pair<int, int>> const& lastClicks() { return gLastClicks; }
void                                    setDryRun(bool dryRun) { gDryRun = dryRun; }
int lastFailedCount() { return gLastFailed; }

}
