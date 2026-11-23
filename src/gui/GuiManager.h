// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "gui/Json.h"
#include "gui/GuiBase.h"

namespace inventory_utils::gui {

class Ui2d;

class GuiManager {
public:
    static GuiManager& get();

    static void installHooks();
    void start();
    void stop();

    void open(std::unique_ptr<GuiBase> screen);
    void replace(std::function<std::unique_ptr<GuiBase>(std::unique_ptr<GuiBase>)> fn);
    void close();
    void forceClose();
    [[nodiscard]] bool     isOpen() const { return mScreen != nullptr; }
    [[nodiscard]] GuiBase* current() const { return mScreen.get(); }

    void render(Ui2d& ui);

    bool click(float x, float y, int button);
    bool scroll(float x, float y, int delta);
    bool key(int vk);
    bool keyUp(int vk);
    bool text(std::string const& utf8);
    void composition(std::string const& utf8);
    [[nodiscard]] bool textEditFocused() const { return mTextEdit; }
    bool debugIme(std::string const& kind, std::string const& utf8);

    [[nodiscard]] json state() const;

private:
    void installVirtualHooks();
    void setTextEdit(bool focused, float x, float y, float height);

    std::unique_ptr<GuiBase> mScreen;
    std::unique_ptr<GuiBase> mPending;
    bool                     mPendingClose = false;
    std::function<std::unique_ptr<GuiBase>(std::unique_ptr<GuiBase>)> mReplace;
    float                    mMouseX = 0, mMouseY = 0, mGuiScale = 1;
    std::uint64_t            mFrames = 0, mReleases = 0;
    bool                     mLastGrabbed = false;
    bool                     mTextEdit = false, mKeyboardClaimed = false, mKeyboardEnabled = false;
};

}
