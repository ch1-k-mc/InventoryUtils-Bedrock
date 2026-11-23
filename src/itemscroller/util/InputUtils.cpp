// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/util/InputUtils.h"

#include "itemscroller/config/Configs.h"
#include "itemscroller/config/Hotkeys.h"
#include "itemscroller/recipes/CraftingHandler.h"
#include "platform/screen/ScreenTracker.h"
#include "platform/input/KeyCodes.h"

namespace inventory_utils::itemscroller::util {

namespace H = config::Hotkeys;
namespace KeyCodes = platform::input::KeyCodes;

bool InputUtils::isRecipeViewOpen() {
    return platform::screen::ScreenTracker::isGuiOpen() && config::Generic::MOD_MAIN_TOGGLE.getBooleanValue() && H::RECIPE_VIEW.getKeybind().isKeybindHeld()
        && recipes::CraftingHandler::isCraftingGui(platform::screen::ScreenTracker::containerScreenName());
}

MoveAction InputUtils::getDragMoveAction(malilib::hotkeys::KeybindMulti const& key) {
    if (&key == &H::KEY_DRAG_FULL_STACKS.getKeybind()) return MoveAction::MOVE_TO_OTHER_STACKS;
    if (&key == &H::KEY_DRAG_LEAVE_ONE.getKeybind()) return MoveAction::MOVE_TO_OTHER_LEAVE_ONE;
    if (&key == &H::KEY_DRAG_MOVE_ONE.getKeybind()) return MoveAction::MOVE_TO_OTHER_MOVE_ONE;
    if (&key == &H::KEY_DRAG_MATCHING.getKeybind()) return MoveAction::MOVE_TO_OTHER_MATCHING;

    if (&key == &H::KEY_DRAG_DROP_STACKS.getKeybind()) return MoveAction::DROP_STACKS;
    if (&key == &H::KEY_DRAG_DROP_LEAVE_ONE.getKeybind()) return MoveAction::DROP_LEAVE_ONE;
    if (&key == &H::KEY_DRAG_DROP_SINGLE.getKeybind()) return MoveAction::DROP_ONE;

    if (&key == &H::KEY_WS_MOVE_UP_STACKS.getKeybind()) return MoveAction::MOVE_UP_STACKS;
    if (&key == &H::KEY_WS_MOVE_UP_MATCHING.getKeybind()) return MoveAction::MOVE_UP_MATCHING;
    if (&key == &H::KEY_WS_MOVE_UP_LEAVE_ONE.getKeybind()) return MoveAction::MOVE_UP_LEAVE_ONE;
    if (&key == &H::KEY_WS_MOVE_UP_SINGLE.getKeybind()) return MoveAction::MOVE_UP_MOVE_ONE;
    if (&key == &H::KEY_WS_MOVE_DOWN_STACKS.getKeybind()) return MoveAction::MOVE_DOWN_STACKS;
    if (&key == &H::KEY_WS_MOVE_DOWN_MATCHING.getKeybind()) return MoveAction::MOVE_DOWN_MATCHING;
    if (&key == &H::KEY_WS_MOVE_DOWN_LEAVE_ONE.getKeybind()) return MoveAction::MOVE_DOWN_LEAVE_ONE;
    if (&key == &H::KEY_WS_MOVE_DOWN_SINGLE.getKeybind()) return MoveAction::MOVE_DOWN_MOVE_ONE;

    return MoveAction::NONE;
}

bool InputUtils::isActionKeyActive(MoveAction action) {
    switch (action) {
    case MoveAction::MOVE_TO_OTHER_STACKS: return H::KEY_DRAG_FULL_STACKS.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_TO_OTHER_LEAVE_ONE: return H::KEY_DRAG_LEAVE_ONE.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_TO_OTHER_MOVE_ONE: return H::KEY_DRAG_MOVE_ONE.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_TO_OTHER_MATCHING: return H::KEY_DRAG_MATCHING.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_TO_OTHER_EVERYTHING: return H::KEY_MOVE_EVERYTHING.getKeybind().isKeybindHeld();
    case MoveAction::DROP_STACKS: return H::KEY_DRAG_DROP_STACKS.getKeybind().isKeybindHeld();
    case MoveAction::DROP_LEAVE_ONE: return H::KEY_DRAG_DROP_LEAVE_ONE.getKeybind().isKeybindHeld();
    case MoveAction::DROP_ONE: return H::KEY_DRAG_DROP_SINGLE.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_UP_STACKS: return H::KEY_WS_MOVE_UP_STACKS.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_UP_MATCHING: return H::KEY_WS_MOVE_UP_MATCHING.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_UP_LEAVE_ONE: return H::KEY_WS_MOVE_UP_LEAVE_ONE.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_UP_MOVE_ONE: return H::KEY_WS_MOVE_UP_SINGLE.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_DOWN_STACKS: return H::KEY_WS_MOVE_DOWN_STACKS.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_DOWN_MATCHING: return H::KEY_WS_MOVE_DOWN_MATCHING.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_DOWN_LEAVE_ONE: return H::KEY_WS_MOVE_DOWN_LEAVE_ONE.getKeybind().isKeybindHeld();
    case MoveAction::MOVE_DOWN_MOVE_ONE: return H::KEY_WS_MOVE_DOWN_SINGLE.getKeybind().isKeybindHeld();
    default: return false;
    }
}

MoveAmount InputUtils::getMoveAmount(MoveAction action) {
    switch (action) {
    case MoveAction::SCROLL_TO_OTHER_MOVE_ONE:
    case MoveAction::MOVE_TO_OTHER_MOVE_ONE:
    case MoveAction::DROP_ONE:
    case MoveAction::MOVE_DOWN_MOVE_ONE:
    case MoveAction::MOVE_UP_MOVE_ONE: return MoveAmount::MOVE_ONE;

    case MoveAction::MOVE_TO_OTHER_LEAVE_ONE:
    case MoveAction::DROP_LEAVE_ONE:
    case MoveAction::MOVE_DOWN_LEAVE_ONE:
    case MoveAction::MOVE_UP_LEAVE_ONE: return MoveAmount::LEAVE_ONE;

    case MoveAction::SCROLL_TO_OTHER_STACKS:
    case MoveAction::MOVE_TO_OTHER_STACKS:
    case MoveAction::DROP_STACKS:
    case MoveAction::MOVE_DOWN_STACKS:
    case MoveAction::MOVE_UP_STACKS: return MoveAmount::FULL_STACKS;

    case MoveAction::SCROLL_TO_OTHER_MATCHING:
    case MoveAction::MOVE_TO_OTHER_MATCHING:
    case MoveAction::DROP_ALL_MATCHING:
    case MoveAction::MOVE_UP_MATCHING:
    case MoveAction::MOVE_DOWN_MATCHING: return MoveAmount::ALL_MATCHING;

    case MoveAction::MOVE_TO_OTHER_EVERYTHING:
    case MoveAction::SCROLL_TO_OTHER_EVERYTHING: return MoveAmount::EVERYTHING;

    default: return MoveAmount::NONE;
    }
}

bool InputUtils::isAttack(int keyCode) { return keyCode == KeyCodes::MOUSE_LEFT; }
bool InputUtils::isUse(int keyCode) { return keyCode == KeyCodes::MOUSE_RIGHT; }
bool InputUtils::isPickBlock(int keyCode) { return keyCode == KeyCodes::MOUSE_MIDDLE; }

}
