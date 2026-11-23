// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/event/AutoRefillHandler.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <optional>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"

#include "ipnext/Log.h"
#include "ipnext/config/Configs.h"
#include "ipnext/event/LockSlotsHandler.h"
#include "ipnext/item/rule/file/RuleFiles.h"
#include "platform/i18n/I18n.h"
#include "platform/input/KeyCodes.h"
#include "platform/inventory/InventorySession.h"
#include "platform/inventory/SlotClicker.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/render/Gui2d.h"
#include "platform/screen/ScreenLayout.h"
#include "platform/screen/ScreenTracker.h"
#include "platform/util/Messages.h"

namespace inventory_utils::ipnext::event {

namespace {

using namespace config;
using item::ItemStack;
using item::ItemType;
using platform::i18n::translate;
namespace KC = platform::input::KeyCodes;

constexpr int  MAIN_HAND = -1;
constexpr char OVERLAY_TEXTURE[] = "inventoryprofilesnext/textures/gui/overlay_new.png";

LocalPlayer* localPlayer() {
    auto ci = ll::service::getClientInstance();
    return ci ? ci->getLocalPlayer() : nullptr;
}

int selectedSlot() {
    auto* p = localPlayer();
    return p ? p->getSelectedItemSlot() : 0;
}

ItemStack readInv(int invSlot) {
    auto* p = localPlayer();
    if (!p) return {};
    if (invSlot >= 0 && invSlot <= 35) return ItemStack::of(p->getInventory().getItem(invSlot));
    if (invSlot == 40) return ItemStack::of(p->getOffhandSlot());
    if (invSlot >= 36 && invSlot <= 39) return ItemStack::of(p->getArmor(static_cast<SharedTypes::Legacy::ArmorSlot>(invSlot - 36)));
    return {};
}

bool endsWith(std::string const& s, std::string const& suffix) { return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0; }

bool hasItemTag(ItemType const& t, char const* tag) {
    auto const* item = t.vanillaStack().mItem.get();
    return item && item->hasTag(HashedString(tag));
}

bool isFood(ItemType const& t) {
    auto const* item = t.vanillaStack().mItem.get();
    return item && item->isFood();
}

bool isHarmfulFood(ItemType const& t) {
    static std::set<std::string> const harmful{"minecraft:rotten_flesh", "minecraft:spider_eye", "minecraft:poisonous_potato", "minecraft:pufferfish", "minecraft:suspicious_stew"};
    return harmful.contains(t.itemId());
}

bool isBucket(ItemType const& t) { return endsWith(t.itemId(), "bucket"); }
bool isEmptyBucket(ItemType const& t) { return t.itemId() == "minecraft:bucket"; }
bool isFullBucket(ItemType const& t) { return isBucket(t) && !isEmptyBucket(t); }
bool isStew(ItemType const& t) {
    auto const& id = t.itemId();
    return id == "minecraft:mushroom_stew" || id == "minecraft:rabbit_stew" || id == "minecraft:beetroot_soup" || id == "minecraft:suspicious_stew";
}
bool isStackable(ItemType const& t) { return t.maxCount() > 1; }
bool hasEnchant(ItemType const& t, int id) {
    auto const& e = t.enchantments();
    return std::any_of(e.begin(), e.end(), [&](auto const& x) { return x.id == id; });
}

std::map<std::string, std::string> componentsWithoutDamage(ItemType const& t) {
    std::map<std::string, std::string> out;
    if (auto const* tag = t.tag()) {
        for (auto const& [k, v] : tag->mTags) {
            if (k == "Damage") continue;
            out[k] = v.get().toSnbt(SnbtFormat::Minimize, 0);
        }
    }
    if (t.aux() != 0 && !t.isDamageable()) out["#aux"] = std::to_string(t.aux());
    return out;
}

int getThreshold(ItemType const& t) {
    if (!t.isDamageable()) return 0;
    int v = AutoRefillSettings::THRESHOLD_UNIT.getOptionListValue() == "PERCENTAGE" ? AutoRefillSettings::TOOL_DAMAGE_THRESHOLD.getIntegerValue() * t.maxDamage() / 100
                                                                                    : AutoRefillSettings::TOOL_DAMAGE_THRESHOLD.getIntegerValue();
    return std::max(0, v);
}

std::string nameOf(ItemType const& t) { return t.hasCustomName() ? t.displayName() : t.translatedName(); }

void notify(ConfigOptionList const& type, std::string const& text) {
    auto const& v = type.getOptionListValue();
    if (v == "CHAT") platform::util::chat(text);
    else if (v == "SUBTITLE") platform::util::popup(text);
    else platform::util::actionBar(text);
}

std::string ipnPing() { return translate("inventoryprofiles.config.notification.tool_replace_ping.ipn"); }

}

struct AutoRefillHandler::Monitor {
    int                   watch;
    std::vector<Monitor*> anothers;
    ItemStack             storedItem;
    int                   storedSlotId = -1;
    int                   tickCount    = 0;
    ItemStack             lastTickItem;
    ItemStack             currentItem;
    int                   currentSlotId = -1;
    int                   lastNotifyDurability      = -1;
    int                   lastNotifyBreakDurability = -1;
    bool                  shouldHandle              = false;
    ItemStack             checkingItem;

    explicit Monitor(int w) : watch(w) {}

    [[nodiscard]] int id() const { return watch == MAIN_HAND ? selectedSlot() : watch; }
    [[nodiscard]] int index() const { return id(); }

    void updateCurrent() {
        lastTickItem  = currentItem;
        currentSlotId = id();
        currentItem   = readInv(currentSlotId);
    }

    void checkShouldHandle(AutoRefillHandler& h) { shouldHandle = currentSlotId == storedSlotId && !isSwapped() && shouldHandleItem(h); }

    void checkHandle(AutoRefillHandler& h) {
        if (shouldHandle) {
            if (tickCount >= AutoRefillSettings::AUTO_REFILL_WAIT_TICK.getIntegerValue()) {
                if (!handle(h)) return;
                updateCurrent();
                unhandled();
            } else {
                ++tickCount;
            }
        } else {
            unhandled();
        }
    }

    [[nodiscard]] bool isSwapped() const {
        if (currentItem == lastTickItem) return false;
        return std::any_of(anothers.begin(), anothers.end(), [&](Monitor const* a) { return currentItem == a->lastTickItem && lastTickItem == a->currentItem; });
    }

    void unhandled() {
        storedItem                = currentItem;
        storedSlotId              = currentSlotId;
        tickCount                 = 0;
        lastNotifyDurability      = storedItem.itemType.durability();
        lastNotifyBreakDurability = storedItem.itemType.durability();
    }

    bool shouldHandleItem(AutoRefillHandler& h) {
        if (AutoRefillSettings::AUTO_REFILL_ENABLE_PER_SLOT_CONFIG.getBooleanValue() && h.mDisabledSlots.contains(index())) return false;
        if (h.mProfilesSwapped.erase(id()) > 0) return false;
        checkingItem = storedItem;
        if (!storedItem.isEmpty() && h.mBlacklist.contains(storedItem.itemType.itemId())) return false;
        if (storedItem.isEmpty()) return false;
        if (currentItem.isEmpty()) return !(AutoRefillSettings::DISABLE_FOR_LOYALTY_ITEMS.getBooleanValue() && hasEnchant(storedItem.itemType, 31));
        auto const& t = currentItem.itemType;
        if (t.isDamageable()) {
            bool tempDisabled = h.mTicksAfterUp < 15;
            if (AutoRefillSettings::REFILL_BEFORE_TOOL_BREAK.getBooleanValue() && !tempDisabled) {
                bool allowBreak = AutoRefillSettings::ALLOW_BREAK_FOR_NON_ENCHANTED.getBooleanValue() && t.enchantments().empty()
                               && t.maxDamage() < AutoRefillSettings::TOOL_MAX_DURABILITY_THRESHOLD.getIntegerValue();
                if (!allowBreak) {
                    int threshold = getThreshold(t);
                    notifyDurabilityChange(t, t.durability(), threshold, true);
                    if (t.durability() <= threshold) {
                        checkingItem = currentItem;
                        return true;
                    }
                }
            } else if (AutoRefillSettings::ALLOW_ALERTS_WITHOUT_TOOL_PROTECTION.getBooleanValue()) {
                notifyDurabilityChange(t, t.durability(), 0, false);
            }
        }
        auto const& s = storedItem.itemType;
        if (isFullBucket(s) && isEmptyBucket(t)) return true;
        if (isEmptyBucket(s) && isFullBucket(t)) return true;
        if ((s.itemId() == "minecraft:potion" || s.itemId() == "minecraft:honey_bottle") && t.itemId() == "minecraft:glass_bottle") return true;
        if (isStew(s) && t.itemId() == "minecraft:bowl") return true;
        return isStackable(t) && currentItem.count <= AutoRefillSettings::STACKABLE_THRESHOLD.getIntegerValue();
    }

    bool handle(AutoRefillHandler& h) {
        auto const& itemType = checkingItem.itemType;
        int         found    = findCorrespondingSlot(checkingItem, currentItem);
        if (found < 0) {
            if (itemType.isDamageable()) notifyFailToChange();
            return true;
        }
        if (platform::inventory::InventorySession::busy()) return false;
        int  stored    = storedSlotId;
        bool stackable = isStackable(currentItem.itemType);
        auto replacing = readInv(found).itemType;
        bool requested = platform::inventory::InventorySession::request([stored, found, stackable](ContainerScreenController& csc, ContainerManagerController& mgr) {
            auto menu = platform::inventory::ContainerMenu::fromManager(mgr, "inventory_screen", &csc);
            platform::inventory::SlotClicker clicker(csc, menu);
            auto slotOf = [&](int inv) -> int {
                for (auto const& s : clicker.menu().slots()) {
                    if (s.kind != platform::inventory::InventoryKind::Container && s.containerSlot == inv) return s.slotNumber;
                }
                return -1;
            };
            int storedId = slotOf(stored), foundId = slotOf(found);
            if (storedId < 0 || foundId < 0) return;
            if (stackable) clicker.shiftClickSlot(storedId);
            if (stored >= 0 && stored <= 8) {
                clicker.clickSlot(foundId, stored, platform::inventory::ClickType::SWAP);
            } else {
                clicker.leftClickSlot(foundId);
                clicker.leftClickSlot(storedId);
                auto const& carried = clicker.menu().getCarried();
                if (!carried.isNull() && carried.mCount > 0) clicker.leftClickSlot(foundId);
            }
        });
        if (!requested) return false;
        ++h.mHandledCount;
        h.mLastAction = "refill " + std::to_string(stored) + " <- " + std::to_string(found) + " (" + itemType.itemId() + ")";
        Log::debug("auto refill: {}", h.mLastAction);
        if (itemType.isDamageable()) notifySuccessfulChange(itemType, replacing);
        return true;
    }

    void notifySuccessfulChange(ItemType const& itemType, ItemType const& replacingWith) {
        if (!AutoRefillSettings::VISUAL_REPLACE_SUCCESS_NOTIFICATION.getBooleanValue()) return;
        notify(AutoRefillSettings::TYPE_VISUAL_REPLACE_SUCCESS_NOTIFICATION,
               ipnPing() + " - \"" + nameOf(itemType) + "\" " + translate("inventoryprofiles.config.notification.tool_replace_success") + " \"" + nameOf(replacingWith) + "\"");
    }

    void notifyFailToChange() {
        if (currentItem.isEmpty() || !AutoRefillSettings::VISUAL_REPLACE_FAILED_NOTIFICATION.getBooleanValue()) return;
        auto const& t = currentItem.itemType;
        int         d = t.durability();
        if (d > getThreshold(t) || d == lastNotifyBreakDurability) return;
        lastNotifyBreakDurability = d;
        notify(AutoRefillSettings::TYPE_VISUAL_REPLACE_FAILED_NOTIFICATION,
               ipnPing() + translate("inventoryprofiles.config.notification.tool_replace_ping.warning") + " - "
                   + translate("inventoryprofiles.config.notification.tool_replace_failed.replacing", {nameOf(t)}));
    }

    void notifyDurabilityChange(ItemType const& t, int durability, int threshold, bool replacing) {
        if (!AutoRefillSettings::VISUAL_DURABILITY_NOTIFICATION.getBooleanValue() || !isItNotifyStep(durability, threshold)) return;
        std::string text = ipnPing() + " - " + translate("inventoryprofiles.config.notification.tool_replace_ping.durability", {nameOf(t), std::to_string(durability)});
        if (replacing) text += translate("inventoryprofiles.config.notification.tool_replace_ping.replacing", {std::to_string(threshold)});
        notify(AutoRefillSettings::TYPE_VISUAL_DURABILITY_NOTIFICATION, text);
    }

    bool isItNotifyStep(int durability, int threshold) {
        if (!(storedItem.itemType == lastTickItem.itemType)) lastNotifyDurability = -1;
        if (lastNotifyDurability != durability && durability != threshold) {
            int num  = AutoRefillSettings::NUMBER_OF_NOTIFICATIONS.getIntegerValue();
            int step = AutoRefillSettings::NOTIFICATION_STEP.getIntegerValue();
            for (int i = threshold; i <= threshold + num * step; i += step) {
                if (durability == i) {
                    lastNotifyDurability = i;
                    return true;
                }
            }
        }
        return false;
    }

    static bool checkNbtIfNeeded(ItemType const& candidate, ItemType const& itemType) {
        if (!AutoRefillSettings::AUTO_REFILL_MATCH_NBT.getBooleanValue()) return true;
        if (isBucket(itemType) && AutoRefillSettings::AUTO_REFILL_IGNORE_NBT_FOR_BUCKETS.getBooleanValue()) return true;
        auto in  = componentsWithoutDamage(itemType);
        auto out = componentsWithoutDamage(candidate);
        if (AutoRefillSettings::AUTO_REFILL_MATCH_NBT_TYPE.getOptionListValue() == "CAN_HAVE_EXTRA") {
            if (in.empty() || out.empty()) return in.empty() && out.empty();
            return std::all_of(in.begin(), in.end(), [&](auto const& kv) {
                auto it = out.find(kv.first);
                return it != out.end() && it->second == kv.second;
            });
        }
        return in == out;
    }

    static bool typeItemMatch(ItemType const& candidate, ItemType const& itemType) {
        if (candidate.itemId() != itemType.itemId()) return false;
        if ((itemType.hasCustomName() || candidate.hasCustomName()) && AutoRefillSettings::AUTO_REFILL_MATCH_CUSTOM_NAME.getBooleanValue()) {
            if (candidate.customName() != itemType.customName()) return false;
        }
        return checkNbtIfNeeded(candidate, itemType);
    }

    using Candidates = std::vector<std::pair<int, ItemStack>>;

    static Candidates filter(Candidates const& in, std::function<bool(ItemStack const&)> const& pred) {
        Candidates out;
        for (auto const& c : in) {
            if (pred(c.second)) out.push_back(c);
        }
        return out;
    }

    static Candidates defaultItemMatch(Candidates const& filtered, ItemType const& itemType) {
        auto exact = filter(filtered, [&](ItemStack const& s) { return typeItemMatch(s.itemType, itemType); });
        if (!exact.empty()) return exact;
        bool allowHarmful = AutoRefillSettings::AUTO_REFILL_MATCH_HARMFUL_FOOD.getBooleanValue();
        if (AutoRefillSettings::AUTO_REFILL_MATCH_ANY_FOOD.getBooleanValue() && isFood(itemType)) {
            return filter(filtered, [&](ItemStack const& s) { return isFood(s.itemType) && (!isHarmfulFood(s.itemType) || allowHarmful); });
        }
        return {};
    }

    static int findCorrespondingSlot(ItemStack const& checking, ItemStack const& current) {
        auto       locked = LockSlotsHandler::get().lockedInvSlots();
        Candidates filtered;
        for (int inv = 9; inv <= 35; ++inv) {
            if (!AutoRefillSettings::IGNORE_LOCKED_SLOTS.getBooleanValue() && std::find(locked.begin(), locked.end(), inv) != locked.end()) continue;
            filtered.emplace_back(inv, readInv(inv));
        }
        auto const& itemType = checking.itemType;
        if (itemType.isDamageable()) {
            int threshold = AutoRefillSettings::REFILL_BEFORE_TOOL_BREAK.getBooleanValue() ? getThreshold(itemType) : -1;
            filtered      = filter(filtered, [&](ItemStack const& s) { return !s.isEmpty() && s.itemType.isDamageable() && s.itemType.durability() > threshold; });
            auto const& id = itemType.itemId();
            auto        sameCategory = [&](std::function<bool(ItemType const&)> is) { filtered = filter(filtered, [&](ItemStack const& s) { return is(s.itemType); }); };
            if (endsWith(id, "_leggings")) sameCategory([](ItemType const& t) { return endsWith(t.itemId(), "_leggings"); });
            else if (endsWith(id, "_boots")) sameCategory([](ItemType const& t) { return endsWith(t.itemId(), "_boots"); });
            else if (endsWith(id, "_helmet") || id == "minecraft:turtle_helmet") sameCategory([](ItemType const& t) { return endsWith(t.itemId(), "_helmet"); });
            else if (endsWith(id, "_chestplate")) sameCategory([](ItemType const& t) { return endsWith(t.itemId(), "_chestplate"); });
            else if (hasItemTag(itemType, "minecraft:is_sword")) sameCategory([](ItemType const& t) { return hasItemTag(t, "minecraft:is_sword"); });
            else if (hasItemTag(itemType, "minecraft:is_shovel")) sameCategory([](ItemType const& t) { return hasItemTag(t, "minecraft:is_shovel"); });
            else if (hasItemTag(itemType, "minecraft:is_pickaxe")) sameCategory([](ItemType const& t) { return hasItemTag(t, "minecraft:is_pickaxe"); });
            else if (hasItemTag(itemType, "minecraft:is_axe")) sameCategory([](ItemType const& t) { return hasItemTag(t, "minecraft:is_axe"); });
            else if (hasItemTag(itemType, "minecraft:is_hoe")) sameCategory([](ItemType const& t) { return hasItemTag(t, "minecraft:is_hoe"); });
            else if (id == "minecraft:fishing_rod") sameCategory([](ItemType const& t) { return t.itemId() == "minecraft:fishing_rod"; });
            else filtered = defaultItemMatch(filtered, itemType);
        } else if (!itemType.comparablePotionEffects().empty()) {
            auto const& want = itemType.comparablePotionEffects();
            filtered         = filter(filtered, [&](ItemStack const& s) {
                auto const& have = s.itemType.comparablePotionEffects();
                return std::all_of(want.begin(), want.end(), [&](auto const& w) {
                    return std::any_of(have.begin(), have.end(), [&](auto const& x) { return x.effect == w.effect; });
                });
            });
        } else {
            filtered = defaultItemMatch(filtered, itemType);
            if (isStackable(itemType) && itemType.itemId() == current.itemType.itemId()) {
                filtered = filter(filtered, [&](ItemStack const& s) { return s.count > current.count; });
            }
        }
        if (filtered.empty()) return -1;
        auto best = item::rule::file::RuleFileRegister::getCustomRuleOrEmpty("auto_refill_best");
        bool smaller = AutoRefillSettings::AUTO_REFILL_PREFER_SMALLER_STACKS.getBooleanValue();
        std::stable_sort(filtered.begin(), filtered.end(), [&](auto const& a, auto const& b) {
            auto const& at = a.second.itemType;
            auto const& bt = b.second.itemType;
            bool am = at.itemId() == itemType.itemId(), bm = bt.itemId() == itemType.itemId();
            if (am != bm) return am;
            if (at.maxDamage() != bt.maxDamage()) return at.maxDamage() > bt.maxDamage();
            if (int r = best->compare(at, bt); r != 0) return r < 0;
            if (a.second.count != b.second.count) return smaller ? a.second.count < b.second.count : a.second.count > b.second.count;
            return false;
        });
        return filtered.front().first;
    }
};

AutoRefillHandler::AutoRefillHandler() { init(); }

AutoRefillHandler& AutoRefillHandler::get() {
    static AutoRefillHandler instance;
    return instance;
}

void AutoRefillHandler::init() {
    for (auto* m : mMonitors) delete m;
    mMonitors.clear();
    mMonitors.push_back(new Monitor(MAIN_HAND));
    mMonitors.push_back(new Monitor(40));
    if (AutoRefillSettings::REFILL_ARMOR.getBooleanValue()) {
        for (int i = 36; i <= 39; ++i) mMonitors.push_back(new Monitor(i));
    }
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < mMonitors.size(); ++j) {
            if (i != j) mMonitors[i]->anothers.push_back(mMonitors[j]);
        }
    }
}

void AutoRefillHandler::blackListChanged() {
    mBlacklist.clear();
    std::string const& v = AutoRefillSettings::AUTOREFILL_BLACKLIST.getStringValue();
    size_t             start = 0;
    while (start <= v.size()) {
        auto        p = v.find(',', start);
        std::string e = v.substr(start, p == std::string::npos ? std::string::npos : p - start);
        auto        b = e.find_first_not_of(' '), en = e.find_last_not_of(' ');
        if (b != std::string::npos) mBlacklist.insert(e.substr(b, en - b + 1));
        if (p == std::string::npos) break;
        start = p + 1;
    }
}

void AutoRefillHandler::onClientTick() {
    if (!Features::ENABLE_AUTO_REFILL.getBooleanValue()) return;
    if (platform::inventory::InventorySession::busy()) return;
    if (isPressing(AutoRefillSettings::AUTO_REFILL_TEMP_DISABLE_REFILL_FOR_TOOLS)) mTicksAfterUp = 0;
    else if (mTicksAfterUp < 16) ++mTicksAfterUp;
    bool dropKey = platform::input::InputHandler::get().isKeyDown('Q');
    if (platform::screen::ScreenTracker::isGuiOpen() || (AutoRefillSettings::DISABLE_FOR_DROP_ITEM.getBooleanValue() && dropKey)) {
        mScreenOpening = true;
        return;
    }
    if (!localPlayer()) return;
    if (mScreenOpening) {
        mScreenOpening = false;
        init();
    }
    platform::item::ScopedItemRegistry registry;
    try {
        for (auto* m : mMonitors) m->updateCurrent();
        for (auto* m : mMonitors) m->checkShouldHandle(*this);
        for (auto* m : mMonitors) m->checkHandle(*this);
    } catch (std::exception const& e) {
        Log::warn("auto refill: {}", e.what());
    }
}

void AutoRefillHandler::onJoinWorld(std::string const& worldName) {
    mWorldName = worldName;
    blackListChanged();
    loadDisabled();
    init();
}

std::filesystem::path AutoRefillHandler::disabledFile() const {
    std::string dir = LockedSlotsSettings::ENABLE_LOCK_SLOTS_PER_SERVER.getBooleanValue() ? mWorldName : std::string();
    for (auto& c : dir) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') c = '_';
    }
    auto            d = configDir() / std::filesystem::u8path(dir);
    std::error_code ec;
    std::filesystem::create_directories(d, ec);
    return d / "refillDisabledSlots.txt";
}

void AutoRefillHandler::loadDisabled() {
    mDisabledSlots.clear();
    mCachedDisabled.clear();
    std::ifstream in(disabledFile(), std::ios::binary);
    std::string   line;
    while (std::getline(in, line)) {
        try {
            mDisabledSlots.insert(std::stoi(line));
        } catch (...) {
        }
    }
    mCachedDisabled.assign(mDisabledSlots.begin(), mDisabledSlots.end());
}

void AutoRefillHandler::saveDisabled() {
    std::vector<int> sorted(mDisabledSlots.begin(), mDisabledSlots.end());
    if (sorted == mCachedDisabled) return;
    mCachedDisabled = sorted;
    std::ofstream out(disabledFile(), std::ios::binary | std::ios::trunc);
    for (size_t i = 0; i < sorted.size(); ++i) out << (i ? "\n" : "") << sorted[i];
}

void AutoRefillHandler::toggleDisableRefillForSlot(int index) {
    if (mDisabledSlots.contains(index)) mDisabledSlots.erase(index);
    else mDisabledSlots.insert(index);
    saveDisabled();
}

bool AutoRefillHandler::onGameToggleKey() {
    if (!Features::ENABLE_AUTO_REFILL.getBooleanValue() || !AutoRefillSettings::AUTO_REFILL_ENABLE_PER_SLOT_CONFIG.getBooleanValue()) return false;
    if (platform::screen::ScreenTracker::isGuiOpen()) return false;
    toggleDisableRefillForSlot(selectedSlot());
    init();
    return true;
}

namespace {
bool isRefillSlot(platform::inventory::MenuSlot const& s, std::string const& screen) {
    if (s.kind == platform::inventory::InventoryKind::Hotbar) return true;
    if (screen != "inventory_screen") return false;
    return s.kind == platform::inventory::InventoryKind::Armor || s.kind == platform::inventory::InventoryKind::Offhand;
}
}

bool AutoRefillHandler::onMouseClick(double, double, int buttonCode, bool buttonState) {
    if (!Features::ENABLE_AUTO_REFILL.getBooleanValue() || !AutoRefillSettings::AUTO_REFILL_ENABLE_PER_SLOT_CONFIG.getBooleanValue()) return false;
    if (buttonCode != KC::MOUSE_LEFT) return false;
    if (!buttonState) {
        if (!mClicked) return false;
        mClicked = false;
        saveDisabled();
        return true;
    }
    if (!isPressing(Hotkeys::AUTO_REFILL_GUI_TOGGLE_FOR_SLOT)) return false;
    auto clicker = platform::inventory::SlotClicker::current();
    if (!clicker.valid()) return false;
    auto const* s = clicker.getSlotUnderMouse();
    if (!s || !isRefillSlot(*s, clicker.menu().screenName())) return false;
    mClicked = true;
    mMode    = mDisabledSlots.contains(s->containerSlot) ? 1 : 0;
    if (mMode == 0) mDisabledSlots.insert(s->containerSlot);
    else mDisabledSlots.erase(s->containerSlot);
    return true;
}

void AutoRefillHandler::onContainerFrame() {
    if (!mClicked) return;
    if (!isPressing(Hotkeys::AUTO_REFILL_GUI_TOGGLE_FOR_SLOT)) {
        mClicked = false;
        saveDisabled();
        return;
    }
    auto clicker = platform::inventory::SlotClicker::current();
    if (!clicker.valid()) return;
    auto const* s = clicker.getSlotUnderMouse();
    if (!s || !isRefillSlot(*s, clicker.menu().screenName())) return;
    if (mMode == 0) mDisabledSlots.insert(s->containerSlot);
    else mDisabledSlots.erase(s->containerSlot);
}

void AutoRefillHandler::onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx) {
    if (!Features::ENABLE_AUTO_REFILL.getBooleanValue() || !AutoRefillSettings::AUTO_REFILL_ENABLE_INDICATOR_ICONS.getBooleanValue()
        || !AutoRefillSettings::AUTO_REFILL_ENABLE_PER_SLOT_CONFIG.getBooleanValue())
        return;
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    auto const& screen = platform::screen::ScreenTracker::containerScreenName();
    auto        rects  = platform::screen::collectSlotRects(view);
    platform::render::Gui2d g(ctx, *ci);
    for (auto const& [key, r] : rects) {
        int inv = -1;
        if (key.first == "hotbar_items") inv = key.second;
        else if (screen == "inventory_screen" && key.first == "armor_items") inv = 36 + key.second;
        else if (screen == "inventory_screen" && key.first == "offhand_items") inv = 40;
        if (inv < 0 || mDisabledSlots.contains(inv)) continue;
        g.blitPixels(OVERLAY_TEXTURE, r.x + 1 + 8 - 16, r.y + 1 + 8 - 16, 40, 72, 32, 32, 256, 256);
    }
    g.flush();
}

void AutoRefillHandler::onHudOverlay(ScreenView&, MinecraftUIRenderContext& ctx) {
    if (!Features::ENABLE_AUTO_REFILL.getBooleanValue() || !AutoRefillSettings::AUTO_REFILL_ENABLE_INDICATOR_ICONS.getBooleanValue()
        || !AutoRefillSettings::AUTO_REFILL_ENABLE_PER_SLOT_CONFIG.getBooleanValue() || !AutoRefillSettings::AUTO_REFILL_ENABLE_HORBAR_INDICATOR_ICONS.getBooleanValue())
        return;
    auto ci = ll::service::getClientInstance();
    if (!ci || !ci->getLocalPlayer()) return;
    platform::render::Gui2d g(ctx, *ci);
    int                     i = g.screenWidth() / 2;
    for (int j = 0; j <= 8; ++j) {
        if (mDisabledSlots.contains(j)) continue;
        int x = i - 90 + j * 20 + 2;
        int y = g.screenHeight() - 16 - 3;
        g.blitPixels(OVERLAY_TEXTURE, x + 8 - 16, y + 8 - 16, 40, 72, 32, 32, 256, 256);
    }
    g.flush();
}

}
