// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

namespace inventory_utils::itemscroller::util {

enum class MoveAction {
    NONE,
    SCROLL_TO_OTHER_MOVE_ONE,
    SCROLL_TO_OTHER_STACKS,
    SCROLL_TO_OTHER_MATCHING,
    SCROLL_TO_OTHER_EVERYTHING,
    SCROLL_TO_THIS_MOVE_ONE,
    SCROLL_TO_THIS_STACKS,
    SCROLL_TO_THIS_MATCHING,
    MOVE_TO_OTHER_MOVE_ONE,
    MOVE_TO_OTHER_LEAVE_ONE,
    MOVE_TO_OTHER_STACKS,
    MOVE_TO_OTHER_MATCHING,
    MOVE_TO_OTHER_EVERYTHING,
    MOVE_UP_MOVE_ONE,
    MOVE_UP_LEAVE_ONE,
    MOVE_UP_STACKS,
    MOVE_UP_MATCHING,
    MOVE_DOWN_MOVE_ONE,
    MOVE_DOWN_LEAVE_ONE,
    MOVE_DOWN_STACKS,
    MOVE_DOWN_MATCHING,
    DROP_ONE,
    DROP_LEAVE_ONE,
    DROP_STACKS,
    DROP_ALL_MATCHING,
};

enum class MoveAmount { NONE, MOVE_ONE, LEAVE_ONE, FULL_STACKS, ALL_MATCHING, EVERYTHING };

}
