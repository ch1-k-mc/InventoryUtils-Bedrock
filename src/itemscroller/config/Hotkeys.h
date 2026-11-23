// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <vector>

#include "malilib/config/ConfigOptions.h"

namespace inventory_utils::itemscroller::config::Hotkeys {

using malilib::config::ConfigHotkey;

extern ConfigHotkey CRAFT_EVERYTHING;
extern ConfigHotkey DROP_ALL_MATCHING;
extern ConfigHotkey MASS_CRAFT;
extern ConfigHotkey MASS_CRAFT_TOGGLE;
extern ConfigHotkey MOVE_CRAFT_RESULTS;
extern ConfigHotkey RECIPE_VIEW;
extern ConfigHotkey SLOT_DEBUG;
extern ConfigHotkey STORE_RECIPE;
extern ConfigHotkey THROW_CRAFT_RESULTS;
extern ConfigHotkey TOGGLE_MOD_ON_OFF;

extern ConfigHotkey KEY_DRAG_DROP_LEAVE_ONE;
extern ConfigHotkey KEY_DRAG_DROP_SINGLE;
extern ConfigHotkey KEY_DRAG_DROP_STACKS;

extern ConfigHotkey KEY_DRAG_LEAVE_ONE;
extern ConfigHotkey KEY_DRAG_MATCHING;
extern ConfigHotkey KEY_DRAG_MOVE_ONE;
extern ConfigHotkey KEY_DRAG_FULL_STACKS;

extern ConfigHotkey KEY_MOVE_EVERYTHING;

extern ConfigHotkey KEY_WS_MOVE_DOWN_LEAVE_ONE;
extern ConfigHotkey KEY_WS_MOVE_DOWN_MATCHING;
extern ConfigHotkey KEY_WS_MOVE_DOWN_SINGLE;
extern ConfigHotkey KEY_WS_MOVE_DOWN_STACKS;
extern ConfigHotkey KEY_WS_MOVE_UP_LEAVE_ONE;
extern ConfigHotkey KEY_WS_MOVE_UP_MATCHING;
extern ConfigHotkey KEY_WS_MOVE_UP_SINGLE;
extern ConfigHotkey KEY_WS_MOVE_UP_STACKS;

extern ConfigHotkey MODIFIER_MOVE_EVERYTHING;
extern ConfigHotkey MODIFIER_MOVE_MATCHING;
extern ConfigHotkey MODIFIER_MOVE_STACK;

std::vector<ConfigHotkey*> const& HOTKEY_LIST();

}
