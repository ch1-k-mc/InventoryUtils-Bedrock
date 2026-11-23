// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <map>
#include <utility>
#include <vector>

class MinecraftUIRenderContext;
class ScreenView;

namespace inventory_utils::ipnext::inventory {

namespace ContainerClicker {

void leftClick(int slotId);
void rightClick(int slotId);
void shiftClick(int slotId);
void qClick(int slotId);
void click(int slotId, int button);

void executeClicks(std::vector<std::pair<int, int>> const& clicks, int interval);
void executeQClicks(std::vector<int> const& slotIds, int interval);

bool busy();
void onContainerFrame();
void postScreenRender(ScreenView& view, MinecraftUIRenderContext& ctx);

int                                     lastClickCount();
int                                     lastFailedCount();
std::vector<std::pair<int, int>> const& lastClicks();
void setDryRun(bool dryRun);

}

}
