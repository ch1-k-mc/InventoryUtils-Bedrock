// SPDX-License-Identifier: AGPL-3.0-only
#include "favorites/HotbarFavorites.h"

#include <filesystem>
#include <algorithm>
#include <fstream>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/input/MouseAction.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/MinecraftPackets.h"
#include "mc/network/packet/CommandRequestPacket.h"
#include "mc/server/commands/CommandOriginType.h"
#include "mc/server/commands/CurrentCmdVersion.h"
#include "mc/world/item/Item.h"
#include "mc/world/level/Level.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/containers/controllers/ContainerController.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/containers/models/ContainerModel.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "nlohmann/json.hpp"

#include "ipnext/item/ItemType.h"
#include "malilib/hotkeys/KeybindMulti.h"
#include "platform/config/ConfigRegistry.h"
#include "platform/i18n/I18n.h"
#include "platform/input/KeyCodes.h"
#include "platform/inventory/SlotClicker.h"
#include "platform/item/ItemSerializer.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/render/Gui2d.h"
#include "platform/screen/ScreenTracker.h"
#include "platform/util/Messages.h"

namespace inventory_utils::favorites {

namespace config {
namespace {
using malilib::hotkeys::KeyAction;
using malilib::hotkeys::KeybindSettings;
KeybindSettings const ANY_KEYS = KeybindSettings::create(KeybindSettings::Context::ANY, KeyAction::PRESS, false, true, false, true);
template <class T>
T applied(T o) {
    o.apply("inventoryutils.favorites");
    return o;
}
}
ConfigBoolean ENABLED     = applied(ConfigBoolean("enabled", true));
ConfigHotkey  SAVE_HOTBAR = applied(ConfigHotkey("saveHotbar", "", ANY_KEYS));

std::vector<std::pair<std::string, std::vector<IConfigBase*>>> categories() { return {{"Generic", {&ENABLED, &SAVE_HOTBAR}}}; }

namespace {
std::filesystem::path                                          file() { return ll::mod::NativeMod::current()->getConfigDir() / "hotbarfavorites.json"; }
void                                                           saveToFile() {
    std::error_code ec;
    std::filesystem::create_directories(file().parent_path(), ec);
    nlohmann::json root = nlohmann::json::object();
    for (auto& [cat, opts] : categories()) writeConfigBase(root, cat, opts);
    std::ofstream out(file(), std::ios::binary | std::ios::trunc);
    out << root.dump(2);
}
void loadFromFile() {
    std::ifstream in(file(), std::ios::binary);
    if (!in) {
        saveToFile();
        return;
    }
    auto root = nlohmann::json::parse(in, nullptr, false);
    if (!root.is_object()) return;
    for (auto& [cat, opts] : categories()) readConfigBase(root, cat, opts);
}
}
}

namespace {

constexpr char const* RESERVE_ID        = "inventoryutils:hotbar_favorites";
constexpr char const* CREATIVE_LIST     = "recipe_search";
constexpr int         BUTTON_SIZE       = 10;
constexpr int         CELL              = 12;
constexpr int         PAD               = 3;
constexpr int         HEADER            = 13;
constexpr int         PANEL_W           = CELL * 9 + PAD * 2;

using IconRow = std::vector<std::unique_ptr<ItemStack>>;
auto* gIcons  = new std::vector<IconRow>();

bool jeiLeftArea(int& x, int& y, int& w, int& h) {
    using Fn    = bool (*)(int*, int*, int*, int*);
    HMODULE dll = ::GetModuleHandleW(L"JustEnoughItems.dll");
    auto    fn  = dll ? reinterpret_cast<Fn>(::GetProcAddress(dll, "jei_get_bookmark_area")) : nullptr;
    return fn && fn(&x, &y, &w, &h);
}

bool reserveInJei(int bottom) {
    using Fn    = bool (*)(char const*, int);
    HMODULE dll = ::GetModuleHandleW(L"JustEnoughItems.dll");
    auto    fn  = dll ? reinterpret_cast<Fn>(::GetProcAddress(dll, "jei_reserve_left_top")) : nullptr;
    return fn && fn(RESERVE_ID, bottom);
}

using platform::i18n::translate;
using platform::inventory::SlotClicker;

std::vector<ll::event::ListenerPtr> gListeners;

std::filesystem::path dataFile() { return ll::mod::NativeMod::current()->getConfigDir() / "hotbar-favorites.json"; }

bool isEmpty(ItemStack const& s) { return s.isNull() || s.mCount == 0; }

bool sameType(ItemStack const& a, ItemStack const& b) {
    if (isEmpty(a) || isEmpty(b)) return isEmpty(a) && isEmpty(b);
    return ipnext::item::ItemType::of(a) == ipnext::item::ItemType::of(b);
}

int creativeIndexOf(ContainerScreenController& csc, ItemStack const& target, bool anyNbt = false) {
    auto mgr = csc.mContainerManagerController.get();
    if (!mgr) return -1;
    auto it = mgr->mContainers->find(CREATIVE_LIST);
    if (it == mgr->mContainers->end() || !it->second) return -1;
    auto* ctrl  = it->second.get();
    auto  model = ctrl->mContainerModel.lock();
    if (!model) return -1;
    auto targetType = ipnext::item::ItemType::of(target);
    bool ignoreDamage = targetType.isDamageable() && targetType.damage() == 0;
    if (ignoreDamage) targetType = targetType.copy(true);
    int size = model->getContainerSize();
    for (int i = 0; i < size; ++i) {
        ItemInstance const& r = ctrl->getRecipeItem(i);
        if (r.mItem.get() != target.mItem.get() || r.mAuxValue != target.mAuxValue) continue;
        if (anyNbt) return i;
        ItemStack rs;
        static_cast<ItemStackBase&>(rs) = r;
        auto rt = ipnext::item::ItemType::of(rs);
        if (ignoreDamage) rt = rt.copy(true);
        if (rt == targetType) return i;
    }
    return -1;
}

bool replaceHotbarByCommand(int slot, ItemStack const& stack) {
    auto  ci     = ll::service::getClientInstance();
    auto* player = ci ? ci->getLocalPlayer() : nullptr;
    Item const* item = stack.mItem.get();
    if (!player || !item) return false;
    std::string id = item->mFullName->getString();
    if (id.starts_with("minecraft:item.")) id = "minecraft:" + id.substr(15);
    std::string command = "/replaceitem entity @s slot.hotbar " + std::to_string(slot) + " " + id + " " + std::to_string(static_cast<int>(stack.mCount));
    if (stack.mAuxValue > 0 && stack.mAuxValue < 32767) command += " " + std::to_string(stack.mAuxValue);
    auto packet = MinecraftPackets::createPacket(MinecraftPacketIds::CommandRequest);
    if (!packet) return false;
    auto& request           = static_cast<CommandRequestPacket&>(*packet);
    *request.mCommand       = command;
    request.mOrigin->mType  = CommandOriginType::Player;
    request.mOrigin->mUUID  = player->getUuid();
    request.mVersion        = CurrentCmdVersion::Latest;
    request.mInternalSource = false;
    packet->sendToServer();
    return true;
}

bool placeOnLocalServer(int slot, std::string const& snbt) {
    auto  ci     = ll::service::getClientInstance();
    auto* player = ci ? ci->getLocalPlayer() : nullptr;
    auto  level  = ll::service::getLevel();
    if (!player || !level || level->isClientSide()) return false;
    ll::thread::ServerThreadExecutor::getDefault().execute([uuid = player->getUuid(), slot, snbt] {
        auto server = ll::service::getLevel();
        if (!server || server->isClientSide()) return;
        auto* p = server->getPlayer(uuid);
        if (!p) return;
        platform::item::ScopedItemRegistry registry;
        ItemStack const stack = platform::item::fromSnbt(snbt);
        if (isEmpty(stack)) return;
        p->getInventory().setItem(slot, stack);
        p->sendInventory(true);
    });
    return true;
}

bool playerIsCreative() {
    auto ci = ll::service::getClientInstance();
    auto* p = ci ? ci->getLocalPlayer() : nullptr;
    return p && p->isCreative();
}

}

HotbarFavorites& HotbarFavorites::get() {
    static HotbarFavorites instance;
    return instance;
}

void HotbarFavorites::install() {
    platform::config::ConfigRegistry::get().add({"favorites", [] { return config::categories(); }, [] { config::loadFromFile(); }, [] { config::saveToFile(); }});
    config::loadFromFile();
    load();
    auto& input = platform::input::InputHandler::get();
    input.addKeybindProvider([](std::vector<malilib::hotkeys::KeybindMulti*>& out) { out.push_back(&config::SAVE_HOTBAR.getKeybind()); });
    config::SAVE_HOTBAR.getKeybind().setCallback([](malilib::hotkeys::KeyAction, malilib::hotkeys::KeybindMulti&) {
        platform::item::ScopedItemRegistry registry;
        return HotbarFavorites::get().saveCurrent();
    });
    gListeners.push_back(ll::event::EventBus::getInstance().emplaceListener<ll::event::MouseInputEvent>(
        [this](ll::event::MouseInputEvent& e) {
            auto& in = platform::input::InputHandler::get();
            if (static_cast<int>(e.actionButtonId()) == ::MouseAction::ActionWheel && mPanelVisible && inPanel(in.mouseX(), in.mouseY())) {
                mScroll += static_cast<int>(e.buttonData()) > 0 ? -1 : 1;
                e.cancel();
                return;
            }
            if (static_cast<int>(e.actionButtonId()) != ::MouseAction::ActionLeft) return;
            if (onMouseClick(in.mouseX(), in.mouseY(), platform::input::KeyCodes::MOUSE_LEFT, e.buttonData() == ::MouseAction::DataDown)) e.cancel();
        },
        ll::event::EventPriority::Highest
    ));
    platform::screen::ScreenTracker::addContainerOverlayRenderer([](ScreenView& view, MinecraftUIRenderContext& ctx) { HotbarFavorites::get().onContainerOverlay(view, ctx); });
    gListeners.push_back(ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientJoinLevelEvent>([](ll::event::ClientJoinLevelEvent&) {
        gIcons = new std::vector<IconRow>();
    }));
    gListeners.push_back(ll::event::EventBus::getInstance().emplaceListener<ll::event::KeyInputEvent>(
        [this](ll::event::KeyInputEvent& e) {
            if (!e.isDown() || e.keyCode() != 'A' || !mPanelVisible) return;
            auto& in  = platform::input::InputHandler::get();
            int   row = rowAt(in.mouseX(), in.mouseY());
            if (row < 0) return;
            remove(row);
            e.cancel();
        },
        ll::event::EventPriority::Highest
    ));
}

void HotbarFavorites::uninstall() {
    for (auto& l : gListeners) {
        if (l) ll::event::EventBus::getInstance().removeListener(l);
    }
    gListeners.clear();
    config::SAVE_HOTBAR.getKeybind().setCallback({});
}

void HotbarFavorites::changed() {
    save();
    gIcons->clear();
}

bool HotbarFavorites::saveCurrent() {
    auto ci = ll::service::getClientInstance();
    auto* p = ci ? ci->getLocalPlayer() : nullptr;
    if (!p) return false;
    Entry e;
    bool  any = false;
    for (int i = 0; i < 9; ++i) {
        ItemStack const& s = p->getInventory().getItem(i);
        if (isEmpty(s)) {
            e.items.emplace_back();
        } else {
            e.items.push_back(platform::item::toSnbt(s));
            any = true;
        }
    }
    if (!any) {
        platform::util::actionBar(translate("inventoryutils.favorites.emptyHotbar"));
        return false;
    }
    e.label = translate("inventoryutils.favorites.label", {std::to_string(mEntries.size() + 1)});
    mEntries.push_back(std::move(e));
    changed();
    platform::util::actionBar(translate("inventoryutils.favorites.saved", {mEntries.back().label}));
    return true;
}

bool HotbarFavorites::remove(int index) {
    if (index < 0 || index >= static_cast<int>(mEntries.size())) return false;
    mEntries.erase(mEntries.begin() + index);
    changed();
    return true;
}

RecallResult HotbarFavorites::recall(int index) {
    RecallResult r;
    if (index < 0 || index >= static_cast<int>(mEntries.size())) return mLast = r;
    auto clicker = SlotClicker::current();
    if (!clicker.valid()) return mLast = r;
    auto const& menu     = clicker.menu();
    bool        creative = playerIsCreative() && platform::screen::ScreenTracker::containerScreenName() == "inventory_screen";
    auto*       csc      = platform::screen::ScreenTracker::containerScreen();
    auto const& entry    = mEntries[index];
    std::vector<int> usedSources;
    for (int i = 0; i < 9 && i < static_cast<int>(entry.items.size()); ++i) {
        int hot = menu.findSlot("hotbar_items", i);
        if (hot < 0) continue;
        ItemStack target = entry.items[i].empty() ? ItemStack() : platform::item::fromSnbt(entry.items[i]);
        ItemStack const& cur = menu.getItem(*menu.getSlot(hot));
        if (sameType(cur, target) && (isEmpty(target) || cur.mCount == target.mCount)) {
            ++r.unchanged;
            continue;
        }
        if (isEmpty(target) && !creative) {
            ++r.unchanged;
            continue;
        }
        auto findInInventory = [&]() -> int {
            for (auto const& s : menu.slots()) {
                if (s.collection != "inventory_items") continue;
                if (std::find(usedSources.begin(), usedSources.end(), s.slotNumber) != usedSources.end()) continue;
                if (sameType(menu.getItem(s), target)) return s.slotNumber;
            }
            return -1;
        };
        if (creative && csc) {
            if (!isEmpty(cur)) {
                clicker.leftClickSlot(hot);
                csc->_handlePlaceAll(CREATIVE_LIST, 0);
            }
            if (isEmpty(target)) continue;
            if (int idx = creativeIndexOf(*csc, target); idx >= 0) {
                csc->_handleTakeAmount(target.mCount, CREATIVE_LIST, idx);
                clicker.leftClickSlot(hot);
                ++r.fromCreative;
            } else if (int src = findInInventory(); src >= 0) {
                clicker.leftClickSlot(src);
                clicker.leftClickSlot(hot);
                usedSources.push_back(src);
                ++r.fromInventory;
            } else if (target.mUserData && placeOnLocalServer(i, entry.items[i])) {
                ++r.fromCreative;
            } else if (int base = creativeIndexOf(*csc, target, true); base >= 0) {
                csc->_handleTakeAmount(target.mCount, CREATIVE_LIST, base);
                clicker.leftClickSlot(hot);
                ++r.fromCreative;
            } else if (replaceHotbarByCommand(i, target)) {
                ++r.fromCreative;
            } else {
                ++r.missing;
            }
        } else {
            int src = findInInventory();
            if (src < 0) {
                if (playerIsCreative() && ((target.mUserData && placeOnLocalServer(i, entry.items[i])) || replaceHotbarByCommand(i, target))) ++r.fromCreative;
                else ++r.missing;
                continue;
            }
            clicker.leftClickSlot(src);
            clicker.leftClickSlot(hot);
            if (!isEmpty(menu.getCarried())) clicker.leftClickSlot(src);
            usedSources.push_back(src);
            ++r.fromInventory;
        }
    }
    mLast = r;
    platform::util::actionBar(translate(
        "inventoryutils.favorites.recalled",
        {entry.label, std::to_string(r.fromCreative), std::to_string(r.fromInventory), std::to_string(r.missing)}
    ));
    return r;
}

int HotbarFavorites::visibleRows() const { return std::max(0, (mPanelH - HEADER - PAD) / CELL); }

bool HotbarFavorites::inPanel(double mx, double my) const {
    return mx >= mPanelX && mx < mPanelX + PANEL_W && my >= mPanelY && my < mPanelY + mPanelH;
}

int HotbarFavorites::rowAt(double mx, double my) const {
    if (!inPanel(mx, my) || my < mPanelY + HEADER) return -1;
    int const row = static_cast<int>(my - (mPanelY + HEADER)) / CELL + mScroll;
    return row < static_cast<int>(mEntries.size()) && row - mScroll < visibleRows() ? row : -1;
}

void HotbarFavorites::onContainerOverlay(ScreenView&, MinecraftUIRenderContext& ctx) {
    mPanelVisible = mButtonVisible = false;
    if (!config::ENABLED.getBooleanValue()) return;
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    platform::item::ScopedItemRegistry registry;
    platform::render::Gui2d            g(ctx, *ci);
    int const                          count = static_cast<int>(mEntries.size());
    int const                          wantH = HEADER + std::max(count, 1) * CELL + PAD;
    int const                          maxH  = g.screenHeight() - 4;
    mPanelH                                  = std::min(wantH, maxH);
    mPanelX                                  = 2;
    mPanelY                                  = 2;
    if (!reserveInJei(mPanelY + mPanelH + 2)) {
        int x, y, w, h;
        if (jeiLeftArea(x, y, w, h) && y + h + 2 + HEADER + CELL + PAD <= g.screenHeight()) {
            mPanelX = x;
            mPanelY = y + h + 2;
            mPanelH = std::min(wantH, g.screenHeight() - mPanelY - 2);
        }
    }
    mPanelVisible = true;
    mScroll       = std::clamp(mScroll, 0, std::max(0, count - visibleRows()));
    mButtonX      = mPanelX + PANEL_W - BUTTON_SIZE - 2;
    mButtonY      = mPanelY + 1;
    mButtonVisible = true;

    double const mx = platform::input::InputHandler::get().mouseX(), my = platform::input::InputHandler::get().mouseY();
    g.fill(mPanelX, mPanelY, mPanelX + PANEL_W, mPanelY + mPanelH, 0xC0101010);
    g.outline(mPanelX, mPanelY, PANEL_W, mPanelH, 0xFF555555);
    g.drawString(translate("inventoryutils.favorites.title"), mPanelX + PAD, mPanelY + 2, 0xFFFFFFFF, true);
    bool const buttonHover = mx >= mButtonX && mx < mButtonX + BUTTON_SIZE && my >= mButtonY && my < mButtonY + BUTTON_SIZE;
    g.fill(mButtonX, mButtonY, mButtonX + BUTTON_SIZE, mButtonY + BUTTON_SIZE, 0xFF000000);
    g.fill(mButtonX + 1, mButtonY + 1, mButtonX + BUTTON_SIZE - 1, mButtonY + BUTTON_SIZE - 1, buttonHover ? 0xFF7C84C8 : 0xFF7A7A7A);
    g.fill(mButtonX + 4, mButtonY + 2, mButtonX + 6, mButtonY + 8, 0xFFFFFFFF);
    g.fill(mButtonX + 2, mButtonY + 4, mButtonX + 8, mButtonY + 6, 0xFFFFFFFF);

    if (gIcons->size() != mEntries.size()) {
        gIcons->clear();
        for (auto const& e : mEntries) {
            IconRow row;
            for (auto const& snbt : e.items) row.push_back(snbt.empty() ? nullptr : std::make_unique<ItemStack>(platform::item::fromSnbt(snbt)));
            gIcons->push_back(std::move(row));
        }
    }
    int const hovered = rowAt(mx, my);
    int const rows    = visibleRows();
    for (int v = 0; v < rows && v + mScroll < count; ++v) {
        int const index = v + mScroll;
        int const y     = mPanelY + HEADER + v * CELL;
        if (index == hovered) g.fill(mPanelX + 1, y, mPanelX + PANEL_W - 1, y + CELL, 0x60FFFFFF);
        auto const& icons = (*gIcons)[static_cast<std::size_t>(index)];
        for (int c = 0; c < 9 && c < static_cast<int>(icons.size()); ++c) {
            if (icons[c] && !isEmpty(*icons[c])) g.renderItem(*icons[c], mPanelX + PAD + c * CELL + 1, y + 1, 0.625f);
        }
    }
    g.flush();
    std::vector<std::string> tip;
    if (buttonHover) tip.push_back(translate("inventoryutils.favorites.save"));
    else if (hovered >= 0) {
        tip.push_back(mEntries[static_cast<std::size_t>(hovered)].label);
        for (auto const& st : (*gIcons)[static_cast<std::size_t>(hovered)]) {
            if (st && !isEmpty(*st)) tip.push_back(std::to_string(static_cast<int>(st->mCount)) + " x " + ipnext::item::ItemType::of(*st).displayName());
        }
        tip.push_back(translate("inventoryutils.favorites.rowHint"));
    }
    if (!tip.empty()) {
        int tw = 0;
        for (auto const& t : tip) tw = std::max(tw, g.stringWidth(t));
        int const th = static_cast<int>(tip.size()) * platform::render::Gui2d::LINE_HEIGHT;
        int const tx = static_cast<int>(mx) + 10;
        int const ty = std::clamp(static_cast<int>(my) - 12, 4, std::max(4, g.screenHeight() - th - 4));
        g.fill(tx - 3, ty - 3, tx + tw + 3, ty + th + 3, 0xF0100010);
        for (std::size_t i = 0; i < tip.size(); ++i) g.drawString(tip[i], tx, ty + static_cast<int>(i) * platform::render::Gui2d::LINE_HEIGHT, 0xFFFFFFFF, true);
    }
    g.flush();
}

bool HotbarFavorites::onMouseClick(double mx, double my, int buttonCode, bool buttonState) {
    if (!buttonState) {
        bool was     = mPressedHere;
        mPressedHere = false;
        return was;
    }
    if (buttonCode != platform::input::KeyCodes::MOUSE_LEFT || !mPanelVisible || !platform::screen::ScreenTracker::containerScreen()) return false;
    if (!inPanel(mx, my)) return false;
    mPressedHere = true;
    platform::item::ScopedItemRegistry registry;
    if (mx >= mButtonX && mx < mButtonX + BUTTON_SIZE && my >= mButtonY && my < mButtonY + BUTTON_SIZE) saveCurrent();
    else if (int row = rowAt(mx, my); row >= 0) recall(row);
    return true;
}

bool HotbarFavorites::clickRowForTest(int index) {
    if (!mPanelVisible || index < mScroll || index - mScroll >= visibleRows()) return false;
    double const x = mPanelX + PANEL_W / 2.0, y = mPanelY + HEADER + (index - mScroll) * CELL + CELL / 2.0;
    bool const   handled = onMouseClick(x, y, platform::input::KeyCodes::MOUSE_LEFT, true);
    onMouseClick(x, y, platform::input::KeyCodes::MOUSE_LEFT, false);
    return handled;
}

std::string HotbarFavorites::debugButton() const {
    return nlohmann::json{{"visible", mButtonVisible}, {"x", mButtonX}, {"y", mButtonY}, {"size", BUTTON_SIZE}, {"panel", {mPanelX, mPanelY, PANEL_W, mPanelH}}}.dump();
}

void HotbarFavorites::load() {
    mEntries.clear();
    std::ifstream in(dataFile(), std::ios::binary);
    if (!in) return;
    auto root = nlohmann::json::parse(in, nullptr, false);
    if (!root.is_object() || !root.contains("entries") || !root["entries"].is_array()) return;
    for (auto const& j : root["entries"]) {
        Entry e;
        e.label = j.value("label", std::string());
        if (j.contains("items") && j["items"].is_array()) {
            for (auto const& s : j["items"]) e.items.push_back(s.is_string() ? s.get<std::string>() : std::string());
        }
        e.items.resize(9);
        mEntries.push_back(std::move(e));
    }
}

void HotbarFavorites::isolateForTest() {
    if (mIsolated) return;
    mStashed  = std::move(mEntries);
    mEntries  = {};
    mIsolated = true;
    gIcons->clear();
}

void HotbarFavorites::restoreAfterTest() {
    if (!mIsolated) return;
    mEntries  = std::move(mStashed);
    mStashed  = {};
    mIsolated = false;
    gIcons->clear();
}

void HotbarFavorites::save() const {
    if (mIsolated) return;
    nlohmann::json arr = nlohmann::json::array();
    for (auto const& e : mEntries) arr.push_back({{"label", e.label}, {"items", e.items}});
    std::error_code ec;
    std::filesystem::create_directories(dataFile().parent_path(), ec);
    std::ofstream out(dataFile(), std::ios::binary | std::ios::trunc);
    out << nlohmann::json{{"entries", arr}}.dump(2);
}

}
