// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <functional>
#include <vector>

namespace inventory_utils::malilib::hotkeys {
class KeybindMulti;
}

namespace inventory_utils::platform::input {

class IKeyboardInputHandler {
public:
    virtual ~IKeyboardInputHandler()                     = default;
    virtual bool onKeyInput(int keyCode, bool keyState) = 0;
};

class IMouseInputHandler {
public:
    virtual ~IMouseInputHandler() = default;
    virtual bool onMouseClick(double mouseX, double mouseY, int buttonCode, bool buttonState) { return false; }
    virtual bool onMouseScroll(double mouseX, double mouseY, double amount) { return false; }
    virtual void onMouseMove(double mouseX, double mouseY) {}
};

class InputHandler {
public:
    static InputHandler& get();

    void install();
    void uninstall();

    void registerKeyboardInputHandler(IKeyboardInputHandler* handler, bool first = false);
    void unregisterKeyboardInputHandler(IKeyboardInputHandler* handler);
    void registerMouseInputHandler(IMouseInputHandler* handler, bool first = false);
    void unregisterMouseInputHandler(IMouseInputHandler* handler);

    void addKeybindProvider(std::function<void(std::vector<malilib::hotkeys::KeybindMulti*>&)> provider);
    void updateUsedKeys();

    [[nodiscard]] double mouseX() const { return mMouseX; }
    [[nodiscard]] double mouseY() const { return mMouseY; }
    [[nodiscard]] bool   isKeyDown(int code) const;
    [[nodiscard]] static bool textInputActive();

    void onClientTick();
    void checkFocus();
    void onFocusLost();

    [[nodiscard]] int handledCount() const { return mHandled; }

private:
    bool onKey(int vk, bool down);
    void trackOnly(int code, bool down);
    bool onMouseButton(int button, bool down, double x, double y);
    bool onWheel(double amount, double x, double y);
    void onMove(double x, double y);
    bool checkKeyBindsForChanges(int code);

    std::vector<IKeyboardInputHandler*>                                                mKeyboardHandlers;
    std::vector<IMouseInputHandler*>                                                   mMouseHandlers;
    std::vector<std::function<void(std::vector<malilib::hotkeys::KeybindMulti*>&)>> mProviders;
    std::vector<std::pair<int, malilib::hotkeys::KeybindMulti*>>                       mHotkeyMap;
    std::vector<int>                                                                   mDownKeys;
    double                                                                             mMouseX  = 0;
    double                                                                             mMouseY  = 0;
    double                                                                             mWheelSum = 0;
    int                                                                                mHandled = 0;
    bool                                                                               mInstalled = false;
    bool                                                                               mSeenMove  = false;
    bool                                                                               mHadFocus  = false;
};

}
