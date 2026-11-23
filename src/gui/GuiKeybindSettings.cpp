// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "gui/GuiKeybindSettings.h"

#include <algorithm>

#include "gui/GuiManager.h"
#include "gui/Ui2d.h"
#include "platform/input/InputHandler.h"

namespace inventory_utils::gui {

using namespace malilib::config;
using malilib::hotkeys::KeyAction;
using malilib::hotkeys::KeybindSettings;

namespace {
constexpr int kVkEscape = 0x1B;
}

GuiKeybindSettings::GuiKeybindSettings(ConfigHotkey& option, std::string displayName, Style style, std::unique_ptr<GuiBase> parent)
: mOption(option),
  mDisplayName(std::move(displayName)),
  mStyle(style),
  mParent(std::move(parent)) {
    auto const& s = option.getKeybind().getSettings();
    mActivateOn.setValueFromString(KeybindSettings::actionName(s.getActivateOn()));
    mContext.setValueFromString(KeybindSettings::contextName(s.getContext()));
    mAllowEmpty.setBooleanValue(s.getAllowEmpty());
    mAllowExtra.setBooleanValue(s.getAllowExtraKeys());
    mOrderSensitive.setBooleanValue(s.isOrderSensitive());
    mExclusive.setBooleanValue(s.isExclusive());
    mCancel.setBooleanValue(s.shouldCancel());
}

std::string GuiKeybindSettings::title() const {
    if (mStyle == Style::LIBIPN) return trOr("libipn.common.gui.config.advanced_keybind_settings", "Advanced Keybind Settings");
    return "\xC2\xA7" "l" + trOr("malilib.gui.title.keybind_settings.advanced", "Advanced Keybind Settings for %s", {mDisplayName}) + "\xC2\xA7" "r";
}

void GuiKeybindSettings::initGui(Ui2d& ui) {
    struct Item {
        char const*  key;
        char const*  fallback;
        IConfigBase* option;
    };
    bool const        ipn = mStyle == Style::LIBIPN;
    std::string const p   = ipn ? "libipn.common.gui.config." : "malilib.gui.label.keybind_settings.";
    std::vector<Item> items{
        {ipn ? "activate_on" : "activate_on", "Activate On", &mActivateOn},
        {"context", "Context", &mContext},
        {ipn ? "" : "allow_empty_keybind", "Allow empty keybind", &mAllowEmpty},
        {"allow_extra_keys", "Allow extra keys", &mAllowExtra},
        {"order_sensitive", "Order Sensitive", &mOrderSensitive},
        {ipn ? "" : "exclusive", "Exclusive", &mExclusive},
        {ipn ? "" : "cancel_further", "Cancel further processing", &mCancel},
    };
    if (ipn) items.erase(std::remove_if(items.begin(), items.end(), [](Item const& i) { return i.key[0] == 0; }), items.end());

    int labelW = 0;
    for (auto const& i : items) labelW = std::max(labelW, ui.textWidth(trOr(p + i.key, i.fallback)));
    float const configW = 100;
    mW                  = std::max(static_cast<float>(labelW) + configW + 30, static_cast<float>(ui.textWidth(title()) + 20));
    mH                  = static_cast<float>(items.size()) * 22 + 30;
    mLeft               = std::floor((static_cast<float>(mWidth) - mW) / 2);
    mTop                = std::floor((static_cast<float>(mHeight) - mH) / 2);

    float y = mTop + 24;
    for (auto const& i : items) {
        float const x  = mLeft + 10;
        std::string id = std::string("ks:") + i.option->getName();
        addLabel("label:" + id, x, y + 4 + 2, trOr(p + i.key, i.fallback));
        float const vx = x + static_cast<float>(labelW) + 10;
        if (i.option->getType() == ConfigType::BOOLEAN) {
            addBooleanButton(id, vx, y, configW, 20, static_cast<ConfigBoolean&>(*i.option), mStyle);
        } else {
            auto& ol = static_cast<ConfigOptionList&>(*i.option);
            addOptionListButton(id, vx, y, configW, 20, ol, optionText(ol.getOptionListValue()));
        }
        y += 22;
    }
}

void GuiKeybindSettings::render(Ui2d& ui, float mx, float my) {
    if (mParent) mParent->render(ui, -1000, -1000);
    if (needsInit(ui)) rebuild(ui);
    ui.fill(mLeft, mTop, mLeft + mW, mTop + mH, 0xFF999999);
    ui.fill(mLeft + 1, mTop + 1, mLeft + mW - 1, mTop + mH - 1, 0xFF000000);
    ui.text(title(), mLeft + 10, mTop + 6, colors::kWhite, true);
    ui.flush();
    renderWidgets(ui, mx, my);
}

bool GuiKeybindSettings::mouseClicked(float mx, float my, int button) {
    if (ConfigGuiBase::mouseClicked(mx, my, button)) return true;
    return true;
}

bool GuiKeybindSettings::keyPressed(int key) {
    if (key == kVkEscape) {
        close();
        return true;
    }
    return ConfigGuiBase::keyPressed(key);
}

void GuiKeybindSettings::apply() {
    auto action = [](std::string const& v) { return v == "RELEASE" ? KeyAction::RELEASE : (v == "BOTH" ? KeyAction::BOTH : KeyAction::PRESS); };
    auto ctx    = [](std::string const& v) {
        return v == "GUI" ? KeybindSettings::Context::GUI : (v == "ANY" ? KeybindSettings::Context::ANY : KeybindSettings::Context::INGAME);
    };
    auto next = KeybindSettings::create(
        ctx(mContext.getOptionListValue()),
        action(mActivateOn.getOptionListValue()),
        mAllowExtra.getBooleanValue(),
        mOrderSensitive.getBooleanValue(),
        mExclusive.getBooleanValue(),
        mCancel.getBooleanValue(),
        mAllowEmpty.getBooleanValue()
    );
    if (!(next == mOption.getKeybind().getSettings())) {
        mOption.getKeybind().setSettings(next);
        markDirty();
        platform::input::InputHandler::get().updateUsedKeys();
    }
}

void GuiKeybindSettings::close() {
    apply();
    if (mParent) mParent->requestInit();
    GuiManager::get().replace([](std::unique_ptr<GuiBase> self) -> std::unique_ptr<GuiBase> {
        auto* me = static_cast<GuiKeybindSettings*>(self.get());
        return std::move(me->mParent);
    });
}

}
