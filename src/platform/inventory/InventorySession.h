// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <functional>
#include <string>

#include "nlohmann/json.hpp"

class ContainerScreenController;
class MinecraftUIRenderContext;
class ScreenView;
class ContainerManagerController;

namespace inventory_utils::platform::inventory {

class InventorySession {
public:
    using Work = std::function<void(ContainerScreenController&, ContainerManagerController&)>;
    using Done = std::function<void(bool ok, std::string const& reason)>;

    enum class Hide { None = 0, Invisible = 1, Cancel = 2 };

    static void install();
    static void uninstall();

    static bool request(Work work, Hide hide = Hide::Invisible, Done done = {}, int holdFrames = 0);
    static bool busy();
    static bool onInventoryKey();

    static nlohmann::json lastResult();

    static bool onBeforeRender(std::string const& screenName, ScreenView& view);
};

}
