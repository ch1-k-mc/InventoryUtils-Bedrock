// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/gui/inject/SortingButtonCollectionWidget.h"

#include <algorithm>
#include <cctype>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"

#include "ipnext/config/Configs.h"
#include "ipnext/inventory/ContainerTypes.h"
#include "ipnext/inventory/GeneralInventoryActions.h"
#include "platform/i18n/I18n.h"
#include "platform/input/InputHandler.h"
#include "platform/input/KeyCodes.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/render/Gui2d.h"
#include "platform/screen/ScreenLayout.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::ipnext::gui::inject {

namespace {

using namespace config;
using inventory::ContainerType;
using platform::i18n::translate;
using platform::render::Gui2d;
namespace KC = platform::input::KeyCodes;

constexpr char const* TEXTURE = "inventoryprofilesnext/textures/gui/gui_buttons.png";
constexpr int         SIZE    = 10;

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string keyText(ConfigHotkey const& h) { return upper(h.getKeybind().getKeysDisplayString()); }

std::string moveAllTooltip() {
    std::string const prefix = "inventoryprofiles.tooltip.move_all_button";
    struct Line {
        char const*         suffix;
        ConfigHotkey const* keybind;
    };
    std::vector<Line> lines{
        {ModSettings::ALWAYS_MOVE_ALL.getBooleanValue() ? "title_move_all" : "title_move_matching", nullptr},
        {ModSettings::ALWAYS_INCLUDE_HOTBAR.getBooleanValue() ? "exclude_hotbar" : "include_hotbar", &ModSettings::INCLUDE_HOTBAR_MODIFIER},
        {ModSettings::ALWAYS_MOVE_ALL.getBooleanValue() ? "move_matching_only" : "move_all", &ModSettings::MOVE_ALL_MODIFIER},
        {"move_focused_only", &ModSettings::MOVE_FOCUS_MACH_MODIFIER},
        {"move_just_refill", &ModSettings::MOVE_JUST_REFILL_MODIFIER},
    };
    std::string out;
    for (auto const& l : lines) {
        if (l.keybind && l.keybind->getKeybind().getKeys().empty()) continue;
        if (!out.empty()) out += "\n";
        out += translate(prefix + "." + l.suffix, {l.keybind ? keyText(*l.keybind) : std::string()});
    }
    return out;
}

std::string sortTooltip() {
    std::string order = translate("inventoryprofiles.enum.sorting_method." + lower(SortSettings::SORT_ORDER.getOptionListValue()));
    std::string s     = translate("inventoryprofiles.tooltip.sort_button") + translate("inventoryprofiles.tooltip.sort_button.current_order", {order});
    if (GuiSettings::CHANGE_SORT_ORDER_WITH_SCROLL_WHEEL.getBooleanValue()) s += translate("inventoryprofiles.tooltip.sort_button.key_help");
    return s;
}

std::vector<std::string> splitLines(std::string const& s) {
    std::vector<std::string> out;
    size_t                   start = 0;
    while (true) {
        auto p = s.find('\n', start);
        out.push_back(s.substr(start, p == std::string::npos ? std::string::npos : p - start));
        if (p == std::string::npos) break;
        start = p + 1;
    }
    return out;
}

}

SortingButtonCollectionWidget& SortingButtonCollectionWidget::get() {
    static SortingButtonCollectionWidget instance;
    return instance;
}

void SortingButtonCollectionWidget::layout(std::string const& screenName, int left, int top, int width, int height) {
    mButtons.clear();
    mScreen          = screenName;
    auto types       = inventory::ContainerTypes::getTypes(screenName);
    bool addChestSide    = types.contains(ContainerType::SORTABLE_STORAGE);
    bool addNonChestSide = types.contains(ContainerType::PURE_BACKPACK);
    bool shouldAdd       = addChestSide || addNonChestSide;
    bool isPlayer        = types.contains(ContainerType::PLAYER);
    bool enabled         = Features::ENABLE_SORTING_AND_MOVING.getBooleanValue() && GuiSettings::INVENTORY_OVERLAY_BUTTONS_VISIBLE.getBooleanValue();

    auto setTopRight    = [&](Button& b, int t, int r) { b.x = left + width - r - SIZE, b.y = top + t; };
    auto setBottomRight = [&](Button& b, int bt, int r) { b.x = left + width - r - SIZE, b.y = top + height - bt - SIZE; };

    int const bottomBase = 82;
    int const topBase    = 5;
    int       right      = 7;
    if (types.contains(ContainerType::CREATIVE)) right += 18;

    bool moveAllVisible = GuiSettings::SHOW_MOVE_ALL_BUTTON.getBooleanValue()
                       && inventory::containsAny(types, {ContainerType::SORTABLE_STORAGE, ContainerType::NO_SORTING_STORAGE, ContainerType::CRAFTING});

    Button moveToContainer{"sort_move_all_button", 50};
    moveToContainer.visible = enabled && moveAllVisible;
    moveToContainer.click   = [](int) { inventory::GeneralInventoryActions::doMoveMatch(false); };
    moveToContainer.tooltip = moveAllTooltip;
    Button moveToPlayer{"sort_move_all_button_player", 60};
    moveToPlayer.visible = enabled && moveAllVisible && !types.contains(ContainerType::CRAFTING);
    moveToPlayer.click   = [](int) { inventory::GeneralInventoryActions::doMoveMatch(true); };
    moveToPlayer.tooltip = moveAllTooltip;
    if (moveAllVisible) {
        setBottomRight(moveToContainer, bottomBase + (isPlayer ? 12 : 0), right);
        if (moveToPlayer.visible) setTopRight(moveToPlayer, topBase, right);
        if (!isPlayer) right += 12;
    }

    auto makeSort = [&](bool playerSide, int kind) {
        Button b;
        b.id = kind == 2 ? "sort_rows_button" : (kind == 1 ? "sort_columns_button" : "sort_button");
        if (playerSide) b.id += "_player";
        b.tx = kind == 2 ? 30 : (kind == 1 ? 20 : 10);
        bool show = kind == 2 ? GuiSettings::SHOW_SORT_IN_ROWS_BUTTON.getBooleanValue()
                  : (kind == 1 ? GuiSettings::SHOW_SORT_IN_COLUMNS_BUTTON.getBooleanValue() : GuiSettings::SHOW_REGULAR_SORT_BUTTON.getBooleanValue());
        b.visible = enabled && show && shouldAdd;
        if (kind == 2) {
            b.click   = [playerSide](int button) {
                if (button == 0) inventory::GeneralInventoryActions::doSortInRows(playerSide);
            };
            b.tooltip = [] { return translate("inventoryprofiles.tooltip.sort_rows_button"); };
        } else if (kind == 1) {
            b.click   = [playerSide](int button) {
                if (button == 0) inventory::GeneralInventoryActions::doSortInColumns(playerSide);
            };
            b.tooltip = [] { return translate("inventoryprofiles.tooltip.sort_columns_button"); };
        } else {
            b.click = [playerSide](int button) {
                if (button == 0) {
                    inventory::GeneralInventoryActions::doSort(playerSide);
                } else if (GuiSettings::CHANGE_SORT_ORDER_WITH_SCROLL_WHEEL.getBooleanValue()) {
                    SortSettings::SORT_ORDER.cycle(button != KC::MOUSE_SCROLL_UP);
                    saveToFile();
                }
            };
            b.tooltip = sortTooltip;
        }
        return b;
    };
    Button sortRows = makeSort(false, 2), sortColumns = makeSort(false, 1), sort = makeSort(false, 0);
    Button sortRowsP = makeSort(true, 2), sortColumnsP = makeSort(true, 1), sortP = makeSort(true, 0);

    if (GuiSettings::SHOW_BUTTONS_BOTH_SIDES.getBooleanValue() && !isPlayer && addChestSide) {
        for (Button* b : {&sortRows, &sortColumns, &sort}) {
            if (!b->visible) continue;
            setTopRight(*b, topBase, right);
            right += 12;
        }
        right = 7 + 12;
        if (types.contains(ContainerType::CREATIVE)) right += 18;
        for (Button* b : {&sortRowsP, &sortColumnsP, &sortP}) {
            if (!b->visible) continue;
            setBottomRight(*b, bottomBase, right);
            right += 12;
        }
    } else {
        for (Button* b : {&sortRowsP, &sortColumnsP, &sortP}) b->visible = false;
        for (Button* b : {&sortRows, &sortColumns, &sort}) {
            if (!b->visible) continue;
            if (addChestSide) setTopRight(*b, topBase, right);
            else setBottomRight(*b, bottomBase, right);
            right += 12;
        }
    }
    Button craftCheck{"sort_crafting_checkbox"};
    bool   isCrafting = types.contains(ContainerType::CRAFTING);
    bool   isAnvil    = types.contains(ContainerType::ANVIL);
    bool   isCutter   = types.contains(ContainerType::STONECUTTER);
    if (isAnvil) {
        craftCheck.visible = GuiSettings::SHOW_FAST_RENAME_CHECKBOX.getBooleanValue();
        craftCheck.tx      = GuiSettings::FAST_RENAME_SAVED_VALUE.getBooleanValue() ? 80 : 70;
        craftCheck.click   = [](int button) {
            if (button != 0) return;
            GuiSettings::FAST_RENAME_SAVED_VALUE.toggleBooleanValue();
            saveToFile();
        };
        craftCheck.tooltip = [] { return translate("inventoryprofiles.tooltip.fast_rename"); };
    } else {
        craftCheck.visible = Features::ENABLE_SORTING_AND_MOVING.getBooleanValue() && GuiSettings::SHOW_CONTINUOUS_CRAFTING_CHECKBOX.getBooleanValue() && (isCrafting || isCutter);
        craftCheck.tx      = GuiSettings::CONTINUOUS_CRAFTING_SAVED_VALUE.getBooleanValue() ? 80 : 70;
        craftCheck.click   = [](int button) {
            if (button != 0) return;
            GuiSettings::CONTINUOUS_CRAFTING_SAVED_VALUE.toggleBooleanValue();
            saveToFile();
        };
        craftCheck.tooltip = [] {
            return translate("inventoryprofiles.tooltip.continuous_crafting_checkbox", {keyText(ModSettings::INCLUDE_HOTBAR_MODIFIER)});
        };
    }
    if (isPlayer) setBottomRight(craftCheck, 113, 31);
    else if (isCutter) setBottomRight(craftCheck, 100, 27);
    else if (isAnvil) setBottomRight(craftCheck, 110, 13);
    else setBottomRight(craftCheck, 96, 81);

    for (Button* b : {&moveToContainer, &moveToPlayer, &sortRows, &sortColumns, &sort, &sortRowsP, &sortColumnsP, &sortP, &craftCheck}) mButtons.push_back(std::move(*b));
    mHasLayout = true;
}

SortingButtonCollectionWidget::Button* SortingButtonCollectionWidget::hit(double mx, double my) {
    if (!mHasLayout || platform::screen::ScreenTracker::containerScreenName() != mScreen || !platform::screen::ScreenTracker::containerScreen()) return nullptr;
    for (auto& b : mButtons) {
        if (b.contains(mx, my)) return &b;
    }
    return nullptr;
}

void SortingButtonCollectionWidget::onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx) {
    if (!GuiSettings::ENABLE_INVENTORY_BUTTONS.getBooleanValue()) {
        mHasLayout = false;
        return;
    }
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    Gui2d       g(ctx, *ci);
    auto const& name = platform::screen::ScreenTracker::containerScreenName();
    auto        area = platform::screen::getGuiArea(view, name, g.screenWidth(), g.screenHeight());
    if (!area) {
        mHasLayout = false;
        return;
    }
    layout(name, area->left, area->top, area->width, area->height);
    double mx = platform::input::InputHandler::get().mouseX();
    double my = platform::input::InputHandler::get().mouseY();
    Button const* hovered = nullptr;
    for (auto const& b : mButtons) {
        if (!b.visible) continue;
        bool isHover = b.contains(mx, my);
        if (isHover) hovered = &b;
        if (!g.blitPixels(TEXTURE, b.x, b.y, b.tx, b.ty + (isHover ? 10 : 0), SIZE, SIZE, 256, 256)) {
            g.fill(b.x, b.y, b.x + SIZE, b.y + SIZE, isHover ? 0xFFA0A0FF : 0xFF8080C0);
        }
    }
    if (hovered && GuiSettings::SHOW_BUTTON_TOOLTIPS.getBooleanValue() && hovered->tooltip) {
        auto text = hovered->tooltip();
        if (!text.empty()) renderTooltip(g, text, static_cast<int>(mx), static_cast<int>(my));
    }
    g.flush();
}

void SortingButtonCollectionWidget::renderTooltip(Gui2d& g, std::string const& text, int mouseX, int mouseY) const {
    auto lines = splitLines(text);
    int  w     = 0;
    for (auto const& l : lines) w = std::max(w, g.stringWidth(l));
    int h = static_cast<int>(lines.size()) * Gui2d::LINE_HEIGHT;
    int x = mouseX + 12, y = mouseY - 12;
    if (x + w + 4 > g.screenWidth()) x = std::max(4, mouseX - 16 - w);
    if (y + h + 4 > g.screenHeight()) y = g.screenHeight() - h - 4;
    if (y < 4) y = 4;
    g.fill(x - 3, y - 3, x + w + 3, y + h + 3, 0xF0100010);
    g.outline(x - 3, y - 3, w + 6, h + 6, 0x505000FF);
    for (size_t i = 0; i < lines.size(); ++i) g.drawString(lines[i], x, y + static_cast<int>(i) * Gui2d::LINE_HEIGHT, 0xFFFFFFFF, true);
}

bool SortingButtonCollectionWidget::onMouseClick(double mouseX, double mouseY, int buttonCode, bool buttonState) {
    if (!buttonState) {
        bool was     = mPressedHere;
        mPressedHere = false;
        return was;
    }
    auto* b = hit(mouseX, mouseY);
    if (!b) return false;
    mPressedHere = true;
    if (buttonCode == KC::MOUSE_LEFT && b->click) {
        platform::item::ScopedItemRegistry registry;
        b->click(0);
    }
    return true;
}

bool SortingButtonCollectionWidget::onMouseScroll(double mouseX, double mouseY, double amount) {
    auto* b = hit(mouseX, mouseY);
    if (!b || amount == 0) return false;
    if (b->click) b->click(amount > 0 ? KC::MOUSE_SCROLL_UP : KC::MOUSE_SCROLL_DOWN);
    return true;
}

nlohmann::json SortingButtonCollectionWidget::debugButtons() const {
    nlohmann::json list = nlohmann::json::array();
    for (auto const& b : mButtons) list.push_back({{"id", b.id}, {"x", b.x}, {"y", b.y}, {"visible", b.visible}, {"tooltip", b.tooltip ? b.tooltip() : ""}});
    return {{"screen", mScreen}, {"hasLayout", mHasLayout}, {"buttons", list}};
}

bool SortingButtonCollectionWidget::clickById(std::string const& id, int button) {
    for (auto& b : mButtons) {
        if (b.id == id && b.visible && b.click) {
            b.click(button);
            return true;
        }
    }
    return false;
}

}
