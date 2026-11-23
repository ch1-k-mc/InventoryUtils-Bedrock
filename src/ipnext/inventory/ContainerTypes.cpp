// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/inventory/ContainerTypes.h"

#include <map>

namespace inventory_utils::ipnext::inventory {

using CT = ContainerType;

std::optional<VanillaContainer> VanillaContainer::current() {
    auto clicker = platform::inventory::SlotClicker::current();
    if (!clicker.valid()) return std::nullopt;
    return VanillaContainer(std::move(clicker));
}

namespace ContainerTypes {

namespace {

ContainerTypeSet const nonStorage{CT::TEMP_SLOTS};
ContainerTypeSet const playerOnly{CT::PURE_BACKPACK, CT::PLAYER, CT::CRAFTING};
ContainerTypeSet const simple{CT::PURE_BACKPACK};
ContainerTypeSet const chestLike{CT::SORTABLE_STORAGE, CT::RECTANGULAR, CT::WIDTH_9};

std::map<std::string, ContainerTypeSet> const& table() {
    static std::map<std::string, ContainerTypeSet> const m{
        {"inventory_screen",      playerOnly                                                          },
        {"enchanting_screen",     simple                                                              },
        {"anvil_screen",          {CT::PURE_BACKPACK, CT::ANVIL}                                      },
        {"beacon_screen",         nonStorage                                                          },
        {"blast_furnace_screen",  simple                                                              },
        {"cartography_screen",    simple                                                              },
        {"furnace_screen",        simple                                                              },
        {"grindstone_screen",     simple                                                              },
        {"loom_screen",           nonStorage                                                          },
        {"stonecutter_screen",    {CT::PURE_BACKPACK, CT::STONECUTTER}                                },
        {"smithing_table_screen", simple                                                              },
        {"smoker_screen",         simple                                                              },
        {"trade_screen",          {CT::TRADER}                                                        },
        {"trade2_screen",         {CT::TRADER}                                                        },
        {"crafting_screen",       {CT::CRAFTING}                                                      },
        {"hopper_screen",         {CT::NO_SORTING_STORAGE, CT::PURE_BACKPACK}                         },
        {"brewing_stand_screen",  nonStorage                                                          },
        {"small_chest_screen",    chestLike                                                           },
        {"large_chest_screen",    chestLike                                                           },
        {"barrel_screen",         chestLike                                                           },
        {"ender_chest_screen",    chestLike                                                           },
        {"shulker_box_screen",    chestLike                                                           },
        {"horse_screen",          {CT::SORTABLE_STORAGE, CT::RECTANGULAR, CT::HEIGHT_3, CT::HORSE_STORAGE}},
        {"dispenser_screen",      {CT::SORTABLE_STORAGE, CT::RECTANGULAR, CT::HEIGHT_3}               },
        {"dropper_screen",        {CT::SORTABLE_STORAGE, CT::RECTANGULAR, CT::HEIGHT_3}               },
    };
    return m;
}

}

ContainerTypeSet getTypes(std::string const& screenName, bool hasSlots) {
    if (!hasSlots) return nonStorage;
    auto const& t  = table();
    auto        it = t.find(screenName);
    if (it != t.end()) return it->second;
    return chestLike;
}

ContainerTypeSet getTypes(VanillaContainer const& container) { return getTypes(container.screenName(), !container.slots().empty()); }

}

}
