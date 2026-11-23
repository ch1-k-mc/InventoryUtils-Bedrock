// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/util/Messages.h"

#include <optional>

#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/player/LocalPlayer.h"

namespace inventory_utils::platform::util {

namespace {
std::string gLast;
}

void chat(std::string const& text) {
    gLast = text;
    ll::mod::NativeMod::current()->getLogger().info("[chat] {}", text);
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    if (auto* player = ci->getLocalPlayer()) player->displayClientMessage(text, std::nullopt);
}

void actionBar(std::string const& text) {
    gLast = text;
    ll::mod::NativeMod::current()->getLogger().info("[actionbar] {}", text);
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    auto gui = ci->getGuiData();
    gui->setActionBarMessage(text, std::nullopt);
}

void popup(std::string const& text) {
    gLast = text;
    ll::mod::NativeMod::current()->getLogger().info("[popup] {}", text);
    auto ci = ll::service::getClientInstance();
    if (!ci) return;
    auto gui = ci->getGuiData();
    gui->setActionBarMessage(text, std::nullopt);
}

std::string const& lastChat() { return gLast; }

}
