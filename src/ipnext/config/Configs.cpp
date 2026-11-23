// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/config/Configs.h"

#include <algorithm>
#include <climits>
#include <filesystem>
#include <fstream>

#include "ll/api/mod/NativeMod.h"

#include "malilib/hotkeys/KeybindMulti.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace inventory_utils::ipnext::config {

namespace {

using malilib::hotkeys::KeyAction;
using malilib::hotkeys::KeybindSettings;
using Ctx = KeybindSettings::Context;

constexpr char const* CATEGORY_KEY = "inventoryprofiles.config";

KeybindSettings const INGAME_DEFAULT = KeybindSettings::create(Ctx::INGAME, KeyAction::PRESS, false, true, false, true);
KeybindSettings const GUI_DEFAULT    = KeybindSettings::create(Ctx::GUI, KeyAction::PRESS, false, true, false, true);
KeybindSettings const ANY_DEFAULT    = KeybindSettings::create(Ctx::ANY, KeyAction::PRESS, false, true, false, true);
KeybindSettings const GUI_EXTRA      = KeybindSettings::create(Ctx::GUI, KeyAction::PRESS, true, true, false, true);
KeybindSettings const INGAME_EXTRA   = KeybindSettings::create(Ctx::INGAME, KeyAction::PRESS, true, true, false, true);
KeybindSettings const INGAME_EXTRA_NO_ORDER = KeybindSettings::create(Ctx::INGAME, KeyAction::PRESS, true, false, false, true);

template <class T>
T applied(T option, char const* category) {
    option.apply(std::string(CATEGORY_KEY) + "." + category);
    return option;
}

std::vector<std::string> const SORTING_METHOD{"DEFAULT", "ITEM_NAME", "ITEM_ID", "RAW_ID", "ACCUMULATED_COUNT_DESCENDING", "ACCUMULATED_COUNT_ASCENDING", "CUSTOM"};
std::vector<std::string> const SORTING_METHOD_INDIVIDUAL{"GLOBAL", "DEFAULT", "ITEM_NAME", "ITEM_ID", "RAW_ID", "CUSTOM"};
std::vector<std::string> const POST_ACTION{"NONE", "GROUP_IN_ROWS", "GROUP_IN_COLUMNS", "DISTRIBUTE_EVENLY", "SHUFFLE", "FILL_ONE", "REVERSE"};
std::vector<std::string> const DIFF_CALCULATOR_TYPE{"SIMPLE", "SCORE_BASED_SINGLE", "SCORE_BASED_DUAL"};

#define IU_BOOL(CAT, NAME, DEF) ConfigBoolean NAME = applied(ConfigBoolean(lower(#NAME), DEF), CAT)
#define IU_INT(CAT, NAME, DEF, MIN, MAX) ConfigInteger NAME = applied(ConfigInteger(lower(#NAME), DEF, MIN, MAX), CAT)
#define IU_STR(CAT, NAME, DEF) ConfigString NAME = applied(ConfigString(lower(#NAME), DEF), CAT)
#define IU_ENUM(CAT, NAME, VALUES, DEF) ConfigOptionList NAME = applied(ConfigOptionList(lower(#NAME), VALUES, DEF), CAT)
#define IU_HOTKEY(CAT, NAME, KEYS, SETTINGS) ConfigHotkey NAME = applied(ConfigHotkey(lower(#NAME), KEYS, SETTINGS), CAT)

int normalizeModifier(int c) {
    switch (c) {
    case VK_RSHIFT:
        return VK_LSHIFT;
    case VK_RCONTROL:
        return VK_LCONTROL;
    case VK_RMENU:
        return VK_LMENU;
    case VK_RWIN:
        return VK_LWIN;
    default:
        return c;
    }
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

}

namespace Features {
IU_BOOL("features", ENABLE_SORTING_AND_MOVING, true);
IU_BOOL("features", ENABLE_LOCK_SLOTS, true);
IU_BOOL("features", ENABLE_AUTO_REFILL, true);
IU_BOOL("features", ENABLE_PROFILES, true);
IU_BOOL("features", VILLAGER_TRADING_ENABLE, true);
std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list{&ENABLE_SORTING_AND_MOVING, &ENABLE_LOCK_SLOTS, &ENABLE_AUTO_REFILL, &ENABLE_PROFILES, &VILLAGER_TRADING_ENABLE};
    return list;
}
}

namespace ModSettings {
IU_HOTKEY("move_matching_items", INCLUDE_HOTBAR_MODIFIER, "LEFT_ALT", GUI_EXTRA);
IU_HOTKEY("move_matching_items", MOVE_ALL_MODIFIER, "LEFT_SHIFT", GUI_EXTRA);
IU_HOTKEY("move_matching_items", MOVE_FOCUS_MACH_MODIFIER, "LEFT_CONTROL", GUI_EXTRA);
IU_HOTKEY("move_matching_items", MOVE_JUST_REFILL_MODIFIER, "CAPS_LOCK", GUI_EXTRA);
IU_BOOL("move_matching_items", ALWAYS_INCLUDE_HOTBAR, false);
IU_BOOL("move_matching_items", ALWAYS_MOVE_ALL, false);
IU_BOOL("move_matching_items", ALWAYS_THROW_ALL, false);
IU_BOOL("randomizer", ENABLE_RANDOMIZER, true);
IU_BOOL("advanced_options", ADD_INTERVAL_BETWEEN_CLICKS, false);
IU_INT("advanced_options", INTERVAL_BETWEEN_CLICKS_MS, 10, 1, 500);
IU_INT("advanced_options", AUTO_CRAFT_DELAY, 1, 1, 20);
IU_INT("advanced_options", CONTINUOUS_CRAFTING_METHOD, 1, 1, 2);
IU_BOOL("advanced_options", HIGHLIGHT_CLICKING_SLOT, true);
IU_BOOL("advanced_options", MOVE_ALL_AT_CURSOR, true);
IU_BOOL("advanced_options", STOP_AT_SCREEN_CLOSE, false);
IU_BOOL("advanced_options", IGNORE_DURABILITY, true);
IU_BOOL("debugs", DEBUG, false);
IU_BOOL("villager_trading", VILLAGER_TRADING_GROUP_1, false);
IU_BOOL("villager_trading", VILLAGER_TRADING_GROUP_2, false);
IU_INT("villager_trading", VILLAGER_TRADING_LOCAL_COLOR, static_cast<int>(0x8296010Bu), INT_MIN, INT_MAX);
IU_INT("villager_trading", VILLAGER_TRADING_GLOBAL_COLOR, static_cast<int>(0x8201960Bu), INT_MIN, INT_MAX);
IU_INT("villager_trading", VILLAGER_TRADING_LOCAL_COLOR1, static_cast<int>(0x82353FA2u), INT_MIN, INT_MAX);
IU_INT("villager_trading", VILLAGER_TRADING_GLOBAL_COLOR1, static_cast<int>(0x82FF763Du), INT_MIN, INT_MAX);
IU_INT("villager_trading", VILLAGER_TRADING_LOCAL_COLOR2, static_cast<int>(0x82F6DF65u), INT_MIN, INT_MAX);
IU_INT("villager_trading", VILLAGER_TRADING_GLOBAL_COLOR2, static_cast<int>(0x82D774EBu), INT_MIN, INT_MAX);
std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list{
        &INCLUDE_HOTBAR_MODIFIER,
        &MOVE_ALL_MODIFIER,
        &MOVE_FOCUS_MACH_MODIFIER,
        &MOVE_JUST_REFILL_MODIFIER,
        &ALWAYS_INCLUDE_HOTBAR,
        &ALWAYS_MOVE_ALL,
        &ALWAYS_THROW_ALL,
        &ENABLE_RANDOMIZER,
        &ADD_INTERVAL_BETWEEN_CLICKS,
        &INTERVAL_BETWEEN_CLICKS_MS,
        &AUTO_CRAFT_DELAY,
        &CONTINUOUS_CRAFTING_METHOD,
        &HIGHLIGHT_CLICKING_SLOT,
        &MOVE_ALL_AT_CURSOR,
        &STOP_AT_SCREEN_CLOSE,
        &IGNORE_DURABILITY,
        &DEBUG,
        &VILLAGER_TRADING_GROUP_1,
        &VILLAGER_TRADING_GROUP_2,
        &VILLAGER_TRADING_LOCAL_COLOR,
        &VILLAGER_TRADING_GLOBAL_COLOR,
        &VILLAGER_TRADING_LOCAL_COLOR1,
        &VILLAGER_TRADING_GLOBAL_COLOR1,
        &VILLAGER_TRADING_LOCAL_COLOR2,
        &VILLAGER_TRADING_GLOBAL_COLOR2,
    };
    return list;
}
}

namespace SortSettings {
IU_ENUM("inventory_sorting", SORT_ORDER, SORTING_METHOD, "DEFAULT");
IU_STR("inventory_sorting", CUSTOM_RULE, "@custom");
IU_BOOL("inventory_sorting", RESTOCK_HOTBAR, false);
IU_BOOL("inventory_sorting", SORT_AT_CURSOR, true);
IU_BOOL("accumulated_count", TREAT_NON_STACKABLE_AS_FULL_STACKS, false);
IU_BOOL("bundles_and_shulker_boxes", INVERT_BUNDLES_CLICKS, false);
IU_BOOL("bundles_and_shulker_boxes", INVERT_SHULKER_CLICKS, false);
std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list{
        &SORT_ORDER,
        &CUSTOM_RULE,
        &RESTOCK_HOTBAR,
        &SORT_AT_CURSOR,
        &TREAT_NON_STACKABLE_AS_FULL_STACKS,
        &INVERT_BUNDLES_CLICKS,
        &INVERT_SHULKER_CLICKS,
    };
    return list;
}
}

namespace AutoRefillSettings {
std::vector<std::string> const THRESHOLD_UNITS{"ABSOLUTE", "PERCENTAGE"};
std::vector<std::string> const NBT_MATCH_TYPES{"EXACT", "CAN_HAVE_EXTRA"};
std::vector<std::string> const NOTIFICATION_TYPES{"SUBTITLE", "HOTBAR", "CHAT"};
IU_BOOL("auto-refill.general", DISABLE_FOR_DROP_ITEM, false);
IU_BOOL("auto-refill.general", IGNORE_LOCKED_SLOTS, true);
IU_BOOL("auto-refill.general", AUTO_REFILL_PREFER_SMALLER_STACKS, false);
IU_BOOL("auto-refill.general", REFILL_ARMOR, true);
IU_BOOL("auto-refill.general", REFILL_BEFORE_TOOL_BREAK, true);
IU_STR("auto-refill.general", AUTOREFILL_BLACKLIST, "");
IU_INT("auto-refill.general", STACKABLE_THRESHOLD, 0, 0, 64);
IU_INT("auto-refill.general", TOOL_DAMAGE_THRESHOLD, 10, 0, 100);
IU_ENUM("auto-refill.general", THRESHOLD_UNIT, THRESHOLD_UNITS, "ABSOLUTE");
IU_INT("auto-refill.general", AUTO_REFILL_WAIT_TICK, 0, 0, 100);
IU_HOTKEY("auto-refill.general", AUTO_REFILL_TEMP_DISABLE_REFILL_FOR_TOOLS, "LEFT_ALT", INGAME_EXTRA);
IU_BOOL("auto-refill.general", AUTO_REFILL_ENABLE_PER_SLOT_CONFIG, true);
IU_BOOL("auto-refill.general", AUTO_REFILL_ENABLE_INDICATOR_ICONS, true);
IU_BOOL("auto-refill.general", AUTO_REFILL_ENABLE_HORBAR_INDICATOR_ICONS, true);
IU_BOOL("auto-refill.matching", DISABLE_FOR_LOYALTY_ITEMS, true);
IU_BOOL("auto-refill.matching", AUTO_REFILL_MATCH_CUSTOM_NAME, true);
IU_BOOL("auto-refill.matching", AUTO_REFILL_MATCH_NBT, true);
IU_ENUM("auto-refill.matching", AUTO_REFILL_MATCH_NBT_TYPE, NBT_MATCH_TYPES, "EXACT");
IU_BOOL("auto-refill.matching", AUTO_REFILL_IGNORE_NBT_FOR_BUCKETS, true);
IU_BOOL("auto-refill.matching", AUTO_REFILL_MATCH_ANY_FOOD, false);
IU_BOOL("auto-refill.matching", AUTO_REFILL_MATCH_HARMFUL_FOOD, false);
IU_BOOL("auto-refill.non-enchanted", ALLOW_BREAK_FOR_NON_ENCHANTED, false);
IU_INT("auto-refill.non-enchanted", TOOL_MAX_DURABILITY_THRESHOLD, 500, 0, 5000);
IU_BOOL("auto-refill.notifications", ALLOW_ALERTS_WITHOUT_TOOL_PROTECTION, false);
IU_BOOL("auto-refill.notifications", VISUAL_DURABILITY_NOTIFICATION, true);
IU_ENUM("auto-refill.notifications", TYPE_VISUAL_DURABILITY_NOTIFICATION, NOTIFICATION_TYPES, "HOTBAR");
IU_BOOL("auto-refill.notifications", AUDIO_DURABILITY_NOTIFICATION, true);
IU_INT("auto-refill.notifications", NUMBER_OF_NOTIFICATIONS, 3, 1, 10);
IU_INT("auto-refill.notifications", NOTIFICATION_STEP, 5, 1, 5);
IU_BOOL("auto-refill.notifications", VISUAL_REPLACE_SUCCESS_NOTIFICATION, true);
IU_BOOL("auto-refill.notifications", AUDIO_REPLACE_SUCCESS_NOTIFICATION, true);
IU_ENUM("auto-refill.notifications", TYPE_VISUAL_REPLACE_SUCCESS_NOTIFICATION, NOTIFICATION_TYPES, "HOTBAR");
IU_BOOL("auto-refill.notifications", VISUAL_REPLACE_FAILED_NOTIFICATION, true);
IU_BOOL("auto-refill.notifications", AUDIO_REPLACE_FAILED_NOTIFICATION, true);
IU_ENUM("auto-refill.notifications", TYPE_VISUAL_REPLACE_FAILED_NOTIFICATION, NOTIFICATION_TYPES, "SUBTITLE");
std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list{
        &DISABLE_FOR_DROP_ITEM, &IGNORE_LOCKED_SLOTS, &AUTO_REFILL_PREFER_SMALLER_STACKS, &REFILL_ARMOR, &REFILL_BEFORE_TOOL_BREAK,
        &AUTOREFILL_BLACKLIST, &STACKABLE_THRESHOLD, &TOOL_DAMAGE_THRESHOLD, &THRESHOLD_UNIT, &AUTO_REFILL_WAIT_TICK,
        &AUTO_REFILL_TEMP_DISABLE_REFILL_FOR_TOOLS, &AUTO_REFILL_ENABLE_PER_SLOT_CONFIG, &AUTO_REFILL_ENABLE_INDICATOR_ICONS,
        &AUTO_REFILL_ENABLE_HORBAR_INDICATOR_ICONS, &DISABLE_FOR_LOYALTY_ITEMS, &AUTO_REFILL_MATCH_CUSTOM_NAME, &AUTO_REFILL_MATCH_NBT,
        &AUTO_REFILL_MATCH_NBT_TYPE, &AUTO_REFILL_IGNORE_NBT_FOR_BUCKETS, &AUTO_REFILL_MATCH_ANY_FOOD, &AUTO_REFILL_MATCH_HARMFUL_FOOD,
        &ALLOW_BREAK_FOR_NON_ENCHANTED, &TOOL_MAX_DURABILITY_THRESHOLD, &ALLOW_ALERTS_WITHOUT_TOOL_PROTECTION, &VISUAL_DURABILITY_NOTIFICATION,
        &TYPE_VISUAL_DURABILITY_NOTIFICATION, &AUDIO_DURABILITY_NOTIFICATION, &NUMBER_OF_NOTIFICATIONS, &NOTIFICATION_STEP,
        &VISUAL_REPLACE_SUCCESS_NOTIFICATION, &AUDIO_REPLACE_SUCCESS_NOTIFICATION, &TYPE_VISUAL_REPLACE_SUCCESS_NOTIFICATION,
        &VISUAL_REPLACE_FAILED_NOTIFICATION, &AUDIO_REPLACE_FAILED_NOTIFICATION, &TYPE_VISUAL_REPLACE_FAILED_NOTIFICATION,
    };
    return list;
}
}

namespace LockedSlotsSettings {
std::vector<std::string> const SWITCH_TYPE{"TOGGLE", "HOLD"};
IU_BOOL("lock_slots.general", ENABLE_LOCK_SLOTS_PER_SERVER, true);
IU_BOOL("lock_slots.general", LOCKED_SLOTS_DISABLE_QUICK_MOVE_THROW, true);
IU_BOOL("lock_slots.general", LOCKED_SLOTS_DISABLE_THROW_FOR_NON_STACKABLE, true);
IU_BOOL("lock_slots.general", LOCK_SLOTS_DISABLE_USER_INTERACTION, false);
IU_BOOL("lock_slots.empty", LOCKED_SLOTS_ALLOW_PICKUP_INTO_EMPTY, false);
IU_INT("lock_slots.empty", LOCKED_SLOTS_DELAY_KEEPER_REINIT_TICKS, 50, 20, 400);
IU_BOOL("lock_slots.empty", LOCKED_SLOTS_EMPTY_HOTBAR_AS_SEMI_LOCKED, false);
IU_STR("lock_slots.empty", LOCKED_SLOTS_EMPTY_HOTBAR_BLACKLIST, "carryon");
IU_HOTKEY("hotkeys", LOCK_SLOTS_SWITCH_CONFIG_MODIFIER, "LEFT_ALT", GUI_EXTRA);
IU_HOTKEY("hotkeys", LOCK_SLOTS_CONFIG_KEY, "BUTTON_1", GUI_EXTRA);
IU_HOTKEY("hotkeys", LOCK_SLOTS_QUICK_CONFIG_KEY, "", GUI_EXTRA);
IU_HOTKEY("hotkeys", LOCK_SLOTS_QUICK_DISABLE, "", GUI_EXTRA);
IU_ENUM("hotkeys", LOCK_SLOTS_CONFIG_SWITCH_TYPE, SWITCH_TYPE, "HOLD");
IU_BOOL("lock_slots.gui", SHOW_LOCKED_SLOTS_BACKGROUND, true);
IU_BOOL("lock_slots.gui", SHOW_LOCKED_SLOTS_FOREGROUND, true);
IU_INT("lock_slots.gui", SHOW_LOCKED_SLOTS_BG_COLOR, static_cast<int>(0xC0FF5555u), INT_MIN, INT_MAX);
IU_INT("lock_slots.gui", SHOW_LOCKED_SLOTS_HOTBAR_COLOR, static_cast<int>(0x5EFF5E41u), INT_MIN, INT_MAX);
IU_BOOL("lock_slots.gui", ALSO_SHOW_LOCKED_SLOTS_IN_HOTBAR, true);
IU_INT("lock_slots.gui", LOCKED_SLOTS_FOREGROUND_STYLE, 2, 1, 6);
std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list{
        &ENABLE_LOCK_SLOTS_PER_SERVER,
        &LOCKED_SLOTS_DISABLE_QUICK_MOVE_THROW,
        &LOCKED_SLOTS_DISABLE_THROW_FOR_NON_STACKABLE,
        &LOCK_SLOTS_DISABLE_USER_INTERACTION,
        &LOCKED_SLOTS_ALLOW_PICKUP_INTO_EMPTY,
        &LOCKED_SLOTS_DELAY_KEEPER_REINIT_TICKS,
        &LOCKED_SLOTS_EMPTY_HOTBAR_AS_SEMI_LOCKED,
        &LOCKED_SLOTS_EMPTY_HOTBAR_BLACKLIST,
        &LOCK_SLOTS_SWITCH_CONFIG_MODIFIER,
        &LOCK_SLOTS_CONFIG_KEY,
        &LOCK_SLOTS_QUICK_CONFIG_KEY,
        &LOCK_SLOTS_QUICK_DISABLE,
        &LOCK_SLOTS_CONFIG_SWITCH_TYPE,
        &SHOW_LOCKED_SLOTS_BACKGROUND,
        &SHOW_LOCKED_SLOTS_FOREGROUND,
        &SHOW_LOCKED_SLOTS_BG_COLOR,
        &SHOW_LOCKED_SLOTS_HOTBAR_COLOR,
        &ALSO_SHOW_LOCKED_SLOTS_IN_HOTBAR,
        &LOCKED_SLOTS_FOREGROUND_STYLE,
    };
    return list;
}
}

namespace GuiSettings {
IU_BOOL("gui.general", INVENTORY_OVERLAY_BUTTONS_VISIBLE, true);
IU_BOOL("profiles", ENABLE_PROFILES_ANNOUNCEMENT, true);
IU_BOOL("inventory", ENABLE_INVENTORY_BUTTONS, true);
IU_BOOL("inventory", SHOW_BUTTONS_BOTH_SIDES, false);
IU_BOOL("inventory", SHOW_CONTINUOUS_CRAFTING_CHECKBOX, true);
IU_ENUM("inventory", CONTINUOUS_CRAFTING_CHECKBOX_VALUE, (std::vector<std::string>{"REMEMBER", "CHECKED", "UNCHECKED"}), "REMEMBER");
IU_BOOL("inventory", CONTINUOUS_CRAFTING_SAVED_VALUE, true);
IU_BOOL("inventory", SHOW_FAST_RENAME_CHECKBOX, true);
IU_ENUM("inventory", FAST_RENAME_CHECKBOX_VALUE, (std::vector<std::string>{"REMEMBER", "CHECKED", "UNCHECKED"}), "UNCHECKED");
IU_BOOL("inventory", FAST_RENAME_SAVED_VALUE, false);
IU_BOOL("inventory", SHOW_REGULAR_SORT_BUTTON, true);
IU_ENUM("inventory", REGULAR_POST_ACTION, POST_ACTION, "NONE");
IU_ENUM("inventory", REGULAR_SORT_ORDER, SORTING_METHOD_INDIVIDUAL, "GLOBAL");
IU_STR("inventory", REGULAR_CUSTOM_RULE, "@custom");
IU_BOOL("inventory", CHANGE_SORT_ORDER_WITH_SCROLL_WHEEL, true);
IU_BOOL("inventory", SHOW_SORT_IN_COLUMNS_BUTTON, true);
IU_ENUM("inventory", IN_COLUMNS_POST_ACTION, POST_ACTION, "GROUP_IN_COLUMNS");
IU_ENUM("inventory", IN_COLUMNS_SORT_ORDER, SORTING_METHOD_INDIVIDUAL, "GLOBAL");
IU_STR("inventory", IN_COLUMNS_CUSTOM_RULE, "@custom");
IU_BOOL("inventory", SHOW_SORT_IN_ROWS_BUTTON, true);
IU_ENUM("inventory", IN_ROWS_POST_ACTION, POST_ACTION, "GROUP_IN_ROWS");
IU_ENUM("inventory", IN_ROWS_SORT_ORDER, SORTING_METHOD_INDIVIDUAL, "GLOBAL");
IU_STR("inventory", IN_ROWS_CUSTOM_RULE, "@custom");
IU_BOOL("inventory", SHOW_MOVE_ALL_BUTTON, true);
IU_BOOL("inventory", SHOW_BUTTON_TOOLTIPS, true);
std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list{
        &INVENTORY_OVERLAY_BUTTONS_VISIBLE,
        &ENABLE_PROFILES_ANNOUNCEMENT,
        &ENABLE_INVENTORY_BUTTONS,
        &SHOW_BUTTONS_BOTH_SIDES,
        &SHOW_CONTINUOUS_CRAFTING_CHECKBOX,
        &CONTINUOUS_CRAFTING_CHECKBOX_VALUE,
        &CONTINUOUS_CRAFTING_SAVED_VALUE,
        &SHOW_FAST_RENAME_CHECKBOX,
        &FAST_RENAME_CHECKBOX_VALUE,
        &FAST_RENAME_SAVED_VALUE,
        &SHOW_REGULAR_SORT_BUTTON,
        &REGULAR_POST_ACTION,
        &REGULAR_SORT_ORDER,
        &REGULAR_CUSTOM_RULE,
        &CHANGE_SORT_ORDER_WITH_SCROLL_WHEEL,
        &SHOW_SORT_IN_COLUMNS_BUTTON,
        &IN_COLUMNS_POST_ACTION,
        &IN_COLUMNS_SORT_ORDER,
        &IN_COLUMNS_CUSTOM_RULE,
        &SHOW_SORT_IN_ROWS_BUTTON,
        &IN_ROWS_POST_ACTION,
        &IN_ROWS_SORT_ORDER,
        &IN_ROWS_CUSTOM_RULE,
        &SHOW_MOVE_ALL_BUTTON,
        &SHOW_BUTTON_TOOLTIPS,
    };
    return list;
}
}

namespace EditProfiles {
ConfigString QUICK_SLOT_PROFILES[10]{
    applied(ConfigString("quick_slot_1_profile", ""), "quick_slots"), applied(ConfigString("quick_slot_2_profile", ""), "quick_slots"),
    applied(ConfigString("quick_slot_3_profile", ""), "quick_slots"), applied(ConfigString("quick_slot_4_profile", ""), "quick_slots"),
    applied(ConfigString("quick_slot_5_profile", ""), "quick_slots"), applied(ConfigString("quick_slot_6_profile", ""), "quick_slots"),
    applied(ConfigString("quick_slot_7_profile", ""), "quick_slots"), applied(ConfigString("quick_slot_8_profile", ""), "quick_slots"),
    applied(ConfigString("quick_slot_9_profile", ""), "quick_slots"), applied(ConfigString("quick_slot_10_profile", ""), "quick_slots"),
};
IU_BOOL("still_under_construction", INCLUDE_CUSTOM_NAME, false);
IU_BOOL("still_under_construction", PROFILES_PER_SERVER, true);
std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list = [] {
        std::vector<IConfigBase*> v;
        for (auto& q : QUICK_SLOT_PROFILES) v.push_back(&q);
        v.push_back(&INCLUDE_CUSTOM_NAME);
        v.push_back(&PROFILES_PER_SERVER);
        return v;
    }();
    return list;
}
}

namespace Hotkeys {
IU_HOTKEY("hotkeys", OPEN_CONFIG_MENU, "R,C", INGAME_DEFAULT);
IU_HOTKEY("hotkeys", RELOAD_CUSTOM_CONFIGS, "R,Y", ANY_DEFAULT);
IU_HOTKEY("inventory", SORT_INVENTORY, "R", GUI_DEFAULT);
IU_HOTKEY("inventory", SORT_INVENTORY_IN_COLUMNS, "", GUI_DEFAULT);
IU_HOTKEY("inventory", SORT_INVENTORY_IN_ROWS, "", GUI_DEFAULT);
IU_HOTKEY("inventory", MOVE_ALL_ITEMS, "R,T", GUI_EXTRA);
IU_HOTKEY("inventory", THROW_ALL_ITEMS, "R,M", GUI_EXTRA);
IU_HOTKEY("auto_refill", AUTO_REFILL_GUI_TOGGLE_FOR_SLOT, "LEFT_CONTROL,BUTTON_1", ANY_DEFAULT);
IU_HOTKEY("auto_refill", AUTO_REFILL_GAME_TOGGLE_FOR_SLOT, "LEFT_CONTROL,R", INGAME_EXTRA_NO_ORDER);
IU_HOTKEY("profiles", APPLY_PROFILE, "R,H", INGAME_DEFAULT);
IU_HOTKEY("profiles", NEXT_PROFILE, "R,Z", INGAME_DEFAULT);
IU_HOTKEY("profiles", PREV_PROFILE, "R,X", INGAME_DEFAULT);
ConfigHotkey PROFILE_KEYS[10]{
    applied(ConfigHotkey("profile_1", "R,1", INGAME_DEFAULT), "profiles"), applied(ConfigHotkey("profile_2", "R,2", INGAME_DEFAULT), "profiles"),
    applied(ConfigHotkey("profile_3", "R,3", INGAME_DEFAULT), "profiles"), applied(ConfigHotkey("profile_4", "R,4", INGAME_DEFAULT), "profiles"),
    applied(ConfigHotkey("profile_5", "R,5", INGAME_DEFAULT), "profiles"), applied(ConfigHotkey("profile_6", "R,6", INGAME_DEFAULT), "profiles"),
    applied(ConfigHotkey("profile_7", "R,7", INGAME_DEFAULT), "profiles"), applied(ConfigHotkey("profile_8", "R,8", INGAME_DEFAULT), "profiles"),
    applied(ConfigHotkey("profile_9", "R,9", INGAME_DEFAULT), "profiles"), applied(ConfigHotkey("profile_10", "R,0", INGAME_DEFAULT), "profiles"),
};
IU_HOTKEY("profiles", SAVE_AS_PROFILE, "R,P", GUI_DEFAULT);
ConfigHotkey GLOBAL_BOOKMARK_TRADE[3] = {applied(ConfigHotkey("global_bookmark_trade", "B", GUI_DEFAULT), "villager_trading"),
                                         applied(ConfigHotkey("global_bookmark_trade1", "", GUI_DEFAULT), "villager_trading"),
                                         applied(ConfigHotkey("global_bookmark_trade2", "", GUI_DEFAULT), "villager_trading")};
ConfigHotkey LOCAL_BOOKMARK_TRADE[3]  = {applied(ConfigHotkey("local_bookmark_trade", "L", GUI_DEFAULT), "villager_trading"),
                                         applied(ConfigHotkey("local_bookmark_trade1", "", GUI_DEFAULT), "villager_trading"),
                                         applied(ConfigHotkey("local_bookmark_trade2", "", GUI_DEFAULT), "villager_trading")};
ConfigHotkey DO_GLOBAL_TRADE[3]       = {applied(ConfigHotkey("do_global_trade", "T", GUI_DEFAULT), "villager_trading"),
                                         applied(ConfigHotkey("do_global_trade1", "", GUI_DEFAULT), "villager_trading"),
                                         applied(ConfigHotkey("do_global_trade2", "", GUI_DEFAULT), "villager_trading")};
ConfigHotkey DO_LOCAL_TRADE[3]        = {applied(ConfigHotkey("do_local_trade", "LEFT_SHIFT,T", GUI_DEFAULT), "villager_trading"),
                                         applied(ConfigHotkey("do_local_trade1", "", GUI_DEFAULT), "villager_trading"),
                                         applied(ConfigHotkey("do_local_trade2", "", GUI_DEFAULT), "villager_trading")};
std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list{
        &OPEN_CONFIG_MENU,
        &RELOAD_CUSTOM_CONFIGS,
        &SORT_INVENTORY,
        &SORT_INVENTORY_IN_COLUMNS,
        &SORT_INVENTORY_IN_ROWS,
        &MOVE_ALL_ITEMS,
        &THROW_ALL_ITEMS,
        &AUTO_REFILL_GUI_TOGGLE_FOR_SLOT,
        &AUTO_REFILL_GAME_TOGGLE_FOR_SLOT,
        &APPLY_PROFILE,
        &NEXT_PROFILE,
        &PREV_PROFILE,
        &PROFILE_KEYS[0], &PROFILE_KEYS[1], &PROFILE_KEYS[2], &PROFILE_KEYS[3], &PROFILE_KEYS[4],
        &PROFILE_KEYS[5], &PROFILE_KEYS[6], &PROFILE_KEYS[7], &PROFILE_KEYS[8], &PROFILE_KEYS[9],
        &SAVE_AS_PROFILE,
        &GLOBAL_BOOKMARK_TRADE[0], &LOCAL_BOOKMARK_TRADE[0], &DO_GLOBAL_TRADE[0], &DO_LOCAL_TRADE[0],
        &GLOBAL_BOOKMARK_TRADE[1], &LOCAL_BOOKMARK_TRADE[1], &DO_GLOBAL_TRADE[1], &DO_LOCAL_TRADE[1],
        &GLOBAL_BOOKMARK_TRADE[2], &LOCAL_BOOKMARK_TRADE[2], &DO_GLOBAL_TRADE[2], &DO_LOCAL_TRADE[2],
    };
    return list;
}
std::vector<ConfigHotkey*> const& HOTKEY_LIST() {
    static std::vector<ConfigHotkey*> const list{
        &OPEN_CONFIG_MENU,
        &RELOAD_CUSTOM_CONFIGS,
        &SORT_INVENTORY,
        &SORT_INVENTORY_IN_COLUMNS,
        &SORT_INVENTORY_IN_ROWS,
        &MOVE_ALL_ITEMS,
        &THROW_ALL_ITEMS,
        &AUTO_REFILL_GAME_TOGGLE_FOR_SLOT,
        &APPLY_PROFILE,
        &NEXT_PROFILE,
        &PREV_PROFILE,
        &PROFILE_KEYS[0], &PROFILE_KEYS[1], &PROFILE_KEYS[2], &PROFILE_KEYS[3], &PROFILE_KEYS[4],
        &PROFILE_KEYS[5], &PROFILE_KEYS[6], &PROFILE_KEYS[7], &PROFILE_KEYS[8], &PROFILE_KEYS[9],
        &SAVE_AS_PROFILE,
        &GLOBAL_BOOKMARK_TRADE[0], &LOCAL_BOOKMARK_TRADE[0], &DO_GLOBAL_TRADE[0], &DO_LOCAL_TRADE[0],
        &GLOBAL_BOOKMARK_TRADE[1], &LOCAL_BOOKMARK_TRADE[1], &DO_GLOBAL_TRADE[1], &DO_LOCAL_TRADE[1],
        &GLOBAL_BOOKMARK_TRADE[2], &LOCAL_BOOKMARK_TRADE[2], &DO_GLOBAL_TRADE[2], &DO_LOCAL_TRADE[2],
        &Debugs::CLEAN_CURSOR,
    };
    return list;
}
}

namespace Debugs {
IU_BOOL("debugs", TRACE_LOGS, false);
IU_BOOL("debugs", INSANE_TRACE_LOGS, false);
IU_BOOL("debugs", FORCE_NO_CLEAN_CURSOR, false);
IU_ENUM("debugs", DIFF_CALCULATOR, DIFF_CALCULATOR_TYPE, "SIMPLE");
IU_HOTKEY("debugs", CLEAN_CURSOR, "X,1", GUI_DEFAULT);
std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list{&TRACE_LOGS, &INSANE_TRACE_LOGS, &FORCE_NO_CLEAN_CURSOR, &DIFF_CALCULATOR, &CLEAN_CURSOR};
    return list;
}
}

#undef IU_BOOL
#undef IU_INT
#undef IU_STR
#undef IU_ENUM
#undef IU_HOTKEY

bool isPressing(ConfigHotkey const& hotkey) {
    auto const& keybind = hotkey.getKeybind();
    auto        keys    = keybind.getKeys();
    if (keys.empty()) return false;
    auto const& settings = keybind.getSettings();
    bool gui = malilib::hotkeys::KeybindMulti::isGuiOpen();
    if (settings.getContext() == Ctx::GUI && !gui) return false;
    if (settings.getContext() == Ctx::INGAME && gui) return false;
    auto normalize = [](std::vector<int> v) {
        for (auto& c : v) c = normalizeModifier(c);
        std::vector<int> out;
        for (int c : v) {
            if (std::find(out.begin(), out.end(), c) == out.end()) out.push_back(c);
        }
        return out;
    };
    auto pressed    = normalize(malilib::hotkeys::KeybindMulti::pressedKeys());
    auto registered = normalize(keys);
    if (pressed.size() < registered.size()) return false;
    if (!settings.getAllowExtraKeys() && pressed.size() != registered.size()) return false;
    if (settings.isOrderSensitive()) {
        auto it = std::find(pressed.rbegin(), pressed.rend(), registered.back());
        if (it == pressed.rend()) return false;
        std::vector<int> upto(pressed.begin(), it.base());
        if (upto.size() < registered.size()) return false;
        return std::equal(registered.begin(), registered.end(), upto.end() - static_cast<long>(registered.size()));
    }
    return std::all_of(registered.begin(), registered.end(), [&](int k) { return std::find(pressed.begin(), pressed.end(), k) != pressed.end(); });
}

std::vector<std::pair<std::string, std::vector<IConfigBase*>>> categories() {
    return {
        {"Features",     Features::OPTIONS()    },
        {"ModSettings",  ModSettings::OPTIONS() },
        {"SortSettings", SortSettings::OPTIONS()},
        {"AutoRefillSettings", AutoRefillSettings::OPTIONS()},
        {"LockedSlotsSettings", LockedSlotsSettings::OPTIONS()},
        {"EditProfiles", EditProfiles::OPTIONS()},
        {"GuiSettings",  GuiSettings::OPTIONS() },
        {"Hotkeys",      Hotkeys::OPTIONS()     },
        {"Debugs",       Debugs::OPTIONS()      },
    };
}

std::filesystem::path configDir() { return ll::mod::NativeMod::current()->getConfigDir() / "inventoryprofilesnext"; }

namespace {
std::filesystem::path configFile() { return configDir() / "inventoryprofiles.json"; }
}

void loadFromFile() {
    auto path = configFile();
    if (!std::filesystem::exists(path)) {
        saveToFile();
        return;
    }
    std::ifstream in(path, std::ios::binary);
    auto          root = nlohmann::json::parse(in, nullptr, false);
    if (!root.is_object()) {
        ll::mod::NativeMod::current()->getLogger().error("inventoryprofiles.json を読めなかった（JSON が壊れている）");
        return;
    }
    for (auto& [cat, options] : categories()) readConfigBase(root, cat, options);
}

void saveToFile() {
    auto            path = configFile();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    nlohmann::json root = nlohmann::json::object();
    for (auto& [cat, options] : categories()) writeConfigBase(root, cat, options);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << root.dump(2);
}

}
