// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include "itemscroller/util/MoveAction.h"

namespace inventory_utils::malilib::hotkeys {
class KeybindMulti;
}

namespace inventory_utils::itemscroller::util {

class InputUtils {
public:
    static bool       isRecipeViewOpen();
    static MoveAction getDragMoveAction(malilib::hotkeys::KeybindMulti const& key);
    static bool       isActionKeyActive(MoveAction action);
    static MoveAmount getMoveAmount(MoveAction action);

    static bool isAttack(int keyCode);
    static bool isUse(int keyCode);
    static bool isPickBlock(int keyCode);
};

}
