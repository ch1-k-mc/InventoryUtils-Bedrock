// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/config/ConfigRegistry.h"

namespace inventory_utils::platform::config {

ConfigRegistry& ConfigRegistry::get() {
    static ConfigRegistry instance;
    return instance;
}

void ConfigRegistry::add(ConfigSource source) { mSources.push_back(std::move(source)); }

void ConfigRegistry::loadAll() {
    for (auto& s : mSources) {
        if (s.load) s.load();
    }
}

void ConfigRegistry::saveAll() {
    for (auto& s : mSources) {
        if (s.save) s.save();
    }
}

malilib::config::IConfigBase* ConfigRegistry::find(std::string const& category, std::string const& name) const {
    for (auto* o : options(category)) {
        if (o->getName() == name) return o;
    }
    return nullptr;
}

std::vector<std::string> ConfigRegistry::categoryNames() const {
    std::vector<std::string> names;
    for (auto const& s : mSources) {
        for (auto const& [cat, _] : s.categories()) names.push_back(s.mod + "." + cat);
    }
    return names;
}

std::vector<malilib::config::IConfigBase*> ConfigRegistry::options(std::string const& category) const {
    for (auto const& s : mSources) {
        for (auto const& [cat, opts] : s.categories()) {
            if (s.mod + "." + cat == category) return opts;
        }
    }
    return {};
}

void ConfigRegistry::saveOwnerOf(std::string const& category) {
    for (auto& s : mSources) {
        if (category.rfind(s.mod + ".", 0) == 0 && s.save) s.save();
    }
}

}
