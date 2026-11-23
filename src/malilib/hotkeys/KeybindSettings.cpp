// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "malilib/hotkeys/KeybindSettings.h"

#include <algorithm>
#include <cctype>

namespace inventory_utils::malilib::hotkeys {

using C = KeybindSettings::Context;
using A = KeyAction;

KeybindSettings const KeybindSettings::DEFAULT                   = create(C::INGAME, A::PRESS, false, true, false, true);
KeybindSettings const KeybindSettings::EXCLUSIVE                 = create(C::INGAME, A::PRESS, false, true, true, true);
KeybindSettings const KeybindSettings::RELEASE                   = create(C::INGAME, A::RELEASE, false, true, false, false);
KeybindSettings const KeybindSettings::RELEASE_ALLOW_EXTRA       = create(C::INGAME, A::RELEASE, true, true, false, false);
KeybindSettings const KeybindSettings::RELEASE_EXCLUSIVE         = create(C::INGAME, A::RELEASE, false, true, true, true);
KeybindSettings const KeybindSettings::NOCANCEL                  = create(C::INGAME, A::PRESS, false, true, false, false);
KeybindSettings const KeybindSettings::PRESS_ALLOWEXTRA          = create(C::INGAME, A::PRESS, true, true, false, true);
KeybindSettings const KeybindSettings::PRESS_ALLOWEXTRA_EMPTY    = create(C::INGAME, A::PRESS, true, true, false, true, true);
KeybindSettings const KeybindSettings::PRESS_NON_ORDER_SENSITIVE = create(C::INGAME, A::PRESS, false, false, false, true);
KeybindSettings const KeybindSettings::INGAME_BOTH               = create(C::INGAME, A::BOTH, false, true, false, true);
KeybindSettings const KeybindSettings::MODIFIER_INGAME           = create(C::INGAME, A::PRESS, true, false, false, false);
KeybindSettings const KeybindSettings::MODIFIER_INGAME_EMPTY     = create(C::INGAME, A::PRESS, true, false, false, false, true);
KeybindSettings const KeybindSettings::MODIFIER_GUI              = create(C::GUI, A::PRESS, true, false, false, false);
KeybindSettings const KeybindSettings::GUI                       = create(C::GUI, A::PRESS, false, true, false, true);

std::string KeybindSettings::contextName(Context c) {
    switch (c) {
    case C::INGAME: return "INGAME";
    case C::GUI: return "GUI";
    default: return "ANY";
    }
}

std::string KeybindSettings::actionName(KeyAction a) {
    switch (a) {
    case A::PRESS: return "PRESS";
    case A::RELEASE: return "RELEASE";
    default: return "BOTH";
    }
}

nlohmann::json KeybindSettings::toJson() const {
    return {
        {"activate_on",      actionName(mActivateOn)},
        {"context",          contextName(mContext)  },
        {"allow_empty",      mAllowEmpty            },
        {"allow_extra_keys", mAllowExtraKeys        },
        {"order_sensitive",  mOrderSensitive        },
        {"exclusive",        mExclusive             },
        {"cancel",           mCancel                },
    };
}

KeybindSettings KeybindSettings::fromJson(nlohmann::json const& obj) {
    auto upper = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        return s;
    };
    Context   context    = C::INGAME;
    KeyAction activateOn = A::PRESS;
    if (obj.contains("context") && obj["context"].is_string()) {
        auto s = upper(obj["context"].get<std::string>());
        if (s == "GUI") context = C::GUI;
        else if (s == "ANY") context = C::ANY;
    }
    if (obj.contains("activate_on") && obj["activate_on"].is_string()) {
        auto s = upper(obj["activate_on"].get<std::string>());
        if (s == "RELEASE") activateOn = A::RELEASE;
        else if (s == "BOTH") activateOn = A::BOTH;
    }
    auto b = [&](char const* key, bool def) { return obj.contains(key) && obj[key].is_boolean() ? obj[key].get<bool>() : def; };
    return create(context, activateOn, b("allow_extra_keys", false), b("order_sensitive", true), b("exclusive", true), b("cancel", true), b("allow_empty", false));
}

}
