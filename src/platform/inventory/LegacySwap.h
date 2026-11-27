// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

class ItemStack;

namespace inventory_utils::platform::inventory::legacy {

bool available();

bool swapSlots(int a, int b);

::ItemStack itemAt(int inv);

}
