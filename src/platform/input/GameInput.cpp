// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/input/GameInput.h"

#include <chrono>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/GuiData.h"
#include "mc/deps/input/Mouse.h"
#include "mc/deps/input/MouseAction.h"
#include "mc/deps/input/MouseDevice.h"

namespace inventory_utils::platform::input::GameInput {

namespace {
float guiScale() {
    auto ci = ll::service::getClientInstance();
    if (!ci) return 1.0f;
    float s = static_cast<float>(ci->getGuiData()->mGuiScale);
    return s > 0 ? s : 1.0f;
}
short px(double ui) { return static_cast<short>(ui * guiScale()); }

using Clock = std::chrono::steady_clock;
Clock::time_point gHoldUntil{};
int               gHoldX = 0, gHoldY = 0;
void hold(short x, short y) {
    gHoldX     = x;
    gHoldY     = y;
    gHoldUntil = Clock::now() + std::chrono::milliseconds(300);
}
}

bool shouldDropMove(int pxX, int pxY) { return Clock::now() < gHoldUntil && (pxX != gHoldX || pxY != gHoldY); }

void move(double uiX, double uiY) {
    hold(px(uiX), px(uiY));
    Mouse::_instance().feed(MouseAction::ActionMove, 0, px(uiX), px(uiY), 0, 0, false);
}

void button(bool down, double uiX, double uiY) {
    hold(px(uiX), px(uiY));
    Mouse::_instance().feed(MouseAction::ActionLeft, down ? MouseAction::DataDown : MouseAction::DataUp, px(uiX), px(uiY), 0, 0, false);
}

void wheel(int delta, double uiX, double uiY) {
    hold(px(uiX), px(uiY));
    Mouse::_instance().feed(MouseAction::ActionWheel, static_cast<schar>(delta), px(uiX), px(uiY), 0, 0, false);
}

}
