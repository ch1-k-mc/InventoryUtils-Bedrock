// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <string>

#include "mc/world/item/ItemStack.h"

namespace inventory_utils::platform::item {

std::string toSnbt(ItemStack const& stack);
ItemStack   fromSnbt(std::string const& snbt);

}
