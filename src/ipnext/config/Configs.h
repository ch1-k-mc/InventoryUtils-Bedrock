// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "malilib/config/ConfigOptions.h"

namespace inventory_utils::ipnext::config {

using namespace malilib::config;

namespace Features {
extern ConfigBoolean ENABLE_SORTING_AND_MOVING;
extern ConfigBoolean ENABLE_LOCK_SLOTS;
extern ConfigBoolean ENABLE_AUTO_REFILL;
extern ConfigBoolean ENABLE_PROFILES;
extern ConfigBoolean VILLAGER_TRADING_ENABLE;
std::vector<IConfigBase*> const& OPTIONS();
}

namespace ModSettings {
extern ConfigHotkey  INCLUDE_HOTBAR_MODIFIER;
extern ConfigHotkey  MOVE_ALL_MODIFIER;
extern ConfigHotkey  MOVE_FOCUS_MACH_MODIFIER;
extern ConfigHotkey  MOVE_JUST_REFILL_MODIFIER;
extern ConfigBoolean ALWAYS_INCLUDE_HOTBAR;
extern ConfigBoolean ALWAYS_MOVE_ALL;
extern ConfigBoolean ALWAYS_THROW_ALL;
extern ConfigBoolean ENABLE_RANDOMIZER;
extern ConfigBoolean ADD_INTERVAL_BETWEEN_CLICKS;
extern ConfigInteger INTERVAL_BETWEEN_CLICKS_MS;
extern ConfigInteger AUTO_CRAFT_DELAY;
extern ConfigInteger CONTINUOUS_CRAFTING_METHOD;
extern ConfigBoolean HIGHLIGHT_CLICKING_SLOT;
extern ConfigBoolean MOVE_ALL_AT_CURSOR;
extern ConfigBoolean STOP_AT_SCREEN_CLOSE;
extern ConfigBoolean IGNORE_DURABILITY;
extern ConfigBoolean DEBUG;
extern ConfigBoolean VILLAGER_TRADING_GROUP_1;
extern ConfigBoolean VILLAGER_TRADING_GROUP_2;
extern ConfigInteger VILLAGER_TRADING_LOCAL_COLOR;
extern ConfigInteger VILLAGER_TRADING_GLOBAL_COLOR;
extern ConfigInteger VILLAGER_TRADING_LOCAL_COLOR1;
extern ConfigInteger VILLAGER_TRADING_GLOBAL_COLOR1;
extern ConfigInteger VILLAGER_TRADING_LOCAL_COLOR2;
extern ConfigInteger VILLAGER_TRADING_GLOBAL_COLOR2;
std::vector<IConfigBase*> const& OPTIONS();
}

namespace SortSettings {
extern ConfigOptionList SORT_ORDER;
extern ConfigString     CUSTOM_RULE;
extern ConfigBoolean    RESTOCK_HOTBAR;
extern ConfigBoolean    SORT_AT_CURSOR;
extern ConfigBoolean    TREAT_NON_STACKABLE_AS_FULL_STACKS;
extern ConfigBoolean    INVERT_BUNDLES_CLICKS;
extern ConfigBoolean    INVERT_SHULKER_CLICKS;
std::vector<IConfigBase*> const& OPTIONS();
}

namespace AutoRefillSettings {
extern ConfigBoolean    DISABLE_FOR_DROP_ITEM;
extern ConfigBoolean    IGNORE_LOCKED_SLOTS;
extern ConfigBoolean    AUTO_REFILL_PREFER_SMALLER_STACKS;
extern ConfigBoolean    REFILL_ARMOR;
extern ConfigBoolean    REFILL_BEFORE_TOOL_BREAK;
extern ConfigString     AUTOREFILL_BLACKLIST;
extern ConfigInteger    STACKABLE_THRESHOLD;
extern ConfigInteger    TOOL_DAMAGE_THRESHOLD;
extern ConfigOptionList THRESHOLD_UNIT;
extern ConfigInteger    AUTO_REFILL_WAIT_TICK;
extern ConfigHotkey     AUTO_REFILL_TEMP_DISABLE_REFILL_FOR_TOOLS;
extern ConfigBoolean    AUTO_REFILL_ENABLE_PER_SLOT_CONFIG;
extern ConfigBoolean    AUTO_REFILL_ENABLE_INDICATOR_ICONS;
extern ConfigBoolean    AUTO_REFILL_ENABLE_HORBAR_INDICATOR_ICONS;
extern ConfigBoolean    DISABLE_FOR_LOYALTY_ITEMS;
extern ConfigBoolean    AUTO_REFILL_MATCH_CUSTOM_NAME;
extern ConfigBoolean    AUTO_REFILL_MATCH_NBT;
extern ConfigOptionList AUTO_REFILL_MATCH_NBT_TYPE;
extern ConfigBoolean    AUTO_REFILL_IGNORE_NBT_FOR_BUCKETS;
extern ConfigBoolean    AUTO_REFILL_MATCH_ANY_FOOD;
extern ConfigBoolean    AUTO_REFILL_MATCH_HARMFUL_FOOD;
extern ConfigBoolean    ALLOW_BREAK_FOR_NON_ENCHANTED;
extern ConfigInteger    TOOL_MAX_DURABILITY_THRESHOLD;
extern ConfigBoolean    ALLOW_ALERTS_WITHOUT_TOOL_PROTECTION;
extern ConfigBoolean    VISUAL_DURABILITY_NOTIFICATION;
extern ConfigOptionList TYPE_VISUAL_DURABILITY_NOTIFICATION;
extern ConfigBoolean    AUDIO_DURABILITY_NOTIFICATION;
extern ConfigInteger    NUMBER_OF_NOTIFICATIONS;
extern ConfigInteger    NOTIFICATION_STEP;
extern ConfigBoolean    VISUAL_REPLACE_SUCCESS_NOTIFICATION;
extern ConfigBoolean    AUDIO_REPLACE_SUCCESS_NOTIFICATION;
extern ConfigOptionList TYPE_VISUAL_REPLACE_SUCCESS_NOTIFICATION;
extern ConfigBoolean    VISUAL_REPLACE_FAILED_NOTIFICATION;
extern ConfigBoolean    AUDIO_REPLACE_FAILED_NOTIFICATION;
extern ConfigOptionList TYPE_VISUAL_REPLACE_FAILED_NOTIFICATION;
std::vector<IConfigBase*> const& OPTIONS();
}

namespace LockedSlotsSettings {
extern ConfigBoolean    ENABLE_LOCK_SLOTS_PER_SERVER;
extern ConfigBoolean    LOCKED_SLOTS_DISABLE_QUICK_MOVE_THROW;
extern ConfigBoolean    LOCKED_SLOTS_DISABLE_THROW_FOR_NON_STACKABLE;
extern ConfigBoolean    LOCK_SLOTS_DISABLE_USER_INTERACTION;
extern ConfigBoolean    LOCKED_SLOTS_ALLOW_PICKUP_INTO_EMPTY;
extern ConfigInteger    LOCKED_SLOTS_DELAY_KEEPER_REINIT_TICKS;
extern ConfigBoolean    LOCKED_SLOTS_EMPTY_HOTBAR_AS_SEMI_LOCKED;
extern ConfigString     LOCKED_SLOTS_EMPTY_HOTBAR_BLACKLIST;
extern ConfigHotkey     LOCK_SLOTS_SWITCH_CONFIG_MODIFIER;
extern ConfigHotkey     LOCK_SLOTS_CONFIG_KEY;
extern ConfigHotkey     LOCK_SLOTS_QUICK_CONFIG_KEY;
extern ConfigHotkey     LOCK_SLOTS_QUICK_DISABLE;
extern ConfigOptionList LOCK_SLOTS_CONFIG_SWITCH_TYPE;
extern ConfigBoolean    SHOW_LOCKED_SLOTS_BACKGROUND;
extern ConfigBoolean    SHOW_LOCKED_SLOTS_FOREGROUND;
extern ConfigInteger    SHOW_LOCKED_SLOTS_BG_COLOR;
extern ConfigInteger    SHOW_LOCKED_SLOTS_HOTBAR_COLOR;
extern ConfigBoolean    ALSO_SHOW_LOCKED_SLOTS_IN_HOTBAR;
extern ConfigInteger    LOCKED_SLOTS_FOREGROUND_STYLE;
std::vector<IConfigBase*> const& OPTIONS();
}

namespace GuiSettings {
extern ConfigBoolean    INVENTORY_OVERLAY_BUTTONS_VISIBLE;
extern ConfigBoolean    ENABLE_PROFILES_ANNOUNCEMENT;
extern ConfigBoolean    ENABLE_INVENTORY_BUTTONS;
extern ConfigBoolean    SHOW_BUTTONS_BOTH_SIDES;
extern ConfigBoolean    SHOW_CONTINUOUS_CRAFTING_CHECKBOX;
extern ConfigOptionList CONTINUOUS_CRAFTING_CHECKBOX_VALUE;
extern ConfigBoolean    CONTINUOUS_CRAFTING_SAVED_VALUE;
extern ConfigBoolean    SHOW_FAST_RENAME_CHECKBOX;
extern ConfigOptionList FAST_RENAME_CHECKBOX_VALUE;
extern ConfigBoolean    FAST_RENAME_SAVED_VALUE;
extern ConfigBoolean    SHOW_REGULAR_SORT_BUTTON;
extern ConfigOptionList REGULAR_POST_ACTION;
extern ConfigOptionList REGULAR_SORT_ORDER;
extern ConfigString     REGULAR_CUSTOM_RULE;
extern ConfigBoolean    CHANGE_SORT_ORDER_WITH_SCROLL_WHEEL;
extern ConfigBoolean    SHOW_SORT_IN_COLUMNS_BUTTON;
extern ConfigOptionList IN_COLUMNS_POST_ACTION;
extern ConfigOptionList IN_COLUMNS_SORT_ORDER;
extern ConfigString     IN_COLUMNS_CUSTOM_RULE;
extern ConfigBoolean    SHOW_SORT_IN_ROWS_BUTTON;
extern ConfigOptionList IN_ROWS_POST_ACTION;
extern ConfigOptionList IN_ROWS_SORT_ORDER;
extern ConfigString     IN_ROWS_CUSTOM_RULE;
extern ConfigBoolean    SHOW_MOVE_ALL_BUTTON;
extern ConfigBoolean    SHOW_BUTTON_TOOLTIPS;
std::vector<IConfigBase*> const& OPTIONS();
}

namespace EditProfiles {
extern ConfigString  QUICK_SLOT_PROFILES[10];
extern ConfigBoolean INCLUDE_CUSTOM_NAME;
extern ConfigBoolean PROFILES_PER_SERVER;
std::vector<IConfigBase*> const& OPTIONS();
}

namespace Hotkeys {
extern ConfigHotkey OPEN_CONFIG_MENU;
extern ConfigHotkey RELOAD_CUSTOM_CONFIGS;
extern ConfigHotkey SORT_INVENTORY;
extern ConfigHotkey SORT_INVENTORY_IN_COLUMNS;
extern ConfigHotkey SORT_INVENTORY_IN_ROWS;
extern ConfigHotkey MOVE_ALL_ITEMS;
extern ConfigHotkey THROW_ALL_ITEMS;
extern ConfigHotkey AUTO_REFILL_GUI_TOGGLE_FOR_SLOT;
extern ConfigHotkey AUTO_REFILL_GAME_TOGGLE_FOR_SLOT;
extern ConfigHotkey APPLY_PROFILE;
extern ConfigHotkey NEXT_PROFILE;
extern ConfigHotkey PREV_PROFILE;
extern ConfigHotkey PROFILE_KEYS[10];
extern ConfigHotkey SAVE_AS_PROFILE;
extern ConfigHotkey GLOBAL_BOOKMARK_TRADE[3];
extern ConfigHotkey LOCAL_BOOKMARK_TRADE[3];
extern ConfigHotkey DO_GLOBAL_TRADE[3];
extern ConfigHotkey DO_LOCAL_TRADE[3];
std::vector<IConfigBase*> const& OPTIONS();
std::vector<ConfigHotkey*> const& HOTKEY_LIST();
}

namespace Debugs {
extern ConfigBoolean    TRACE_LOGS;
extern ConfigBoolean    INSANE_TRACE_LOGS;
extern ConfigBoolean    FORCE_NO_CLEAN_CURSOR;
extern ConfigOptionList DIFF_CALCULATOR;
extern ConfigHotkey     CLEAN_CURSOR;
std::vector<IConfigBase*> const& OPTIONS();
}

bool isPressing(ConfigHotkey const& hotkey);

std::vector<std::pair<std::string, std::vector<IConfigBase*>>> categories();
void loadFromFile();
void saveToFile();
std::filesystem::path configDir();

}
