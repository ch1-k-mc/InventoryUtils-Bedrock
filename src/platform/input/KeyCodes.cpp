// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/input/KeyCodes.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Windows.h>

namespace inventory_utils::platform::input::KeyCodes {

namespace {

struct Entry {
    int         code;
    char const* name;
};

std::vector<Entry> const& entries() {
    static std::vector<Entry> const table = [] {
        std::vector<Entry> t;
        static char letters[26][2];
        for (int i = 0; i < 26; ++i) {
            letters[i][0] = static_cast<char>('A' + i);
            letters[i][1] = 0;
            t.push_back({'A' + i, letters[i]});
        }
        static char digits[10][2];
        for (int i = 0; i < 10; ++i) {
            digits[i][0] = static_cast<char>('0' + i);
            digits[i][1] = 0;
            t.push_back({'0' + i, digits[i]});
        }
        static char fkeys[24][4];
        for (int i = 0; i < 24; ++i) {
            std::snprintf(fkeys[i], sizeof(fkeys[i]), "F%d", i + 1);
            t.push_back({VK_F1 + i, fkeys[i]});
        }
        static char kp[10][5];
        for (int i = 0; i < 10; ++i) {
            std::snprintf(kp[i], sizeof(kp[i]), "KP_%d", i);
            t.push_back({VK_NUMPAD0 + i, kp[i]});
        }
        std::pair<int, char const*> const rest[] = {
            {VK_LCONTROL,   "LEFT_CONTROL" },
            {VK_RCONTROL,   "RIGHT_CONTROL"},
            {VK_LSHIFT,     "LEFT_SHIFT"   },
            {VK_RSHIFT,     "RIGHT_SHIFT"  },
            {VK_LMENU,      "LEFT_ALT"     },
            {VK_RMENU,      "RIGHT_ALT"    },
            {VK_LWIN,       "LEFT_SUPER"   },
            {VK_RWIN,       "RIGHT_SUPER"  },
            {VK_SPACE,      "SPACE"        },
            {VK_TAB,        "TAB"          },
            {VK_RETURN,     "ENTER"        },
            {VK_ESCAPE,     "ESCAPE"       },
            {VK_BACK,       "BACKSPACE"    },
            {VK_CAPITAL,    "CAPS_LOCK"    },
            {VK_OEM_3,      "GRAVE_ACCENT" },
            {VK_OEM_MINUS,  "MINUS"        },
            {VK_OEM_PLUS,   "EQUAL"        },
            {VK_OEM_4,      "LEFT_BRACKET" },
            {VK_OEM_6,      "RIGHT_BRACKET"},
            {VK_OEM_5,      "BACKSLASH"    },
            {VK_OEM_1,      "SEMICOLON"    },
            {VK_OEM_7,      "APOSTROPHE"   },
            {VK_OEM_COMMA,  "COMMA"        },
            {VK_OEM_PERIOD, "PERIOD"       },
            {VK_OEM_2,      "SLASH"        },
            {VK_INSERT,     "INSERT"       },
            {VK_DELETE,     "DELETE"       },
            {VK_HOME,       "HOME"         },
            {VK_END,        "END"          },
            {VK_PRIOR,      "PAGE_UP"      },
            {VK_NEXT,       "PAGE_DOWN"    },
            {VK_UP,         "UP"           },
            {VK_DOWN,       "DOWN"         },
            {VK_LEFT,       "LEFT"         },
            {VK_RIGHT,      "RIGHT"        },
            {VK_ADD,        "KP_ADD"       },
            {VK_SUBTRACT,   "KP_SUBTRACT"  },
            {VK_MULTIPLY,   "KP_MULTIPLY"  },
            {VK_DIVIDE,     "KP_DIVIDE"    },
            {VK_DECIMAL,    "KP_DECIMAL"   },
            {VK_NUMLOCK,    "NUM_LOCK"     },
            {VK_SCROLL,     "SCROLL_LOCK"  },
            {VK_PAUSE,      "PAUSE"        },
            {VK_SNAPSHOT,   "PRINT_SCREEN" },
            {VK_APPS,       "MENU"         },
            {MOUSE_LEFT,        "MOUSE_LEFT"       },
            {MOUSE_RIGHT,       "MOUSE_RIGHT"      },
            {MOUSE_MIDDLE,      "MOUSE_MIDDLE"     },
            {MOUSE_BUTTON_4,    "MOUSE_BUTTON_4"   },
            {MOUSE_BUTTON_5,    "MOUSE_BUTTON_5"   },
            {MOUSE_SCROLL_UP,   "MOUSE_SCROLL_UP"  },
            {MOUSE_SCROLL_DOWN, "MOUSE_SCROLL_DOWN"},
        };
        for (auto const& [code, name] : rest) t.push_back({code, name});
        return t;
    }();
    return table;
}

std::unordered_map<std::string, int> const& aliases() {
    static std::unordered_map<std::string, int> const m{
        {"BUTTON_1",       MOUSE_LEFT     },
        {"BUTTON_2",       MOUSE_RIGHT    },
        {"BUTTON_3",       MOUSE_MIDDLE   },
        {"BUTTON_4",       MOUSE_BUTTON_4 },
        {"BUTTON_5",       MOUSE_BUTTON_5 },
        {"RETURN",         VK_RETURN      },
        {"LCONTROL",       VK_LCONTROL    },
        {"RCONTROL",       VK_RCONTROL    },
        {"LSHIFT",         VK_LSHIFT      },
        {"RSHIFT",         VK_RSHIFT      },
        {"LALT",           VK_LMENU       },
        {"RALT",           VK_RMENU       },
        {"LCTRL",          VK_LCONTROL    },
        {"RCTRL",          VK_RCONTROL    },
        {"GRAVE",          VK_OEM_3       },
        {"EQUALS",         VK_OEM_PLUS    },
        {"PAGEUP",         VK_PRIOR       },
        {"PAGEDOWN",       VK_NEXT        },
        {"ESC",            VK_ESCAPE      },
    };
    return m;
}

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

}

int fromName(std::string const& name) {
    std::string n = upper(name);
    for (auto const& e : entries()) {
        if (n == e.name) return e.code;
    }
    auto it = aliases().find(n);
    return it != aliases().end() ? it->second : UNKNOWN;
}

std::string toName(int code) {
    for (auto const& e : entries()) {
        if (e.code == code) return e.name;
    }
    return {};
}

std::string displayName(int code) {
    switch (code) {
    case VK_LCONTROL:
    case VK_RCONTROL: return "Ctrl";
    case VK_LSHIFT:
    case VK_RSHIFT: return "Shift";
    case VK_LMENU:
    case VK_RMENU: return "Alt";
    case MOUSE_LEFT: return "左クリック";
    case MOUSE_RIGHT: return "右クリック";
    case MOUSE_MIDDLE: return "中クリック";
    case MOUSE_SCROLL_UP: return "ホイール上";
    case MOUSE_SCROLL_DOWN: return "ホイール下";
    default: break;
    }
    return toName(code);
}

int normalize(int vk) {
    switch (vk) {
    case VK_SHIFT: return VK_LSHIFT;
    case VK_CONTROL: return VK_LCONTROL;
    case VK_MENU: return VK_LMENU;
    default: return vk;
    }
}

}
