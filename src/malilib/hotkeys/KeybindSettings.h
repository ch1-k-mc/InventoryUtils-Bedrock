// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <string>

#include "nlohmann/json.hpp"

namespace inventory_utils::malilib::hotkeys {

enum class KeyAction { PRESS, RELEASE, BOTH };

class KeybindSettings {
public:
    enum class Context { INGAME, GUI, ANY };

    static KeybindSettings create(Context context, KeyAction activateOn, bool allowExtraKeys, bool orderSensitive, bool exclusive, bool cancel, bool allowEmpty = false) {
        return KeybindSettings(context, activateOn, allowExtraKeys, orderSensitive, exclusive, cancel, allowEmpty);
    }

    static KeybindSettings const DEFAULT;
    static KeybindSettings const EXCLUSIVE;
    static KeybindSettings const RELEASE;
    static KeybindSettings const RELEASE_ALLOW_EXTRA;
    static KeybindSettings const RELEASE_EXCLUSIVE;
    static KeybindSettings const NOCANCEL;
    static KeybindSettings const PRESS_ALLOWEXTRA;
    static KeybindSettings const PRESS_ALLOWEXTRA_EMPTY;
    static KeybindSettings const PRESS_NON_ORDER_SENSITIVE;
    static KeybindSettings const INGAME_BOTH;
    static KeybindSettings const MODIFIER_INGAME;
    static KeybindSettings const MODIFIER_INGAME_EMPTY;
    static KeybindSettings const MODIFIER_GUI;
    static KeybindSettings const GUI;

    [[nodiscard]] Context   getContext() const { return mContext; }
    [[nodiscard]] KeyAction getActivateOn() const { return mActivateOn; }
    [[nodiscard]] bool      getAllowEmpty() const { return mAllowEmpty; }
    [[nodiscard]] bool      getAllowExtraKeys() const { return mAllowExtraKeys; }
    [[nodiscard]] bool      isOrderSensitive() const { return mOrderSensitive; }
    [[nodiscard]] bool      isExclusive() const { return mExclusive; }
    [[nodiscard]] bool      shouldCancel() const { return mCancel; }

    [[nodiscard]] nlohmann::json toJson() const;
    static KeybindSettings       fromJson(nlohmann::json const& obj);

    bool operator==(KeybindSettings const&) const = default;

    static std::string contextName(Context c);
    static std::string actionName(KeyAction a);

private:
    KeybindSettings(Context context, KeyAction activateOn, bool allowExtraKeys, bool orderSensitive, bool exclusive, bool cancel, bool allowEmpty)
    : mContext(context),
      mActivateOn(activateOn),
      mAllowEmpty(allowEmpty),
      mAllowExtraKeys(allowExtraKeys),
      mOrderSensitive(orderSensitive),
      mExclusive(exclusive),
      mCancel(cancel) {}

    Context   mContext;
    KeyAction mActivateOn;
    bool      mAllowEmpty;
    bool      mAllowExtraKeys;
    bool      mOrderSensitive;
    bool      mExclusive;
    bool      mCancel;
};

}
