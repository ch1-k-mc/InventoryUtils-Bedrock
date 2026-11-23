// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "malilib/hotkeys/KeybindSettings.h"

namespace inventory_utils::malilib::hotkeys {

class KeybindMulti;

using HotkeyCallback = std::function<bool(KeyAction action, KeybindMulti& key)>;

class KeybindMulti {
public:
    static KeybindMulti fromStorageString(std::string const& str, KeybindSettings const& settings);

    [[nodiscard]] KeybindSettings const& getSettings() const { return mSettings; }
    void                                 setSettings(KeybindSettings const& settings) { mSettings = settings; }
    void                                 setCallback(HotkeyCallback callback) { mCallback = std::move(callback); }

    [[nodiscard]] bool isValid() const { return !mKeys.empty() || mSettings.getAllowEmpty(); }
    [[nodiscard]] bool isPressed() const { return mPressed && !mPressedLast && mHeldTime == 0; }
    [[nodiscard]] bool isKeybindHeld() const { return mPressed || (mSettings.getAllowEmpty() && mKeys.empty()); }

    bool updateIsPressed();
    void tick();

    void clearKeys();
    void addKey(int code);
    void removeKey(int code);
    [[nodiscard]] std::vector<int> const& getKeys() const { return mKeys; }

    [[nodiscard]] std::string getStringValue() const;
    [[nodiscard]] std::string getDefaultStringValue() const { return mDefaultStorageString; }
    void                      setValueFromString(std::string const& str);
    [[nodiscard]] std::string getKeysDisplayString() const;
    [[nodiscard]] bool        isModified() const { return getStringValue() != mDefaultStorageString; }
    [[nodiscard]] bool        areSettingsModified() const { return !(mSettings == mDefaultSettings); }
    void                      resetToDefault() { setValueFromString(mDefaultStorageString); }
    void                      resetSettingsToDefaults() { mSettings = mDefaultSettings; }
    [[nodiscard]] bool        matches(int code) const { return mKeys.size() == 1 && mKeys.front() == code; }
    [[nodiscard]] bool        overlaps(KeybindMulti const& other) const;
    [[nodiscard]] bool        contextOverlaps(KeybindMulti const& other) const;

    [[nodiscard]] nlohmann::json getAsJson() const;
    void                         setValueFromJson(nlohmann::json const& element);

    static void                    onKeyInputPre(int code, bool pressed);
    static void                    reCheckPressedKeys(std::function<bool(int)> const& isKeyDown);
    static bool                    isPressedKey(int code);
    static std::vector<int> const& pressedKeys();
    static std::string             getActiveKeysString();
    static int                     getTriggeredCount();
    static void                    clearPressedKeys();

    static void setGuiOpenQuery(std::function<bool()> query);
    static bool isGuiOpen();

private:
    KeybindMulti(std::string defaultStorageString, KeybindSettings const& settings)
    : mDefaultStorageString(std::move(defaultStorageString)),
      mDefaultSettings(settings),
      mSettings(settings) {}

    bool triggerKeyAction(bool pressedLast);

    std::string      mDefaultStorageString;
    KeybindSettings  mDefaultSettings;
    KeybindSettings  mSettings;
    std::vector<int> mKeys;
    bool             mPressed     = false;
    bool             mPressedLast = false;
    int              mHeldTime    = 0;
    HotkeyCallback   mCallback;
};

}
