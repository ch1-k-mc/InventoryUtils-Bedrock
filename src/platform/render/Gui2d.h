// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <cstdint>
#include <optional>
#include <string>

class MinecraftUIRenderContext;
class ClientInstance;
class ItemStack;
class Font;
class BaseActorRenderContext;

namespace inventory_utils::platform::render {

class Gui2d {
public:
    static constexpr int LINE_HEIGHT = 10;

    Gui2d(MinecraftUIRenderContext& ctx, ClientInstance& ci);
    ~Gui2d();
    Gui2d(Gui2d const&)            = delete;
    Gui2d& operator=(Gui2d const&) = delete;

    [[nodiscard]] int screenWidth() const { return mScreenWidth; }
    [[nodiscard]] int screenHeight() const { return mScreenHeight; }

    void fill(int x0, int y0, int x1, int y1, uint32_t argb);
    void outline(int x, int y, int w, int h, uint32_t argb);
    void drawString(std::string const& text, int x, int y, uint32_t argb, bool shadow = false, float scale = 1.0f);
    [[nodiscard]] int stringWidth(std::string const& text, float scale = 1.0f);
    void renderItem(ItemStack const& stack, int x, int y, float scale = 1.0f);
    bool blit(std::string const& texturePath, int x, int y, int width, int height, float u0, float v0, float u1, float v1);
    bool blitPixels(std::string const& texturePath, int x, int y, int u, int v, int width, int height, int texW, int texH);
    void flush();

private:
    Font& font();
    void  flushImages();

    MinecraftUIRenderContext&             mCtx;
    ClientInstance&                       mClient;
    int                                   mScreenWidth  = 0;
    int                                   mScreenHeight = 0;
    bool                                  mTextPending  = false;
    bool                                  mImagesPending = false;
    void const*                           mPendingTexture = nullptr;
    Font*                                 mFont         = nullptr;
    struct Impl;
    Impl* mImpl;
};

}
