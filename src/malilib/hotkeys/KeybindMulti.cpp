// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "malilib/hotkeys/KeybindMulti.h"

#include <algorithm>
#include <sstream>

#include "platform/input/KeyCodes.h"

namespace inventory_utils::malilib::hotkeys {

namespace {

namespace KeyCodes = platform::input::KeyCodes;

std::vector<int>      gPressedKeys;
int                   gTriggeredCount = 0;
std::function<bool()> gGuiOpen;

bool contains(std::vector<int> const& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

bool containsAll(std::vector<int> const& haystack, std::vector<int> const& needles) {
    return std::all_of(needles.begin(), needles.end(), [&](int n) { return contains(haystack, n); });
}

}

KeybindMulti KeybindMulti::fromStorageString(std::string const& str, KeybindSettings const& settings) {
    KeybindMulti keybind(str, settings);
    keybind.setValueFromString(str);
    return keybind;
}

bool KeybindMulti::updateIsPressed() {
    auto context = mSettings.getContext();
    bool guiOpen = gGuiOpen ? gGuiOpen() : false;
    if (mKeys.empty() || (context != KeybindSettings::Context::ANY && ((context == KeybindSettings::Context::INGAME) != !guiOpen))) {
        mPressed  = false;
        mHeldTime = 0;
        return false;
    }

    bool       allowExtraKeys  = mSettings.getAllowExtraKeys();
    bool       allowOutOfOrder = !mSettings.isOrderSensitive();
    bool       pressedLast     = mPressed;
    auto const sizePressed     = gPressedKeys.size();
    auto const sizeRequired    = mKeys.size();

    if (sizePressed >= sizeRequired && (allowExtraKeys || sizePressed == sizeRequired)) {
        size_t keyIndex = 0;
        mPressed        = containsAll(gPressedKeys, mKeys);

        for (size_t i = 0; i < sizePressed; ++i) {
            int code = gPressedKeys[i];
            if (mKeys[keyIndex] == code) {
                if (++keyIndex >= sizeRequired) break;
            } else if ((!allowOutOfOrder && (keyIndex > 0 || sizePressed == sizeRequired)) || (!contains(mKeys, code) && !allowExtraKeys)) {
                mPressed = false;
                break;
            }
        }
    } else {
        mPressed = false;
    }

    if (!mPressed && pressedLast) mHeldTime = 0;
    KeyAction activateOn = mSettings.getActivateOn();
    if (mPressed != pressedLast && (gTriggeredCount == 0 || !mSettings.isExclusive())
        && (activateOn == KeyAction::BOTH || mPressed == (activateOn == KeyAction::PRESS))) {
        bool cancel = triggerKeyAction(pressedLast) && mSettings.shouldCancel();
        if (cancel) ++gTriggeredCount;
        return cancel;
    }
    return false;
}

bool KeybindMulti::triggerKeyAction(bool pressedLast) {
    bool cancel = false;
    if (!mPressed) {
        mHeldTime            = 0;
        KeyAction activateOn = mSettings.getActivateOn();
        if (pressedLast && mCallback && (activateOn == KeyAction::RELEASE || activateOn == KeyAction::BOTH)) {
            cancel = mCallback(KeyAction::RELEASE, *this);
        }
    } else if (!pressedLast && mHeldTime == 0) {
        KeyAction activateOn = mSettings.getActivateOn();
        if (mCallback && (activateOn == KeyAction::PRESS || activateOn == KeyAction::BOTH)) {
            cancel = mCallback(KeyAction::PRESS, *this);
        }
    }
    return cancel;
}

void KeybindMulti::tick() {
    if (mPressed) ++mHeldTime;
    mPressedLast = mPressed;
}

void KeybindMulti::clearKeys() {
    mKeys.clear();
    mPressed  = false;
    mHeldTime = 0;
}

void KeybindMulti::addKey(int code) {
    if (!contains(mKeys, code)) mKeys.push_back(code);
}

void KeybindMulti::removeKey(int code) { std::erase(mKeys, code); }

std::string KeybindMulti::getStringValue() const {
    std::string s;
    for (size_t i = 0; i < mKeys.size(); ++i) {
        if (i > 0) s += ",";
        s += KeyCodes::toName(mKeys[i]);
    }
    return s;
}

void KeybindMulti::setValueFromString(std::string const& str) {
    clearKeys();
    std::stringstream ss(str);
    std::string       name;
    while (std::getline(ss, name, ',')) {
        auto b = name.find_first_not_of(" \t");
        auto e = name.find_last_not_of(" \t");
        if (b == std::string::npos) continue;
        int code = KeyCodes::fromName(name.substr(b, e - b + 1));
        if (code != KeyCodes::UNKNOWN) addKey(code);
    }
}

std::string KeybindMulti::getKeysDisplayString() const {
    std::string s;
    for (size_t i = 0; i < mKeys.size(); ++i) {
        if (i > 0) s += " + ";
        s += KeyCodes::displayName(mKeys[i]);
    }
    return s;
}

bool KeybindMulti::contextOverlaps(KeybindMulti const& other) const {
    auto c1 = mSettings.getContext();
    auto c2 = other.mSettings.getContext();
    if (c1 == KeybindSettings::Context::ANY || c2 == KeybindSettings::Context::ANY || c1 == c2) {
        auto a1 = mSettings.getActivateOn();
        auto a2 = other.mSettings.getActivateOn();
        return a1 == KeyAction::BOTH || a2 == KeyAction::BOTH || a1 == a2;
    }
    return false;
}

bool KeybindMulti::overlaps(KeybindMulti const& other) const {
    if (&other == this || other.mKeys.size() > mKeys.size()) return false;
    if (!contextOverlaps(other)) return false;
    bool        o1 = mSettings.isOrderSensitive();
    bool        o2 = other.mSettings.isOrderSensitive();
    auto const& k1 = mKeys;
    auto const& k2 = other.mKeys;
    auto        l1 = k1.size();
    auto        l2 = k2.size();
    if (l1 == 0 || l2 == 0) return false;
    if ((!mSettings.getAllowExtraKeys() && l1 < l2 && k1.front() != k2.front())
        || (!other.mSettings.getAllowExtraKeys() && l2 < l1 && k1.front() != k2.front())) {
        return false;
    }
    if (o1 && o2) {
        auto const& shorter = l1 < l2 ? k1 : k2;
        auto const& longer  = l1 < l2 ? k2 : k1;
        return std::search(longer.begin(), longer.end(), shorter.begin(), shorter.end()) != longer.end();
    }
    return l1 <= l2 ? containsAll(k2, k1) : containsAll(k1, k2);
}

nlohmann::json KeybindMulti::getAsJson() const {
    nlohmann::json obj{{"keys", getStringValue()}};
    if (areSettingsModified()) obj["settings"] = mSettings.toJson();
    return obj;
}

void KeybindMulti::setValueFromJson(nlohmann::json const& element) {
    if (element.is_object()) {
        if (element.contains("keys") && element["keys"].is_string()) setValueFromString(element["keys"].get<std::string>());
        if (element.contains("settings") && element["settings"].is_object()) setSettings(KeybindSettings::fromJson(element["settings"]));
    } else if (element.is_string()) {
        setValueFromString(element.get<std::string>());
    }
}

void KeybindMulti::onKeyInputPre(int code, bool pressed) {
    if (code == KeyCodes::UNKNOWN) return;
    if (pressed) {
        if (!contains(gPressedKeys, code)) gPressedKeys.push_back(code);
    } else {
        std::erase(gPressedKeys, code);
    }
}

void KeybindMulti::reCheckPressedKeys(std::function<bool(int)> const& isKeyDown) {
    std::erase_if(gPressedKeys, [&](int code) { return !isKeyDown(code); });
    if (gPressedKeys.empty()) gTriggeredCount = 0;
}

bool KeybindMulti::isPressedKey(int code) { return contains(gPressedKeys, code); }

std::vector<int> const& KeybindMulti::pressedKeys() { return gPressedKeys; }

std::string KeybindMulti::getActiveKeysString() {
    if (gPressedKeys.empty()) return "<none>";
    std::string s;
    for (size_t i = 0; i < gPressedKeys.size(); ++i) {
        if (i > 0) s += " + ";
        s += KeyCodes::toName(gPressedKeys[i]) + " (" + std::to_string(gPressedKeys[i]) + ")";
    }
    return s;
}

int KeybindMulti::getTriggeredCount() { return gTriggeredCount; }

void KeybindMulti::clearPressedKeys() {
    gPressedKeys.clear();
    gTriggeredCount = 0;
}

void KeybindMulti::setGuiOpenQuery(std::function<bool()> query) { gGuiOpen = std::move(query); }
bool KeybindMulti::isGuiOpen() { return gGuiOpen ? gGuiOpen() : false; }

}
