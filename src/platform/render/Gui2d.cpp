// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/render/Gui2d.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/ScreenSizeData.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/ItemStack.h"

#include "platform/render/TextureManager.h"

namespace inventory_utils::platform::render {

namespace {
mce::Color toColor(uint32_t argb) {
    return mce::Color(
        static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(argb & 0xFF) / 255.0f,
        static_cast<float>((argb >> 24) & 0xFF) / 255.0f
    );
}
float alphaOf(uint32_t argb) { return static_cast<float>((argb >> 24) & 0xFF) / 255.0f; }
}

struct Gui2d::Impl {
    std::optional<BaseActorRenderContext> actorContext;
};

Gui2d::Gui2d(MinecraftUIRenderContext& ctx, ClientInstance& ci) : mCtx(ctx), mClient(ci), mImpl(new Impl) {
    auto guiData  = ci.getGuiData();
    Vec2 uiSize   = guiData->mScreenSizeData.get().clientUIScreenSize.get();
    mScreenWidth  = static_cast<int>(uiSize.x);
    mScreenHeight = static_cast<int>(uiSize.y);
}

Gui2d::~Gui2d() {
    flush();
    delete mImpl;
}

Font& Gui2d::font() {
    if (!mFont) {
        FontHandle handle = mClient.getFontHandle();
        mFont = handle.mDefaultFont.get();
    }
    return *mFont;
}

void Gui2d::flushImages() {
    if (!mImagesPending) return;
    mImagesPending = false;
    mCtx.flushImages(mce::Color(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, HashedString(TextureManager::flushMaterialName()));
}

void Gui2d::flush() {
    flushImages();
    if (mTextPending) {
        mTextPending = false;
        mCtx.flushText(0.0f, std::nullopt);
    }
}

bool Gui2d::blit(std::string const& texturePath, int x, int y, int width, int height, float u0, float v0, float u1, float v1) {
    if (width <= 0 || height <= 0) return false;
    auto const* texture = TextureManager::get().getClientTexture(mCtx, texturePath);
    if (!texture) return false;
    if (mTextPending) {
        mTextPending = false;
        mCtx.flushText(0.0f, std::nullopt);
    }
    if (mImagesPending && mPendingTexture != texture) flushImages();
    mPendingTexture = texture;
    mCtx.drawImage(*texture, glm::vec2(static_cast<float>(x), static_cast<float>(y)), glm::vec2(static_cast<float>(width), static_cast<float>(height)), glm::vec2(u0, v0), glm::vec2(u1 - u0, v1 - v0), false);
    mImagesPending = true;
    return true;
}

bool Gui2d::blitPixels(std::string const& texturePath, int x, int y, int u, int v, int width, int height, int texW, int texH) {
    float tw = static_cast<float>(texW), th = static_cast<float>(texH);
    return blit(texturePath, x, y, width, height, static_cast<float>(u) / tw, static_cast<float>(v) / th, static_cast<float>(u + width) / tw, static_cast<float>(v + height) / th);
}

void Gui2d::fill(int x0, int y0, int x1, int y1, uint32_t argb) {
    if (x1 <= x0 || y1 <= y0) return;
    flush();
    RectangleArea rect{static_cast<float>(x0), static_cast<float>(x1), static_cast<float>(y0), static_cast<float>(y1)};
    mCtx.fillRectangle(rect, toColor(argb), alphaOf(argb));
}

void Gui2d::outline(int x, int y, int w, int h, uint32_t argb) {
    if (w <= 0 || h <= 0) return;
    fill(x, y, x + w, y + 1, argb);
    fill(x, y + h - 1, x + w, y + h, argb);
    fill(x, y + 1, x + 1, y + h - 1, argb);
    fill(x + w - 1, y + 1, x + w, y + h - 1, argb);
}

void Gui2d::drawString(std::string const& text, int x, int y, uint32_t argb, bool shadow, float scale) {
    if (text.empty()) return;
    RectangleArea    rect{static_cast<float>(x), static_cast<float>(mScreenWidth), static_cast<float>(y), static_cast<float>(y) + LINE_HEIGHT * scale};
    TextMeasureData  tmd{scale, 0.0f, shadow, false, false, ui::TextAlignment::Left};
    CaretMeasureData cmd{0, false};
    mCtx.drawText(font(), rect, std::string(text), toColor(argb), alphaOf(argb), ui::TextAlignment::Left, tmd, cmd);
    mTextPending = true;
}

int Gui2d::stringWidth(std::string const& text, float scale) { return mCtx.getLineLength(font(), text, scale, false); }

void Gui2d::renderItem(ItemStack const& stack, int x, int y, float scale) {
    if (stack.isNull()) return;
    ItemRenderer* renderer = mClient.getItemRenderer();
    if (!renderer) return;
    flush();
    if (!mImpl->actorContext) mImpl->actorContext.emplace(mCtx.mScreenContext, mClient, mClient.getMinecraftGame());
    renderer->renderGuiItemNew(*mImpl->actorContext, stack, 0, static_cast<float>(x), static_cast<float>(y), false, 1.0f, 1.0f, scale, 0);
}

}
