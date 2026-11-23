// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "itemscroller/config/Configs.h"

#include <filesystem>
#include <fstream>

#include "ll/api/mod/NativeMod.h"

#include "itemscroller/config/Hotkeys.h"
#include "itemscroller/recipes/CraftingHandler.h"

namespace inventory_utils::itemscroller::config {

namespace {

constexpr char const* CONFIG_FILE_NAME = "itemscroller.json";
constexpr char const* GENERIC_KEY      = "itemscroller.config.generic";
constexpr char const* TOGGLES_KEY      = "itemscroller.config.toggles";

template <class T>
T applied(T option, char const* prefix) {
    option.apply(prefix);
    return option;
}

std::filesystem::path configFile() { return ll::mod::NativeMod::current()->getConfigDir() / CONFIG_FILE_NAME; }

void getStrings(nlohmann::json const& root, std::set<std::string>& out, char const* key) {
    out.clear();
    if (!root.contains(key) || !root[key].is_array()) return;
    for (auto const& v : root[key]) {
        if (v.is_string()) out.insert(v.get<std::string>());
    }
}

void writeStrings(nlohmann::json& root, std::set<std::string> const& values, char const* key) {
    nlohmann::json arr = nlohmann::json::array();
    for (auto const& v : values) arr.push_back(v);
    root[key] = arr;
}

}

namespace Generic {
ConfigBoolean    CRAFTING_RENDER_RECIPE_ITEMS         = applied(ConfigBoolean("craftingRenderRecipeItems", true), GENERIC_KEY);
ConfigBoolean    DEBUG_MESSAGES                       = applied(ConfigBoolean("debugMessages", false), GENERIC_KEY);
ConfigBoolean    MOD_MAIN_TOGGLE                      = applied(ConfigBoolean("modMainToggle", true), GENERIC_KEY);
ConfigBoolean    ENABLE_IN_CREATIVE                   = applied(ConfigBoolean("enableInCreative", true), GENERIC_KEY);
ConfigInteger    MASS_CRAFT_INTERVAL                  = applied(ConfigInteger("massCraftInterval", 2, 1, 60), GENERIC_KEY);
ConfigInteger    MASS_CRAFT_ITERATIONS                = applied(ConfigInteger("massCraftIterations", 36, 1, 256), GENERIC_KEY);
ConfigBoolean    MASS_CRAFT_SWAPS                     = applied(ConfigBoolean("massCraftSwapsOnly", false), GENERIC_KEY);
ConfigBoolean    MASS_CRAFT_RECIPE_BOOK               = applied(ConfigBoolean("massCraftUseRecipeBook", true), GENERIC_KEY);
ConfigBoolean    MASS_CRAFT_HOLD                      = applied(ConfigBoolean("massCraftHold", false), GENERIC_KEY);
ConfigInteger    PACKET_RATE_LIMIT                    = applied(ConfigInteger("packetRateLimit", 4, 1, 1024), GENERIC_KEY);
ConfigInteger    RECIPE_BOOK_FAILURE_LIMIT            = applied(ConfigInteger("recipeBookFailureLimit", 0, 0, 512), GENERIC_KEY);
ConfigBoolean    SCROLL_CRAFT_STORE_RECIPES_TO_FILE   = applied(ConfigBoolean("craftingRecipesSaveToFile", true), GENERIC_KEY);
ConfigBoolean    SCROLL_CRAFT_RECIPE_FILE_GLOBAL      = applied(ConfigBoolean("craftingRecipesSaveFileIsGlobal", false), GENERIC_KEY);
ConfigBoolean    RATE_LIMIT_CLICK_PACKETS             = applied(ConfigBoolean("rateLimitClickPackets", false), GENERIC_KEY);
ConfigBoolean    REVERSE_SCROLL_DIRECTION_SINGLE      = applied(ConfigBoolean("reverseScrollDirectionSingle", false), GENERIC_KEY);
ConfigBoolean    REVERSE_SCROLL_DIRECTION_STACKS      = applied(ConfigBoolean("reverseScrollDirectionStacks", false), GENERIC_KEY);
ConfigBoolean    USE_RECIPE_CACHING                   = applied(ConfigBoolean("useRecipeCaching", true), GENERIC_KEY);
ConfigBoolean    SLOT_POSITION_AWARE_SCROLL_DIRECTION = applied(ConfigBoolean("useSlotPositionAwareScrollDirection", false), GENERIC_KEY);
ConfigBoolean    SORT_SHULKER_BOXES_AT_END            = applied(ConfigBoolean("sortShulkerBoxesAtEnd", true), GENERIC_KEY);
ConfigBoolean    SORT_SHULKER_BOXES_INVERTED          = applied(ConfigBoolean("sortShulkerBoxesInverted", false), GENERIC_KEY);
ConfigBoolean    SORT_BUNDLES_AT_END                  = applied(ConfigBoolean("sortBundlesAtEnd", true), GENERIC_KEY);
ConfigBoolean    SORT_BUNDLES_INVERTED                = applied(ConfigBoolean("sortBundlesInverted", false), GENERIC_KEY);
ConfigOptionList TRANSLATION_LANGUAGE = applied(ConfigOptionList("translationLanguage", {"en_us", "ja_jp"}, "en_us"), GENERIC_KEY);
ConfigOptionList TRANSLATION_MODE     = applied(ConfigOptionList("translationMode", {"vanilla", "malilib", "independent", "off"}, "vanilla"), GENERIC_KEY);

std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list{
        &CRAFTING_RENDER_RECIPE_ITEMS,
        &ENABLE_IN_CREATIVE,
        &DEBUG_MESSAGES,
        &MASS_CRAFT_INTERVAL,
        &MASS_CRAFT_ITERATIONS,
        &MASS_CRAFT_SWAPS,
        &MASS_CRAFT_RECIPE_BOOK,
        &MASS_CRAFT_HOLD,
        &MOD_MAIN_TOGGLE,
        &PACKET_RATE_LIMIT,
        &RATE_LIMIT_CLICK_PACKETS,
        &RECIPE_BOOK_FAILURE_LIMIT,
        &SCROLL_CRAFT_STORE_RECIPES_TO_FILE,
        &SCROLL_CRAFT_RECIPE_FILE_GLOBAL,
        &REVERSE_SCROLL_DIRECTION_SINGLE,
        &REVERSE_SCROLL_DIRECTION_STACKS,
        &SLOT_POSITION_AWARE_SCROLL_DIRECTION,
        &SORT_SHULKER_BOXES_AT_END,
        &SORT_SHULKER_BOXES_INVERTED,
        &SORT_BUNDLES_AT_END,
        &SORT_BUNDLES_INVERTED,
        &TRANSLATION_LANGUAGE,
        &TRANSLATION_MODE,
        &USE_RECIPE_CACHING,
    };
    return list;
}
}

namespace Toggles {
ConfigBoolean CRAFTING_FEATURES       = applied(ConfigBoolean("enableCraftingFeatures", true), TOGGLES_KEY);
ConfigBoolean DROP_MATCHING           = applied(ConfigBoolean("enableDropkeyDropMatching", true), TOGGLES_KEY);
ConfigBoolean ITEM_MOVING_FALLBACK    = applied(ConfigBoolean("enableItemMovingFallback", false), TOGGLES_KEY);
ConfigBoolean RIGHT_CLICK_CRAFT_STACK = applied(ConfigBoolean("enableRightClickCraftingOneStack", true), TOGGLES_KEY);
ConfigBoolean SCROLL_EVERYTHING       = applied(ConfigBoolean("enableScrollingEverything", true), TOGGLES_KEY);
ConfigBoolean SCROLL_MATCHING         = applied(ConfigBoolean("enableScrollingMatchingStacks", true), TOGGLES_KEY);
ConfigBoolean SCROLL_SINGLE           = applied(ConfigBoolean("enableScrollingSingle", true), TOGGLES_KEY);
ConfigBoolean SCROLL_STACKS           = applied(ConfigBoolean("enableScrollingStacks", true), TOGGLES_KEY);
ConfigBoolean SHIFT_DROP_ITEMS        = applied(ConfigBoolean("enableShiftDropItems", true), TOGGLES_KEY);
ConfigBoolean SHIFT_PLACE_ITEMS       = applied(ConfigBoolean("enableShiftPlaceItems", true), TOGGLES_KEY);

std::vector<IConfigBase*> const& OPTIONS() {
    static std::vector<IConfigBase*> const list{
        &CRAFTING_FEATURES,
        &DROP_MATCHING,
        &ITEM_MOVING_FALLBACK,
        &RIGHT_CLICK_CRAFT_STACK,
        &SCROLL_EVERYTHING,
        &SCROLL_MATCHING,
        &SCROLL_SINGLE,
        &SCROLL_STACKS,
        &SHIFT_DROP_ITEMS,
        &SHIFT_PLACE_ITEMS,
    };
    return list;
}
}

std::set<std::string> GUI_BLACKLIST;
std::set<std::string> SLOT_BLACKLIST;

namespace {
std::vector<malilib::config::IConfigBase*> hotkeysAsBase() {
    std::vector<malilib::config::IConfigBase*> v;
    for (auto* h : Hotkeys::HOTKEY_LIST()) v.push_back(h);
    return v;
}
}

std::vector<std::pair<std::string, std::vector<malilib::config::IConfigBase*>>> categories() {
    return {
        {"Generic", Generic::OPTIONS()},
        {"Hotkeys", hotkeysAsBase()  },
        {"Toggles", Toggles::OPTIONS()},
    };
}

malilib::config::IConfigBase* find(std::string const& category, std::string const& name) {
    for (auto& [cat, options] : categories()) {
        if (cat != category) continue;
        for (auto* o : options) {
            if (o->getName() == name) return o;
        }
    }
    return nullptr;
}

void registerCraftingGrids() {
    using recipes::CraftingHandler;
    CraftingHandler::clearDefinitions();
    CraftingHandler::addCraftingGridDefinition("crafting_screen", 0, CraftingHandler::SlotRange(1, 9));
    CraftingHandler::addCraftingGridDefinition("inventory_screen", 0, CraftingHandler::SlotRange(1, 4));
}

void loadFromFile() {
    auto path = configFile();
    auto log  = [](std::string const& m) { ll::mod::NativeMod::current()->getLogger().info("{}", m); };
    if (!std::filesystem::exists(path)) {
        log("itemscroller.json が無いので既定値で作る");
        saveToFile();
        registerCraftingGrids();
        return;
    }
    std::ifstream in(path, std::ios::binary);
    auto          root = nlohmann::json::parse(in, nullptr, false);
    if (!root.is_object()) {
        ll::mod::NativeMod::current()->getLogger().error("loadFromFile(): itemscroller.json を読めなかった（JSON が壊れている）");
        return;
    }
    for (auto& [cat, options] : categories()) malilib::config::readConfigBase(root, cat, options);
    getStrings(root, GUI_BLACKLIST, "guiBlacklist");
    getStrings(root, SLOT_BLACKLIST, "slotBlacklist");
    registerCraftingGrids();
}

void saveToFile() {
    auto path = configFile();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    nlohmann::json root = nlohmann::json::object();
    for (auto& [cat, options] : categories()) malilib::config::writeConfigBase(root, cat, options);
    writeStrings(root, GUI_BLACKLIST, "guiBlacklist");
    writeStrings(root, SLOT_BLACKLIST, "slotBlacklist");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << root.dump(2);
}

}
