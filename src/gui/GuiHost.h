// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <string>

#include "gui/Json.h"

namespace inventory_utils::gui::host {

void install();
void uninstall();

void openConfig();
void openItemScrollerConfig();
void openIpnConfig();
void openOtherConfig();
bool openByName(char const* which);
void requestOpen(std::string const& which);

[[nodiscard]] bool isOpen();
void               close();

json state();
json click(float x, float y, int button);
json scroll(float x, float y, int delta);
json drag(std::string const& id, float t);
json key(int vk, bool down);
json text(char const* utf8);

}
