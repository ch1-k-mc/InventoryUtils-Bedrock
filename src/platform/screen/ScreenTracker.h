// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <functional>
#include <memory>
#include <set>
#include <string>

class ScreenController;
class ScreenView;
class MinecraftUIRenderContext;
class ContainerScreenController;
class ContainerManagerController;

namespace inventory_utils::platform::screen {

class ScreenTracker {
public:
    static void install();
    static void uninstall();

    static void addContainerFrameListener(std::function<void()> listener);

    static void addContainerOverlayRenderer(std::function<void(ScreenView&, MinecraftUIRenderContext&)> renderer);

    static void addHudOverlayRenderer(std::function<void(ScreenView&, MinecraftUIRenderContext&)> renderer);

    static ContainerScreenController*  containerScreen();
    static ContainerManagerController* containerManager();
    static std::string const&          containerScreenName();

    static ContainerManagerController* hudManager();

    static std::set<std::string> const& seenScreens();

    static bool isContainerScreenName(std::string const& name);

    static bool isGuiOpen();
};

}
