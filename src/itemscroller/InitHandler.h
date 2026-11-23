// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/itemscroller (26.3 @9d67667, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

namespace inventory_utils::itemscroller {

class InitHandler {
public:
    static void registerModHandlers();
    static void unregisterModHandlers();
};

}
