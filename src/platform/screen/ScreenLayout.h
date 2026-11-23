// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ScreenView;

namespace inventory_utils::platform::screen {

struct GuiArea {
    int left = 0, top = 0, width = 0, height = 0;
};

std::optional<GuiArea> getGuiArea(ScreenView& view, std::string const& screenName, int screenWidth, int screenHeight);

struct SlotRect {
    int x = 0, y = 0, w = 18, h = 18;
};
using SlotRects = std::map<std::pair<std::string, int>, SlotRect>;

SlotRects collectSlotRects(ScreenView& view, int maxNodes = 6000);

std::vector<SlotRect> collectNamedRects(ScreenView& view, std::string_view name, int maxNodes = 6000);

std::string dumpControlsJson(ScreenView& view, std::string const& filter, int maxNodes);

}
