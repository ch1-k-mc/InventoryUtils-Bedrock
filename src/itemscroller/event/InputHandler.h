// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include "platform/input/InputHandler.h"

namespace inventory_utils::itemscroller::event {

class InputHandler : public platform::input::IKeyboardInputHandler, public platform::input::IMouseInputHandler {
public:
    static InputHandler& getInstance();

    bool onKeyInput(int keyCode, bool keyState) override;
    bool onMouseClick(double mouseX, double mouseY, int buttonCode, bool buttonState) override;
    bool onMouseScroll(double mouseX, double mouseY, double amount) override;
    void onMouseMove(double mouseX, double mouseY) override;

    void onContainerFrame();

private:
    bool handleInput(int keyCode, bool keyState, double dWheel);
};

}
