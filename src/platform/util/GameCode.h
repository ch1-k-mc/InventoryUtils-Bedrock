// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cstdint>
#include <optional>

#include "ll/api/memory/Memory.h"

namespace inventory_utils::platform::gamecode {

bool inGameImage(void const* p);

void const* follow(void const* p);

template <class Fn>
void const* implOf(Fn fn) {
    return follow(ll::memory::toFuncPtr(fn));
}

std::optional<unsigned> thunkIndex(void const* thunk);

template <class Fn>
std::optional<unsigned> vtableIndex(Fn virt) {
    if (auto i = ll::memory::getVtableIndex(virt)) return *i;
    return thunkIndex(ll::memory::toFuncPtr(virt));
}

template <class Fn>
std::uintptr_t slotOf(void const* subobject, Fn virt) {
    if (!subobject) return 0;
    auto idx = vtableIndex(virt);
    if (!idx) return 0;
    auto const* vt = *static_cast<void const* const* const*>(subobject);
    return reinterpret_cast<std::uintptr_t>(vt[*idx]);
}

std::uintptr_t hookable(std::uintptr_t target);

template <class Fn>
std::uintptr_t hookTarget(void const* subobject, Fn virt) {
    return hookable(slotOf(subobject, virt));
}

template <class Fn, class Impl>
bool slotIs(void const* vptr, Fn virt, Impl impl) {
    auto idx = vtableIndex(virt);
    if (!vptr || !idx) return false;
    return follow(static_cast<void const* const*>(vptr)[*idx]) == implOf(impl);
}

}
