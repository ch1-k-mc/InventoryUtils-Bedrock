// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/render/TextureManager.h"

#include "ll/api/mod/NativeMod.h"

#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/resource/ResourceFileSystem.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"

namespace inventory_utils::platform::render {

namespace {

struct Candidate {
    char const*        name;
    ResourceFileSystem fs;
};

constexpr Candidate kCandidates[]{
    {"Raw+abs",           ResourceFileSystem::Raw          },
    {"RawPersistent+abs", ResourceFileSystem::RawPersistent},
    {"ExternalDir+abs",   ResourceFileSystem::ExternalDir  },
    {"UserPackage+abs",   ResourceFileSystem::UserPackage  },
};
constexpr auto kWaitPerCandidate = std::chrono::seconds(5);

bool hasFlag(TextureLoadState s, TextureLoadState f) { return (static_cast<uint8_t>(s) & static_cast<uint8_t>(f)) != 0; }

}

TextureManager& TextureManager::get() {
    static TextureManager instance;
    return instance;
}

std::string const& TextureManager::flushMaterialName() {
    static std::string const name = "ui_flush";
    return name;
}

mce::ClientTexture const* TextureManager::getClientTexture(MinecraftUIRenderContext& ctx, std::string const& relativePath) {
    std::lock_guard lock(mMutex);
    Entry&          e = mEntries[relativePath];
    if (e.failed) return nullptr;
    constexpr int kCount = static_cast<int>(std::size(kCandidates));
    while (e.attempt < kCount) {
        auto const& c = kCandidates[e.attempt];
        if (!e.texture) {
            auto             dir  = ll::mod::NativeMod::current()->getModDir() / "assets";
            std::u8string    u8   = (dir / std::filesystem::path(relativePath)).generic_u8string();
            std::string      path(reinterpret_cast<char const*>(u8.c_str()), u8.size());
            ResourceLocation loc(Core::PathView(path), c.fs);
            e.texture = std::make_shared<mce::TexturePtr>(ctx.getTexture(loc, false));
            e.started = std::chrono::steady_clock::now();
            e.how     = c.name;
        }
        auto const& data = e.texture->mClientTexture.get();
        if (data) {
            bool missing = data->mIsMissingTexture == IsMissingTexture::Yes;
            bool loaded  = hasFlag(data->mTextureLoadState, TextureLoadState::LoadedBit);
            if (loaded && !missing) {
                if (!e.loaded) {
                    e.loaded = true;
                    ll::mod::NativeMod::current()->getLogger().info("Loaded texture ({}): {}", e.how, relativePath);
                }
                return &data->mClientTexture.get();
            }
            if (loaded && missing) {
                e.texture.reset();
                ++e.attempt;
                continue;
            }
        }
        if (std::chrono::steady_clock::now() - e.started < kWaitPerCandidate) return nullptr;
        e.texture.reset();
        ++e.attempt;
    }
    e.failed = true;
    ll::mod::NativeMod::current()->getLogger().warn("Could not load texture (tried all candidates): {}", relativePath);
    return nullptr;
}

nlohmann::json TextureManager::status() const {
    std::lock_guard lock(mMutex);
    nlohmann::json  j = nlohmann::json::object();
    for (auto const& [path, e] : mEntries) j[path] = {{"loaded", e.loaded}, {"failed", e.failed}, {"how", e.how}, {"attempt", e.attempt}};
    return j;
}

}
