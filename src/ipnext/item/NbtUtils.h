// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "mc/deps/nbt/Tag.h"

class CompoundTag;

namespace inventory_utils::ipnext::item {

class ItemType;

namespace NbtUtils {

using TagPtr = std::shared_ptr<Tag const>;

int compareNbt(CompoundTag const* a, CompoundTag const* b);
int compareTag(Tag const& a, Tag const& b);

TagPtr parseNbtOrEmpty(std::string const& nbt);

bool matchNbt(Tag const* a, Tag const* b);
bool matchNbtNoExtra(Tag const* a, Tag const* b);

Tag const* componentNbt(ItemType const& itemType, std::string const& componentId);

int compareComponents(ItemType const& a, ItemType const& b);

bool        isNumber(Tag const& t);
bool        isString(Tag const& t);
bool        isCompound(Tag const& t);
bool        isList(Tag const& t);
double      asDouble(Tag const& t);
std::string asString(Tag const& t);
std::vector<Tag const*> asList(Tag const& t);

class NbtPath {
public:
    static std::optional<NbtPath> of(std::string const& path);
    [[nodiscard]] std::vector<Tag const*> getTags(ItemType const& itemType, std::string const& componentId) const;
    [[nodiscard]] std::vector<Tag const*> getTagsFor(Tag const& root) const;
    [[nodiscard]] std::string const&      toString() const { return mText; }

private:
    struct Node {
        enum class Kind { Key, Index, All } kind = Kind::Key;
        std::string key;
        int         index = 0;
    };
    std::string       mText;
    std::vector<Node> mNodes;
};

}
}
