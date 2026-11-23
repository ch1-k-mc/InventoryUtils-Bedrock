// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/i18n/I18n.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>

#include "ll/api/mod/NativeMod.h"

#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"

#include "nlohmann/json.hpp"

namespace inventory_utils::platform::i18n {

namespace {

std::mutex                         gMutex;
std::vector<std::string>           gNamespaces;
std::string                        gLoadedLanguage;
std::map<std::string, std::string> gCurrent;
std::map<std::string, std::string> gFallback;
bool                               gLoaded = false;

std::string gameLanguage() {
    try {
        std::string code = *getI18n().getCurrentLanguage()->mCode;
        std::transform(code.begin(), code.end(), code.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return code.empty() ? std::string("en_us") : code;
    } catch (...) {
        return "en_us";
    }
}

void loadInto(std::map<std::string, std::string>& out, std::string const& lang) {
    auto assets = ll::mod::NativeMod::current()->getModDir() / "assets";
    for (auto const& ns : gNamespaces) {
        std::ifstream in(assets / ns / "lang" / (lang + ".json"), std::ios::binary);
        if (!in) continue;
        auto j = nlohmann::json::parse(in, nullptr, false);
        if (!j.is_object()) continue;
        for (auto const& [k, v] : j.items()) {
            if (v.is_string()) out[k] = v.get<std::string>();
        }
    }
}

void ensureLoaded() {
    std::string lang = gameLanguage();
    if (gLoaded && lang == gLoadedLanguage) return;
    gCurrent.clear();
    gFallback.clear();
    loadInto(gFallback, "en_us");
    if (lang != "en_us") loadInto(gCurrent, lang);
    gLoadedLanguage = lang;
    gLoaded         = true;
}

std::string format(std::string const& text, std::vector<std::string> const& args) {
    std::string out;
    size_t      next = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 1 < text.size()) {
            if (text[i + 1] == 's') {
                out += next < args.size() ? args[next++] : std::string();
                ++i;
                continue;
            }
            if (text[i + 1] == '%') {
                out += '%';
                ++i;
                continue;
            }
        }
        out += text[i];
    }
    return out;
}

}

void addNamespace(std::string const& ns) {
    std::lock_guard lock(gMutex);
    if (std::find(gNamespaces.begin(), gNamespaces.end(), ns) == gNamespaces.end()) gNamespaces.push_back(ns);
    gLoaded = false;
}

std::string translate(std::string const& key, std::vector<std::string> const& args) {
    std::lock_guard lock(gMutex);
    ensureLoaded();
    if (auto it = gCurrent.find(key); it != gCurrent.end()) return format(it->second, args);
    if (auto it = gFallback.find(key); it != gFallback.end()) return format(it->second, args);
    return key;
}

void reload() {
    std::lock_guard lock(gMutex);
    gLoaded = false;
}

}
