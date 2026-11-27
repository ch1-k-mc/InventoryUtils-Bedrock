// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/inventory/InventorySession.h"

#include <chrono>

#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/CraftingType.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/gui/VisibilityFlag.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"

#include "platform/inventory/OpenInventory.h"
#include "platform/item/ScopedItemRegistry.h"
#include "platform/screen/ScreenTracker.h"

namespace inventory_utils::platform::inventory {

namespace {

using screen::ScreenTracker;

enum class State { Idle, Opening, Holding, Closing };

constexpr int kOpenTimeoutFrames  = 120;
constexpr int kCloseTimeoutFrames = 120;
constexpr auto kWatchdog           = std::chrono::seconds(4);

struct Session {
    State                                 state = State::Idle;
    InventorySession::Work                work;
    InventorySession::Done                done;
    InventorySession::Hide                hide   = InventorySession::Hide::Invisible;
    std::weak_ptr<UIControl>              hiddenRoot;
    ui::VisibilityFlag                    rootVisible = ui::VisibilityFlag::All;
    int                                   frames = 0;
    int                                   openFrames = 0;
    int                                   holdFrames = 0;
    std::chrono::steady_clock::time_point started;
};

Session        gSession;
nlohmann::json gLast = nlohmann::json::object();

void finish(bool ok, std::string const& reason) {
    if (auto root = gSession.hiddenRoot.lock()) root->mVisible = gSession.rootVisible;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - gSession.started).count();
    gLast        = {
        {"ok",         ok                 },
        {"reason",     reason             },
        {"openFrames", gSession.openFrames},
        {"frames",     gSession.frames    },
        {"ms",         elapsed            },
        {"hide",       static_cast<int>(gSession.hide)},
    };
    auto done      = std::move(gSession.done);
    gSession       = Session{};
    if (done) done(ok, reason);
}

void closeHidden(ContainerScreenController& csc) {
    csc.leaveScreen("inventory_screen");
    gSession.state = State::Closing;
}

void runWork(ContainerScreenController& csc, ContainerManagerController& mgr) {
    item::ScopedItemRegistry registry;
    gSession.work(csc, mgr);
}

}

void InventorySession::install() {}

void InventorySession::uninstall() { gSession = Session{}; }

bool InventorySession::busy() { return gSession.state != State::Idle; }

bool InventorySession::onInventoryKey() { return busy(); }

nlohmann::json InventorySession::lastResult() { return gLast; }

bool InventorySession::request(Work work, Hide hide, Done done, int holdFrames) {
    if (busy() || !work) return false;
    gSession.work    = std::move(work);
    gSession.done    = std::move(done);
    gSession.hide    = hide;
    gSession.holdFrames = holdFrames;
    gSession.started = std::chrono::steady_clock::now();
    if (auto* csc = ScreenTracker::containerScreen()) {
        if (auto* mgr = ScreenTracker::containerManager()) {
            gSession.state = State::Opening;
            runWork(*csc, *mgr);
            finish(true, "already_open");
            return true;
        }
    }
    auto ci = ll::service::getClientInstance();
    if (!ci) {
        gSession = Session{};
        return false;
    }
    gSession.state = State::Opening;
    openInventoryLikeKey();
    return true;
}

bool InventorySession::onBeforeRender(std::string const& screenName, ScreenView& view) {
    if (gSession.state == State::Idle) return false;
    if (std::chrono::steady_clock::now() - gSession.started > kWatchdog) {
        auto ci = ll::service::getClientInstance();
        if (ci && ci->getTopScreenName() == "inventory_screen" && !gSession.hiddenRoot.expired()) {
            if (auto* csc = ScreenTracker::containerScreen()) closeHidden(*csc);
        }
        finish(false, "watchdog");
        return false;
    }
    if (screenName == "hud_screen" || screenName == "inventory_screen") ++gSession.frames;
    if (screenName == "hud_screen") {
        if (gSession.state == State::Opening && gSession.frames > kOpenTimeoutFrames) finish(false, "open_timeout");
        else if (gSession.state == State::Closing) {
            auto ci = ll::service::getClientInstance();
            if (ci && ci->getTopScreenName() != "inventory_screen") finish(true, "ok");
            else if (gSession.frames > gSession.openFrames + kCloseTimeoutFrames) finish(false, "close_timeout");
        }
        return false;
    }
    if (screenName != "inventory_screen") return false;
    if (gSession.state == State::Holding && gSession.frames >= gSession.openFrames + gSession.holdFrames) {
        if (auto* csc = ScreenTracker::containerScreen()) closeHidden(*csc);
        else gSession.state = State::Closing;
    }
    if (gSession.state == State::Opening) {
        auto* csc = ScreenTracker::containerScreen();
        auto* mgr = ScreenTracker::containerManager();
        if (csc && mgr) {
            gSession.openFrames = gSession.frames;
            runWork(*csc, *mgr);
            if (gSession.holdFrames > 0) {
                gSession.state = State::Holding;
            } else {
                closeHidden(*csc);
            }
        } else if (gSession.hide == Hide::Cancel) {
            return true;
        }
    }
    if (gSession.hide == Hide::Cancel && gSession.state == State::Opening) return true;
    if (gSession.hide == Hide::Invisible && gSession.hiddenRoot.expired()) {
        if (auto* tree = view.mVisualTree.get()) {
            std::shared_ptr<UIControl> root = tree->mRootControl;
            if (root) {
                gSession.rootVisible = root->mVisible;
                root->mVisible       = ui::VisibilityFlag::None;
                gSession.hiddenRoot  = root;
            }
        }
    }
    return false;
}

}
