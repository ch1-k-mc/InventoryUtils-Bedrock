// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "malilib/config/ConfigOptions.h"

namespace inventory_utils::platform::config {

struct ConfigSource {
    std::string                                                                                  mod;
    std::function<std::vector<std::pair<std::string, std::vector<malilib::config::IConfigBase*>>>()> categories;
    std::function<void()>                                                                        load;
    std::function<void()>                                                                        save;
};

class ConfigRegistry {
public:
    static ConfigRegistry& get();

    void add(ConfigSource source);
    void loadAll();
    void saveAll();

    [[nodiscard]] malilib::config::IConfigBase* find(std::string const& category, std::string const& name) const;
    [[nodiscard]] std::vector<std::string> categoryNames() const;
    [[nodiscard]] std::vector<malilib::config::IConfigBase*> options(std::string const& category) const;
    void saveOwnerOf(std::string const& category);

private:
    std::vector<ConfigSource> mSources;
};

}
