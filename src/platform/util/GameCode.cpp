// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/util/GameCode.h"

#include <utility>

#include <Windows.h>

namespace inventory_utils::platform::gamecode {

bool inGameImage(void const* p) {
    static auto const range = [] {
        auto base = reinterpret_cast<std::uintptr_t>(::GetModuleHandleW(nullptr));
        auto const* dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(base);
        auto const* nt  = reinterpret_cast<IMAGE_NT_HEADERS64 const*>(base + dos->e_lfanew);
        return std::pair{base, base + nt->OptionalHeader.SizeOfImage};
    }();
    auto a = reinterpret_cast<std::uintptr_t>(p);
    return a >= range.first && a < range.second;
}

void const* follow(void const* p) {
    for (int i = 0; i < 4 && p && !inGameImage(p); ++i) {
        auto const* b = static_cast<std::uint8_t const*>(p);
        if (b[0] == 0xFF && b[1] == 0x25) {
            p = *reinterpret_cast<void const* const*>(b + 6 + *reinterpret_cast<std::int32_t const*>(b + 2));
        } else if (b[0] == 0x48 && b[1] == 0xFF && b[2] == 0x25) {
            p = *reinterpret_cast<void const* const*>(b + 7 + *reinterpret_cast<std::int32_t const*>(b + 3));
        } else if (b[0] == 0xE9) {
            p = b + 5 + *reinterpret_cast<std::int32_t const*>(b + 1);
        } else {
            break;
        }
    }
    return p;
}

std::uintptr_t hookable(std::uintptr_t target) {
    if (!target) return 0;
    auto const* p = static_cast<std::uint8_t const*>(follow(reinterpret_cast<void const*>(target)));
    if (!inGameImage(p) || p[0] == 0xC3 || p[0] == 0xCC) return 0;
    return reinterpret_cast<std::uintptr_t>(p);
}

std::optional<unsigned> thunkIndex(void const* thunk) {
    auto const* b = static_cast<std::uint8_t const*>(thunk);
    if (!b) return std::nullopt;
    for (int k = 0; k < 4 && b[0] == 0xE9; ++k) b = b + 5 + *reinterpret_cast<std::int32_t const*>(b + 1);
    for (int i = 0; i + 3 < 40; ++i) {
        if (!(b[i] == 0x48 && b[i + 1] == 0x8B && b[i + 2] == 0x01)) continue;
        auto const* c = b + i + 3;
        if (c[0] == 0x48 && c[1] == 0x8B && c[2] == 0x40) return static_cast<unsigned>(c[3]) / 8;
        if (c[0] == 0x48 && c[1] == 0x8B && c[2] == 0x80) return *reinterpret_cast<std::uint32_t const*>(c + 3) / 8;
        if (c[0] == 0xFF && c[1] == 0x60) return static_cast<unsigned>(c[2]) / 8;
        if (c[0] == 0xFF && c[1] == 0xA0) return *reinterpret_cast<std::uint32_t const*>(c + 2) / 8;
        if (c[0] == 0xFF && c[1] == 0x20) return 0u;
        if (c[0] == 0x48 && c[1] == 0x8B && c[2] == 0x00) return 0u;
        return std::nullopt;
    }
    return std::nullopt;
}

}
