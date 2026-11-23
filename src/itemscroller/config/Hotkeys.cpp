// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/config/Hotkeys.h"

namespace inventory_utils::itemscroller::config::Hotkeys {

namespace {

using malilib::hotkeys::KeyAction;
using malilib::hotkeys::KeybindSettings;
using Ctx = KeybindSettings::Context;

constexpr char const* HOTKEYS_KEY = "itemscroller.config.hotkeys";

KeybindSettings const DEFAULT_SETTINGS  = KeybindSettings::create(Ctx::INGAME, KeyAction::PRESS, false, true, false, true);
KeybindSettings const GUI               = KeybindSettings::create(Ctx::GUI, KeyAction::PRESS, false, true, false, true);
KeybindSettings const GUI_RELAXED       = KeybindSettings::create(Ctx::GUI, KeyAction::PRESS, true, false, false, false);
KeybindSettings const GUI_RELAXED_CANCEL = KeybindSettings::create(Ctx::GUI, KeyAction::PRESS, true, false, false, true);
KeybindSettings const GUI_NO_ORDER      = KeybindSettings::create(Ctx::GUI, KeyAction::PRESS, false, false, false, true);

ConfigHotkey make(char const* name, char const* keys, KeybindSettings const& settings) {
    ConfigHotkey h(name, keys, settings);
    h.apply(HOTKEYS_KEY);
    return h;
}

}

ConfigHotkey CRAFT_EVERYTHING    = make("craftEverything", "LEFT_CONTROL,C", GUI_NO_ORDER);
ConfigHotkey DROP_ALL_MATCHING   = make("dropAllMatching", "LEFT_CONTROL,LEFT_SHIFT,Q", GUI_NO_ORDER);
ConfigHotkey MASS_CRAFT          = make("massCraft", "LEFT_CONTROL,LEFT_ALT,C", GUI_NO_ORDER);
ConfigHotkey MASS_CRAFT_TOGGLE   = make("massCraftToggle", "", DEFAULT_SETTINGS);
ConfigHotkey MOVE_CRAFT_RESULTS  = make("moveCraftResults", "LEFT_CONTROL,M", GUI_NO_ORDER);
ConfigHotkey RECIPE_VIEW         = make("recipeView", "A", GUI_RELAXED);
ConfigHotkey SLOT_DEBUG          = make("slotDebug", "LEFT_CONTROL,LEFT_ALT,LEFT_SHIFT,I", GUI_NO_ORDER);
ConfigHotkey STORE_RECIPE        = make("storeRecipe", "MOUSE_MIDDLE", GUI_RELAXED_CANCEL);
ConfigHotkey THROW_CRAFT_RESULTS = make("throwCraftResults", "LEFT_CONTROL,T", GUI_NO_ORDER);
ConfigHotkey TOGGLE_MOD_ON_OFF   = make("toggleModOnOff", "", GUI);

ConfigHotkey KEY_DRAG_DROP_LEAVE_ONE = make("keyDragDropLeaveOne", "LEFT_SHIFT,Q,MOUSE_RIGHT", GUI_NO_ORDER);
ConfigHotkey KEY_DRAG_DROP_SINGLE    = make("keyDragDropSingle", "Q,MOUSE_LEFT", GUI_NO_ORDER);
ConfigHotkey KEY_DRAG_DROP_STACKS    = make("keyDragDropStacks", "LEFT_SHIFT,Q,MOUSE_LEFT", GUI_NO_ORDER);

ConfigHotkey KEY_DRAG_LEAVE_ONE   = make("keyDragMoveLeaveOne", "LEFT_SHIFT,MOUSE_RIGHT", GUI_NO_ORDER);
ConfigHotkey KEY_DRAG_MATCHING    = make("keyDragMoveMatching", "LEFT_ALT,MOUSE_LEFT", GUI_NO_ORDER);
ConfigHotkey KEY_DRAG_MOVE_ONE    = make("keyDragMoveOne", "LEFT_CONTROL,MOUSE_LEFT", GUI_NO_ORDER);
ConfigHotkey KEY_DRAG_FULL_STACKS = make("keyDragMoveStacks", "LEFT_SHIFT,MOUSE_LEFT", GUI_NO_ORDER);

ConfigHotkey KEY_MOVE_EVERYTHING = make("keyMoveEverything", "LEFT_ALT,LEFT_SHIFT,MOUSE_LEFT", GUI_NO_ORDER);

ConfigHotkey KEY_WS_MOVE_DOWN_LEAVE_ONE = make("wsMoveDownLeaveOne", "S,MOUSE_RIGHT", GUI_NO_ORDER);
ConfigHotkey KEY_WS_MOVE_DOWN_MATCHING  = make("wsMoveDownMatching", "LEFT_ALT,S,MOUSE_LEFT", GUI_NO_ORDER);
ConfigHotkey KEY_WS_MOVE_DOWN_SINGLE    = make("wsMoveDownSingle", "S,MOUSE_LEFT", GUI_NO_ORDER);
ConfigHotkey KEY_WS_MOVE_DOWN_STACKS    = make("wsMoveDownStacks", "LEFT_SHIFT,S,MOUSE_LEFT", GUI_NO_ORDER);
ConfigHotkey KEY_WS_MOVE_UP_LEAVE_ONE   = make("wsMoveUpLeaveOne", "W,MOUSE_RIGHT", GUI_NO_ORDER);
ConfigHotkey KEY_WS_MOVE_UP_MATCHING    = make("wsMoveUpMatching", "LEFT_ALT,W,MOUSE_LEFT", GUI_NO_ORDER);
ConfigHotkey KEY_WS_MOVE_UP_SINGLE      = make("wsMoveUpSingle", "W,MOUSE_LEFT", GUI_NO_ORDER);
ConfigHotkey KEY_WS_MOVE_UP_STACKS      = make("wsMoveUpStacks", "LEFT_SHIFT,W,MOUSE_LEFT", GUI_NO_ORDER);

ConfigHotkey MODIFIER_MOVE_EVERYTHING = make("modifierMoveEverything", "LEFT_ALT,LEFT_SHIFT", GUI_NO_ORDER);
ConfigHotkey MODIFIER_MOVE_MATCHING   = make("modifierMoveMatching", "LEFT_ALT", GUI_NO_ORDER);
ConfigHotkey MODIFIER_MOVE_STACK      = make("modifierMoveStack", "LEFT_SHIFT", GUI_NO_ORDER);

std::vector<ConfigHotkey*> const& HOTKEY_LIST() {
    static std::vector<ConfigHotkey*> const list{
        &TOGGLE_MOD_ON_OFF,

        &CRAFT_EVERYTHING,
        &DROP_ALL_MATCHING,
        &MASS_CRAFT,
        &MASS_CRAFT_TOGGLE,
        &MOVE_CRAFT_RESULTS,
        &RECIPE_VIEW,
        &SLOT_DEBUG,
        &STORE_RECIPE,
        &THROW_CRAFT_RESULTS,

        &MODIFIER_MOVE_EVERYTHING,
        &MODIFIER_MOVE_MATCHING,
        &MODIFIER_MOVE_STACK,

        &KEY_DRAG_FULL_STACKS,
        &KEY_DRAG_LEAVE_ONE,
        &KEY_DRAG_MATCHING,
        &KEY_DRAG_MOVE_ONE,

        &KEY_DRAG_DROP_LEAVE_ONE,
        &KEY_DRAG_DROP_SINGLE,
        &KEY_DRAG_DROP_STACKS,

        &KEY_MOVE_EVERYTHING,

        &KEY_WS_MOVE_DOWN_LEAVE_ONE,
        &KEY_WS_MOVE_DOWN_MATCHING,
        &KEY_WS_MOVE_DOWN_SINGLE,
        &KEY_WS_MOVE_DOWN_STACKS,
        &KEY_WS_MOVE_UP_LEAVE_ONE,
        &KEY_WS_MOVE_UP_MATCHING,
        &KEY_WS_MOVE_UP_SINGLE,
        &KEY_WS_MOVE_UP_STACKS,
    };
    return list;
}

}
