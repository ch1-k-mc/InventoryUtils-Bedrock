// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "mc/world/containers/controllers/ItemTransferAmount.h"

namespace inventory_utils::platform::inventory {

void setTransferAmountLayout(int layout);
int  transferAmountLayout();

ItemTransferAmount makeTransferAmount(ItemTransferAmount::ItemTransferAmountTag tag, int value);

}
