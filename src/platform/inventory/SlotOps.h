// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <string>

#include "nlohmann/json.hpp"

namespace inventory_utils::platform::inventory {

struct Slot {
    std::string collection;
    int         index = 0;
};

nlohmann::json runOp(std::string const& op, Slot const& src, Slot const& dst, int amount, bool all, bool hud = false);

nlohmann::json snapshot(int limitPerCollection, bool includeEmpty, bool hud = false);

nlohmann::json slotJson(Slot const& slot, bool hud = false);

}
