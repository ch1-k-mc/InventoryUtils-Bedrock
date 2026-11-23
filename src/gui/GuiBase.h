// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gui/Json.h"
#include "gui/Widgets.h"

namespace inventory_utils::gui {

class Ui2d;

class GuiBase {
public:
    virtual ~GuiBase() = default;

    virtual void initGui(Ui2d& ui) = 0;
    [[nodiscard]] virtual std::string title() const = 0;
    [[nodiscard]] virtual char const* name() const = 0;

    virtual void render(Ui2d& ui, float mx, float my);
    virtual void drawContents(Ui2d& , float , float ) {}
    virtual void drawOverlay(Ui2d& , float , float ) {}

    virtual bool mouseClicked(float mx, float my, int button);
    virtual bool mouseScrolled(float , float , int ) { return false; }
    void         mouseDragged(float mx, float my);
    void         mouseReleased() { mDragId.clear(); }
    virtual bool keyPressed(int key);
    virtual bool keyReleased(int key);
    virtual bool charTyped(std::string const& utf8);

    void requestInit() { mNeedsInit = true; }
    [[nodiscard]] bool needsInit(Ui2d const& ui) const;
    void               rebuild(Ui2d& ui);
    [[nodiscard]] TextField* focusedField() const;
    void               renderWidgets(Ui2d& ui, float mx, float my);
    [[nodiscard]] json tree() const;

    static constexpr float kLeft = 20, kTop = 10;

protected:
    template <class T>
    T& add(std::unique_ptr<T> w) {
        T& ref = *w;
        mWidgets.push_back(std::move(w));
        return ref;
    }
    Button& addButton(std::string id, float x, float y, float w, float h, std::string text, std::function<void(int)> onPress);
    Label&  addLabel(std::string id, float x, float y, std::string text, std::uint32_t color = colors::kWhite);

    std::vector<std::unique_ptr<Widget>> mWidgets;
    int                                  mWidth = 0, mHeight = 0;

private:
    bool        mNeedsInit = true;
    std::string mDragId;
};

}
