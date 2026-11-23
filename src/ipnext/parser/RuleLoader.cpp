// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/parser/RuleLoader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/registry/ItemRegistryRef.h"

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/item/ItemType.h"
#include "ipnext/item/rule/file/RuleFiles.h"
#include "ipnext/parser/RuleParser.h"

namespace inventory_utils::ipnext::parser::RuleLoader {

namespace {

constexpr char const* INTERNAL_FILE_DISPLAY_NAME = "<internal rules.txt>";

constexpr char const* INTERNAL_RULES_TXT = R"(// Default rules file

@default
    @creative_menu_order

@creative_menu_order
    ::custom_name
    ::search_tab_index
    @default_components_rule

@item_name
    ::display_name
    @default_components_rule

@item_id
    ::item_id
    @default_components_rule

@raw_id
    ::raw_id
    @default_components_rule

@default_components_rule
    ::enchantments_tooltip_order
    ::damage
    ::display_name
    ::potion_effect
    ::components_comparator

@auto_refill_best
    @default_components_rule

@accumulated_count_descending
    ::accumulated_count(number_order = descending)
    @default

@accumulated_count_ascending
    ::accumulated_count(number_order = ascending)
    @default
)";

void refreshWildcardCandidates() {
    std::set<std::string> ids;
    if (auto ci = ll::service::getClientInstance()) {
        ItemRegistryRef reg = ci->getItemRegistry();
        try {
            for (auto const& [name, weak] : reg.getNameToItemMap()) ids.insert(name.getString());
        } catch (...) {
        }
    }
    item::rule::file::setWildcardCandidates(std::move(ids));
}

std::vector<std::filesystem::path> getFiles() {
    std::vector<std::filesystem::path> out;
    auto                               dir = config::configDir();
    std::error_code                    ec;
    if (!std::filesystem::is_directory(dir, ec)) return out;
    std::regex const re(R"(^rules-v2\.(?:.*\.)?txt$)");
    for (auto const& e : std::filesystem::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        auto name = e.path().filename().string();
        if (std::regex_match(name, re)) out.push_back(e.path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

}

void load() { reload(false); }

void reload(bool fromUserInput) {
    if (fromUserInput) item::rule::file::tellPlayer("Loading custom sort rules...");
    refreshWildcardCandidates();
    item::ItemType::clearCreativeIndexCache();
    std::vector<std::shared_ptr<item::rule::file::RuleFile>> files;
    files.push_back(std::make_shared<item::rule::file::RuleFile>(INTERNAL_FILE_DISPLAY_NAME, INTERNAL_RULES_TXT, fromUserInput));
    for (auto const& path : getFiles()) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            Log::error("Failed to read file {}", path.filename().string());
            continue;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        files.push_back(std::make_shared<item::rule::file::RuleFile>(path.filename().string(), ss.str(), fromUserInput));
    }
    Log::trace("[-] Total {} rule files (including <internal>)", files.size());
    item::rule::file::RuleFileRegister::reloadRuleFiles(std::move(files), fromUserInput);
    TemporaryRuleParser::onReload();
    if (fromUserInput) item::rule::file::tellPlayer("");
}

}
