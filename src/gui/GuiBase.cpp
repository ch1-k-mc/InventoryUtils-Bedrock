// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "gui/GuiBase.h"

#include <algorithm>

#include "gui/Ui2d.h"

namespace inventory_utils::gui {

bool GuiBase::needsInit(Ui2d const& ui) const { return mNeedsInit || ui.width() != mWidth || ui.height() != mHeight; }

void GuiBase::rebuild(Ui2d& ui) {
    std::string keepId, keepText, keepComposition;
    std::size_t keepCursor = 0, keepHighlight = 0, keepScroll = 0;
    if (auto* f = focusedField()) {
        keepId          = f->id;
        keepText        = f->text;
        keepComposition = f->composition;
        keepCursor      = f->cursor;
        keepHighlight   = f->highlight;
        keepScroll      = f->scroll;
    }
    mWidth  = ui.width();
    mHeight = ui.height();
    mWidgets.clear();
    mNeedsInit = false;
    initGui(ui);
    if (keepId.empty()) return;
    for (auto& w : mWidgets) {
        auto* tf = dynamic_cast<TextField*>(w.get());
        if (!tf || tf->id != keepId || tf->text != keepText) continue;
        tf->focused     = true;
        tf->composition = keepComposition;
        tf->cursor      = keepCursor;
        tf->highlight   = keepHighlight;
        tf->scroll      = keepScroll;
    }
}

TextField* GuiBase::focusedField() const {
    for (auto const& w : mWidgets) {
        if (auto* tf = dynamic_cast<TextField*>(w.get()); tf && tf->focused) return tf;
    }
    return nullptr;
}

void GuiBase::render(Ui2d& ui, float mx, float my) {
    if (needsInit(ui)) rebuild(ui);
    ui.fill(0, 0, static_cast<float>(mWidth), static_cast<float>(mHeight), colors::kBackground);
    ui.text(title(), kLeft, kTop, colors::kWhite, true);
    ui.flush();
    drawContents(ui, mx, my);
    renderWidgets(ui, mx, my);
}

void GuiBase::renderWidgets(Ui2d& ui, float mx, float my) {
    for (auto& w : mWidgets) w->render(ui, mx, my);
    drawOverlay(ui, mx, my);
    for (auto const& w : mWidgets) {
        auto const* b = w.get();
        if (b->hoverText.empty() || !b->contains(mx, my)) continue;
        std::vector<std::string> lines;
        std::size_t              start = 0;
        while (start <= b->hoverText.size()) {
            auto nl = b->hoverText.find('\n', start);
            if (nl == std::string::npos) nl = b->hoverText.size();
            lines.push_back(b->hoverText.substr(start, nl - start));
            start = nl + 1;
        }
        int maxW = 0;
        for (auto const& l : lines) maxW = std::max(maxW, ui.textWidth(l));
        float const h  = static_cast<float>(lines.size()) * Ui2d::kLineHeight;
        float       tx = mx + 12, ty = my - 12;
        if (tx + maxW + 6 > mWidth) tx = mx - 16 - maxW;
        if (ty + h + 6 > mHeight) ty = mHeight - h - 6;
        if (ty < 4) ty = 4;
        ui.fill(tx - 3, ty - 4, tx + maxW + 3, ty + h + 3, 0xF0100010);
        ui.outline(tx - 3, ty - 4, static_cast<float>(maxW + 6), h + 7, 0x505000FF);
        float ly = ty;
        for (auto const& l : lines) {
            ui.text(l, tx, ly, 0xFFFFFFFF, true);
            ly += Ui2d::kLineHeight;
        }
        ui.flush();
        break;
    }
}

bool GuiBase::mouseClicked(float mx, float my, int button) {
    for (auto& w : mWidgets) {
        if (auto* kb = dynamic_cast<KeybindButton*>(w.get()); kb && kb->capturing) return kb->mouseClicked(mx, my, button);
    }
    Widget* hit = nullptr;
    for (auto& w : mWidgets) {
        if (w->contains(mx, my)) {
            hit = w.get();
            break;
        }
    }
    for (auto& w : mWidgets) {
        if (auto* tf = dynamic_cast<TextField*>(w.get()); tf && tf->focused && tf != hit) {
            tf->commit();
            if (mNeedsInit) return true;
        }
    }
    if (!hit) return false;
    bool const handled = hit->mouseClicked(mx, my, button);
    if (handled && button == 0 && hit->draggable()) mDragId = hit->id;
    return handled;
}

void GuiBase::mouseDragged(float mx, float my) {
    if (mDragId.empty()) return;
    for (auto& w : mWidgets) {
        if (w->id == mDragId) {
            w->mouseDragged(mx, my);
            return;
        }
    }
}

bool GuiBase::keyPressed(int key) {
    for (auto& w : mWidgets) {
        if (auto* kb = dynamic_cast<KeybindButton*>(w.get()); kb && kb->capturing) return kb->keyDown(key);
        if (auto* tf = dynamic_cast<TextField*>(w.get()); tf && tf->focused) return tf->keyPressed(key);
    }
    return false;
}

bool GuiBase::keyReleased(int key) {
    for (auto& w : mWidgets) {
        if (auto* kb = dynamic_cast<KeybindButton*>(w.get()); kb && kb->capturing) return kb->keyUp(key);
    }
    return false;
}

bool GuiBase::charTyped(std::string const& utf8) {
    for (auto& w : mWidgets) {
        if (auto* tf = dynamic_cast<TextField*>(w.get()); tf && tf->focused) return tf->charTyped(utf8);
    }
    return false;
}

json GuiBase::tree() const {
    json widgets = json::array();
    for (auto const& w : mWidgets) widgets.push_back(w->toJson());
    return json{{"screen", name()}, {"title", title()}, {"size", {mWidth, mHeight}}, {"widgets", widgets}};
}

Button& GuiBase::addButton(std::string id, float x, float y, float w, float h, std::string text, std::function<void(int)> onPress) {
    auto b     = std::make_unique<Button>();
    b->id      = std::move(id);
    b->x       = x;
    b->y       = y;
    b->w       = w;
    b->h       = h;
    b->text    = std::move(text);
    b->onPress = std::move(onPress);
    return add(std::move(b));
}

Label& GuiBase::addLabel(std::string id, float x, float y, std::string text, std::uint32_t color) {
    auto l   = std::make_unique<Label>();
    l->id    = std::move(id);
    l->x     = x;
    l->y     = y;
    l->text  = std::move(text);
    l->color = color;
    return add(std::move(l));
}

}
