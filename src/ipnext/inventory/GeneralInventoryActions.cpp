// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/inventory/GeneralInventoryActions.h"

#include <algorithm>
#include <cctype>

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/inventory/AdvancedContainer.h"
#include "ipnext/inventory/ContainerClicker.h"
#include "ipnext/inventory/action/Actions.h"
#include "ipnext/item/rule/file/RuleFiles.h"
#include "ipnext/parser/RuleParser.h"

namespace inventory_utils::ipnext::inventory::GeneralInventoryActions {

using namespace config;
using namespace AreaTypes;
using CT = ContainerType;

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool forcePlayerSide(VanillaContainer const& c) {
    if (!SortSettings::SORT_AT_CURSOR.getBooleanValue()) return false;
    auto const* focused = c.focusedSlot();
    return focused && isPlayerInventory(*focused);
}

void innerDoSort(item::rule::RulePtr const& sortingRule, action::PostAction postAction, bool forcePlayer) {
    AdvancedContainer::withTracker([&](AdvancedContainer::TrackerDsl& dsl) {
        auto const& vanilla         = dsl.vanillaContainer();
        bool        forcePlayerSide_ = forcePlayer || forcePlayerSide(vanilla);
        ItemArea    target;
        if (forcePlayerSide_ || dsl.get(sortableItemStorage()).isEmpty()) {
            target = dsl.get(playerStorage() - lockedSlots() - disabled(vanilla));
            if (SortSettings::RESTOCK_HOTBAR.getBooleanValue()) {
                auto hands = dsl.asSubTracker(dsl.get(playerHands() + playerHotbar()));
                auto from  = dsl.asSubTracker(target);
                action::restockFrom(hands, from);
            }
        } else {
            target = dsl.get(sortableItemStorage() - disabled(vanilla));
        }
        auto sub = dsl.asSubTracker(target);
        action::sort(sub, *sortingRule, postAction, target.isRectangular(), target.width(), target.height());
    });
}

void doSortWith(ConfigOptionList const& sortOrder, ConfigString const& customRule, ConfigOptionList const& postAction, bool forcePlayer) {
    if (!Features::ENABLE_SORTING_AND_MOVING.getBooleanValue()) return;
    try {
        innerDoSort(ruleFor(sortOrder.getOptionListValue(), customRule.getStringValue()), action::postActionFromName(postAction.getOptionListValue()), forcePlayer);
    } catch (std::exception const& e) {
        Log::warn("doSort: {}", e.what());
    }
}

}

item::rule::RulePtr ruleForGlobal(std::string const& sortingMethod) {
    if (sortingMethod == "CUSTOM") return parser::TemporaryRuleParser::parse(SortSettings::CUSTOM_RULE.getStringValue());
    return item::rule::file::RuleFileRegister::getCustomRuleOrEmpty(lower(sortingMethod));
}

item::rule::RulePtr ruleFor(std::string const& sortingMethodIndividual, std::string const& customContent) {
    if (sortingMethodIndividual == "GLOBAL") return ruleForGlobal(SortSettings::SORT_ORDER.getOptionListValue());
    if (sortingMethodIndividual == "CUSTOM") return parser::TemporaryRuleParser::parse(customContent);
    return ruleForGlobal(sortingMethodIndividual);
}

void doSort(bool forcePlayer) { doSortWith(GuiSettings::REGULAR_SORT_ORDER, GuiSettings::REGULAR_CUSTOM_RULE, GuiSettings::REGULAR_POST_ACTION, forcePlayer); }

void doSortInColumns(bool forcePlayer) {
    doSortWith(GuiSettings::IN_COLUMNS_SORT_ORDER, GuiSettings::IN_COLUMNS_CUSTOM_RULE, GuiSettings::IN_COLUMNS_POST_ACTION, forcePlayer);
}

void doSortInRows(bool forcePlayer) { doSortWith(GuiSettings::IN_ROWS_SORT_ORDER, GuiSettings::IN_ROWS_CUSTOM_RULE, GuiSettings::IN_ROWS_POST_ACTION, forcePlayer); }

void doMoveMatch() {
    if (!Features::ENABLE_SORTING_AND_MOVING.getBooleanValue()) return;
    auto vanilla = VanillaContainer::current();
    if (!vanilla) return;
    auto types = ContainerTypes::getTypes(*vanilla);
    if (types.contains(CT::CREATIVE)) return;
    if (!containsAny(types, {CT::SORTABLE_STORAGE, CT::NO_SORTING_STORAGE, CT::CRAFTING})) return;
    bool forceToPlayer = false;
    if (ModSettings::MOVE_ALL_AT_CURSOR.getBooleanValue()) {
        if (auto const* f = vanilla->focusedSlot()) forceToPlayer = !isPlayerInventory(*f);
    }
    doMoveMatch(forceToPlayer);
}

void doMoveMatch(bool toPlayer) {
    auto vanilla = VanillaContainer::current();
    if (!vanilla) return;
    auto types = ContainerTypes::getTypes(*vanilla);
    if (types.contains(CT::CREATIVE)) return;
    if (types.contains(CT::CRAFTING)) {
        bool includeHotbar = isPressing(ModSettings::INCLUDE_HOTBAR_MODIFIER);
        AdvancedContainer::withTracker([&](AdvancedContainer::TrackerDsl& dsl) {
            auto player = (includeHotbar ? (playerStorage() + playerHotbar() + playerOffhand()) : playerStorage()) - lockedSlots();
            auto source = dsl.asSubTracker(dsl.get(player - disabled(dsl.vanillaContainer())));
            auto dest   = dsl.asSubTracker(dsl.get(craftingIngredient()));
            action::moveMatchCraftingTo(source, dest);
        });
        return;
    }
    bool includeHotbar  = isPressing(ModSettings::INCLUDE_HOTBAR_MODIFIER) != ModSettings::ALWAYS_INCLUDE_HOTBAR.getBooleanValue();
    bool moveAll        = isPressing(ModSettings::MOVE_ALL_MODIFIER) != ModSettings::ALWAYS_MOVE_ALL.getBooleanValue();
    auto cursorType     = item::ItemType::of(vanilla->cursorStack());
    bool moveFocusMatch = isPressing(ModSettings::MOVE_FOCUS_MACH_MODIFIER) || !cursorType.isEmpty();
    bool moveJustRefill = isPressing(ModSettings::MOVE_JUST_REFILL_MODIFIER);
    item::ItemType focusType;
    if (auto const* f = vanilla->focusedSlot()) focusType = item::ItemType::of(vanilla->itemOf(*f));
    if (focusType.isEmpty()) focusType = cursorType;
    AdvancedContainer::withTracker([&](AdvancedContainer::TrackerDsl& dsl) {
        auto const& v      = dsl.vanillaContainer();
        auto        player = ((includeHotbar || (toPlayer && moveJustRefill)) ? (playerStorage() + playerHotbar() + playerOffhand()) : playerStorage()) - lockedSlots() - disabled(v);
        auto        container = itemStorage() - disabled(v);
        auto        source    = dsl.asSubTracker(dsl.get(toPlayer ? container : player));
        auto        dest      = dsl.asSubTracker(dsl.get(toPlayer ? player : container));
        if (moveAll) action::moveAllTo(source, dest);
        else if (moveFocusMatch) action::moveFocusMatchTo(source, dest, focusType);
        else if (moveJustRefill) action::refillStacksTo(source, dest);
        else action::moveMatchTo(source, dest);
    });
}

void doThrowMatch() {
    if (!Features::ENABLE_SORTING_AND_MOVING.getBooleanValue()) return;
    auto vanilla = VanillaContainer::current();
    if (!vanilla) return;
    auto types = ContainerTypes::getTypes(*vanilla);
    if (types.contains(CT::CREATIVE)) return;
    if (!containsAny(types, {CT::SORTABLE_STORAGE, CT::NO_SORTING_STORAGE, CT::CRAFTING})) return;
    bool isContainer = false;
    if (ModSettings::MOVE_ALL_AT_CURSOR.getBooleanValue()) {
        if (auto const* f = vanilla->focusedSlot()) isContainer = !isPlayerInventory(*f);
    }
    bool includeHotbar = isPressing(ModSettings::INCLUDE_HOTBAR_MODIFIER) != ModSettings::ALWAYS_INCLUDE_HOTBAR.getBooleanValue();
    bool throwAll      = isPressing(ModSettings::MOVE_ALL_MODIFIER) != ModSettings::ALWAYS_THROW_ALL.getBooleanValue();

    auto player = (includeHotbar ? (playerStorage() + playerHotbar()) : playerStorage()) - lockedSlots();
    auto source = ((isContainer ? itemStorage() : player) - disabled(*vanilla)).getItemArea(*vanilla);
    auto const&      slots = vanilla->slots();
    std::vector<int> actUpon;
    if (throwAll) {
        for (int i : source.slotIndices()) {
            if (!item::ItemStack::of(vanilla->itemOf(slots[i])).isEmpty()) actUpon.push_back(slots[i].slotNumber);
        }
    } else {
        item::ItemStack focused;
        if (auto const* f = vanilla->focusedSlot()) focused = item::ItemStack::of(vanilla->itemOf(*f));
        if (focused.isEmpty()) focused = item::ItemStack::of(vanilla->cursorStack());
        bool ignore      = ModSettings::IGNORE_DURABILITY.getBooleanValue();
        auto focusedType = focused.itemType.copy(ignore);
        if (!focused.isEmpty()) {
            for (int i : source.slotIndices()) {
                auto s = item::ItemStack::of(vanilla->itemOf(slots[i]));
                if (!s.isEmpty() && s.itemType.copy(ignore) == focusedType) actUpon.push_back(slots[i].slotNumber);
            }
        }
    }
    if (actUpon.empty()) return;
    int interval = ModSettings::ADD_INTERVAL_BETWEEN_CLICKS.getBooleanValue() ? ModSettings::INTERVAL_BETWEEN_CLICKS_MS.getIntegerValue() : 0;
    ContainerClicker::executeQClicks(actUpon, interval);
}

void cleanCursor() {
    auto vanilla = VanillaContainer::current();
    if (!vanilla || item::ItemStack::of(vanilla->cursorStack()).isEmpty()) return;
    AdvancedContainer::run(true, [](AdvancedContainer& c) { c.cleanCursor(); });
}

}
