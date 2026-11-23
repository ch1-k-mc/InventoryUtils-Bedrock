// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "platform/util/GameCode.h"
#include "mc/deps/application/gamecore/Platform_GameCore.h"
#include "mc/deps/input/HIDController.h"
#include "mc/gamecore/AppPlatform_GameCore.h"
#include "mc/client/game/IMinecraftGame.h"
#include "gui/GuiManager.h"

#include <atomic>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInputCallbacks.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/input/KeyboardManager.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/input/enums/FocusImpact.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/deps/input/MouseAction.h"
#include "mc/deps/input/win/HIDControllerGameCoreDesktop.h"

#include "platform/input/KeyCodes.h"
#include "gui/Ui2d.h"

namespace inventory_utils::gui {

namespace {

std::vector<ll::event::ListenerPtr> gListeners;
constexpr int                       kVkEscape = 0x1B;

bool hudOnTop() {
    auto ci = ll::service::getClientInstance();
    if (!ci) return false;
    std::string const top = ci->getTopScreenName();
    return top == "hud_screen" || top == "toast_screen";
}

std::uintptr_t gTextInputTarget = 0;
std::uintptr_t gGameGrabTarget  = 0;
std::uintptr_t gImeUpdateTarget = 0, gImeConfirmTarget = 0, gImeEndTarget = 0;
bool           gInImeConfirm = false, gConfirmHandled = false;

HIDController* gameHid() {
    auto ap = ll::service::getAppPlatform();
    if (!ap) return nullptr;
    auto& gc = static_cast<AppPlatform_GameCore&>(*ap);
    auto* pg = gc.mPlatformGameCore.get();
    return pg ? pg->mHIDController.get() : nullptr;
}
int            gVirtualHooks    = 1;

json gLastEvents = json::array();
void note(char const* name, json data) {
    data["event"] = name;
    gLastEvents.push_back(std::move(data));
    if (gLastEvents.size() > 16) gLastEvents.erase(gLastEvents.begin());
}

}

LL_TYPE_INSTANCE_HOOK(
    InvUtilsGuiTextInputHook,
    ll::memory::HookPriority::Normal,
    HIDController,
    gTextInputTarget,
    void,
    std::string const& utf8Text
) {
    if (GuiManager::get().isOpen() && hudOnTop()) {
        GuiManager::get().text(utf8Text);
        if (gInImeConfirm) gConfirmHandled = true;
        return;
    }
    origin(utf8Text);
}

LL_STATIC_HOOK(
    InvUtilsGuiTextCharHook,
    ll::memory::HookPriority::High,
    &ClientInputCallbacks::handleTextChar,
    void,
    ::IClientInstance& client,
    std::string const& utf8text,
    ::FocusImpact      focusImpact
) {
    if (GuiManager::get().isOpen() && GuiManager::get().textEditFocused()) {
        GuiManager::get().text(utf8text);
        if (gInImeConfirm) gConfirmHandled = true;
        return;
    }
    origin(client, utf8text, focusImpact);
}

LL_TYPE_INSTANCE_HOOK(InvUtilsGuiImeUpdateHook, ll::memory::HookPriority::Normal, HIDController, gImeUpdateTarget, void, std::string const& utf8Text) {
    GuiManager::get().composition(utf8Text);
    origin(utf8Text);
}

LL_TYPE_INSTANCE_HOOK(InvUtilsGuiImeEndHook, ll::memory::HookPriority::Normal, HIDController, gImeEndTarget, void) {
    GuiManager::get().composition("");
    origin();
}

LL_TYPE_INSTANCE_HOOK(InvUtilsGuiImeConfirmHook, ll::memory::HookPriority::Normal, HIDController, gImeConfirmTarget, void, std::string const& utf8Text) {
    gInImeConfirm   = true;
    gConfirmHandled = false;
    origin(utf8Text);
    gInImeConfirm = false;
    if (!gConfirmHandled && GuiManager::get().textEditFocused()) GuiManager::get().text(utf8Text);
    GuiManager::get().composition("");
}

namespace {
bool                       gAllowGrab = false;
std::atomic<long long>     gGrabBlocked{0};
std::atomic<long long>     gMoveAbs{0}, gMoveRel{0}, gButtons{0};
std::atomic<int>           gLastX{0}, gLastY{0}, gLastDx{0}, gLastDy{0}, gLastBtnX{0}, gLastBtnY{0};
bool blockGrab() {
    if (gAllowGrab || !GuiManager::get().isOpen()) return false;
    ++gGrabBlocked;
    return true;
}
}

LL_TYPE_INSTANCE_HOOK(InvUtilsGuiClientGrabHook, ll::memory::HookPriority::Normal, ClientInstance, &ClientInstance::$grabMouse, void) {
    if (blockGrab()) return;
    origin();
}

LL_TYPE_INSTANCE_HOOK(InvUtilsGuiGameGrabHook, ll::memory::HookPriority::Normal, IMinecraftGame, gGameGrabTarget, void) {
    if (blockGrab()) return;
    origin();
}

GuiManager& GuiManager::get() {
    static GuiManager instance;
    return instance;
}

void GuiManager::installHooks() {
    InvUtilsGuiClientGrabHook::hook();
    InvUtilsGuiTextCharHook::hook();
}

void GuiManager::start() {
    auto& bus = ll::event::EventBus::getInstance();
    gListeners.push_back(bus.emplaceListener<ll::event::KeyInputEvent>(
        [this](ll::event::KeyInputEvent& e) {
            if (!isOpen() || !hudOnTop()) return;
            int const k = platform::input::KeyCodes::normalize(e.keyCode());
            if (e.isDown()) key(k);
            else keyUp(k);
            if (e.isDown()) e.cancel();
        },
        ll::event::EventPriority::Highest
    ));
    gListeners.push_back(bus.emplaceListener<ll::event::MouseInputEvent>(
        [this](ll::event::MouseInputEvent& e) {
            if (!isOpen() || !hudOnTop()) return;
            int const action = static_cast<int>(e.actionButtonId());
            if (action == ::MouseAction::ActionMoveRelative) {
                ++gMoveRel;
                gLastDx = e.dx();
                gLastDy = e.dy();
            }
            if (action == ::MouseAction::ActionMove) {
                ++gMoveAbs;
                gLastX = e.x();
                gLastY = e.y();
                mMouseX = static_cast<float>(e.x()) / mGuiScale;
                mMouseY = static_cast<float>(e.y()) / mGuiScale;
                if (mScreen) mScreen->mouseDragged(mMouseX, mMouseY);
            } else if (action == ::MouseAction::ActionWheel) {
                scroll(mMouseX, mMouseY, static_cast<int>(e.buttonData()));
            } else if (action == ::MouseAction::ActionLeft || action == ::MouseAction::ActionRight || action == ::MouseAction::ActionMiddle) {
                ++gButtons;
                if (e.buttonData() == ::MouseAction::DataDown) {
                    int const button = action == ::MouseAction::ActionLeft ? 0 : (action == ::MouseAction::ActionRight ? 1 : 2);
                    gLastBtnX = e.x();
                    gLastBtnY = e.y();
                    click(mMouseX, mMouseY, button);
                } else {
                    if (mScreen) mScreen->mouseReleased();
                    return;
                }
            }
            e.cancel();
        },
        ll::event::EventPriority::Highest
    ));
}

void GuiManager::stop() {
    setTextEdit(false, 0, 0, 0);
    if (gVirtualHooks == 0) {
        if (gGameGrabTarget) InvUtilsGuiGameGrabHook::unhook();
        if (gTextInputTarget) InvUtilsGuiTextInputHook::unhook();
        if (gImeUpdateTarget) InvUtilsGuiImeUpdateHook::unhook();
        if (gImeConfirmTarget) InvUtilsGuiImeConfirmHook::unhook();
        if (gImeEndTarget) InvUtilsGuiImeEndHook::unhook();
    }
    gVirtualHooks = 1;
    auto& bus = ll::event::EventBus::getInstance();
    for (auto& l : gListeners) bus.removeListener(l);
    gListeners.clear();
    mScreen.reset();
    mPending.reset();
}

void GuiManager::open(std::unique_ptr<GuiBase> screen) {
    if (!screen) return;
    std::string const n = screen->name();
    if (mScreen) {
        mPending = std::move(screen);
    } else {
        mScreen = std::move(screen);
    }
    note("gui.opened", json{{"screen", n}});
}

void GuiManager::replace(std::function<std::unique_ptr<GuiBase>(std::unique_ptr<GuiBase>)> fn) {
    if (mScreen) mReplace = std::move(fn);
}

void GuiManager::close() {
    if (!mScreen) return;
    mPendingClose = true;
}

void GuiManager::forceClose() {
    setTextEdit(false, 0, 0, 0);
    if (!mScreen && !mPending) return;
    mScreen.reset();
    mPending.reset();
    mReplace      = nullptr;
    mPendingClose = false;
    note("gui.closed", json{{"screen", "forced"}});
}

void GuiManager::render(Ui2d& ui) {
    if (mPendingClose) {
        std::string const n = mScreen ? mScreen->name() : "";
        mScreen.reset();
        mPending.reset();
        mReplace      = nullptr;
        mPendingClose = false;
        setTextEdit(false, 0, 0, 0);
        if (auto ci = ll::service::getClientInstance()) {
            gAllowGrab = true;
            ci->grabMouse();
            gAllowGrab = false;
        }
        note("gui.closed", json{{"screen", n}});
    }
    if (mPending) mScreen = std::move(mPending);
    if (mReplace && mScreen) {
        auto fn  = std::move(mReplace);
        mReplace = nullptr;
        mScreen  = fn(std::move(mScreen));
        if (mScreen) note("gui.replaced", json{{"screen", mScreen->name()}});
    }
    if (!mScreen) return;
    if (gVirtualHooks == 1) installVirtualHooks();
    ++mFrames;
    mGuiScale = ui.guiScale() > 0 ? ui.guiScale() : 1.f;
    if (auto ci = ll::service::getClientInstance()) {
        mLastGrabbed = ci->getMouseGrabbed();
        if (mLastGrabbed) {
            ci->releaseMouse();
            ++mReleases;
        }
    }
    mScreen->render(ui, mMouseX, mMouseY);
    auto* field = mScreen->focusedField();
    if (field && hudOnTop()) setTextEdit(true, field->x, field->y + field->h, field->h);
    else setTextEdit(false, 0, 0, 0);
}

bool GuiManager::click(float x, float y, int button) {
    if (!mScreen) return false;
    mMouseX = x;
    mMouseY = y;
    bool const handled = mScreen->mouseClicked(x, y, button);
    note("gui.clicked", json{{"pos", {x, y}}, {"button", button}, {"handled", handled}});
    return handled;
}

void GuiManager::setTextEdit(bool focused, float x, float y, float height) {
    if (focused == mTextEdit) return;
    mTextEdit = focused;
    auto ci   = ll::service::getClientInstance();
    if (!ci) return;
    auto& kb = ci->getKeyboardManager();
    if (focused) {
        mKeyboardClaimed = kb.tryClaimKeyboardOwnership();
        if (!mKeyboardClaimed) return;
        mKeyboardEnabled = kb.tryEnableKeyboard(std::string{}, 1024, false, false, false, ::Vec2{x, y}, height);
    } else {
        if (mKeyboardEnabled) kb.disableKeyboard();
        if (mKeyboardClaimed) kb.releaseKeyboardOwnership();
        mKeyboardClaimed = mKeyboardEnabled = false;
    }
}

void GuiManager::composition(std::string const& utf8) {
    if (!mScreen) return;
    if (auto* f = mScreen->focusedField()) f->composition = utf8;
}

bool GuiManager::debugIme(std::string const& kind, std::string const& utf8) {
    auto* hid = gameHid();
    if (!hid) return false;
    if (kind == "update") hid->imeUpdateCompositionText(utf8);
    else if (kind == "confirm") hid->imeConfirmComposition(utf8);
    else if (kind == "end") hid->imeEndComposition();
    else return false;
    return true;
}

bool GuiManager::keyUp(int vk) { return mScreen && mScreen->keyReleased(vk); }

bool GuiManager::text(std::string const& utf8) {
    bool const handled = mScreen && mScreen->charTyped(utf8);
    note("gui.text", json{{"chars", utf8.size()}, {"handled", handled}});
    return handled;
}

bool GuiManager::scroll(float x, float y, int delta) { return mScreen && mScreen->mouseScrolled(x, y, delta); }

bool GuiManager::key(int vk) {
    if (!mScreen) return false;
    if (mScreen->keyPressed(vk)) return true;
    if (vk == kVkEscape) {
        close();
        return true;
    }
    return false;
}

json GuiManager::state() const {
    json j{
        {"open", isOpen()},
        {"frames", mFrames},
        {"mouse", {mMouseX, mMouseY}},
        {"mouseGrabbed", mLastGrabbed},
        {"releases", mReleases},
        {"grabBlocked", gGrabBlocked.load()},
        {"raw", {{"moveAbs", gMoveAbs.load()}, {"moveRel", gMoveRel.load()}, {"buttons", gButtons.load()}, {"last", {gLastX.load(), gLastY.load()}}, {"lastDelta", {gLastDx.load(), gLastDy.load()}}, {"lastButtonEventPos", {gLastBtnX.load(), gLastBtnY.load()}}}},
    };
    j["virtualHooks"] = gVirtualHooks;
    j["events"]       = gLastEvents;
    if (mScreen) j["tree"] = mScreen->tree();
    return j;
}

}

namespace inventory_utils::gui {
void GuiManager::installVirtualHooks() {
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    gGameGrabTarget    = platform::gamecode::hookTarget(&ci->getMinecraftGame(), &::IMinecraftGame::grabMouse);
    HIDController* hid = gameHid();
    if (hid) {
        gTextInputTarget  = platform::gamecode::hookTarget(hid, &::HIDController::onTextInput);
        gImeUpdateTarget  = platform::gamecode::hookTarget(hid, &::HIDController::imeUpdateCompositionText);
        gImeConfirmTarget = platform::gamecode::hookTarget(hid, &::HIDController::imeConfirmComposition);
        gImeEndTarget     = platform::gamecode::hookTarget(hid, &::HIDController::imeEndComposition);
    }
    int rc = 0;
    if (gGameGrabTarget) InvUtilsGuiGameGrabHook::hook(false);
    else rc = -1;
    if (gTextInputTarget) InvUtilsGuiTextInputHook::hook(false);
    else rc -= 2;
    if (gImeUpdateTarget) InvUtilsGuiImeUpdateHook::hook(false);
    if (gImeConfirmTarget) InvUtilsGuiImeConfirmHook::hook(false);
    if (gImeEndTarget) InvUtilsGuiImeEndHook::hook(false);
    gVirtualHooks = rc;
}
}
