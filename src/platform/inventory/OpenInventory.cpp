// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/inventory/OpenInventory.h"

#include <chrono>

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/MinecraftPackets.h"
#include "mc/network/packet/InteractPacket.h"
#include "mc/network/packet/InteractPacketPayload.h"

namespace inventory_utils::platform::inventory {

namespace {
std::chrono::steady_clock::time_point gDropCloseUntil{};
}

bool openInventoryLikeKey() {
    auto ci = ll::service::getClientInstance();
    auto* player = ci ? ci->getLocalPlayer() : nullptr;
    if (!player) return false;
    auto packet = MinecraftPackets::createPacket(MinecraftPacketIds::Interact);
    if (!packet) return false;
    auto& interact     = static_cast<InteractPacket&>(*packet);
    interact.mAction   = InteractPacketPayload::Action::OpenInventory;
    interact.mTargetId = player->getRuntimeID();
    player->sendNetworkPacket(interact);
    gDropCloseUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    player->openInventory();
    return true;
}

bool takeSpuriousClose() {
    if (std::chrono::steady_clock::now() > gDropCloseUntil) return false;
    gDropCloseUntil = {};
    return true;
}

}
