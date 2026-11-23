// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/input/InputHandler.h"

#include <algorithm>
#include <cmath>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/deps/input/Mouse.h"
#include "mc/deps/input/MouseDevice.h"
#include "mc/client/gui/GuiData.h"
#include "mc/deps/input/MouseAction.h"

#include "malilib/hotkeys/KeybindMulti.h"
#include "platform/input/GameInput.h"
#include "platform/input/KeyCodes.h"
#include "platform/inventory/InventorySession.h"
#include "platform/item/ScopedItemRegistry.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace inventory_utils::platform::input {

namespace {

namespace ev = ll::event;
using malilib::hotkeys::KeybindMulti;

std::vector<ev::ListenerPtr> gListeners;

float guiScale() {
    auto ci = ll::service::getClientInstance();
    if (!ci) return 1.0f;
    auto  gui = ci->getGuiData();
    float s   = static_cast<float>(gui->mGuiScale);
    return s > 0 ? s : 1.0f;
}

}

InputHandler& InputHandler::get() {
    static InputHandler instance;
    return instance;
}

void InputHandler::install() {
    if (mInstalled) return;
    mInstalled = true;
    auto& bus  = ev::EventBus::getInstance();
    gListeners.push_back(bus.emplaceListener<ev::KeyInputEvent>([this](ev::KeyInputEvent& e) {
        if (e.isCancelled()) {
            trackOnly(KeyCodes::normalize(e.keyCode()), e.isDown());
            return;
        }
        if (onKey(e.keyCode(), e.isDown())) e.cancel();
    }));
    gListeners.push_back(bus.emplaceListener<ev::MouseInputEvent>([this](ev::MouseInputEvent& e) {
        float  scale  = guiScale();
        double x      = static_cast<double>(e.x()) / scale;
        double y      = static_cast<double>(e.y()) / scale;
        int    action = static_cast<int>(e.actionButtonId());
        if (action == MouseAction::ActionMove && GameInput::shouldDropMove(e.x(), e.y())) {
            e.cancel();
            return;
        }
        if (e.isCancelled()) {
            if (action == MouseAction::ActionLeft) trackOnly(KeyCodes::mouseButtonToCode(0), e.buttonData() == MouseAction::DataDown);
            else if (action == MouseAction::ActionRight) trackOnly(KeyCodes::mouseButtonToCode(1), e.buttonData() == MouseAction::DataDown);
            else if (action == MouseAction::ActionMiddle) trackOnly(KeyCodes::mouseButtonToCode(2), e.buttonData() == MouseAction::DataDown);
            return;
        }
        if (action != MouseAction::ActionMove && action != MouseAction::ActionMoveRelative && e.x() == 0 && e.y() == 0 && mSeenMove) {
            x = mMouseX;
            y = mMouseY;
        }
        bool   cancel = false;
        switch (action) {
        case MouseAction::ActionMove: onMove(x, y); break;
        case MouseAction::ActionMoveRelative: break;
        case MouseAction::ActionWheel: cancel = onWheel(static_cast<double>(e.buttonData()), x, y); break;
        case MouseAction::ActionLeft: cancel = onMouseButton(0, e.buttonData() == MouseAction::DataDown, x, y); break;
        case MouseAction::ActionRight: cancel = onMouseButton(1, e.buttonData() == MouseAction::DataDown, x, y); break;
        case MouseAction::ActionMiddle: cancel = onMouseButton(2, e.buttonData() == MouseAction::DataDown, x, y); break;
        default: break;
        }
        if (cancel) e.cancel();
    }));
    gListeners.push_back(bus.emplaceListener<ev::ClientLevelTickEvent>([this](ev::ClientLevelTickEvent&) { onClientTick(); }));
}

void InputHandler::uninstall() {
    for (auto& l : gListeners) {
        if (l) ev::EventBus::getInstance().removeListener(l);
    }
    gListeners.clear();
    mInstalled = false;
    KeybindMulti::clearPressedKeys();
    mDownKeys.clear();
}

void InputHandler::registerKeyboardInputHandler(IKeyboardInputHandler* handler, bool first) {
    if (std::find(mKeyboardHandlers.begin(), mKeyboardHandlers.end(), handler) != mKeyboardHandlers.end()) return;
    if (first) mKeyboardHandlers.insert(mKeyboardHandlers.begin(), handler);
    else mKeyboardHandlers.push_back(handler);
}

void InputHandler::unregisterKeyboardInputHandler(IKeyboardInputHandler* handler) { std::erase(mKeyboardHandlers, handler); }

void InputHandler::registerMouseInputHandler(IMouseInputHandler* handler, bool first) {
    if (std::find(mMouseHandlers.begin(), mMouseHandlers.end(), handler) != mMouseHandlers.end()) return;
    if (first) mMouseHandlers.insert(mMouseHandlers.begin(), handler);
    else mMouseHandlers.push_back(handler);
}

void InputHandler::unregisterMouseInputHandler(IMouseInputHandler* handler) { std::erase(mMouseHandlers, handler); }

void InputHandler::addKeybindProvider(std::function<void(std::vector<KeybindMulti*>&)> provider) {
    mProviders.push_back(std::move(provider));
    updateUsedKeys();
}

void InputHandler::updateUsedKeys() {
    mHotkeyMap.clear();
    std::vector<KeybindMulti*> all;
    for (auto& p : mProviders) p(all);
    for (auto* kb : all) {
        for (int code : kb->getKeys()) mHotkeyMap.emplace_back(code, kb);
    }
}

void InputHandler::trackOnly(int code, bool down) {
    if (down) {
        if (!isKeyDown(code)) mDownKeys.push_back(code);
    } else {
        std::erase(mDownKeys, code);
        if (KeybindMulti::isPressedKey(code)) {
            KeybindMulti::onKeyInputPre(code, false);
            checkKeyBindsForChanges(code);
        }
    }
}

bool InputHandler::isKeyDown(int code) const { return std::find(mDownKeys.begin(), mDownKeys.end(), code) != mDownKeys.end(); }

bool InputHandler::checkKeyBindsForChanges(int code) {
    bool cancel = false;
    for (auto& [key, kb] : mHotkeyMap) {
        if (key == code) cancel |= kb->updateIsPressed();
    }
    return cancel;
}

bool InputHandler::onKey(int vk, bool down) {
    int code = KeyCodes::normalize(vk);
    if (down && code == 'E' && inventory::InventorySession::onInventoryKey()) return true;
    if (down) {
        if (!isKeyDown(code)) mDownKeys.push_back(code);
    } else {
        std::erase(mDownKeys, code);
    }
    item::ScopedItemRegistry registry;
    KeybindMulti::onKeyInputPre(code, down);
    bool cancel = checkKeyBindsForChanges(code);
    for (auto* h : mKeyboardHandlers) {
        if (h->onKeyInput(code, down)) {
            ++mHandled;
            return true;
        }
    }
    if (cancel) ++mHandled;
    return cancel;
}

bool InputHandler::onMouseButton(int button, bool down, double x, double y) {
    mMouseX  = x;
    mMouseY  = y;
    int code = KeyCodes::mouseButtonToCode(button);
    if (down) {
        if (!isKeyDown(code)) mDownKeys.push_back(code);
    } else {
        std::erase(mDownKeys, code);
    }
    item::ScopedItemRegistry registry;
    KeybindMulti::onKeyInputPre(code, down);
    bool cancel = checkKeyBindsForChanges(code);
    for (auto* h : mMouseHandlers) {
        if (h->onMouseClick(x, y, code, down)) {
            ++mHandled;
            return true;
        }
    }
    if (cancel) ++mHandled;
    return cancel;
}

bool InputHandler::onWheel(double raw, double x, double y) {
    mMouseX = x;
    mMouseY = y;
    if (raw == 0) return false;
    double amount = raw > 0 ? 1.0 : -1.0;
    item::ScopedItemRegistry registry;
    int  code   = amount > 0 ? KeyCodes::MOUSE_SCROLL_UP : KeyCodes::MOUSE_SCROLL_DOWN;
    KeybindMulti::onKeyInputPre(code, true);
    bool cancel = checkKeyBindsForChanges(code);
    KeybindMulti::onKeyInputPre(code, false);
    checkKeyBindsForChanges(code);
    if (mWheelSum != 0.0 && std::signbit(amount) != std::signbit(mWheelSum)) mWheelSum = 0.0;
    mWheelSum += amount;
    double whole = std::trunc(mWheelSum);
    if (whole != 0.0) {
        mWheelSum -= whole;
        for (auto* h : mMouseHandlers) {
            if (h->onMouseScroll(x, y, whole)) {
                ++mHandled;
                return true;
            }
        }
    }
    if (cancel) ++mHandled;
    return cancel;
}

void InputHandler::onMove(double x, double y) {
    mSeenMove = true;
    mMouseX   = x;
    mMouseY = y;
    for (auto* h : mMouseHandlers) h->onMouseMove(x, y);
}

void InputHandler::checkFocus() {
    DWORD pid = 0;
    ::GetWindowThreadProcessId(::GetForegroundWindow(), &pid);
    bool const focused = pid == ::GetCurrentProcessId();
    if (mHadFocus && !focused) onFocusLost();
    mHadFocus = focused;
}

void InputHandler::onFocusLost() {
    mDownKeys.clear();
    KeybindMulti::clearPressedKeys();
    item::ScopedItemRegistry registry;
    std::vector<KeybindMulti*> all;
    for (auto& p : mProviders) p(all);
    for (auto* kb : all) kb->updateIsPressed();
    KeybindMulti::clearPressedKeys();
}

void InputHandler::onClientTick() {
    auto const* states = reinterpret_cast<char const*>(&Mouse::_instance()._buttonStates);
    for (int b = 0; b < 3; ++b) {
        int code = KeyCodes::mouseButtonToCode(b);
        bool const swapped = ::GetSystemMetrics(SM_SWAPBUTTON) != 0;
        int const  vk      = b == 0 ? (swapped ? VK_RBUTTON : VK_LBUTTON) : (b == 1 ? (swapped ? VK_LBUTTON : VK_RBUTTON) : VK_MBUTTON);
        if (isKeyDown(code) && states[b + 1] == 0 && (::GetAsyncKeyState(vk) & 0x8000) == 0) {
            std::erase(mDownKeys, code);
            KeybindMulti::onKeyInputPre(code, false);
        }
    }
    std::vector<KeybindMulti*> all;
    for (auto& p : mProviders) p(all);
    for (auto* kb : all) kb->tick();
    KeybindMulti::reCheckPressedKeys([this](int code) { return isKeyDown(code); });
}

}
