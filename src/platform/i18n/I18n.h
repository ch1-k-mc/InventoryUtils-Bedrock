// SPDX-License-Identifier: AGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0) and Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (C) masa and contributors; Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <string>
#include <vector>

namespace inventory_utils::platform::i18n {

void addNamespace(std::string const& ns);

std::string translate(std::string const& key, std::vector<std::string> const& args = {});

void reload();

}
