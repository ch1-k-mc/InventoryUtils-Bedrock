// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <string>

namespace inventory_utils::command {

void install();
void uninstall();

bool handle(std::string const& commandLine);

}
