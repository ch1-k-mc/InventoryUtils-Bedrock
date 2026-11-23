// SPDX-License-Identifier: AGPL-3.0-only
#include "craftingtools/CraftingTools.h"

#include <algorithm>
#include <array>
#include <map>
#include <filesystem>
#include <fstream>

#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemDescriptor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/Recipes.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "nlohmann/json.hpp"

#include "ipnext/inventory/AdvancedContainer.h"
#include "ipnext/inventory/ContainerClicker.h"
#include "ipnext/inventory/GeneralInventoryActions.h"
#include "ipnext/inventory/action/Actions.h"
#include "malilib/hotkeys/KeybindMulti.h"
#include "platform/config/ConfigRegistry.h"
#include "platform/i18n/I18n.h"
#include "platform/input/KeyCodes.h"
#include "platform/inventory/SlotClicker.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/render/Gui2d.h"
#include "platform/screen/ScreenLayout.h"
#include "platform/screen/ScreenTracker.h"
#include "platform/util/Messages.h"

namespace inventory_utils::craftingtools {

namespace config {
namespace {
using malilib::hotkeys::KeyAction;
using malilib::hotkeys::KeybindSettings;
using Ctx = KeybindSettings::Context;
KeybindSettings const GUI_KEYS = KeybindSettings::create(Ctx::GUI, KeyAction::PRESS, false, true, false, true);
template <class T>
T applied(T o) {
    o.apply("inventoryutils.craftingtools");
    return o;
}
}

ConfigHotkey     ROTATE                   = applied(ConfigHotkey("rotate", "", GUI_KEYS));
ConfigHotkey     ROTATE_COUNTER_CLOCKWISE = applied(ConfigHotkey("rotateCounterClockwise", "", GUI_KEYS));
ConfigHotkey     BALANCE                  = applied(ConfigHotkey("balance", "", GUI_KEYS));
ConfigHotkey     SPREAD                   = applied(ConfigHotkey("spread", "", GUI_KEYS));
ConfigHotkey     CLEAR                    = applied(ConfigHotkey("clear", "", GUI_KEYS));
ConfigHotkey     COMPRESS                 = applied(ConfigHotkey("compress", "", GUI_KEYS));
ConfigHotkey     COMPRESS_ALL             = applied(ConfigHotkey("compressAll", "", GUI_KEYS));
ConfigHotkey     DECOMPRESS               = applied(ConfigHotkey("decompress", "", GUI_KEYS));
ConfigHotkey     DECOMPRESS_ALL           = applied(ConfigHotkey("decompressAll", "", GUI_KEYS));
ConfigOptionList MODE                     = applied(ConfigOptionList("mode", {"BUTTONS_AND_KEYS", "KEYS_ONLY", "DISABLED"}, "BUTTONS_AND_KEYS"));
ConfigBoolean    SHOW_BUTTONS             = applied(ConfigBoolean("showButtons", true));

std::vector<ConfigHotkey*> const& hotkeys() {
    static std::vector<ConfigHotkey*> const list{&ROTATE, &ROTATE_COUNTER_CLOCKWISE, &BALANCE, &SPREAD, &CLEAR, &COMPRESS, &COMPRESS_ALL, &DECOMPRESS, &DECOMPRESS_ALL};
    return list;
}

std::vector<std::pair<std::string, std::vector<IConfigBase*>>> categories() {
    std::vector<IConfigBase*> generic{&MODE, &SHOW_BUTTONS};
    std::vector<IConfigBase*> keys;
    for (auto* h : hotkeys()) keys.push_back(h);
    return {{"Generic", generic}, {"Hotkeys", keys}};
}

namespace {
std::filesystem::path file() { return ll::mod::NativeMod::current()->getConfigDir() / "craftingtools.json"; }
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

void saveToFile() {
    std::error_code ec;
    std::filesystem::create_directories(file().parent_path(), ec);
    nlohmann::json root = nlohmann::json::object();
    for (auto& [cat, opts] : categories()) writeConfigBase(root, cat, opts);
    std::ofstream out(file(), std::ios::binary | std::ios::trunc);
    out << root.dump(2);
}
}

namespace {

using ipnext::inventory::AdvancedContainer;
using ipnext::inventory::VanillaContainer;
using ipnext::item::ItemStack;
using platform::inventory::MenuSlot;
using platform::inventory::SlotClicker;
using platform::i18n::translate;

bool enabled() { return config::MODE.getOptionListValue() != "DISABLED"; }

LocalPlayer* localPlayer() {
    auto ci = ll::service::getClientInstance();
    return ci ? ci->getLocalPlayer() : nullptr;
}

struct Grid {
    std::vector<int> slots;
    int              width = 0;
    int              outputSlot = -1;
    [[nodiscard]] bool valid() const { return width > 0; }
};

Grid currentGrid(VanillaContainer const& v) {
    Grid g;
    auto const& screen = v.screenName();
    if (screen != "crafting_screen") return g;
    std::vector<std::pair<int, int>> byIndex;
    for (auto const& s : v.slots()) {
        if (s.collection == "crafting_input_items") byIndex.emplace_back(s.index, s.slotNumber);
        if (s.collection == "crafting_output_items") g.outputSlot = s.slotNumber;
    }
    std::sort(byIndex.begin(), byIndex.end());
    for (auto const& [i, n] : byIndex) g.slots.push_back(n);
    if (g.slots.size() == 9) g.width = 3;
    else if (g.slots.size() == 4) g.width = 2;
    return g;
}

bool rearrange(std::function<void(std::vector<ItemStack*> const& grid)> const& change) {
    auto v = VanillaContainer::current();
    if (!v) return false;
    auto g = currentGrid(*v);
    if (!g.valid()) return false;
    AdvancedContainer::withTracker([&](AdvancedContainer::TrackerDsl& dsl) {
        std::vector<ItemStack*> grid;
        for (int id : g.slots) grid.push_back(&dsl.tracker.slots[id]);
        change(grid);
    });
    return true;
}

void writeCount(ItemStack& s, ipnext::item::ItemType const& type, int count) {
    if (count <= 0) {
        s.setEmpty();
        return;
    }
    s.itemType = type;
    s.count    = count;
}

struct CompressRecipe {
    Recipe const* recipe = nullptr;
    int           n      = 0;
};

std::vector<RecipeIngredient const*> nonNullIngredients(Recipe const& r) {
    std::vector<RecipeIngredient const*> out;
    for (auto const& ing : *r.mMyIngredients) {
        if (!ing.isNull()) out.push_back(&ing);
    }
    return out;
}

Recipe const* findRecipe(Item const* item, int want, int gridWidth, bool singleIngredient) {
    auto ci = ll::service::getClientInstance();
    if (!ci || !ci->getLevel() || !item) return nullptr;
    Recipes& recipes = ci->getLevel()->getRecipes();
    auto it          = recipes.mRecipes->find(HashedString("crafting_table"));
    if (it == recipes.mRecipes->end()) return nullptr;
    for (auto const& [id, recipe] : it->second) {
        if (!recipe || recipe->isMultiRecipe()) continue;
        auto ings = nonNullIngredients(*recipe);
        if (static_cast<int>(ings.size()) != (singleIngredient ? 1 : want)) continue;
        if (!std::all_of(ings.begin(), ings.end(), [&](RecipeIngredient const* i) { return i->getItem() == item; })) continue;
        auto const& results = recipe->getResultItems();
        if (results.size() != 1 || results[0].isNull()) continue;
        if (singleIngredient && static_cast<int>(results[0].mCount) != want) continue;
        if (!recipe->isShapeless() && (recipe->mWidth > gridWidth || recipe->mHeight > gridWidth)) continue;
        return recipe.get();
    }
    return nullptr;
}

std::vector<int> recipePositions(Recipe const& r, int gridWidth) {
    std::vector<int> out;
    auto const&      ings = *r.mMyIngredients;
    if (r.isShapeless() || r.mWidth <= 0) {
        int n = static_cast<int>(nonNullIngredients(r).size());
        for (int i = 0; i < n; ++i) out.push_back(i);
        return out;
    }
    for (size_t i = 0; i < ings.size(); ++i) {
        if (ings[i].isNull()) continue;
        int x = static_cast<int>(i) % r.mWidth, y = static_cast<int>(i) / r.mWidth;
        out.push_back(y * gridWidth + x);
    }
    return out;
}

bool craftWith(bool decompressMode, bool all, int slotOverride) {
    auto clicker = SlotClicker::current();
    auto v       = VanillaContainer::current();
    if (!clicker.valid() || !v) return false;
    auto g = currentGrid(*v);
    if (!g.valid() || g.outputSlot < 0) return false;
    auto const* hovered = slotOverride >= 0 ? clicker.menu().getSlot(slotOverride) : clicker.getSlotUnderMouse();
    if (!hovered || hovered->kind == platform::inventory::InventoryKind::Container) return false;
    auto target = ItemStack::of(clicker.menu().getItem(*hovered));
    if (target.isEmpty()) return false;
    Item const*   item   = target.itemType.vanillaStack().mItem.get();
    Recipe const* recipe = nullptr;
    int           n      = 0;
    if (decompressMode) {
        for (int want : {9, 4}) {
            if ((recipe = findRecipe(item, want, g.width, true))) break;
        }
        n = 1;
    } else {
        for (int want : {9, 4}) {
            if (want == 9 && g.width < 3) continue;
            if ((recipe = findRecipe(item, want, g.width, false))) {
                n = want;
                break;
            }
        }
    }
    if (!recipe) {
        platform::util::actionBar(translate(decompressMode ? "inventoryutils.craftingtools.noDecompressRecipe" : "inventoryutils.craftingtools.noCompressRecipe"));
        return false;
    }
    for (int id : g.slots) {
        if (!ItemStack::of(clicker.menu().getItem(*clicker.menu().getSlot(id))).isEmpty()) clicker.shiftClickSlot(id);
    }
    int hoveredId = hovered->slotNumber;
    std::vector<int> sources{hoveredId};
    for (auto const& s : clicker.menu().slots()) {
        if (s.kind == platform::inventory::InventoryKind::Container || s.slotNumber == hoveredId) continue;
        auto st = ItemStack::of(clicker.menu().getItem(s));
        if (!st.isEmpty() && st.itemType == target.itemType) sources.push_back(s.slotNumber);
    }
    int total = 0;
    for (int id : sources) {
        auto st = ItemStack::of(clicker.menu().getItem(*clicker.menu().getSlot(id)));
        if (st.itemType == target.itemType) total += st.count;
    }
    int k = all ? std::min(target.itemType.maxCount(), total / n) : (total >= n ? 1 : 0);
    if (k <= 0) return false;
    auto positions = recipePositions(*recipe, g.width);
    size_t src     = 0;
    auto   pickNext = [&]() {
        while (src < sources.size()) {
            int id = sources[src++];
            if (!ItemStack::of(clicker.menu().getItem(*clicker.menu().getSlot(id))).isEmpty()) {
                clicker.leftClickSlot(id);
                return true;
            }
        }
        return false;
    };
    if (!pickNext()) return false;
    for (int pos : positions) {
        if (pos < 0 || pos >= static_cast<int>(g.slots.size())) continue;
        int left = k;
        while (left > 0) {
            int have = ItemStack::of(clicker.menu().getCarried()).count;
            if (have <= 0 && !pickNext()) break;
            have = ItemStack::of(clicker.menu().getCarried()).count;
            int put = std::min(left, have);
            clicker.placeFromCursor(g.slots[pos], put);
            left -= put;
        }
    }
    if (!ItemStack::of(clicker.menu().getCarried()).isEmpty()) clicker.leftClickSlot(hoveredId);
    if (!ItemStack::of(clicker.menu().getCarried()).isEmpty()) ipnext::inventory::GeneralInventoryActions::cleanCursor();
    clicker.shiftClickSlot(g.outputSlot);
    return true;
}

}

namespace Actions {

bool rotate(bool counterClockwise) {
    if (!enabled()) return false;
    return rearrange([counterClockwise](std::vector<ItemStack*> const& grid) {
        std::vector<int> ring = grid.size() == 9 ? std::vector<int>{0, 1, 2, 5, 8, 7, 6, 3} : std::vector<int>{0, 1, 3, 2};
        std::vector<ItemStack> old;
        for (auto* s : grid) old.push_back(*s);
        size_t n = ring.size();
        for (size_t i = 0; i < n; ++i) {
            size_t to   = counterClockwise ? (i + n - 1) % n : (i + 1) % n;
            *grid[ring[to]] = old[ring[i]];
        }
    });
}

bool balance() {
    if (!enabled()) return false;
    return rearrange([](std::vector<ItemStack*> const& grid) {
        std::vector<std::pair<ipnext::item::ItemType, std::vector<ItemStack*>>> groups;
        for (auto* s : grid) {
            if (s->isEmpty()) continue;
            auto it = std::find_if(groups.begin(), groups.end(), [&](auto const& gr) { return s->itemType == gr.first; });
            if (it == groups.end()) groups.push_back({s->itemType, {s}});
            else it->second.push_back(s);
        }
        for (auto& [type, list] : groups) {
            int total = 0;
            for (auto* s : list) total += s->count;
            auto counts = ipnext::inventory::action::distributeMonotonic(total, static_cast<int>(list.size()));
            for (size_t i = 0; i < list.size(); ++i) writeCount(*list[i], type, counts[i]);
        }
    });
}

bool spread() {
    if (!enabled()) return false;
    auto v = VanillaContainer::current();
    if (!v) return false;
    bool ok = false;
    rearrange([&](std::vector<ItemStack*> const& grid) {
        ipnext::item::ItemType type;
        int                    total = 0;
        for (auto* s : grid) {
            if (s->isEmpty()) continue;
            if (type.isEmpty()) type = s->itemType;
            else if (!(s->itemType == type)) return;
            total += s->count;
        }
        if (type.isEmpty()) return;
        auto counts = ipnext::inventory::action::distributeMonotonic(total, static_cast<int>(grid.size()));
        for (size_t i = 0; i < grid.size(); ++i) writeCount(*grid[i], type, std::min(counts[i], type.maxCount()));
        ok = true;
    });
    return ok;
}

bool clear(bool force) {
    if (!enabled()) return false;
    auto v = VanillaContainer::current();
    if (!v) return false;
    auto g = currentGrid(*v);
    if (!g.valid()) return false;
    for (int id : g.slots) {
        if (!ItemStack::of(v->itemOf(v->slots()[id])).isEmpty()) ipnext::inventory::ContainerClicker::shiftClick(id);
    }
    if (force) {
        auto clicker = SlotClicker::current();
        if (!clicker.valid()) return true;
        for (int id : g.slots) {
            if (!ItemStack::of(clicker.menu().getItem(*clicker.menu().getSlot(id))).isEmpty()) clicker.dropStack(id);
        }
    }
    return true;
}

bool compress(bool all, int slot) { return enabled() && craftWith(false, all, slot); }
bool decompress(bool all, int slot) { return enabled() && craftWith(true, all, slot); }

}

namespace {

constexpr int BUTTON_SIZE = 16;
constexpr int BUTTON_GAP  = 2;

constexpr char const* BUTTON_TEXTURE = "inventoryutils/textures/gui/crafting_buttons.png";

void drawButton(platform::render::Gui2d& g, int column, int x, int y, bool hover) {
    if (g.blitPixels(BUTTON_TEXTURE, x, y, column * BUTTON_SIZE, hover ? BUTTON_SIZE : 0, BUTTON_SIZE, BUTTON_SIZE, 96, 32)) return;
    g.fill(x, y, x + BUTTON_SIZE, y + BUTTON_SIZE, 0xFF000000);
    g.fill(x + 1, y + 1, x + BUTTON_SIZE - 1, y + BUTTON_SIZE - 1, hover ? 0xFF7C84C8 : 0xFF7A7A7A);
}

bool shiftDown() {
    auto& in = platform::input::InputHandler::get();
    return in.isKeyDown(VK_LSHIFT) || in.isKeyDown(VK_RSHIFT);
}

}

GridButtons& GridButtons::get() {
    static GridButtons instance;
    return instance;
}

void GridButtons::onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx) {
    mButtons.clear();
    if (config::MODE.getOptionListValue() != "BUTTONS_AND_KEYS" || !config::SHOW_BUTTONS.getBooleanValue()) return;
    auto v = VanillaContainer::current();
    if (!v || !currentGrid(*v).valid()) return;
    auto rects = platform::screen::collectSlotRects(view);
    auto it    = rects.find({"crafting_input_items", 0});
    if (it == rects.end()) return;
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    int pitch = it->second.h > 0 ? it->second.h : 18;
    int x     = it->second.x - BUTTON_SIZE - BUTTON_GAP, y = it->second.y + 1;
    mButtons  = {
        {"rotate", "", x, y},
        {"balance", "", x, y + pitch},
        {"clear", "", x, y + pitch * 2},
    };
    bool                    shift = shiftDown();
    platform::render::Gui2d g(ctx, *ci);
    double        mx = platform::input::InputHandler::get().mouseX(), my = platform::input::InputHandler::get().mouseY();
    Button const* hovered = nullptr;
    for (auto const& b : mButtons) {
        bool hover = mx >= b.x && mx < b.x + BUTTON_SIZE && my >= b.y && my < b.y + BUTTON_SIZE;
        if (hover) hovered = &b;
        int const base = b.id == "rotate" ? 0 : (b.id == "balance" ? 2 : 4);
        drawButton(g, base + (shift ? 1 : 0), b.x, b.y, hover);
    }
    if (hovered) {
        static std::map<std::string, std::pair<char const*, char const*>> const keys{
            {"rotate",  {"rotate", "rotateCounterClockwise"}},
            {"balance", {"balance", "spread"}               },
            {"clear",   {"clear", "forceClear"}             },
        };
        auto const& k    = keys.at(hovered->id);
        std::string text = translate(std::string("inventoryutils.craftingtools.button.") + (shift ? k.second : k.first));
        int         w    = g.stringWidth(text);
        int         tx = static_cast<int>(mx) + 10, ty = static_cast<int>(my) - 12;
        g.fill(tx - 3, ty - 3, tx + w + 3, ty + platform::render::Gui2d::LINE_HEIGHT + 3, 0xF0100010);
        g.drawString(text, tx, ty, 0xFFFFFFFF, true);
    }
    g.flush();
}

bool GridButtons::clickById(std::string const& id, bool shift) {
    if (id == "rotate") return Actions::rotate(shift);
    if (id == "balance") return shift ? Actions::spread() : Actions::balance();
    if (id == "clear") return Actions::clear(shift);
    return false;
}

bool GridButtons::onMouseClick(double mx, double my, int buttonCode, bool buttonState) {
    if (!buttonState) {
        bool was     = mPressedHere;
        mPressedHere = false;
        return was;
    }
    if (buttonCode != platform::input::KeyCodes::MOUSE_LEFT) return false;
    if (platform::screen::ScreenTracker::containerScreenName() != "crafting_screen" || !platform::screen::ScreenTracker::containerScreen()) {
        mButtons.clear();
        return false;
    }
    for (auto const& b : mButtons) {
        if (mx >= b.x && mx < b.x + BUTTON_SIZE && my >= b.y && my < b.y + BUTTON_SIZE) {
            mPressedHere = true;
            platform::item::ScopedItemRegistry registry;
            clickById(b.id, shiftDown());
            return true;
        }
    }
    return false;
}

std::string GridButtons::debugJson() const {
    nlohmann::json list = nlohmann::json::array();
    if (platform::screen::ScreenTracker::containerScreenName() != "crafting_screen" || !platform::screen::ScreenTracker::containerScreen()) return list.dump();
    for (auto const& b : mButtons) list.push_back({{"id", b.id}, {"x", b.x}, {"y", b.y}});
    return list.dump();
}

void install() {
    platform::config::ConfigRegistry::get().add({
        "craftingtools",
        [] { return config::categories(); },
        [] { config::loadFromFile(); },
        [] { config::saveToFile(); },
    });
    config::loadFromFile();
    platform::i18n::addNamespace("inventoryutils");
    auto& input = platform::input::InputHandler::get();
    input.addKeybindProvider([](std::vector<malilib::hotkeys::KeybindMulti*>& out) {
        for (auto* h : config::hotkeys()) out.push_back(&h->getKeybind());
    });
    using malilib::hotkeys::KeyAction;
    using malilib::hotkeys::KeybindMulti;
    auto bind = [](config::ConfigHotkey& h, bool (*f)()) {
        h.getKeybind().setCallback([f](KeyAction, KeybindMulti&) {
            if (!VanillaContainer::current()) return false;
            return f();
        });
    };
    bind(config::ROTATE, [] { return Actions::rotate(false); });
    bind(config::ROTATE_COUNTER_CLOCKWISE, [] { return Actions::rotate(true); });
    bind(config::BALANCE, [] { return Actions::balance(); });
    bind(config::SPREAD, [] { return Actions::spread(); });
    bind(config::CLEAR, [] { return Actions::clear(); });
    bind(config::COMPRESS, [] { return Actions::compress(false); });
    bind(config::COMPRESS_ALL, [] { return Actions::compress(true); });
    bind(config::DECOMPRESS, [] { return Actions::decompress(false); });
    bind(config::DECOMPRESS_ALL, [] { return Actions::decompress(true); });
    input.registerMouseInputHandler(&GridButtons::get());
    platform::screen::ScreenTracker::addContainerOverlayRenderer([](ScreenView& view, MinecraftUIRenderContext& ctx) { GridButtons::get().onContainerOverlay(view, ctx); });
}

void uninstall() {
    for (auto* h : config::hotkeys()) h->getKeybind().setCallback({});
    platform::input::InputHandler::get().unregisterMouseInputHandler(&GridButtons::get());
}

}
