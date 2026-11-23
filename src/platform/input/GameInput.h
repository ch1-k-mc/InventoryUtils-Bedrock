// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

namespace inventory_utils::platform::input::GameInput {

void move(double uiX, double uiY);
void button(bool down, double uiX, double uiY);
void wheel(int delta, double uiX, double uiY);
bool shouldDropMove(int pxX, int pxY);

}
