// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "gui/Widgets.h"

#include <algorithm>
#include <cmath>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "platform/input/KeyCodes.h"
#include "gui/Ui2d.h"

namespace inventory_utils::gui {

json Widget::toJson() const { return json{{"id", id}, {"rect", {x, y, w, h}}, {"visible", visible}}; }

void Label::render(Ui2d& ui, float, float) {
    if (!visible) return;
    float       ly    = y;
    std::size_t start = 0;
    while (start <= text.size()) {
        auto nl = text.find('\n', start);
        if (nl == std::string::npos) nl = text.size();
        ui.text(text.substr(start, nl - start), x, ly, color, shadow);
        ly += Ui2d::kLineHeight;
        start = nl + 1;
    }
    ui.flush();
}

json Label::toJson() const {
    json j    = Widget::toJson();
    j["type"] = "label";
    j["text"] = text;
    return j;
}

void Button::render(Ui2d& ui, float mx, float my) {
    if (!visible) return;
    bool const hover = enabled && contains(mx, my);
    ui.fill(x, y, x + w, y + h, colors::kButtonBorder);
    ui.fill(x + 1, y + 1, x + w - 1, y + h - 1, !enabled ? colors::kButtonDisabled : (hover ? colors::kButtonHover : colors::kButton));
    if (enabled) {
        ui.fill(x + 1, y + 1, x + w - 1, y + 2, 0x40FFFFFF);
        ui.fill(x + 1, y + 1, x + 2, y + h - 1, 0x40FFFFFF);
    }
    float const tw = static_cast<float>(ui.textWidth(text));
    ui.text(text, x + (w - tw) / 2.f, y + (h - 8) / 2.f, !enabled ? colors::kTextDisabled : (hover ? colors::kTextHover : colors::kText), true);
    ui.flush();
}

bool Button::mouseClicked(float mx, float my, int button) {
    if (!enabled || !contains(mx, my)) return false;
    if (onPress) onPress(button);
    return true;
}

json Button::toJson() const {
    json j       = Widget::toJson();
    j["type"]    = "button";
    j["text"]    = text;
    j["enabled"] = enabled;
    if (!hoverText.empty()) j["hover"] = hoverText;
    return j;
}

namespace {
std::size_t nextCp(std::string const& s, std::size_t i) {
    if (i >= s.size()) return s.size();
    ++i;
    while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
    return i;
}
std::size_t prevCp(std::string const& s, std::size_t i) {
    if (i == 0) return 0;
    --i;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
    return i;
}
constexpr int kVkBack = 0x08, kVkReturn = 0x0D, kVkEscape = 0x1B, kVkEnd = 0x23, kVkHome = 0x24, kVkLeft = 0x25, kVkRight = 0x27,
              kVkDelete = 0x2E;
}

namespace {
bool ctrlDown() { return (::GetKeyState(VK_CONTROL) & 0x8000) != 0; }
bool shiftDown() { return (::GetKeyState(VK_SHIFT) & 0x8000) != 0; }

std::string clipboardText() {
    std::string out;
    if (!::OpenClipboard(nullptr)) return out;
    if (HANDLE h = ::GetClipboardData(CF_UNICODETEXT)) {
        if (auto* w = static_cast<wchar_t const*>(::GlobalLock(h))) {
            int len = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
            if (len > 1) {
                out.resize(static_cast<std::size_t>(len - 1));
                ::WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
            }
            ::GlobalUnlock(h);
        }
    }
    ::CloseClipboard();
    return out;
}

void setClipboardText(std::string const& utf8) {
    int wlen = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return;
    HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, static_cast<std::size_t>(wlen) * sizeof(wchar_t));
    if (!mem) return;
    if (auto* w = static_cast<wchar_t*>(::GlobalLock(mem))) {
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w, wlen);
        ::GlobalUnlock(mem);
    }
    if (!::OpenClipboard(nullptr)) {
        ::GlobalFree(mem);
        return;
    }
    ::EmptyClipboard();
    if (!::SetClipboardData(CF_UNICODETEXT, mem)) ::GlobalFree(mem);
    ::CloseClipboard();
}

std::size_t wordPos(std::string const& s, std::size_t pos, bool forward) {
    if (forward) {
        while (pos < s.size() && s[pos] != ' ') pos = nextCp(s, pos);
        while (pos < s.size() && s[pos] == ' ') ++pos;
    } else {
        while (pos > 0 && s[pos - 1] == ' ') --pos;
        while (pos > 0 && s[prevCp(s, pos)] != ' ') pos = prevCp(s, pos);
    }
    return pos;
}
}

void TextField::render(Ui2d& ui, float mx, float my) {
    (void)mx;
    (void)my;
    if (!visible) return;
    std::uint32_t const border = invalid ? 0xFFFF4040 : (focused ? 0xFFFFFFFF : 0xFFA0A0A0);
    ui.fill(x, y, x + w, y + h, border);
    ui.fill(x + 1, y + 1, x + w - 1, y + h - 1, 0xFF000000);
    float const ty     = y + (h - 8) / 2.f;
    float const left   = x + 4;
    float const avail  = std::max(1.f, w - 8);
    float const compW  = composition.empty() ? 0.f : static_cast<float>(ui.textWidth(composition));
    cursor             = std::min(cursor, text.size());
    highlight          = std::min(highlight, text.size());
    if (scroll > cursor) scroll = cursor;
    while (scroll < cursor && static_cast<float>(ui.textWidth(text.substr(scroll, cursor - scroll))) + compW > avail) scroll = nextCp(text, scroll);
    caretX.clear();
    std::size_t end = scroll;
    caretX.emplace_back(end, left);
    while (end < text.size()) {
        std::size_t const n  = nextCp(text, end);
        float const       tw = static_cast<float>(ui.textWidth(text.substr(scroll, n - scroll)));
        if (tw > avail) break;
        end = n;
        caretX.emplace_back(end, left + tw);
    }
    auto xOf = [&](std::size_t pos) {
        for (auto const& [b, px] : caretX) {
            if (b >= pos) return px;
        }
        return caretX.back().second;
    };
    if (focused && cursor != highlight) {
        float const a = xOf(std::min(cursor, highlight)), b = xOf(std::max(cursor, highlight));
        ui.fill(a, ty - 1, b, ty + 9, 0x800050FF);
    }
    if (composition.empty()) {
        ui.text(text.substr(scroll, end - scroll), left, ty, 0xFFE0E0E0, true);
    } else {
        float const cx = xOf(cursor);
        ui.text(text.substr(scroll, cursor - scroll), left, ty, 0xFFE0E0E0, true);
        ui.text(composition, cx, ty, 0xFFE0E0E0, true);
        ui.flush();
        ui.fill(cx, ty + 9, cx + compW, ty + 10, 0xFFE0E0E0);
        if (cursor < end) ui.text(text.substr(cursor, end - cursor), cx + compW, ty, 0xFFE0E0E0, true);
    }
    ui.flush();
    if (focused && composition.empty() && (::GetTickCount64() % 1000) < 600) {
        float const cx = xOf(cursor);
        ui.fill(cx, ty - 1, cx + 1, ty + 9, 0xFFD0D0D0);
    }
}

bool TextField::mouseClicked(float mx, float my, int) {
    if (!contains(mx, my)) return false;
    focused = true;
    std::size_t pos = text.size();
    if (!caretX.empty()) {
        float best = 1e9f;
        for (auto const& [b, px] : caretX) {
            float const d = std::abs(px - mx);
            if (d < best) {
                best = d;
                pos  = b;
            }
        }
    }
    moveCursor(pos, shiftDown());
    return true;
}

void TextField::moveCursor(std::size_t pos, bool extend) {
    cursor = std::min(pos, text.size());
    if (!extend) highlight = cursor;
}

void TextField::insertText(std::string const& s) {
    std::string clean;
    for (char c : s) {
        auto const u = static_cast<unsigned char>(c);
        if (u >= 0x20 && u != 0x7F) clean.push_back(c);
    }
    std::size_t const a = std::min(cursor, highlight), b = std::max(cursor, highlight);
    if (clean.empty() && a == b) return;
    std::size_t const room = maxLength + (b - a) > text.size() ? maxLength + (b - a) - text.size() : 0;
    if (clean.size() > room) {
        std::size_t cut = room;
        while (cut > 0 && (static_cast<unsigned char>(clean[cut]) & 0xC0) == 0x80) --cut;
        clean.resize(cut);
    }
    text.replace(a, b - a, clean);
    cursor = highlight = a + clean.size();
    invalid            = false;
    if (onChange) onChange(text);
}

void TextField::deleteChars(bool forward, bool word) {
    if (cursor != highlight) {
        insertText("");
        return;
    }
    std::size_t const to = word ? wordPos(text, cursor, forward) : (forward ? nextCp(text, cursor) : prevCp(text, cursor));
    if (to == cursor) return;
    highlight = to;
    insertText("");
}

bool TextField::keyPressed(int vk) {
    if (!focused) return false;
    if (!composition.empty()) return true;
    bool const ctrl = ctrlDown(), shift = shiftDown();
    if (ctrl && !shift) {
        std::size_t const a = std::min(cursor, highlight), b = std::max(cursor, highlight);
        switch (vk) {
        case 'A':
            cursor    = text.size();
            highlight = 0;
            return true;
        case 'C': setClipboardText(text.substr(a, b - a)); return true;
        case 'V': insertText(clipboardText()); return true;
        case 'X':
            setClipboardText(text.substr(a, b - a));
            insertText("");
            return true;
        default: break;
        }
    }
    switch (vk) {
    case kVkBack: deleteChars(false, ctrl); return true;
    case kVkDelete: deleteChars(true, ctrl); return true;
    case kVkLeft: moveCursor(ctrl ? wordPos(text, cursor, false) : prevCp(text, cursor), shift); return true;
    case kVkRight: moveCursor(ctrl ? wordPos(text, cursor, true) : nextCp(text, cursor), shift); return true;
    case kVkHome: moveCursor(0, shift); return true;
    case kVkEnd: moveCursor(text.size(), shift); return true;
    case kVkReturn: commit(); return true;
    case kVkEscape: focused = false; return true;
    default: return true;
    }
}

bool TextField::charTyped(std::string const& utf8) {
    if (!focused) return false;
    composition.clear();
    std::string run;
    auto        flush = [&] {
        if (!run.empty()) insertText(run);
        run.clear();
    };
    for (char c : utf8) {
        auto const u = static_cast<unsigned char>(c);
        if (u == 0x08) {
            flush();
            deleteChars(false, false);
        } else if (u == 0x7F) {
            flush();
            deleteChars(true, false);
        } else {
            run.push_back(c);
        }
    }
    flush();
    return true;
}

void TextField::commit() {
    focused = false;
    if (onCommit) invalid = !onCommit(text);
}

json TextField::toJson() const {
    json j       = Widget::toJson();
    j["type"]    = "textfield";
    j["text"]    = text;
    j["focused"] = focused;
    j["invalid"] = invalid;
    return j;
}

void KeybindButton::render(Ui2d& ui, float mx, float my) {
    if (capturing) {
        std::string const shown = text;
        text                    = std::string("> \xC2\xA7" "e") + shown + "\xC2\xA7r <";
        Button::render(ui, mx, my);
        text = shown;
        return;
    }
    Button::render(ui, mx, my);
}

bool KeybindButton::mouseClicked(float mx, float my, int button) {
    if (capturing) {
        int const k = platform::input::KeyCodes::mouseButtonToCode(button);
        keyDown(k);
        keyUp(k);
        return true;
    }
    if (!enabled || !contains(mx, my)) return false;
    capturing = true;
    keys.clear();
    held.clear();
    return true;
}

bool KeybindButton::keyDown(int key) {
    if (!capturing) return false;
    if (key == kVkEscape) {
        capturing = false;
        if (onSet) onSet("");
        return true;
    }
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
    if (std::find(held.begin(), held.end(), key) == held.end()) held.push_back(key);
    std::string s;
    for (int k : keys) s += (s.empty() ? "" : ",") + platform::input::KeyCodes::toName(k);
    text = s;
    return true;
}

bool KeybindButton::keyUp(int key) {
    if (!capturing) return false;
    held.erase(std::remove(held.begin(), held.end(), key), held.end());
    if (held.empty() && !keys.empty()) {
        capturing = false;
        std::string s;
        for (int k : keys) s += (s.empty() ? "" : ",") + platform::input::KeyCodes::toName(k);
        if (onSet) onSet(s);
    }
    return true;
}

json KeybindButton::toJson() const {
    json j         = Button::toJson();
    j["type"]      = "keybind";
    j["capturing"] = capturing;
    return j;
}

void CheckBox::render(Ui2d& ui, float mx, float my) {
    if (!visible) return;
    bool const hover = contains(mx, my);
    ui.fill(x, y, x + w, y + h, hover ? 0xFFFFFFFF : 0xFFA0A0A0);
    ui.fill(x + 1, y + 1, x + w - 1, y + h - 1, 0xFF000000);
    if (checked) ui.fill(x + 3, y + 3, x + w - 3, y + h - 3, 0xFFE0E0E0);
}

bool CheckBox::mouseClicked(float mx, float my, int) {
    if (!contains(mx, my)) return false;
    checked = !checked;
    if (onToggle) onToggle(checked);
    return true;
}

json CheckBox::toJson() const {
    json j       = Widget::toJson();
    j["type"]    = "checkbox";
    j["checked"] = checked;
    return j;
}

std::string onOff(bool on) { return on ? "\xC2\xA7" "aON\xC2\xA7r" : "\xC2\xA7" "cOFF\xC2\xA7r"; }

void ColorSwatch::render(Ui2d& ui, float, float) {
    if (!visible) return;
    ui.fill(x, y, x + w, y + h, 0xFFFFFFFF);
    ui.fill(x + 1, y + 1, x + w - 1, y + h - 1, 0xFF000000);
    ui.fill(x + 2, y + 2, x + w - 2, y + h - 2, argb);
}

}
