// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "nlohmann/json.hpp"

class MinecraftUIRenderContext;
namespace mce {
class ClientTexture;
class TexturePtr;
}

namespace inventory_utils::platform::render {

class TextureManager {
public:
    static TextureManager& get();

    mce::ClientTexture const* getClientTexture(MinecraftUIRenderContext& ctx, std::string const& relativePath);

    static std::string const& flushMaterialName();

    [[nodiscard]] nlohmann::json status() const;

private:
    struct Entry {
        std::shared_ptr<mce::TexturePtr>      texture;
        int                                   attempt = 0;
        std::chrono::steady_clock::time_point started{};
        bool                                  loaded = false;
        bool                                  failed = false;
        std::string                           how;
    };

    std::map<std::string, Entry> mEntries;
    mutable std::mutex           mMutex;
};

}
