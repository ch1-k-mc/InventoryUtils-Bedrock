// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/inventory/ContainerMenu.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/controllers/ContainerController.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/containers/models/ContainerModel.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"

namespace inventory_utils::platform::inventory {

namespace {

struct CollectionOrder {
    std::string_view name;
    bool             output;
};

constexpr std::array<CollectionOrder, 35> kContainerOrder{{
    {"crafting_output_items", true},
    {"crafting_input_items", false},
    {"armor_items", false},
    {"container_items", false},
    {"furnace_ingredient_items", false},
    {"furnace_fuel_items", false},
    {"furnace_output_items", true},
    {"brewing_result_items", false},
    {"brewing_input_item", false},
    {"brewing_fuel_item", false},
    {"stonecutter_input_items", false},
    {"stonecutter_result_items", true},
    {"smithing_table_template_items", false},
    {"smithing_table_input_items", false},
    {"smithing_table_material_items", false},
    {"smithing_table_result_items", true},
    {"grindstone_input_items", false},
    {"grindstone_additional_items", false},
    {"grindstone_result_items", true},
    {"loom_input_items", false},
    {"loom_dye_items", false},
    {"loom_material_items", false},
    {"loom_result_items", true},
    {"cartography_input_items", false},
    {"cartography_additional_items", false},
    {"cartography_result_items", true},
    {"enchanting_input_items", false},
    {"enchanting_lapis_items", false},
    {"beacon_payment_items", false},
    {"anvil_input_items", false},
    {"anvil_material_items", false},
    {"anvil_result_items", true},
    {"trade2_ingredient1_item", false},
    {"trade2_ingredient2_item", false},
    {"trade2_result_item", true},
}};

int collectionSize(ContainerManagerController& mgr, std::string const& name) {
    auto it = mgr.mContainers->find(name);
    if (it == mgr.mContainers->end() || !it->second) return -1;
    auto model = it->second->mContainerModel.lock();
    return model ? model->getContainerSize() : -1;
}

ItemStack const& emptyStack() {
    static ItemStack const empty;
    return empty;
}

int layoutContainer(std::vector<MenuSlot>& slots, std::string const& screen) {
    int bottom      = 18;
    int singleX     = 30;
    for (auto& s : slots) {
        if (s.kind != InventoryKind::Container && s.kind != InventoryKind::Armor) continue;
        auto const& c = s.collection;
        int         i = s.index;
        if (c == "armor_items") {
            s.x = 8, s.y = 8 + 18 * i;
        } else if (c == "crafting_input_items") {
            if (screen == "inventory_screen") s.x = 98 + 18 * (i % 2), s.y = 18 + 18 * (i / 2);
            else s.x = 30 + 18 * (i % 3), s.y = 17 + 18 * (i / 3);
        } else if (c == "crafting_output_items") {
            if (screen == "inventory_screen") s.x = 154, s.y = 28;
            else s.x = 124, s.y = 35;
        } else if (c == "container_items") {
            if (screen == "hopper_screen") s.x = 44 + 18 * i, s.y = 20;
            else if (screen == "dispenser_screen" || screen == "dropper_screen") s.x = 62 + 18 * (i % 3), s.y = 17 + 18 * (i / 3);
            else s.x = 8 + 18 * (i % 9), s.y = 18 + 18 * (i / 9);
        } else if (c == "furnace_ingredient_items") {
            s.x = 56, s.y = 17;
        } else if (c == "furnace_fuel_items") {
            s.x = 56, s.y = 53;
        } else if (c == "furnace_output_items") {
            s.x = 116, s.y = 35;
        } else {
            s.x = singleX, s.y = 35;
            singleX += 22;
        }
        bottom = std::max(bottom, s.y);
    }
    if (screen == "inventory_screen" || screen == "crafting_screen") return 52;
    return bottom;
}

}

ContainerMenu ContainerMenu::fromManager(ContainerManagerController& mgr, std::string const& screenName, ContainerScreenController* screen) {
    ContainerMenu menu;
    menu.mManager = &mgr;
    menu.mScreenController = screen;
    menu.mScreen  = screenName;
    int           nextId    = 1;
    constexpr int kPlayerId = 0;
    auto          add       = [&](std::string const& name, InventoryKind kind, int inventoryId, bool output, int containerSlotBase) {
        int size = collectionSize(mgr, name);
        for (int i = 0; i < size; ++i) {
            MenuSlot s;
            s.slotNumber    = static_cast<int>(menu.mSlots.size());
            s.collection    = name;
            s.index         = i;
            s.kind          = kind;
            s.inventoryId   = inventoryId;
            s.containerSlot = containerSlotBase + i;
            s.isOutput      = output;
            menu.mSlots.push_back(std::move(s));
        }
    };
    for (auto const& c : kContainerOrder) {
        std::string name(c.name);
        if (name == "armor_items") add(name, InventoryKind::Armor, kPlayerId, false, 36);
        else add(name, InventoryKind::Container, nextId++, c.output, 0);
    }
    add("inventory_items", InventoryKind::Player, kPlayerId, false, 9);
    add("hotbar_items", InventoryKind::Hotbar, kPlayerId, false, 0);
    add("offhand_items", InventoryKind::Offhand, kPlayerId, false, 40);

    int invTop = layoutContainer(menu.mSlots, screenName) + 14 + 18;
    for (auto& s : menu.mSlots) {
        switch (s.kind) {
        case InventoryKind::Player: s.x = 8 + 18 * (s.index % 9), s.y = invTop + 18 * (s.index / 9); break;
        case InventoryKind::Hotbar: s.x = 8 + 18 * s.index, s.y = invTop + 58; break;
        case InventoryKind::Offhand: s.x = 77, s.y = 62; break;
        default: break;
        }
    }
    return menu;
}

MenuSlot const* ContainerMenu::getSlot(int slotNumber) const {
    if (slotNumber < 0 || slotNumber >= static_cast<int>(mSlots.size())) return nullptr;
    return &mSlots[static_cast<size_t>(slotNumber)];
}

int ContainerMenu::findSlot(std::string const& collection, int index) const {
    auto it = std::find_if(mSlots.begin(), mSlots.end(), [&](MenuSlot const& s) { return s.collection == collection && s.index == index; });
    return it == mSlots.end() ? -1 : it->slotNumber;
}

ItemStack const& ContainerMenu::getItem(MenuSlot const& slot) const {
    if (!mManager) return emptyStack();
    auto it = mManager->mContainers->find(slot.collection);
    if (it == mManager->mContainers->end() || !it->second) return emptyStack();
    if (slot.isOutput) {
        if (!mScreenController) return emptyStack();
        auto& out                        = mOutputCache[slot.slotNumber];
        static_cast<ItemStackBase&>(out) = mScreenController->_getVisualItemStack(slot.collection, slot.index);
        return out;
    }
    auto model = it->second->mContainerModel.lock();
    if (!model || slot.index >= model->getContainerSize()) return emptyStack();
    return model->getItemStack(slot.index);
}

ItemStack const& ContainerMenu::getCarried() const {
    MenuSlot cursor;
    cursor.collection = "cursor_items";
    cursor.index      = 0;
    return getItem(cursor);
}

}
