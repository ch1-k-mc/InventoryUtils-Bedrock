// SPDX-License-Identifier: AGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0) and Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (C) masa and contributors; Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <string>

namespace inventory_utils::platform::input {

namespace KeyCodes {

inline constexpr int UNKNOWN = 0;

inline constexpr int MOUSE_LEFT   = -1;
inline constexpr int MOUSE_RIGHT  = -2;
inline constexpr int MOUSE_MIDDLE = -3;
inline constexpr int MOUSE_BUTTON_4 = -4;
inline constexpr int MOUSE_BUTTON_5 = -5;
inline constexpr int MOUSE_SCROLL_UP   = -50;
inline constexpr int MOUSE_SCROLL_DOWN = -51;

inline bool isMouse(int code) { return code < 0; }
inline bool isMouseButton(int code) { return code <= -1 && code >= -8; }
inline int  mouseButtonToCode(int button) { return -1 - button; }

int fromName(std::string const& name);
std::string toName(int code);
std::string displayName(int code);

int normalize(int vk);

}

}
