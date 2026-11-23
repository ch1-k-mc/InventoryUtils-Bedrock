// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ipnext/profiles/ProfilesConfig.h"

namespace inventory_utils::ipnext::event {

class ProfileSwitchHandler {
public:
    static ProfileSwitchHandler& get();

    void onClientTick();
    void onJoinWorld(std::string const& worldName);
    void load(bool fromUserInput = false);
    void save();

    bool onApplyProfile();
    bool onNextProfile();
    bool onPrevProfile();
    bool onQuickProfile(int index);
    bool onSaveAsProfile();

    bool switchToProfileName(std::string const& name);
    void applyCurrent();

    [[nodiscard]] std::vector<profiles::ProfileData> const& profiles() const { return mProfiles; }
    [[nodiscard]] std::vector<profiles::ProfileData> const& savedProfiles() const { return mSavedProfiles; }
    [[nodiscard]] std::optional<std::string>               activeProfileName() const;
    [[nodiscard]] std::filesystem::path                     file() const;
    [[nodiscard]] std::string const&                        lastResult() const { return mLastResult; }

    profiles::ProfileData createProfileFromCurrentState() const;

private:
    void init(profiles::ProfileData const& newProfile);
    void applyProfile();
    void reloadActiveProfile();
    int  byName(std::string const& name) const;
    int  nextOrFirst();
    int  prevOrLast();

    std::vector<profiles::ProfileData> mProfiles;
    std::vector<profiles::ProfileData> mSavedProfiles;
    [[nodiscard]] int                  profileCount() const;
    [[nodiscard]] profiles::ProfileData profileAt(int index) const;
    profiles::ProfileData              mTarget{"", profiles::ProfileSlotId::NONE, {}, false};
    int                                mActiveProfileId = 0;
    bool                               mDoApply         = false;
    std::string                        mWorldName;
    std::string                        mLastResult;
};

}
