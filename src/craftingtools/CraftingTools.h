// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <string>
#include <vector>

#include "malilib/config/ConfigOptions.h"
#include "platform/input/InputHandler.h"

class MinecraftUIRenderContext;
class ScreenView;

namespace inventory_utils::craftingtools {

namespace config {
using namespace malilib::config;
extern ConfigHotkey     ROTATE;
extern ConfigHotkey     ROTATE_COUNTER_CLOCKWISE;
extern ConfigHotkey     BALANCE;
extern ConfigHotkey     SPREAD;
extern ConfigHotkey     CLEAR;
extern ConfigHotkey     COMPRESS;
extern ConfigHotkey     COMPRESS_ALL;
extern ConfigHotkey     DECOMPRESS;
extern ConfigHotkey     DECOMPRESS_ALL;
extern ConfigOptionList MODE;
extern ConfigBoolean    SHOW_BUTTONS;
std::vector<std::pair<std::string, std::vector<IConfigBase*>>> categories();
std::vector<ConfigHotkey*> const&                              hotkeys();
void                                                           loadFromFile();
void                                                           saveToFile();
}

namespace Actions {
bool rotate(bool counterClockwise);
bool balance();
bool spread();
bool clear(bool force = false);
bool compress(bool all, int slot = -1);
bool decompress(bool all, int slot = -1);
}

class GridButtons : public platform::input::IMouseInputHandler {
public:
    static GridButtons& get();
    void onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx);
    bool onMouseClick(double mouseX, double mouseY, int buttonCode, bool buttonState) override;
    [[nodiscard]] std::string debugJson() const;
    bool clickById(std::string const& id, bool shift);

private:
    struct Button {
        std::string id;
        std::string label;
        int         x = 0, y = 0;
    };
    std::vector<Button> mButtons;
    bool                mPressedHere = false;
};

void install();
void uninstall();

}
