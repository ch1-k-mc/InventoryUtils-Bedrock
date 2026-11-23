// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "gui/GuiConfigs.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

#include "craftingtools/CraftingTools.h"
#include "favorites/HotbarFavorites.h"
#include "gui/Ui2d.h"
#include "ipnext/InitHandler.h"
#include "ipnext/config/Configs.h"
#include "ipnext/event/ProfileSwitchHandler.h"
#include "itemscroller/config/Configs.h"
#include "itemscroller/config/Hotkeys.h"
#include "platform/i18n/I18n.h"
#include "platform/util/Messages.h"

namespace inventory_utils::gui {

using namespace malilib::config;
namespace cfg = ipnext::config;

namespace {

constexpr char const* LABELS_PREFIX  = "inventoryprofiles.gui.config.";
constexpr char const* OPTIONS_PREFIX = "inventoryprofiles.config.";
constexpr char const* LIBIPN_WIDGETS = "libipn/textures/gui/widgets.png";
constexpr float       kModTabsH      = 24;

struct LayoutItem {
    char        kind;
    char const* value;
    char const* type;
};
struct LayoutObject {
    char const*                    name;
    std::vector<LayoutItem> const* items;
};
#define IU_LAYOUT_BEGIN(NAME) std::vector<LayoutItem> const LAYOUT_##NAME{
#define IU_LAYOUT_ITEM(K, V, T) {K, V, T},
#define IU_LAYOUT_END(NAME) };
#include "ipnext/config/ConfigLayout.gen.inc"
#undef IU_LAYOUT_BEGIN
#undef IU_LAYOUT_ITEM
#undef IU_LAYOUT_END

std::vector<LayoutObject> const OBJECTS{
    {"Features", &LAYOUT_Features},
    {"ModSettings", &LAYOUT_ModSettings},
    {"GuiSettings", &LAYOUT_GuiSettings},
    {"SortSettings", &LAYOUT_SortSettings},
    {"LockedSlotsSettings", &LAYOUT_LockedSlotsSettings},
    {"AutoRefillSettings", &LAYOUT_AutoRefillSettings},
    {"EditProfiles", &LAYOUT_EditProfiles},
    {"ScrollSettings", &LAYOUT_ScrollSettings},
    {"Hotkeys", &LAYOUT_Hotkeys},
    {"Tweaks", &LAYOUT_Tweaks},
    {"Debugs", &LAYOUT_Debugs},
};

std::set<std::string> const BUTTONS{"open_config_folder", "reload_rule_files", "open_config_profiles_help", "open_server_profiles"};

constexpr int               MOD_COUNT = 4;
constexpr char const*       MOD_IDS[MOD_COUNT] = {"itemscroller", "ipnext", "craftingtools", "favorites"};
int                         gMod      = 0;
int                         gSelectedByMod[MOD_COUNT]{};
#define gSelected gSelectedByMod[gMod]
std::map<int, float>        gScrollYRaw;
float&                      scrollOf(int section) { return gScrollYRaw[gMod * 100 + section]; }
std::string                 gSearch;
bool                        gSearchFocused = false;
std::set<std::string>       gTextMode;
std::string                 gReloadedUntil;
unsigned long long          gReloadedAt = 0;

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string tr(std::string const& key, std::vector<std::string> const& args = {}) { return platform::i18n::translate(key, args); }

IConfigBase* findOption(std::string const& key) {
    for (auto& [cat, options] : cfg::categories()) {
        for (auto* o : options) {
            if (o->getName() == key) return o;
        }
    }
    if (cfg::Hotkeys::OPEN_CONFIG_MENU.getName() == key) return &cfg::Hotkeys::OPEN_CONFIG_MENU;
    return nullptr;
}

bool isColor(std::string const& key) {
    static std::set<std::string> const colors = [] {
        std::set<std::string> s;
        for (auto const& o : OBJECTS) {
            for (auto const& i : *o.items) {
                if (i.kind == 'o' && std::string(i.type) == "color") s.insert(i.value);
            }
        }
        return s;
    }();
    return colors.contains(key);
}

std::string displayName(std::string const& key) { return trOr(std::string(OPTIONS_PREFIX) + "name." + key, key); }
std::string description(std::string const& key) { return trOr(std::string(OPTIONS_PREFIX) + "description." + key, ""); }

std::string enumText(std::string const& enumType, std::string const& value) {
    std::string const v = lower(value);
    if (enumType.empty()) return value;
    if (enumType == "sorting_method_individual") {
        if (value == "GLOBAL") return tr("inventoryprofiles.enum.sorting_method.global", {enumText("sorting_method", cfg::SortSettings::SORT_ORDER.getOptionListValue())});
        return tr("inventoryprofiles.enum.sorting_method." + v);
    }
    return trOr("inventoryprofiles.enum." + enumType + "." + v, value);
}

std::string hexColor(int argb) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "#%08X", static_cast<unsigned>(argb));
    return buf;
}

void openPath(std::wstring const& p) { ::ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

struct Slider : Widget {
    int                      minValue = 0, maxValue = 100, value = 0;
    std::string              text;
    std::function<void(int)> onSet;
    bool                     enabled = true;

    void render(Ui2d& ui, float mx, float my) override {
        if (!visible) return;
        bool const hover = enabled && contains(mx, my);
        ui.fill(x, y, x + w, y + h, colors::kButtonBorder);
        ui.fill(x + 1, y + 1, x + w - 1, y + h - 1, 0xFF3A3A3A);
        float const range = static_cast<float>(std::max(1, maxValue - minValue));
        float const knobX = x + (w - 8) * (static_cast<float>(value - minValue) / range);
        ui.fill(knobX, y, knobX + 8, y + h, colors::kButtonBorder);
        ui.fill(knobX + 1, y + 1, knobX + 7, y + h - 1, hover ? colors::kButtonHover : colors::kButton);
        float const tw = static_cast<float>(ui.textWidth(text));
        ui.text(text, x + (w - tw) / 2.f, y + (h - 8) / 2.f, hover ? colors::kTextHover : colors::kText, true);
        ui.flush();
    }
    void setFrom(float mx) {
        float const t = std::clamp((mx - x - 4) / std::max(1.f, w - 8), 0.f, 1.f);
        value         = minValue + static_cast<int>(std::lround(t * static_cast<float>(maxValue - minValue)));
        text          = std::to_string(value);
        if (onSet) onSet(value);
    }
    bool mouseClicked(float mx, float my, int button) override {
        if (!enabled || !contains(mx, my) || button != 0) return false;
        setFrom(mx);
        return true;
    }
    [[nodiscard]] bool draggable() const override { return true; }
    void               mouseDragged(float mx, float) override {
        if (enabled) setFrom(mx);
    }
    [[nodiscard]] json toJson() const override {
        json j     = Widget::toJson();
        j["type"]  = "slider";
        j["value"] = value;
        j["range"] = {minValue, maxValue};
        return j;
    }
};

struct ColorSquare : Widget {
    std::uint32_t argb = 0xFFFFFFFF;
    void          render(Ui2d& ui, float, float) override {
        if (!visible) return;
        ui.fill(x, y, x + w, y + h, 0xFFFFFFFF);
        ui.fill(x + 1, y + 1, x + w - 1, y + h - 1, 0xFF000000);
        ui.fill(x + 2, y + 2, x + w - 2, y + h - 2, argb);
    }
};

struct SpriteButton : Widget {
    int                   u = 16, v = 0, hoverU = 32;
    std::function<void()> onPress;
    void                  render(Ui2d& ui, float mx, float my) override {
        if (!visible) return;
        bool const hover = contains(mx, my);
        if (!ui.sprite(LIBIPN_WIDGETS, x, y, hover ? hoverU : u, v, 16, 16)) ui.outline(x, y, w, h, 0xFFA0A0A0);
        ui.flush();
    }
    bool mouseClicked(float mx, float my, int) override {
        if (!contains(mx, my)) return false;
        if (onPress) onPress();
        return true;
    }
};

}

GuiConfigs::GuiConfigs() { buildSections(); }

void        GuiConfigs::setMod(int mod) { gMod = std::clamp(mod, 0, MOD_COUNT - 1); }
char const* GuiConfigs::modId() { return MOD_IDS[gMod]; }

void GuiConfigs::buildSections() {
    mSections.clear();
    if (gMod == 1) {
        for (auto const& o : OBJECTS) {
            if (std::string(o.name) == "Debugs" && !cfg::ModSettings::DEBUG.getBooleanValue()) continue;
            bool any = false;
            for (auto const& i : *o.items) {
                if (i.kind == 'o' && (findOption(i.value) || BUTTONS.contains(i.value))) any = true;
            }
            if (any) mSections.push_back({o.name, tr(std::string(LABELS_PREFIX) + o.name), {}});
        }
    } else if (gMod == 0) {
        namespace C = itemscroller::config;
        std::vector<IConfigBase*> hotkeys;
        for (auto* k : C::Hotkeys::HOTKEY_LIST()) hotkeys.push_back(k);
        mSections.push_back({"Generic", tr("itemscroller.gui.button.config_gui.generic"), C::Generic::OPTIONS()});
        mSections.push_back({"Toggles", tr("itemscroller.gui.button.config_gui.toggles"), C::Toggles::OPTIONS()});
        mSections.push_back({"Hotkeys", tr("itemscroller.gui.button.config_gui.hotkeys"), hotkeys});
    } else if (gMod == 2) {
        auto ct = craftingtools::config::categories();
        mSections.push_back({"Generic", tr("inventoryutils.gui.section.generic"), ct[0].second});
        mSections.push_back({"Hotkeys", tr("inventoryutils.gui.section.hotkeys"), ct[1].second});
    } else {
        mSections.push_back({"Generic", tr("inventoryutils.gui.section.generic"), favorites::config::categories()[0].second});
    }
    gSelected = std::clamp(gSelected, 0, std::max(0, static_cast<int>(mSections.size()) - 1));
}

int         GuiConfigs::selectedIndex() { return gSelected; }
std::string GuiConfigs::selectedName() const { return mSections.empty() ? "" : mSections[static_cast<std::size_t>(gSelected)].name; }

std::string GuiConfigs::title() const { return tr("inventoryutils.gui.title.configs", {"(Bedrock)"}); }

void GuiConfigs::buildEntries() {
    mEntries.clear();
    mAnchors.clear();
    mContentHeight = 0;
    if (mSections.empty()) return;
    if (gMod != 1) {
        float y = 0;
        for (auto* o : mSections[static_cast<std::size_t>(gSelected)].options) {
            Entry e{Entry::Kind::OPTION};
            e.height = 21;
            e.option = o;
            e.key    = o->getName();
            e.label  = trOr(o->getPrettyNameKey(), "");
            if (e.label.empty()) e.label = trOr(o->getNameKey(), o->getName());
            e.desc = trOr(o->getCommentKey(), "");
            e.top  = y;
            y += 21;
            mEntries.push_back(std::move(e));
        }
        mContentHeight = y;
        return;
    }
    std::string const sec = selectedName();
    LayoutObject const* obj = nullptr;
    for (auto const& o : OBJECTS) {
        if (sec == o.name) obj = &o;
    }
    if (!obj) return;
    auto const& items = *obj->items;
    auto        present = [&](std::size_t from, bool stopAtGap) {
        for (std::size_t j = from; j < items.size(); ++j) {
            char const k = items[j].kind;
            if (k == 'c' || k == 'h' || (stopAtGap && k == 'g')) return false;
            if (k == 'o' && (findOption(items[j].value) || BUTTONS.contains(items[j].value))) return true;
        }
        return false;
    };
    bool  hidden = false;
    float top    = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        auto const& it = items[i];
        if (it.kind == 'h') {
            hidden = true;
            continue;
        }
        if (it.kind == 'g') {
            hidden = false;
            if (!present(i + 1, true) && !(i + 1 < items.size() && items[i + 1].kind == 'c' && present(i + 2, false))) continue;
            Entry e{Entry::Kind::GAP};
            e.height = std::atoi(it.value);
            e.top    = top;
            top += static_cast<float>(e.height);
            mEntries.push_back(e);
            continue;
        }
        if (it.kind == 'c') {
            hidden = false;
            if (!present(i + 1, false)) continue;
            std::string const key  = it.value;
            std::string       name = tr(key);
            if (name == key) name = key.substr(key.rfind('.') + 1);
            Entry e{Entry::Kind::CATEGORY};
            e.height = 20;
            e.text   = name;
            e.top    = top;
            mAnchors.push_back({name, top});
            top += 20;
            mEntries.push_back(e);
            continue;
        }
        if (hidden) continue;
        std::string const key = it.value;
        if (BUTTONS.contains(key)) {
            Entry e{Entry::Kind::BUTTON};
            e.height = 21;
            e.key    = key;
            e.label  = displayName(key);
            e.desc   = description(key);
            e.ipn    = true;
            e.type   = "button";
            e.top    = top;
            top += 21;
            mEntries.push_back(e);
            continue;
        }
        IConfigBase* o = findOption(key);
        if (!o) continue;
        Entry e{Entry::Kind::OPTION};
        e.height = 21;
        e.option = o;
        e.key    = key;
        e.label  = displayName(key);
        e.desc   = description(key);
        e.ipn    = true;
        e.type   = it.type;
        if (e.type == "same") e.type = isColor(key) ? "color" : "";
        e.top = top;
        top += 21;
        mEntries.push_back(e);
    }
    mContentHeight = top;
}

std::string GuiConfigs::valueText(Entry const& e) const {
    if (!e.option) return "";
    if (e.option->getType() == ConfigType::OPTION_LIST && e.type.rfind("enum:", 0) == 0) {
        return enumText(e.type.substr(5), static_cast<ConfigOptionList*>(e.option)->getOptionListValue());
    }
    if (e.option->getType() == ConfigType::OPTION_LIST) return optionText(e.option->getStringValue());
    if (e.option->getType() == ConfigType::HOTKEY) return keysDisplayText(static_cast<ConfigHotkey*>(e.option)->getKeybind(), Style::LIBIPN);
    return e.option->getStringValue();
}

bool GuiConfigs::matches(Entry const& e) const {
    if (gSearch.empty() || e.kind == Entry::Kind::GAP || e.kind == Entry::Kind::CATEGORY) return true;
    std::string const q = lower(gSearch);
    return lower(valueText(e)).find(q) != std::string::npos || lower(e.label).find(q) != std::string::npos || lower(e.key).find(q) != std::string::npos;
}

void GuiConfigs::addEntryWidgets(Ui2d& ui, Entry const& e, float x, float y, float w, bool active) {
    std::string const id = e.key;
    auto& l     = addLabel("label:" + id, x + 2, y + 6, e.label);
    l.w         = static_cast<float>(ui.textWidth(l.text));
    l.h         = 9;
    l.hoverText = active ? e.desc : "";

    float const wx = x + std::round(w * 0.60f);
    float const ww = x + w - wx;

    if (e.kind == Entry::Kind::BUTTON) {
        std::string text;
        if (id == "open_config_folder") text = tr("inventoryprofiles.gui.config.button.open_config_folder");
        else if (id == "reload_rule_files")
            text = gReloadedUntil == id && GetTickCount64() - gReloadedAt < 5000 ? tr("inventoryprofiles.gui.config.button.reload_rule_files.reloaded")
                                                                                 : tr("inventoryprofiles.gui.config.button.reload_rule_files");
        else if (id == "open_config_profiles_help") text = tr("inventoryprofiles.gui.config.profiles_help_button");
        else text = tr("inventoryprofiles.gui.config.profiles_config_button");
        auto& b   = addButton("value:" + id, wx + 2, y, ww - 4, 20, text, [this, id](int button) {
            if (button != 0) return;
            if (id == "open_config_folder") openPath(cfg::configDir().wstring());
            else if (id == "reload_rule_files") {
                ipnext::InitHandler::reloadCustomConfigs();
                gReloadedUntil = id;
                gReloadedAt    = GetTickCount64();
            } else if (id == "open_config_profiles_help") openPath(L"https://inventory-profiles-next.github.io/profiles/");
            else openPath(ipnext::event::ProfileSwitchHandler::get().file().wstring());
            requestInit();
        });
        b.enabled = active && !(id == "reload_rule_files" && gReloadedUntil == id && GetTickCount64() - gReloadedAt < 5000);
        return;
    }

    IConfigBase& o = *e.option;
    std::string const resetText = trOr("libipn.common.gui.config.reset", "Reset");
    float const       resetW    = static_cast<float>(ui.textWidth(resetText) + 15);
    float const       resetX    = wx + ww - resetW;
    bool const        hotkey    = o.getType() == ConfigType::HOTKEY;
    auto&             rb        = addButton("reset:" + id, resetX, y, resetW, 20, resetText, [this, &o, hotkey](int) {
        if (hotkey) static_cast<ConfigHotkey&>(o).getKeybind().resetToDefault();
        else o.resetToDefault();
        changed(o);
    });
    rb.enabled = active && (hotkey ? static_cast<ConfigHotkey&>(o).getKeybind().isModified() : o.isModified());
    float const fx = wx, fw = resetX - 1 - wx;

    switch (o.getType()) {
    case ConfigType::BOOLEAN: {
        auto& b   = addBooleanButton("value:" + id, fx, y, fw, 20, static_cast<ConfigBoolean&>(o), Style::LIBIPN);
        b.enabled = active;
        break;
    }
    case ConfigType::OPTION_LIST: {
        auto& b   = addOptionListButton("value:" + id, fx, y, fw, 20, static_cast<ConfigOptionList&>(o), valueText(e));
        b.enabled = active;
        break;
    }
    case ConfigType::HOTKEY: {
        auto& hk = static_cast<ConfigHotkey&>(o);
        addKeybindSettingsIcon("settings:" + id, fx, y, hk, e.label, e.ipn ? Style::LIBIPN : Style::MALILIB);
        auto& kb   = addHotkeyButton("hotkey:" + id, fx + 22, y, fw - 22, 20, hk);
        kb.text    = keysDisplayText(hk.getKeybind(), Style::LIBIPN);
        kb.enabled = active;
        break;
    }
    case ConfigType::INTEGER: {
        auto& n = static_cast<ConfigInteger&>(o);
        if (e.type == "color") {
            auto sq  = std::make_unique<ColorSquare>();
            sq->id   = "color:" + id;
            sq->x    = fx + fw - 20;
            sq->y    = y;
            sq->w    = 20;
            sq->h    = 20;
            sq->argb = static_cast<std::uint32_t>(n.getIntegerValue());
            add(std::move(sq));
            auto& tf    = addValueField("value:" + id, fx + 2, y + 1, fw - 26, 18, o);
            tf.text     = hexColor(n.getIntegerValue());
            tf.cursor   = tf.text.size();
            tf.onCommit = [this, &n](std::string const& text) {
                std::string t = text;
                if (!t.empty() && t[0] == '#') t.erase(0, 1);
                try {
                    std::size_t used = 0;
                    auto        v    = std::stoul(t, &used, 16);
                    if (used != t.size() || t.empty() || t.size() > 8) return false;
                    if (t.size() <= 6) v |= 0xFF000000u;
                    n.setIntegerValue(static_cast<int>(v));
                    changed(n);
                    return true;
                } catch (...) {
                    return false;
                }
            };
            break;
        }
        bool const text = gTextMode.contains(id);
        auto       sb   = std::make_unique<SpriteButton>();
        sb->id          = "numToggle:" + id;
        sb->x           = fx + fw - 16;
        sb->y           = y + 2;
        sb->w           = 16;
        sb->h           = 16;
        sb->u           = 16;
        sb->hoverU      = 32;
        sb->v           = text ? 0 : 16;
        sb->onPress     = [this, id] {
            if (gTextMode.contains(id)) gTextMode.erase(id);
            else gTextMode.insert(id);
            requestInit();
        };
        add(std::move(sb));
        float const vw = fw - 16 - 2 - 1;
        if (text) {
            addValueField("value:" + id, fx + 2, y + 1, vw - 2, 18, o);
        } else {
            auto s      = std::make_unique<Slider>();
            s->id       = "value:" + id;
            s->x        = fx;
            s->y        = y;
            s->w        = vw;
            s->h        = 20;
            s->minValue = n.getMinIntegerValue();
            s->maxValue = n.getMaxIntegerValue();
            s->value    = n.getIntegerValue();
            s->text     = std::to_string(n.getIntegerValue());
            s->enabled  = active;
            s->onSet    = [this, &n](int v) {
                if (v == n.getIntegerValue()) return;
                n.setIntegerValue(v);
                changed(n);
            };
            add(std::move(s));
        }
        break;
    }
    default: {
        addValueField("value:" + id, fx + 2, y + 1, fw - 4, 18, o);
        break;
    }
    }
}

void GuiConfigs::initGui(Ui2d& ui) {
    int const W = mWidth, H = mHeight;
    mDimmed.clear();

    {
        auto&             hk        = cfg::Hotkeys::OPEN_CONFIG_MENU;
        float const       x         = static_cast<float>(W) - 10 - 150;
        std::string const resetText = trOr("libipn.common.gui.config.reset", "Reset");
        float const       resetW    = static_cast<float>(ui.textWidth(resetText) + 15);
        addKeybindSettingsIcon("settings:open_config_menu", x, 5, hk, displayName(hk.getName()), Style::LIBIPN);
        auto& kb = addHotkeyButton("hotkey:open_config_menu", x + 22, 5, 150 - 22 - 1 - resetW, 20, hk);
        kb.text  = keysDisplayText(hk.getKeybind(), Style::LIBIPN);
        auto& rb = addButton("reset:open_config_menu", x + 150 - resetW, 5, resetW, 20, resetText, [this, &hk](int) {
            hk.getKeybind().resetToDefault();
            changed(hk);
        });
        rb.enabled = hk.getKeybind().isModified();
    }

    {
        std::string const names[MOD_COUNT] = {"Item Scroller", "Inventory Profiles Next", tr("inventoryutils.gui.button.config_gui.craftingtools_generic"), tr("inventoryutils.gui.button.config_gui.favorites")};
        float             tx               = 10;
        for (int i = 0; i < MOD_COUNT; ++i) {
            float const tw = static_cast<float>(ui.textWidth(names[i]) + 10);
            auto&       b  = addButton(std::string("mod:") + MOD_IDS[i], tx, 26, tw, 20, names[i], [this, i](int) {
                gMod = i;
                buildSections();
                requestInit();
            });
            b.enabled      = gMod != i;
            tx += tw + 2;
        }
    }

    std::string const searchName = trOr("libipn.common.gui.config.settings_screen.search-box", "Search");
    {
        auto& l     = addLabel("label:search", 20, 32 + kModTabsH, searchName);
        l.w         = static_cast<float>(ui.textWidth(searchName) + 5);
        l.h         = 16;
        l.hoverText = trOr("libipn.common.gui.config.description.settings_screen.search-box", "");
        float const left   = 20 + 5 + static_cast<float>(ui.textWidth(searchName));
        float const rw     = static_cast<float>(ui.textWidth("R") + 9);
        auto        tf     = std::make_unique<TextField>();
        tf->id             = "search";
        tf->x              = left;
        tf->y              = 29 + kModTabsH;
        tf->w              = static_cast<float>(W) - 10 - left - rw - 1;
        tf->h              = 12;
        tf->text           = gSearch;
        tf->cursor         = gSearch.size();
        tf->focused        = gSearchFocused;
        tf->onChange       = [this](std::string const& t) {
            gSearch        = t;
            gSearchFocused = true;
            requestInit();
        };
        tf->onCommit = [](std::string const&) { return true; };
        mSearch      = &add(std::move(tf));
        auto& r      = addButton("reset:search", static_cast<float>(W) - 10 - rw, 29 + kModTabsH, rw, 12, "R", [this](int) {
            gSearch.clear();
            requestInit();
        });
        r.enabled = !gSearch.empty();
    }

    float const navTop = 30 + kModTabsH + 10 + 5;
    float       navW   = 0;
    for (auto const& s : mSections) navW = std::max(navW, static_cast<float>(ui.textWidth(s.label) + 20));
    for (std::size_t i = 0; i < mSections.size(); ++i) {
        auto& b = addButton("nav:" + mSections[i].name, 10, navTop + static_cast<float>(i) * 22, navW, 20, mSections[i].label, [this, i](int) {
            gSelected = static_cast<int>(i);
            requestInit();
        });
        b.enabled = gSelected != static_cast<int>(i);
    }

    buildEntries();
    mListLeft   = 10 + navW + 5;
    mListTop    = navTop;
    mListRight  = static_cast<float>(W) - 10;
    mListBottom = static_cast<float>(H) - 10;

    float const headerH = mAnchors.empty() ? 0 : 14;
    {
        std::string const more  = trOr("libipn.common.gui.config.more", "...");
        float const       avail = mListRight - mListLeft - 20 - static_cast<float>(ui.textWidth(more));
        float             ax    = mListLeft;
        bool              cut   = false;
        for (auto& a : mAnchors) {
            float const tw = static_cast<float>(ui.textWidth("\xC2\xA7" "n" + a.text));
            if (ax - mListLeft + tw > avail) {
                cut = true;
                a.w = 0;
                continue;
            }
            a.x = ax;
            a.y = mListTop + (14 + 1) / 2 - 4;
            a.w = tw;
            ax += tw + 10;
        }
        if (cut) {
            auto& l     = addLabel("anchor:more", ax, mListTop + (14 + 1) / 2 - 4, more);
            l.w         = static_cast<float>(ui.textWidth(more));
            l.h         = 9;
            std::string all;
            for (auto const& a : mAnchors) all += (all.empty() ? "" : "\n") + a.text;
            l.hoverText = all;
        }
    }
    mContentTop = mListTop + headerH;

    float const viewH    = mListBottom - mContentTop;
    float const maxScroll = std::max(0.f, mContentHeight - viewH);
    float&      scroll    = scrollOf(gSelected);
    scroll                = std::clamp(scroll, 0.f, maxScroll);
    float const entryW    = mListRight - mListLeft - (maxScroll > 0 ? 6.f : 0.f) - 2;
    for (auto const& e : mEntries) {
        float const y = mContentTop + e.top - scroll;
        if (y < mContentTop || y + static_cast<float>(e.height) > mListBottom) continue;
        if (e.kind == Entry::Kind::GAP) continue;
        if (e.kind == Entry::Kind::CATEGORY) {
            auto& l = addLabel("category:" + e.text, mListLeft + (entryW - static_cast<float>(ui.textWidth(e.text))) / 2, y + (20 - 8) / 2, e.text);
            (void)l;
            continue;
        }
        bool const active = matches(e);
        addEntryWidgets(ui, e, mListLeft, y, entryW, active);
        if (!active) mDimmed.emplace_back(y, static_cast<float>(e.height));
    }
}

void GuiConfigs::drawContents(Ui2d& ui, float mx, float my) {
    if (mSearch) gSearchFocused = mSearch->focused;
    for (auto const& a : mAnchors) {
        if (a.w <= 0) continue;
        bool const hover = mx >= a.x && mx < a.x + a.w && my >= mListTop && my < mListTop + 14;
        ui.text(hover ? "\xC2\xA7" "e\xC2\xA7" "n" + a.text : a.text, a.x, a.y, colors::kWhite, true);
    }
    ui.flush();
    float const viewH = mListBottom - mContentTop;
    if (mContentHeight > viewH && viewH > 0) {
        float const x0    = mListRight - 6;
        float const thumb = std::max(8.f, viewH * viewH / mContentHeight);
        float const pos   = (viewH - thumb) * (scrollOf(gSelected) / (mContentHeight - viewH));
        ui.fill(x0, mContentTop, mListRight, mListBottom, 0xFF000000);
        ui.fill(x0, mContentTop + pos, mListRight, mContentTop + pos + thumb, 0xFF808080);
        ui.fill(x0, mContentTop + pos, mListRight - 1, mContentTop + pos + thumb - 1, 0xFFC0C0C0);
    }
}

void GuiConfigs::drawOverlay(Ui2d& ui, float, float) {
    float const entryRight = mListRight - (mContentHeight > mListBottom - mContentTop ? 6.f : 0.f) - 2;
    for (auto const& [y, h] : mDimmed) ui.fill(mListLeft, y, entryRight, y + h, 0xD2000000);
}

bool GuiConfigs::mouseClicked(float mx, float my, int button) {
    if (button == 0 && my >= mListTop && my < mListTop + 14) {
        for (auto const& a : mAnchors) {
            if (a.w > 0 && mx >= a.x && mx < a.x + a.w) {
                scrollOf(gSelected) = a.scrollTo;
                requestInit();
                return true;
            }
        }
    }
    float const entryRight = mListRight - (mContentHeight > mListBottom - mContentTop ? 6.f : 0.f) - 2;
    if (mx >= mListLeft && mx < entryRight) {
        for (auto const& [y, h] : mDimmed) {
            if (my >= y && my < y + h) return true;
        }
    }
    return ConfigGuiBase::mouseClicked(mx, my, button);
}

std::string const& GuiConfigs::searchText() { return gSearch; }

bool GuiConfigs::mouseScrolled(float mx, float my, int delta) {
    if (mx < mListLeft) return false;
    scrollOf(gSelected) += delta > 0 ? -20.f : 20.f;
    if (scrollOf(gSelected) < 0) scrollOf(gSelected) = 0;
    requestInit();
    (void)my;
    return true;
}

}
