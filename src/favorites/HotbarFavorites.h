// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <string>
#include <vector>

#include "malilib/config/ConfigOptions.h"
#include "platform/input/InputHandler.h"

class MinecraftUIRenderContext;
class ScreenView;

namespace inventory_utils::favorites {

namespace config {
using namespace malilib::config;
extern ConfigBoolean ENABLED;
extern ConfigHotkey  SAVE_HOTBAR;
std::vector<std::pair<std::string, std::vector<IConfigBase*>>> categories();
}

struct Entry {
    std::string              label;
    std::vector<std::string> items;
};

struct RecallResult {
    int fromCreative  = 0;
    int fromInventory = 0;
    int missing       = 0;
    int unchanged     = 0;
};

class HotbarFavorites : public platform::input::IMouseInputHandler {
public:
    static HotbarFavorites& get();

    void install();
    void uninstall();

    bool         saveCurrent();
    bool         remove(int index);
    RecallResult recall(int index);
    bool         clickRowForTest(int index);
    void         isolateForTest();
    void         restoreAfterTest();

    [[nodiscard]] std::vector<Entry> const& entries() const { return mEntries; }
    [[nodiscard]] RecallResult const&       lastRecall() const { return mLast; }
    [[nodiscard]] std::string               debugButton() const;

    void onContainerOverlay(ScreenView& view, MinecraftUIRenderContext& ctx);
    bool onMouseClick(double mouseX, double mouseY, int buttonCode, bool buttonState) override;

private:
    void        load();
    void        save() const;
    void        changed();
    [[nodiscard]] int  visibleRows() const;
    [[nodiscard]] bool panelActive() const;
    [[nodiscard]] bool inPanel(double mx, double my) const;
    [[nodiscard]] int  rowAt(double mx, double my) const;

    std::vector<Entry> mEntries;
    std::vector<Entry> mStashed;
    bool               mIsolated = false;
    RecallResult       mLast;
    int                mButtonX = 0, mButtonY = 0;
    bool               mButtonVisible = false;
    bool               mPressedHere   = false;
    bool               mPanelVisible  = false;
    unsigned long long mPanelDrawnAt  = 0;
    int                mPanelX = 2, mPanelY = 2, mPanelH = 0, mScroll = 0;
};

}
