// SPDX-License-Identifier: AGPL-3.0-only
#include "command/InvUtilsCommand.h"

#include "ll/api/mod/NativeMod.h"
#include "platform/inventory/OpenInventory.h"

#include <sstream>
#include <vector>

#include "ll/api/memory/Hook.h"

#include "mc/client/gui/screens/controllers/ChatScreenController.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/Packet.h"
#include "mc/network/packet/CommandRequestPacket.h"
#include "mc/network/packet/CommandRequestPacketPayload.h"

#include "gui/GuiHost.h"
#include "platform/config/ConfigRegistry.h"
#include "platform/input/InputHandler.h"
#include "platform/util/Messages.h"

namespace inventory_utils::command {

namespace {

using platform::config::ConfigRegistry;
using platform::util::chat;

std::vector<std::string> split(std::string const& s) {
    std::vector<std::string> out;
    std::istringstream       in(s);
    std::string              w;
    while (in >> w) out.push_back(w);
    return out;
}

void usage() {
    chat("/invutils config list [section] | get <section> <name> | set <section> <name> <value> | reset <section> <name>, /invutils reload | save, /invutils gui [itemscroller | ipnext | craftingtools | favorites]");
}

void configCommand(std::vector<std::string> const& a) {
    auto& reg = ConfigRegistry::get();
    if (a.size() < 3) return usage();
    std::string const& sub = a[2];
    if (sub == "list") {
        if (a.size() < 4) {
            std::string names;
            for (auto const& n : reg.categoryNames()) names += (names.empty() ? "" : ", ") + n;
            return chat("Sections: " + names);
        }
        auto opts = reg.options(a[3]);
        if (opts.empty()) return chat("No such section: " + a[3]);
        for (auto* o : opts) chat(o->getName() + " = " + o->getStringValue());
        return;
    }
    if (a.size() < 5) return usage();
    auto* option = reg.find(a[3], a[4]);
    if (!option) return chat("No such option: " + a[3] + " " + a[4]);
    if (sub == "get") return chat(a[3] + " " + option->getName() + " = " + option->getStringValue());
    if (sub == "reset") {
        option->resetToDefault();
    } else if (sub == "set") {
        if (a.size() < 6) return usage();
        std::string value = a[5];
        for (size_t i = 6; i < a.size(); ++i) value += " " + a[i];
        if (!option->setValueFromString(value)) return chat("Value rejected: " + value);
    } else {
        return usage();
    }
    reg.saveOwnerOf(a[3]);
    platform::input::InputHandler::get().updateUsedKeys();
    chat(a[3] + " " + option->getName() + " = " + option->getStringValue());
}

LL_TYPE_INSTANCE_HOOK(
    InvUtilsLoopbackSendToServerHook,
    ll::memory::HookPriority::Normal,
    LoopbackPacketSender,
    &LoopbackPacketSender::$sendToServer,
    void,
    ::Packet& packet
) {
    if (packet.getId() == MinecraftPacketIds::ContainerClose && platform::inventory::takeSpuriousClose()) {
        return;
    }
    if (packet.getId() == MinecraftPacketIds::CommandRequest) {
        auto const& req = static_cast<::CommandRequestPacket const&>(packet);
        if (handle(req.mCommand.get())) return;
    }
    origin(packet);
}

}

bool handle(std::string const& commandLine) {
    std::string line = commandLine;
    if (!line.empty() && line[0] == '/') line.erase(0, 1);
    auto a = split(line);
    if (a.empty() || a[0] != "invutils") return false;
    if (a.size() < 2) {
        usage();
        return true;
    }
    auto& reg = ConfigRegistry::get();
    if (a[1] == "config") configCommand(a);
    else if (a[1] == "reload") {
        reg.loadAll();
        platform::input::InputHandler::get().updateUsedKeys();
        chat("Reloaded the config files");
    } else if (a[1] == "save") {
        reg.saveAll();
        chat("Saved the config files");
    } else if (a[1] == "gui") {
        std::string const which = a.size() >= 3 ? a[2] : "itemscroller";
        gui::host::requestOpen(which);
    } else usage();
    return true;
}

void install() {
    InvUtilsLoopbackSendToServerHook::hook();
}

void uninstall() {
    InvUtilsLoopbackSendToServerHook::unhook();
}

}
