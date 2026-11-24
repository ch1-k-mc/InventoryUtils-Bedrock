// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/inventory/SlotOps.h"

#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/controllers/ContainerController.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/containers/models/ContainerModel.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"

#include "platform/item/ScopedItemRegistry.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::platform::inventory {

namespace {

using screen::ScreenTracker;

nlohmann::json itemJson(ItemStack const& s) {
    if (s.isNull()) return nullptr;
    Item const* item = s.mItem.get();
    return nlohmann::json{
        {"id",    item ? item->mFullName->getString() : ""},
        {"aux",   s.mAuxValue                         },
        {"count", s.mCount                            },
    };
}

ContainerController* findCollection(ContainerManagerController& mgr, std::string const& name) {
    auto it = mgr.mContainers->find(name);
    if (it == mgr.mContainers->end()) return nullptr;
    return it->second.get();
}

}

ContainerManagerController* managerOf(bool hud) { return hud ? ScreenTracker::hudManager() : ScreenTracker::containerManager(); }

nlohmann::json slotJson(Slot const& slot, bool hud) {
    item::ScopedItemRegistry scoped;
    auto* mgr = managerOf(hud);
    if (!mgr) return nullptr;
    auto* ctrl = findCollection(*mgr, slot.collection);
    if (!ctrl) return nullptr;
    auto model = ctrl->mContainerModel.lock();
    if (!model || slot.index < 0 || slot.index >= model->getContainerSize()) return nullptr;
    return itemJson(model->getItemStack(slot.index));
}

nlohmann::json snapshot(int limitPerCollection, bool includeEmpty, bool hud) {
    item::ScopedItemRegistry scoped;
    nlohmann::json r{{"screen", hud ? std::string("hud_screen") : ScreenTracker::containerScreenName()}};
    auto*          mgr = managerOf(hud);
    if (!mgr) {
        r["error"] = hud ? "No HUD manager" : "No container screen is open";
        return r;
    }
    nlohmann::json byName = nlohmann::json::object();
    for (auto const& [name, ctrl] : *mgr->mContainers) {
        nlohmann::json c{{"name", name}};
        if (ctrl) {
            auto model = ctrl->mContainerModel.lock();
            if (model) {
                int size     = model->getContainerSize();
                c["size"]    = size;
                c["model"]   = model->mContainerStringName.get();
                nlohmann::json items = nlohmann::json::array();
                for (int i = 0; i < size && static_cast<int>(items.size()) < limitPerCollection; ++i) {
                    auto j = itemJson(model->getItemStack(i));
                    if (j.is_null() && !includeEmpty) continue;
                    if (j.is_null()) j = nlohmann::json::object();
                    j["slot"] = i;
                    items.push_back(j);
                }
                c["items"] = items;
            }
        }
        byName[name] = c;
    }
    r["collections"] = byName;
    return r;
}

nlohmann::json runOp(std::string const& op, Slot const& src, Slot const& dst, int amount, bool all, bool hud) {
    item::ScopedItemRegistry scoped;
    auto* csc = hud ? nullptr : ScreenTracker::containerScreen();
    auto* mgr = managerOf(hud);
    if (!mgr) return {{"ok", false}, {"error", hud ? "No HUD manager" : "No container screen is open"}};
    if (!csc && op.rfind("screen.", 0) == 0) return {{"ok", false}, {"error", "screen.* works only on a container screen"}};
    SlotData       s(src.collection, src.index);
    SlotData       d(dst.collection, dst.index);
    nlohmann::json r{{"ok", true}, {"op", op}};
    if (op == "manager.takeAmount") r["returned"] = mgr->handleTakeAmount(d, amount, s);
    else if (op == "manager.takeAll") r["returned"] = mgr->handleTakeAll(d, s);
    else if (op == "manager.takeHalf") r["returned"] = mgr->handleTakeHalf(d, s);
    else if (op == "manager.placeOne") r["returned"] = mgr->handlePlaceOne(s, d);
    else if (op == "manager.placeAmount") r["returned"] = mgr->handlePlaceAmount(s, amount, d);
    else if (op == "manager.swap") r["returned"] = mgr->handleSwap(s, d);
    else if (op == "screen.autoPlace") csc->_handleAutoPlace(amount, src.collection, src.index);
    else if (op == "screen.takeAmount") csc->_handleTakeAmount(amount, src.collection, src.index);
    else if (op == "screen.takeHalf") csc->_handleTakeHalf(src.collection, src.index);
    else if (op == "screen.placeAll") csc->_handlePlaceAll(src.collection, src.index);
    else if (op == "screen.placeOne") csc->_handlePlaceOne(src.collection, src.index);
    else if (op == "screen.takePlace") r["returned"] = csc->_handleTakePlace(src.collection, src.index, all);
    else return {{"ok", false}, {"error", "Unknown op: " + op}};
    r["after"] = {
        {"src",    slotJson(src, hud)                },
        {"dst",    slotJson(dst, hud)                },
        {"cursor", slotJson({"cursor_items", 0}, hud)},
    };
    return r;
}

}
