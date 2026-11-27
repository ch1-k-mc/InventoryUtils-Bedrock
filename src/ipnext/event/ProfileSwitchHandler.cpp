// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/event/ProfileSwitchHandler.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/world/ContainerID.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/enchanting/Enchant.h"

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/event/AutoRefillHandler.h"
#include "ipnext/item/rule/file/RuleFiles.h"
#include "platform/i18n/I18n.h"
#include "platform/inventory/InventorySession.h"
#include "platform/inventory/LegacySwap.h"
#include "platform/inventory/SlotClicker.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/screen/ScreenTracker.h"
#include "platform/util/Messages.h"

namespace inventory_utils::ipnext::event {

namespace {

using namespace config;
using namespace profiles;
using platform::inventory::ClickType;
using platform::inventory::SlotClicker;

LocalPlayer* localPlayer() {
    auto ci = ll::service::getClientInstance();
    return ci ? ci->getLocalPlayer() : nullptr;
}

std::vector<int> allSlots() {
    std::vector<int> v;
    for (int i = 9; i <= 35; ++i) v.push_back(i);
    for (int i = 0; i <= 8; ++i) v.push_back(i);
    v.push_back(40);
    for (int i = 36; i <= 39; ++i) v.push_back(i);
    return v;
}

std::string sanitized(std::string s) {
    for (auto& c : s) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') c = '_';
    }
    return s;
}

struct Applier {
    SlotClicker* clicker = nullptr;

    int menuSlotOf(int inv) const {
        for (auto const& s : clicker->menu().slots()) {
            if (s.kind != platform::inventory::InventoryKind::Container && s.containerSlot == inv) return s.slotNumber;
        }
        return -1;
    }
    item::ItemStack itemAt(int inv) const {
        if (!clicker) return item::ItemStack::of(platform::inventory::legacy::itemAt(inv));
        int id = menuSlotOf(inv);
        if (id < 0) return {};
        auto const* s = clicker->menu().getSlot(id);
        return s ? item::ItemStack::of(clicker->menu().getItem(*s)) : item::ItemStack();
    }

    void swapSlots(int to, int found) {
        AutoRefillHandler::get().markProfileSwapped(to);
        if (!clicker) {
            platform::inventory::legacy::swapSlots(to, found);
            return;
        }
        int toId = menuSlotOf(to), foundId = menuSlotOf(found);
        if (toId < 0 || foundId < 0) return;
        if (to >= 0 && to <= 8) {
            clicker->clickSlot(foundId, to, ClickType::SWAP);
        } else {
            clicker->leftClickSlot(foundId);
            clicker->leftClickSlot(toId);
            auto const& carried = clicker->menu().getCarried();
            if (!carried.isNull() && carried.mCount > 0) clicker->leftClickSlot(foundId);
        }
    }

    std::optional<int> bestMatch(ProfileSlot const& slotData, ProfileItemData const& to, std::vector<int> const& from) const {
        auto             rule = item::rule::file::RuleFileRegister::getCustomRuleOrEmpty(slotData.order.value_or("auto_refill_best"));
        std::vector<int> candidates;
        for (int inv : from) {
            auto s = itemAt(inv);
            if (!s.isEmpty() && to.match(s.itemType)) candidates.push_back(inv);
        }
        std::stable_sort(candidates.begin(), candidates.end(), [&](int i, int j) { return rule->compare(itemAt(i).itemType, itemAt(j).itemType) < 0; });
        if (candidates.empty()) return std::nullopt;
        return candidates.back();
    }

    bool findAndSwap(ProfileSlot const& slotData, std::vector<int> const& sourceSlots) {
        int slot = invSlotOf(slotData.id);
        if (slot < 0) return false;
        if (!slotData.items.empty()) {
            for (auto const& target : slotData.items) {
                if (auto with = bestMatch(slotData, target, sourceSlots)) {
                    if (*with != slot) swapSlots(slot, *with);
                    return true;
                }
            }
            return false;
        }
        std::vector<int> targets;
        for (int inv : allSlots()) {
            if (itemAt(inv).isEmpty()) targets.push_back(inv);
        }
        std::vector<int> noHotbar;
        for (int t : targets) {
            if (t > 8) noHotbar.push_back(t);
        }
        if (!noHotbar.empty()) targets = noHotbar;
        for (int t : targets) {
            if (itemAt(t).isEmpty()) {
                if (itemAt(slot).isEmpty()) return true;
                swapSlots(slot, t);
                return true;
            }
        }
        return false;
    }
};

}

ProfileSwitchHandler& ProfileSwitchHandler::get() {
    static ProfileSwitchHandler instance;
    return instance;
}

std::optional<std::string> ProfileSwitchHandler::activeProfileName() const {
    if (!mTarget.valid) return std::nullopt;
    return mTarget.name;
}

std::filesystem::path ProfileSwitchHandler::file() const {
    std::string     dir = EditProfiles::PROFILES_PER_SERVER.getBooleanValue() ? sanitized(mWorldName) : std::string();
    auto            d   = configDir() / std::filesystem::u8path(dir);
    std::error_code ec;
    std::filesystem::create_directories(d, ec);
    return d / "profiles-V2.txt";
}

void ProfileSwitchHandler::onJoinWorld(std::string const& worldName) {
    mWorldName = worldName;
    load(false);
}

void ProfileSwitchHandler::load(bool fromUserInput) {
    mProfiles.clear();
    mSavedProfiles.clear();
    if (fromUserInput) platform::util::chat("Loading profiles...");
    std::ifstream in(file(), std::ios::binary);
    if (in) {
        std::stringstream ss;
        ss << in.rdbuf();
        try {
            auto temp = ProfilesConfig::getProfiles(ss.str());
            if (fromUserInput) platform::util::chat("Found " + std::to_string(temp.size()) + " profiles");
            for (auto& p : temp) {
                std::string upper = p.name;
                std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                (upper == "SAVED" ? mSavedProfiles : mProfiles).push_back(std::move(p));
            }
        } catch (ProfileSyntaxError const& e) {
            Log::warn("profiles: line {}: {}", e.line, e.what());
            platform::util::chat(std::string("Loading Profile settings failed: ") + e.what());
        }
    }
    reloadActiveProfile();
}

void ProfileSwitchHandler::save() {
    std::ofstream out(file(), std::ios::binary | std::ios::trunc);
    out << ProfilesConfig::asString(mProfiles) << "\n\n\n\n" << ProfilesConfig::asString(mSavedProfiles);
}

void ProfileSwitchHandler::reloadActiveProfile() {
    auto activeName  = activeProfileName();
    mActiveProfileId = -1;
    init(ProfileData{"", ProfileSlotId::NONE, {}, false});
    if (activeName) {
        switchToProfileName(*activeName);
        mDoApply = false;
    }
}

void ProfileSwitchHandler::init(ProfileData const& newProfile) {
    if (mTarget == newProfile) {
        mTarget  = newProfile;
        mDoApply = mTarget.valid;
    } else {
        mTarget = newProfile;
        if (mTarget.valid) mDoApply = true;
    }
}

int ProfileSwitchHandler::byName(std::string const& name) const {
    for (size_t i = 0; i < mProfiles.size(); ++i) {
        if (mProfiles[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

bool ProfileSwitchHandler::switchToProfileName(std::string const& name) {
    int index = byName(name);
    if (index < 0) return false;
    mActiveProfileId = index;
    init(mProfiles[index]);
    return true;
}

int ProfileSwitchHandler::profileCount() const { return static_cast<int>(mProfiles.size() + mSavedProfiles.size()); }

profiles::ProfileData ProfileSwitchHandler::profileAt(int index) const {
    auto const named = static_cast<int>(mProfiles.size());
    if (index < named) return mProfiles[static_cast<std::size_t>(index)];
    auto p = mSavedProfiles[static_cast<std::size_t>(index - named)];
    p.name = "Saved " + std::to_string(index - named + 1);
    return p;
}

int ProfileSwitchHandler::nextOrFirst() {
    int next         = mActiveProfileId + 1;
    mActiveProfileId = next < profileCount() ? next : 0;
    return mActiveProfileId;
}

int ProfileSwitchHandler::prevOrLast() {
    int next         = mActiveProfileId - 1;
    mActiveProfileId = next >= 0 ? next : profileCount() - 1;
    return mActiveProfileId;
}

void ProfileSwitchHandler::applyCurrent() { mDoApply = mTarget.valid; }

bool ProfileSwitchHandler::onPrevProfile() {
    if (!Features::ENABLE_PROFILES.getBooleanValue()) return false;
    if (profileCount() > 0) init(profileAt(nextOrFirst()));
    else platform::util::actionBar(platform::i18n::translate("inventoryutils.profiles.none"));
    return true;
}

bool ProfileSwitchHandler::onNextProfile() {
    if (!Features::ENABLE_PROFILES.getBooleanValue()) return false;
    if (profileCount() > 0) init(profileAt(prevOrLast()));
    else platform::util::actionBar(platform::i18n::translate("inventoryutils.profiles.none"));
    return true;
}

bool ProfileSwitchHandler::onApplyProfile() {
    if (!Features::ENABLE_PROFILES.getBooleanValue()) return false;
    if (!mTarget.valid && profileCount() > 0) {
        mActiveProfileId = profileCount() - 1;
        init(profileAt(mActiveProfileId));
    }
    if (!mTarget.valid) platform::util::actionBar(platform::i18n::translate("inventoryutils.profiles.none"));
    applyCurrent();
    return true;
}

bool ProfileSwitchHandler::onQuickProfile(int index) {
    if (!Features::ENABLE_PROFILES.getBooleanValue() || index < 0 || index > 9) return false;
    auto const& name = EditProfiles::QUICK_SLOT_PROFILES[index].getStringValue();
    if (!name.empty() && switchToProfileName(name)) return true;
    if (index >= profileCount()) {
        platform::util::actionBar(platform::i18n::translate("inventoryutils.profiles.noSlot", {std::to_string((index + 1) % 10)}));
        return true;
    }
    mActiveProfileId = index;
    init(profileAt(index));
    return true;
}

ProfileData ProfileSwitchHandler::createProfileFromCurrentState() const {
    ProfileData p{"Saved", ProfileSlotId::NONE, {}, true};
    auto*       player = localPlayer();
    if (!player) return p;
    p.active = slotIdOfInvSlot(player->getSelectedItemSlot());
    for (int inv : allSlots()) {
        auto id = slotIdOfInvSlot(inv);
        if (id == ProfileSlotId::NONE) continue;
        ::ItemStack const* v = nullptr;
        if (inv <= 35) v = &player->getInventory().getItem(inv);
        else if (inv == 40) v = &player->getOffhandSlot();
        else v = &player->getArmor(static_cast<SharedTypes::Legacy::ArmorSlot>(inv - 36));
        auto stack = item::ItemStack::of(*v);
        if (stack.isEmpty()) continue;
        ProfileItemData d;
        d.itemId     = {ProfileItemId::Kind::Id, stack.itemType.itemId()};
        d.customName = EditProfiles::INCLUDE_CUSTOM_NAME.getBooleanValue() ? stack.itemType.customName() : std::string();
        auto const& ench = stack.itemType.enchantments();
        if (!ench.empty()) {
            std::string snbt = "{";
            auto&       list = Enchant::mEnchants();
            for (size_t i = 0; i < ench.size(); ++i) {
                std::string name = (ench[i].id >= 0 && ench[i].id < static_cast<int>(list.size()) && list[ench[i].id]) ? list[ench[i].id]->mStringId->getString() : std::to_string(ench[i].id);
                snbt += (i ? "," : "") + std::string("'minecraft:") + name + "':" + std::to_string(ench[i].level);
            }
            snbt += "}";
            d.components = std::vector<ProfileComponentData>{{"minecraft:enchantments", snbt}};
        }
        p.slots.push_back(ProfileSlot{id, std::nullopt, {d}});
    }
    return p;
}

bool ProfileSwitchHandler::onSaveAsProfile() {
    if (!Features::ENABLE_PROFILES.getBooleanValue()) return false;
    platform::item::ScopedItemRegistry registry;
    mSavedProfiles.push_back(createProfileFromCurrentState());
    save();
    mActiveProfileId = profileCount() - 1;
    mTarget          = profileAt(mActiveProfileId);
    mDoApply         = false;
    platform::util::chat(platform::i18n::translate("inventoryprofiles.profiles.created_new_saved"));
    return true;
}

void ProfileSwitchHandler::onClientTick() {
    if (!mDoApply || !Features::ENABLE_PROFILES.getBooleanValue()) return;
    if (platform::inventory::InventorySession::busy() || !localPlayer()) return;
    auto const& screen = platform::screen::ScreenTracker::containerScreenName();
    bool        ok     = !platform::screen::ScreenTracker::isGuiOpen() || (platform::screen::ScreenTracker::containerScreen() && screen == "inventory_screen");
    if (!ok) return;
    mDoApply = false;
    applyProfile();
}

void ProfileSwitchHandler::applyProfile() {
    if (GuiSettings::ENABLE_PROFILES_ANNOUNCEMENT.getBooleanValue()) platform::util::actionBar(mTarget.name);
    ProfileData target = mTarget;
    if (platform::inventory::legacy::available()) {
        platform::item::ScopedItemRegistry registry;
        Applier                            a{nullptr};
        auto                               sourceSlots = allSlots();
        for (auto const& slot : target.slots) {
            if (a.findAndSwap(slot, sourceSlots)) std::erase(sourceSlots, invSlotOf(slot.id));
        }
        mLastResult = "applied " + target.name;
        if (target.active != ProfileSlotId::NONE) {
            if (auto* p = localPlayer()) p->mInventory->selectSlot(invSlotOf(target.active), ContainerID::Inventory);
        }
        return;
    }
    bool        requested = platform::inventory::InventorySession::request(
        [target](ContainerScreenController& csc, ContainerManagerController& mgr) {
            auto        menu = platform::inventory::ContainerMenu::fromManager(mgr, "inventory_screen", &csc);
            SlotClicker clicker(csc, menu);
            Applier     a{&clicker};
            auto        sourceSlots = allSlots();
            for (auto const& slot : target.slots) {
                if (a.findAndSwap(slot, sourceSlots)) {
                    int s = invSlotOf(slot.id);
                    std::erase(sourceSlots, s);
                }
            }
        },
        platform::inventory::InventorySession::Hide::Invisible,
        [this, target](bool ok, std::string const& reason) {
            mLastResult = (ok ? "applied " : "failed ") + target.name + (reason.empty() ? "" : " (" + reason + ")");
            if (target.active != ProfileSlotId::NONE) {
                if (auto* p = localPlayer()) p->mInventory->selectSlot(invSlotOf(target.active), ContainerID::Inventory);
            }
        }
    );
    if (!requested) mDoApply = true;
}

}
