// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "gui/Ui2d.h"

#include <cmath>

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/GuiData.h"

namespace inventory_utils::gui {

namespace {
int px(float v) { return static_cast<int>(std::lround(v)); }
}

Ui2d::Ui2d(MinecraftUIRenderContext& ctx, ClientInstance& ci) : mGui(ctx, ci) {
    mGuiScale = ci.getGuiData()->mGuiScale;
}

void Ui2d::fill(float x0, float y0, float x1, float y1, std::uint32_t argb) { mGui.fill(px(x0), px(y0), px(x1), px(y1), argb); }

void Ui2d::outline(float x, float y, float w, float h, std::uint32_t argb) { mGui.outline(px(x), px(y), px(w), px(h), argb); }

void Ui2d::text(std::string const& s, float x, float y, std::uint32_t argb, bool shadow, float scale) {
    mGui.drawString(s, px(x), px(y), argb, shadow, scale);
}

int Ui2d::textWidth(std::string const& s, float scale) { return mGui.stringWidth(s, scale); }

void Ui2d::item(::ItemStack const& stack, float x, float y, float scale) { mGui.renderItem(stack, px(x), px(y), scale); }

bool Ui2d::sprite(std::string const& texture, float x, float y, int u, int v, int w, int h, int texW, int texH) {
    return mGui.blitPixels(texture, px(x), px(y), u, v, w, h, texW, texH);
}

void Ui2d::flush() { mGui.flush(); }

}
