// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "platform/input/InputHandler.h"

class MinecraftUIRenderContext;
class ScreenView;

namespace inventory_utils::platform::render {
class Gui2d;
}

namespace inventory_utils::ipnext::gui::inject {

class SortingButtonCollectionWidget : public platform::input::IMouseInputHandler {
public:
    static SortingButtonCollectionWidget& get();

    void onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx);

    bool onMouseClick(double mouseX, double mouseY, int buttonCode, bool buttonState) override;
    bool onMouseScroll(double mouseX, double mouseY, double amount) override;

    [[nodiscard]] nlohmann::json debugButtons() const;
    bool                         clickById(std::string const& id, int button);

private:
    struct Button {
        std::string                       id;
        int                               tx = 0, ty = 0;
        int                               x = 0, y = 0;
        bool                              visible = false;
        std::function<void(int)>          click;
        std::function<std::string()>      tooltip;
        [[nodiscard]] bool contains(double mx, double my) const { return visible && mx >= x && mx < x + 10 && my >= y && my < y + 10; }
    };

    void         layout(std::string const& screenName, int left, int top, int width, int height);
    Button*      hit(double mx, double my);
    void         renderTooltip(platform::render::Gui2d& g, std::string const& text, int mouseX, int mouseY) const;

    std::vector<Button> mButtons;
    std::string         mScreen;
    bool                mHasLayout   = false;
    bool                mPressedHere = false;
};

}
