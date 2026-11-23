// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <memory>
#include <string>

#include "gui/GuiBase.h"
#include "malilib/config/ConfigOptions.h"

namespace inventory_utils::gui {

enum class Style { MALILIB, LIBIPN };

class ConfigGuiBase : public GuiBase {
public:
    static bool consumeDirty();
    static void markDirty();

protected:
    void changed(malilib::config::IConfigBase& option);

    Button& addBooleanButton(std::string const& id, float x, float y, float w, float h, malilib::config::ConfigBoolean& option, Style style);
    Button& addOptionListButton(std::string const& id, float x, float y, float w, float h, malilib::config::ConfigOptionList& option, std::string text);
    KeybindButton& addHotkeyButton(std::string const& id, float x, float y, float w, float h, malilib::config::ConfigHotkey& option);
    Widget& addKeybindSettingsIcon(std::string const& id, float x, float y, malilib::config::ConfigHotkey& option, std::string const& displayName, Style style);
    TextField& addValueField(std::string const& id, float x, float y, float w, float h, malilib::config::IConfigBase& option);

    void openKeybindSettings(malilib::config::ConfigHotkey& option, std::string const& displayName, Style style);
};

std::string keysDisplayText(malilib::hotkeys::KeybindMulti const& kb, Style style);
std::string keybindSettingsTooltip(malilib::hotkeys::KeybindMulti const& kb, Style style);
std::string trOr(std::string const& key, std::string const& fallback, std::vector<std::string> const& args = {});
std::string optionText(std::string const& value);

}
