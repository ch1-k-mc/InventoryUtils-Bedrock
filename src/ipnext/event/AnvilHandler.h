// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "ipnext/item/ItemType.h"

namespace inventory_utils::ipnext::event {

class AnvilHandler {
public:
    static AnvilHandler& get();

    void onClientTick();

    [[nodiscard]] int                restoreCount() const { return mRestoreCount; }
    [[nodiscard]] std::string const& lastText() const { return mLastText; }
    [[nodiscard]] std::string        currentName() const;

private:
    struct Snapshot {
        std::vector<std::pair<int, item::ItemType>> inputs;
        bool                                        hasResult = false;
        std::string                                 name;
    };
    Snapshot snapshot() const;
    void     restore();

    void*       mScreen = nullptr;
    Snapshot    mLast;
    int         mStage = 0;
    int         mWait  = 0;
    Snapshot    mPending;
    std::string mLastText;
    int         mRestoreCount = 0;
};

}
