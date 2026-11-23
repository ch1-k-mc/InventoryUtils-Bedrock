// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "ll/api/mod/NativeMod.h"

namespace inventory_utils {

class InventoryUtils {

public:
    static InventoryUtils& getInstance();

    InventoryUtils() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();

    bool enable();

    bool disable();

private:
    ll::mod::NativeMod& mSelf;
};

}
