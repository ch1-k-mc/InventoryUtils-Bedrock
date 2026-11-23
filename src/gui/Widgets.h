// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "gui/Json.h"

namespace inventory_utils::gui {

class Ui2d;

struct Widget {
    float       x = 0, y = 0, w = 0, h = 0;
    std::string id;
    bool        visible = true;
    std::string hoverText;

    virtual ~Widget() = default;
    [[nodiscard]] bool contains(float mx, float my) const { return visible && mx >= x && mx < x + w && my >= y && my < y + h; }
    virtual void       render(Ui2d& ui, float mx, float my) = 0;
    virtual bool       mouseClicked(float , float , int ) { return false; }
    [[nodiscard]] virtual bool draggable() const { return false; }
    virtual void               mouseDragged(float , float ) {}
    [[nodiscard]] virtual json toJson() const;
};

struct Label : Widget {
    std::string   text;
    std::uint32_t color  = 0xFFFFFFFF;
    bool          shadow = true;
    void          render(Ui2d& ui, float mx, float my) override;
    [[nodiscard]] json toJson() const override;
};

struct Button : Widget {
    std::string                text;
    bool                       enabled = true;
    std::function<void(int)>   onPress;
    void                       render(Ui2d& ui, float mx, float my) override;
    bool                       mouseClicked(float mx, float my, int button) override;
    [[nodiscard]] json         toJson() const override;
};

struct TextField : Widget {
    std::string                             text;
    std::size_t                             cursor    = 0;
    std::size_t                             highlight = 0;
    std::size_t                             scroll    = 0;
    std::string                             composition;
    std::vector<std::pair<std::size_t, float>> caretX;
    bool                                    focused   = false;
    bool                                    invalid   = false;
    std::size_t                             maxLength = 256;
    std::function<bool(std::string const&)> onCommit;
    std::function<void(std::string const&)> onChange;

    void               render(Ui2d& ui, float mx, float my) override;
    bool               mouseClicked(float mx, float my, int button) override;
    bool               keyPressed(int vk);
    bool               charTyped(std::string const& utf8);
    void               commit();
    void               insertText(std::string const& s);
    void               deleteChars(bool forward, bool word);
    void               moveCursor(std::size_t pos, bool extend);
    [[nodiscard]] json toJson() const override;
};

struct KeybindButton : Button {
    bool                                    capturing = false;
    std::vector<int>                        keys, held;
    std::function<void(std::string const&)> onSet;

    void               render(Ui2d& ui, float mx, float my) override;
    bool               mouseClicked(float mx, float my, int button) override;
    bool               keyDown(int key);
    bool               keyUp(int key);
    [[nodiscard]] json toJson() const override;
};

struct CheckBox : Widget {
    bool                      checked = false;
    std::function<void(bool)> onToggle;
    void                      render(Ui2d& ui, float mx, float my) override;
    bool                      mouseClicked(float mx, float my, int button) override;
    [[nodiscard]] json        toJson() const override;
};

std::string onOff(bool on);

struct ColorSwatch : Widget {
    std::uint32_t argb = 0xFFFFFFFF;
    void          render(Ui2d& ui, float mx, float my) override;
};

namespace colors {
inline constexpr std::uint32_t kText          = 0xFFE0E0E0;
inline constexpr std::uint32_t kTextHover     = 0xFFFFFFA0;
inline constexpr std::uint32_t kTextDisabled  = 0xFFA0A0A0;
inline constexpr std::uint32_t kButton        = 0xFF6F6F6F;
inline constexpr std::uint32_t kButtonHover   = 0xFF7E88BE;
inline constexpr std::uint32_t kButtonDisabled = 0xFF2E2E2E;
inline constexpr std::uint32_t kButtonBorder  = 0xFF000000;
inline constexpr std::uint32_t kBackground    = 0xC0101010;
inline constexpr std::uint32_t kWhite         = 0xFFFFFFFF;
}

}
