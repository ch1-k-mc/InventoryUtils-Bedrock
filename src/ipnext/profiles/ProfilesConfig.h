// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "ipnext/item/ItemType.h"
#include "ipnext/item/NbtUtils.h"

namespace inventory_utils::ipnext::profiles {

enum class ProfileSlotId { NONE, HOT1, HOT2, HOT3, HOT4, HOT5, HOT6, HOT7, HOT8, HOT9, FEET, LEGS, CHEST, HEAD, OFFHAND };

std::optional<ProfileSlotId> slotIdFromName(std::string const& name);
std::string                  slotIdName(ProfileSlotId id);
int                          invSlotOf(ProfileSlotId id);
ProfileSlotId                slotIdOfInvSlot(int invSlot);

struct ProfileItemId {
    enum class Kind { Id, Tag, Wildcard } kind = Kind::Id;
    std::string value;
    [[nodiscard]] bool        test(item::ItemType const& type) const;
    [[nodiscard]] std::string toString() const;
};

struct ProfileComponentData {
    std::string               id;
    std::optional<std::string> componentNbt;
    [[nodiscard]] std::string toString() const;
};

struct ProfileItemData {
    ProfileItemId                                    itemId;
    std::string                                      customName;
    std::optional<std::vector<ProfileComponentData>> components;
    [[nodiscard]] bool        match(item::ItemType const& type) const;
    [[nodiscard]] std::string toString() const;
};

struct ProfileSlot {
    ProfileSlotId                id = ProfileSlotId::NONE;
    std::optional<std::string>   order;
    std::vector<ProfileItemData> items;
    [[nodiscard]] std::string    toString() const;
};

struct ProfileData {
    std::string              name;
    ProfileSlotId            active = ProfileSlotId::NONE;
    std::vector<ProfileSlot> slots;
    bool                     valid = true;
    [[nodiscard]] std::string toString() const;
    bool operator==(ProfileData const& o) const { return name == o.name && valid == o.valid && toString() == o.toString(); }
};

class ProfileSyntaxError : public std::runtime_error {
public:
    ProfileSyntaxError(int line_, std::string msg) : std::runtime_error(std::move(msg)), line(line_) {}
    int line;
};

struct ProfilesConfig {
    static std::vector<ProfileData> getProfiles(std::string const& content);
    static std::string              asString(std::vector<ProfileData> const& profiles);
};

std::string escape(std::string const& s);
std::string unescape(std::string const& quoted);

}
