// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <set>

namespace inventory_utils::ipnext::event {

class LockedSlotKeeper {
public:
    static LockedSlotKeeper& get();

    void onClientTick();
    void onJoinWorld();
    void init();

    [[nodiscard]] std::set<int> const& emptyLockedSlots() const { return mEmptyLocked; }
    [[nodiscard]] int                  movedCount() const { return mMoved; }

private:
    std::set<int> mEmptyLocked;
    std::set<int> mEmptyNonLocked;
    bool          mScreenOpening = false;
    bool          mWorldJoined   = false;
    int           mTicksAfterJoin = 0;
    bool          mProcessing     = false;
    int           mMoved          = 0;
};

}
