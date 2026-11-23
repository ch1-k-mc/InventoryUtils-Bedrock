// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "gui/ConfigGuiBase.h"

#include <cctype>
#include <climits>

#include "gui/GuiKeybindSettings.h"
#include "gui/GuiManager.h"
#include "gui/Ui2d.h"
#include "platform/i18n/I18n.h"
#include "platform/input/InputHandler.h"
#include "platform/input/KeyCodes.h"

namespace inventory_utils::gui {

using namespace malilib::config;
using malilib::hotkeys::KeybindMulti;
using malilib::hotkeys::KeybindSettings;

namespace {

bool gDirty = false;

constexpr char const* MALILIB_WIDGETS = "malilib/textures/gui/gui_widgets.png";
constexpr char const* LIBIPN_WIDGETS  = "libipn/textures/gui/widgets.png";

struct KeybindSettingsIcon : Widget {
    ConfigHotkey*                   option = nullptr;
    Style                           style  = Style::MALILIB;
    std::function<void(int button)> onPress;

    void render(Ui2d& ui, float mx, float my) override {
        if (!visible || !option) return;
        auto const& kb = option->getKeybind();
        auto const& s  = kb.getSettings();
        if (style == Style::MALILIB) {
            ui.fill(x, y, x + 20, y + 20, kb.areSettingsModified() ? 0xFFFFBB33 : 0xFFFFFFFF);
            ui.fill(x + 1, y + 1, x + 19, y + 19, 0xFF000000);
            int const w  = 18;
            int const v1 = static_cast<int>(s.getActivateOn()) * w;
            int const v2 = s.getAllowExtraKeys() ? 0 : w;
            int const v3 = s.isOrderSensitive() ? w : 0;
            int const v4 = s.isExclusive() ? w : 0;
            int const v5 = s.shouldCancel() ? w : 0;
            ui.sprite(MALILIB_WIDGETS, x + 1, y + 1, 0, v1, w, w);
            ui.sprite(MALILIB_WIDGETS, x + 1, y + 1, 18, v2, w, w);
            ui.sprite(MALILIB_WIDGETS, x + 1, y + 1, 36, v3, w, w);
            ui.sprite(MALILIB_WIDGETS, x + 1, y + 1, 54, v4, w, w);
            ui.sprite(MALILIB_WIDGETS, x + 1, y + 1, 72, v5, w, w);
        } else {
            int const u = kb.areSettingsModified() ? 40 : 20;
            int const v = 160 + static_cast<int>(s.getActivateOn()) * 20;
            if (!ui.sprite(LIBIPN_WIDGETS, x, y, u, v, 20, 20)) ui.outline(x, y, 20, 20, 0xFFA0A0A0);
        }
        (void)mx;
        (void)my;
        ui.flush();
    }
    bool mouseClicked(float mx, float my, int button) override {
        if (!contains(mx, my)) return false;
        if (onPress) onPress(button);
        return true;
    }
    [[nodiscard]] json toJson() const override {
        json j    = Widget::toJson();
        j["type"] = "keybindSettings";
        return j;
    }
};

std::vector<std::string> splitKeys(std::string const& s) {
    std::vector<std::string> out;
    std::size_t              start = 0;
    while (start < s.size()) {
        auto comma = s.find(',', start);
        if (comma == std::string::npos) comma = s.size();
        if (comma > start) out.push_back(s.substr(start, comma - start));
        start = comma + 1;
    }
    return out;
}

}

std::string trOr(std::string const& key, std::string const& fallback, std::vector<std::string> const& args) {
    std::string s = platform::i18n::translate(key, args);
    return s == key ? fallback : s;
}

std::string optionText(std::string const& value) {
    std::string k = value;
    for (char& c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return trOr("inventoryutils.option." + k, value);
}

bool ConfigGuiBase::consumeDirty() {
    bool const d = gDirty;
    gDirty       = false;
    return d;
}
void ConfigGuiBase::markDirty() { gDirty = true; }

void ConfigGuiBase::changed(IConfigBase& option) {
    gDirty = true;
    if (option.getType() == ConfigType::HOTKEY) platform::input::InputHandler::get().updateUsedKeys();
    requestInit();
}

std::string keysDisplayText(KeybindMulti const& kb, Style style) {
    auto        keys = splitKeys(kb.getStringValue());
    std::string s;
    for (auto const& k : keys) s += (s.empty() ? "" : " + ") + k;
    if (s.empty() && style == Style::MALILIB) return trOr("malilib.gui.button.empty_keybind", "NONE");
    return s;
}

std::string keybindSettingsTooltip(KeybindMulti const& kb, Style style) {
    auto const& s   = kb.getSettings();
    auto        act = optionText(KeybindSettings::actionName(s.getActivateOn()));
    auto        ctx = optionText(KeybindSettings::contextName(s.getContext()));
    if (style == Style::MALILIB) {
        std::string const yes = trOr("malilib.gui.label.yes", "yes"), no = trOr("malilib.gui.label.no", "no");
        auto              yn  = [&](bool v, bool goodWhenYes) {
            return std::string(v == goodWhenYes ? "\xC2\xA7" "a" : "\xC2\xA7" "6") + (v ? yes : no);
        };
        std::string t = "\xC2\xA7" "f\xC2\xA7" "n" + trOr("malilib.gui.label.keybind_settings.title_advanced_keybind_settings", "Advanced Keybind settings") + "\xC2\xA7" "r";
        t += "\n" + trOr("malilib.gui.label.keybind_settings.activate_on", "Activate On") + ": \xC2\xA7" "9" + act + "\xC2\xA7" "r";
        t += "\n" + trOr("malilib.gui.label.keybind_settings.allow_empty_keybind", "Allow empty keybind") + ": " + yn(s.getAllowEmpty(), true) + "\xC2\xA7" "r";
        t += "\n" + trOr("malilib.gui.label.keybind_settings.allow_extra_keys", "Allow extra keys") + ": " + yn(s.getAllowExtraKeys(), true) + "\xC2\xA7" "r";
        t += "\n" + trOr("malilib.gui.label.keybind_settings.order_sensitive", "Order Sensitive") + ": " + yn(s.isOrderSensitive(), false) + "\xC2\xA7" "r";
        t += "\n" + trOr("malilib.gui.label.keybind_settings.cancel_further", "Cancel further processing") + ": " + yn(s.shouldCancel(), false) + "\xC2\xA7" "r";
        t += "\n" + trOr("malilib.gui.label.keybind_settings.exclusive", "Exclusive") + ": " + yn(s.isExclusive(), false) + "\xC2\xA7" "r";
        t += "\n" + trOr("malilib.gui.label.keybind_settings.context", "Context") + ": \xC2\xA7" "9" + ctx + "\xC2\xA7" "r";
        t += "\n\n" + trOr("malilib.gui.label.keybind_settings.tips", "- Left click to configure\n- Right click to reset to defaults");
        return t;
    }
    std::string const p   = "libipn.common.gui.config.";
    std::string const yes = trOr(p + "yes", "Yes"), no = trOr(p + "no", "No");
    std::string       t   = "\xC2\xA7" "n" + trOr(p + "advanced_keybind_settings", "Advanced Keybind Settings") + "\xC2\xA7" "r";
    t += "\n" + trOr(p + "activate_on", "Activate On") + ": \xC2\xA7" "9" + act + "\xC2\xA7" "r";
    t += "\n" + trOr(p + "context", "Context") + ": \xC2\xA7" "9" + ctx + "\xC2\xA7" "r";
    t += "\n" + trOr(p + "allow_extra_keys", "Allow Extra Keys") + ": \xC2\xA7" "6" + (s.getAllowExtraKeys() ? yes : no) + "\xC2\xA7" "r";
    t += "\n" + trOr(p + "order_sensitive", "Order Sensitive") + ": \xC2\xA7" "6" + (s.isOrderSensitive() ? yes : no) + "\xC2\xA7" "r";
    t += "\n\n" + trOr(p + "keybind_settings_tips", "Left click to configure, right click to reset");
    return t;
}

Button& ConfigGuiBase::addBooleanButton(std::string const& id, float x, float y, float w, float h, ConfigBoolean& option, Style style) {
    std::string text;
    if (style == Style::MALILIB) {
        text = option.getBooleanValue() ? trOr("malilib.gui.button.true", "\xC2\xA7" "2true\xC2\xA7" "r")
                                        : trOr("malilib.gui.button.false", "\xC2\xA7" "4false\xC2\xA7" "r");
    } else {
        text = option.getBooleanValue() ? trOr("libipn.common.gui.config.true", "true") : trOr("libipn.common.gui.config.false", "false");
    }
    return addButton(id, x, y, w, h, text, [this, &option](int) {
        option.toggleBooleanValue();
        changed(option);
    });
}

Button& ConfigGuiBase::addOptionListButton(std::string const& id, float x, float y, float w, float h, ConfigOptionList& option, std::string text) {
    return addButton(id, x, y, w, h, std::move(text), [this, &option](int button) {
        option.cycle(button != 1);
        changed(option);
    });
}

KeybindButton& ConfigGuiBase::addHotkeyButton(std::string const& id, float x, float y, float w, float h, ConfigHotkey& option) {
    auto kb   = std::make_unique<KeybindButton>();
    kb->id    = id;
    kb->x     = x;
    kb->y     = y;
    kb->w     = w;
    kb->h     = h;
    kb->text  = keysDisplayText(option.getKeybind(), Style::MALILIB);
    kb->onSet = [this, &option](std::string const& keys) {
        option.setValueFromString(keys);
        changed(option);
    };
    return add(std::move(kb));
}

Widget& ConfigGuiBase::addKeybindSettingsIcon(std::string const& id, float x, float y, ConfigHotkey& option, std::string const& displayName, Style style) {
    auto w       = std::make_unique<KeybindSettingsIcon>();
    w->id        = id;
    w->x         = x;
    w->y         = y;
    w->w         = 20;
    w->h         = 20;
    w->option    = &option;
    w->style     = style;
    w->hoverText = keybindSettingsTooltip(option.getKeybind(), style);
    w->onPress   = [this, &option, displayName, style](int button) {
        if (button == 1) {
            option.getKeybind().resetSettingsToDefaults();
            changed(option);
        } else if (button == 0) {
            openKeybindSettings(option, displayName, style);
        }
    };
    return add(std::move(w));
}

TextField& ConfigGuiBase::addValueField(std::string const& id, float x, float y, float w, float h, IConfigBase& option) {
    auto tf       = std::make_unique<TextField>();
    tf->id        = id;
    tf->x         = x;
    tf->y         = y;
    tf->w         = w;
    tf->h         = h;
    tf->text      = option.getStringValue();
    tf->cursor    = tf->text.size();
    tf->maxLength = option.getType() == ConfigType::STRING_LIST ? 4096 : 1024;
    tf->onCommit  = [this, &option](std::string const& text) {
        if (option.getType() == ConfigType::INTEGER) {
            auto& n = static_cast<ConfigInteger&>(option);
            try {
                std::size_t used = 0;
                long long   v    = std::stoll(text, &used);
                if (used != text.size() || v < n.getMinIntegerValue() || v > n.getMaxIntegerValue()) return false;
            } catch (...) {
                return false;
            }
        }
        if (text == option.getStringValue()) return true;
        bool const ok = option.setValueFromString(text);
        if (ok) changed(option);
        return ok;
    };
    return add(std::move(tf));
}

void ConfigGuiBase::openKeybindSettings(ConfigHotkey& option, std::string const& displayName, Style style) {
    GuiManager::get().replace([&option, displayName, style](std::unique_ptr<GuiBase> self) -> std::unique_ptr<GuiBase> {
        return std::make_unique<GuiKeybindSettings>(option, displayName, style, std::move(self));
    });
}

}
