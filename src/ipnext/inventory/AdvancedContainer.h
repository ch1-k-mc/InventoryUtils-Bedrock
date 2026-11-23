// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <functional>
#include <optional>

#include "ipnext/inventory/AreaTypes.h"
#include "ipnext/inventory/sandbox/ContainerSandbox.h"

namespace inventory_utils::ipnext::inventory {

using data::ItemTracker;
using data::SubTracker;

class AdvancedContainer {
public:
    class Dsl {
    public:
        explicit Dsl(AdvancedContainer& owner) : mOwner(owner) {}
        [[nodiscard]] ItemArea get(AreaType const& type) const { return type.getItemArea(mOwner.mVanilla); }
        [[nodiscard]] VanillaContainer const& vanillaContainer() const { return mOwner.mVanilla; }

    protected:
        AdvancedContainer& mOwner;
    };

    class TrackerDsl : public Dsl {
    public:
        TrackerDsl(AdvancedContainer& owner, ItemTracker& tracker) : Dsl(owner), tracker(tracker) {}
        [[nodiscard]] SubTracker asSubTracker(ItemArea const& area) const { return tracker.subTracker(area.slotIndices()); }
        ItemTracker& tracker;
    };

    class SandboxDsl : public Dsl {
    public:
        SandboxDsl(AdvancedContainer& owner, sandbox::ContainerSandbox& sb) : Dsl(owner), sandbox(sb) {}
        [[nodiscard]] ItemTracker const& sandboxTracker() const { return sandbox.items(); }
        sandbox::ContainerSandbox& sandbox;
    };

    explicit AdvancedContainer(VanillaContainer vanilla);

    void sandbox(std::function<void(SandboxDsl&)> const& block);
    void tracker(std::function<void(TrackerDsl&)> const& block);
    void arrange(bool instant = false);
    void cleanCursor();

    static void run(bool instant, std::function<void(AdvancedContainer&)> const& block);
    static void withTracker(std::function<void(TrackerDsl&)> const& block, bool instant = false, bool cleanCursor = true);

private:
    void cursorPutTo(sandbox::ContainerSandbox& sb, ItemArea const& destination, bool skipEmpty);
    [[nodiscard]] std::vector<std::pair<int, int>> slotIdClicks();

    VanillaContainer      mVanilla;
    sandbox::ItemPlanner  mPlanner;
};

}
