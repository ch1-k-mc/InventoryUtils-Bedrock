// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <set>
#include <string>
#include <vector>

#include "malilib/config/ConfigOptions.h"

namespace inventory_utils::itemscroller::config {

namespace Generic {
using namespace malilib::config;
extern ConfigBoolean    CRAFTING_RENDER_RECIPE_ITEMS;
extern ConfigBoolean    DEBUG_MESSAGES;
extern ConfigBoolean    MOD_MAIN_TOGGLE;
extern ConfigBoolean    ENABLE_IN_CREATIVE;
extern ConfigInteger    MASS_CRAFT_INTERVAL;
extern ConfigInteger    MASS_CRAFT_ITERATIONS;
extern ConfigBoolean    MASS_CRAFT_SWAPS;
extern ConfigBoolean    MASS_CRAFT_RECIPE_BOOK;
extern ConfigBoolean    MASS_CRAFT_HOLD;
extern ConfigInteger    PACKET_RATE_LIMIT;
extern ConfigInteger    RECIPE_BOOK_FAILURE_LIMIT;
extern ConfigBoolean    SCROLL_CRAFT_STORE_RECIPES_TO_FILE;
extern ConfigBoolean    SCROLL_CRAFT_RECIPE_FILE_GLOBAL;
extern ConfigBoolean    RATE_LIMIT_CLICK_PACKETS;
extern ConfigBoolean    REVERSE_SCROLL_DIRECTION_SINGLE;
extern ConfigBoolean    REVERSE_SCROLL_DIRECTION_STACKS;
extern ConfigBoolean    USE_RECIPE_CACHING;
extern ConfigBoolean    SLOT_POSITION_AWARE_SCROLL_DIRECTION;
extern ConfigBoolean    SORT_SHULKER_BOXES_AT_END;
extern ConfigBoolean    SORT_SHULKER_BOXES_INVERTED;
extern ConfigBoolean    SORT_BUNDLES_AT_END;
extern ConfigBoolean    SORT_BUNDLES_INVERTED;
extern ConfigOptionList TRANSLATION_LANGUAGE;
extern ConfigOptionList TRANSLATION_MODE;

std::vector<IConfigBase*> const& OPTIONS();
}

namespace Toggles {
using namespace malilib::config;
extern ConfigBoolean CRAFTING_FEATURES;
extern ConfigBoolean DROP_MATCHING;
extern ConfigBoolean ITEM_MOVING_FALLBACK;
extern ConfigBoolean RIGHT_CLICK_CRAFT_STACK;
extern ConfigBoolean SCROLL_EVERYTHING;
extern ConfigBoolean SCROLL_MATCHING;
extern ConfigBoolean SCROLL_SINGLE;
extern ConfigBoolean SCROLL_STACKS;
extern ConfigBoolean SHIFT_DROP_ITEMS;
extern ConfigBoolean SHIFT_PLACE_ITEMS;

std::vector<IConfigBase*> const& OPTIONS();
}

extern std::set<std::string> GUI_BLACKLIST;
extern std::set<std::string> SLOT_BLACKLIST;

void loadFromFile();
void saveToFile();

malilib::config::IConfigBase* find(std::string const& category, std::string const& name);
std::vector<std::pair<std::string, std::vector<malilib::config::IConfigBase*>>> categories();

}
