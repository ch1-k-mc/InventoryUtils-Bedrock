// SPDX-License-Identifier: AGPL-3.0-only
#include "gui/GuiHost.h"

#include <chrono>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/VisibilityFlag.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenView.h"

#include "gui/ConfigGuiBase.h"
#include "gui/GuiConfigs.h"
#include "gui/GuiManager.h"
#include "gui/Ui2d.h"
#include "ipnext/event/AutoRefillHandler.h"
#include "platform/config/ConfigRegistry.h"
#include "platform/i18n/I18n.h"

namespace inventory_utils::gui::host {

namespace ev = ll::event;

namespace {

std::vector<ev::ListenerPtr> gListeners;
long long                    gDraws   = 0;
bool                         gWasOpen = false;
int                          gSaves   = 0;
std::string                  gLastClosed;
std::string                  gPendingOpen;
std::chrono::steady_clock::time_point gPendingUntil;

struct HiddenPart {
    char const*                       screen;
    char const*                       name;
    std::weak_ptr<UIControl>          control;
    std::optional<ui::VisibilityFlag> saved;
};
std::vector<HiddenPart> gParts{
    {"hud_screen", "centered_gui_elements_at_bottom_middle", {}, {}},
    {"hud_screen", "chat_stack", {}, {}},
    {"hud_crosshair_screen", "cursor", {}, {}},
};

std::shared_ptr<UIControl> findControl(std::shared_ptr<UIControl> const& c, std::string_view name, int depth = 0) {
    if (!c || depth > 32) return nullptr;
    if (c->mName.get() == name) return c;
    for (auto const& child : c->mChildren.get()) {
        if (auto r = findControl(child, name, depth + 1)) return r;
    }
    return nullptr;
}

void applyHidden(ScreenView& view, std::string const& screen) {
    bool const  open = GuiManager::get().isOpen();
    VisualTree* tree = view.mVisualTree.get();
    if (!tree) return;
    for (auto& p : gParts) {
        if (screen.find(p.screen) == std::string::npos) continue;
        auto c = p.control.lock();
        if (!c) {
            if (!open && !p.saved) continue;
            c         = findControl(tree->mRootControl, p.name);
            p.control = c;
            if (!c) continue;
        }
        if (open) {
            if (!p.saved) p.saved = static_cast<ui::VisibilityFlag>(c->mVisible);
            c->mVisible = ui::VisibilityFlag::None;
        } else if (p.saved) {
            c->mVisible = *p.saved;
            p.saved.reset();
        }
    }
}

void onClosed() {
    if (ConfigGuiBase::consumeDirty()) {
        platform::config::ConfigRegistry::get().saveAll();
        ++gSaves;
    }
    ipnext::event::AutoRefillHandler::get().init();
}

bool canOpen() {
    auto ci = ll::service::getClientInstance();
    return ci && ci->getLocalPlayer() && ci->getTopScreenName() == "hud_screen";
}

void onBefore(ev::BeforeUIRenderEvent& e) {
    std::string const name = e.screenView().getScreenName();
    if (name.find("hud_screen") == std::string::npos && name.find("hud_crosshair_screen") == std::string::npos) return;
    applyHidden(e.screenView(), name);
}

void onAfter(ev::AfterUIRenderEvent& e) {
    if (e.screenView().getScreenName() != "hud_screen") return;
    if (!gPendingOpen.empty()) {
        if (std::chrono::steady_clock::now() > gPendingUntil) gPendingOpen.clear();
        else if (canOpen()) {
            std::string const w = std::move(gPendingOpen);
            gPendingOpen.clear();
            ::inventory_utils::gui::host::openByName(w.c_str());
        }
    }
    bool const wasOpen = GuiManager::get().isOpen();
    if (!wasOpen && !gWasOpen) return;
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    std::string const before = wasOpen && GuiManager::get().current() ? GuiManager::get().current()->name() : "";
    Ui2d ui(e.uiRenderContext(), *ci);
    GuiManager::get().render(ui);
    ui.flush();
    ++gDraws;
    bool const nowOpen = GuiManager::get().isOpen();
    if (gWasOpen && !nowOpen) {
        gLastClosed = before;
        onClosed();
    }
    gWasOpen = nowOpen;
}

void openScreen(std::unique_ptr<GuiBase> screen) {
    if (!canOpen()) return;
    GuiManager::get().open(std::move(screen));
    gWasOpen = true;
}

}

void openConfig() { openScreen(std::make_unique<GuiConfigs>()); }

void openItemScrollerConfig() {
    GuiConfigs::setMod(0);
    openScreen(std::make_unique<GuiConfigs>());
}

void openIpnConfig() {
    GuiConfigs::setMod(1);
    openScreen(std::make_unique<GuiConfigs>());
}

void openOtherConfig() {
    GuiConfigs::setMod(2);
    openScreen(std::make_unique<GuiConfigs>());
}

bool openByName(char const* which) {
    std::string const w = which ? which : "";
    if (w == "itemscroller") openItemScrollerConfig();
    else if (w == "ipnext" || w == "ipn") openIpnConfig();
    else if (w == "other" || w == "craftingtools") openOtherConfig();
    else if (w == "favorites") {
        GuiConfigs::setMod(3);
        openScreen(std::make_unique<GuiConfigs>());
    }
    else return false;
    return true;
}

void requestOpen(std::string const& which) {
    gPendingOpen  = which;
    gPendingUntil = std::chrono::steady_clock::now() + std::chrono::seconds(5);
}

bool isOpen() { return GuiManager::get().isOpen(); }
void close() { GuiManager::get().close(); }

void install() {
    if (!gListeners.empty()) return;
    platform::i18n::addNamespace("itemscroller");
    platform::i18n::addNamespace("malilib");
    platform::i18n::addNamespace("libipn");
    GuiManager::get().start();
    auto& bus = ev::EventBus::getInstance();
    gListeners.push_back(bus.emplaceListener<ev::BeforeUIRenderEvent>(onBefore));
    gListeners.push_back(bus.emplaceListener<ev::AfterUIRenderEvent>(onAfter, ev::EventPriority::Low));
    gListeners.push_back(bus.emplaceListener<ev::ClientJoinLevelEvent>([](ev::ClientJoinLevelEvent&) {
        if (GuiManager::get().isOpen()) onClosed();
        GuiManager::get().forceClose();
        gWasOpen = false;
    }));
}

void uninstall() {
    auto& bus = ev::EventBus::getInstance();
    for (auto& l : gListeners) {
        if (l) bus.removeListener(l);
    }
    gListeners.clear();
    GuiManager::get().stop();
}

json state() {
    json j          = GuiManager::get().state();
    j["draws"]      = gDraws;
    j["saves"]      = gSaves;
    j["lastClosed"] = gLastClosed;
    if (auto* s = GuiManager::get().current()) {
        if (auto* c = dynamic_cast<GuiConfigs*>(s)) {
            j["mod"]     = GuiConfigs::modId();
            j["section"] = c->selectedName();
            j["search"]  = GuiConfigs::searchText();
            if (auto* f = c->focusedField()) j["composition"] = f->composition;
        }
    }
    json parts = json::array();
    for (auto const& p : gParts) parts.push_back(json{{"name", p.name}, {"found", !p.control.expired()}, {"hidden", p.saved.has_value()}});
    j["hiddenParts"] = parts;
    return j;
}

json click(float x, float y, int button) { return json{{"handled", GuiManager::get().click(x, y, button)}}; }
json drag(std::string const& id, float t) {
    auto* s = GuiManager::get().current();
    if (!s) return json{{"handled", false}};
    json const tree = s->tree();
    for (auto const& w : tree["widgets"]) {
        if (w.value("id", "") != id) continue;
        auto const& r  = w["rect"];
        float const x  = r[0].get<float>(), y = r[1].get<float>() + r[3].get<float>() / 2, width = r[2].get<float>();
        bool const  ok = GuiManager::get().click(x + width / 2, y, 0);
        s->mouseDragged(x + width * t, y);
        s->mouseReleased();
        return json{{"handled", ok}};
    }
    return json{{"handled", false}};
}

json scroll(float x, float y, int delta) { return json{{"handled", GuiManager::get().scroll(x, y, delta)}}; }
json key(int vk, bool down) { return json{{"handled", down ? GuiManager::get().key(vk) : GuiManager::get().keyUp(vk)}}; }
json text(char const* utf8) { return json{{"handled", GuiManager::get().text(utf8 ? utf8 : "")}}; }

}
