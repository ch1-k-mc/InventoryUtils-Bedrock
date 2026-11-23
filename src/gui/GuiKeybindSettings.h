// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <memory>
#include <string>

#include "gui/ConfigGuiBase.h"

namespace inventory_utils::gui {

class GuiKeybindSettings : public ConfigGuiBase {
public:
    GuiKeybindSettings(malilib::config::ConfigHotkey& option, std::string displayName, Style style, std::unique_ptr<GuiBase> parent);

    void        initGui(Ui2d& ui) override;
    std::string title() const override;
    char const* name() const override { return "GuiKeybindSettings"; }
    void        render(Ui2d& ui, float mx, float my) override;
    bool        keyPressed(int key) override;
    bool        mouseClicked(float mx, float my, int button) override;

    void close();

private:
    void apply();

    malilib::config::ConfigHotkey& mOption;
    std::string                    mDisplayName;
    Style                          mStyle;
    std::unique_ptr<GuiBase>       mParent;
    malilib::config::ConfigOptionList mActivateOn{"activateOn", {"PRESS", "RELEASE", "BOTH"}, "PRESS"};
    malilib::config::ConfigOptionList mContext{"context", {"INGAME", "GUI", "ANY"}, "INGAME"};
    malilib::config::ConfigBoolean    mAllowEmpty{"allowEmpty", false};
    malilib::config::ConfigBoolean    mAllowExtra{"allowExtra", false};
    malilib::config::ConfigBoolean    mOrderSensitive{"orderSensitive", false};
    malilib::config::ConfigBoolean    mExclusive{"exclusive", false};
    malilib::config::ConfigBoolean    mCancel{"cancel", false};
    float                             mLeft = 0, mTop = 0, mW = 0, mH = 0;
};

}
