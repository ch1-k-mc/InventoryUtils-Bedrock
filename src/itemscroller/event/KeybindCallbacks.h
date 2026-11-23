// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include "malilib/hotkeys/KeybindMulti.h"

namespace inventory_utils::itemscroller::event {

class KeybindCallbacks {
public:
    static KeybindCallbacks& getInstance();

    void setCallbacks();
    void clearCallbacks();
    [[nodiscard]] bool functionalityEnabled() const;

    bool onKeyAction(malilib::hotkeys::KeyAction action, malilib::hotkeys::KeybindMulti& key);
    void onClientTick();

private:
    int mMassCraftTicker = 0;
};

}
