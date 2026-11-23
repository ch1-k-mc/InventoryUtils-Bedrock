// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/inventory/TransferAmount.h"

#include <cstring>

ItemTransferAmount::ItemTransferAmount() { std::memset(static_cast<void*>(this), 0, sizeof(ItemTransferAmount)); }

ItemTransferAmount::ItemTransferAmount(ItemTransferAmount const& other) {
    std::memcpy(static_cast<void*>(this), static_cast<void const*>(&other), sizeof(ItemTransferAmount));
}

ItemTransferAmount& ItemTransferAmount::operator=(ItemTransferAmount const& other) {
    std::memcpy(static_cast<void*>(this), static_cast<void const*>(&other), sizeof(ItemTransferAmount));
    return *this;
}

namespace inventory_utils::platform::inventory {

namespace {
int gLayout = 0;
}

void setTransferAmountLayout(int layout) { gLayout = layout; }
int  transferAmountLayout() { return gLayout; }

ItemTransferAmount makeTransferAmount(ItemTransferAmount::ItemTransferAmountTag tag, int value) {
    ItemTransferAmount amount;
    auto*              bytes = reinterpret_cast<unsigned char*>(&amount);
    int                t     = static_cast<int>(tag);
    if (gLayout == 0) {
        std::memcpy(bytes, &t, 4);
        std::memcpy(bytes + 4, &value, 4);
    } else {
        std::memcpy(bytes, &value, 4);
        std::memcpy(bytes + 4, &t, 4);
    }
    return amount;
}

}
