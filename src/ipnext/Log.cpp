// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/Log.h"

#include "ll/api/io/Logger.h"
#include "ll/api/mod/NativeMod.h"

#include "ipnext/config/Configs.h"
#include "mod/InventoryUtils.h"

namespace inventory_utils::ipnext::Log {

bool traceEnabled() { return config::Debugs::TRACE_LOGS.getBooleanValue(); }
bool debugEnabled() { return config::ModSettings::DEBUG.getBooleanValue() || traceEnabled(); }

void write(int level, std::string const& message) {
    auto& logger = ::inventory_utils::InventoryUtils::getInstance().getSelf().getLogger();
    switch (level) {
    case 0:
    case 1:
    case 2:
        logger.info("[ipn] {}", message);
        break;
    case 3:
        logger.warn("[ipn] {}", message);
        break;
    default:
        logger.error("[ipn] {}", message);
        break;
    }
}

}
