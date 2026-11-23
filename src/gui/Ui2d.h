// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <cstdint>
#include <string>

#include "platform/render/Gui2d.h"

class ClientInstance;
class ItemStack;
class MinecraftUIRenderContext;

namespace inventory_utils::gui {

class Ui2d {
public:
    Ui2d(MinecraftUIRenderContext& ctx, ClientInstance& ci);
    Ui2d(Ui2d const&)            = delete;
    Ui2d& operator=(Ui2d const&) = delete;

    static constexpr int kLineHeight = 10;

    [[nodiscard]] int   width() const { return mGui.screenWidth(); }
    [[nodiscard]] int   height() const { return mGui.screenHeight(); }
    [[nodiscard]] float guiScale() const { return mGuiScale; }

    void fill(float x0, float y0, float x1, float y1, std::uint32_t argb);
    void outline(float x, float y, float w, float h, std::uint32_t argb);
    void text(std::string const& s, float x, float y, std::uint32_t argb = 0xFFFFFFFF, bool shadow = true, float scale = 1.0f);
    [[nodiscard]] int textWidth(std::string const& s, float scale = 1.0f);
    void              item(::ItemStack const& stack, float x, float y, float scale = 1.0f);
    bool sprite(std::string const& texture, float x, float y, int u, int v, int w, int h, int texW = 256, int texH = 256);
    void flush();

private:
    platform::render::Gui2d mGui;
    float                   mGuiScale = 1.0f;
};

}
