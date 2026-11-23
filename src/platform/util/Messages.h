// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <string>

namespace inventory_utils::platform::util {

void chat(std::string const& text);

void actionBar(std::string const& text);
void popup(std::string const& text);

std::string const& lastChat();

}
