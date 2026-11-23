// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/libIPN (master @89a2efa, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "libipn/util/StringComparators.h"

#include <algorithm>
#include <cwctype>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace inventory_utils::libipn::util {

namespace {

int sign(int v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

char16_t toUpper(char16_t c) { return static_cast<char16_t>(std::towupper(static_cast<wint_t>(c))); }
char16_t toLower(char16_t c) { return static_cast<char16_t>(std::towlower(static_cast<wint_t>(c))); }

int compareIntLists(std::vector<int> const& a, std::vector<int> const& b) {
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

int compareDigits(std::u16string const& s1, size_t b1, size_t e1, char16_t base1, std::u16string const& s2, size_t b2, size_t e2, char16_t base2) {
    while (b1 + 1 < e1 && s1[b1] == base1) ++b1;
    while (b2 + 1 < e2 && s2[b2] == base2) ++b2;
    size_t l1 = e1 - b1, l2 = e2 - b2;
    if (l1 != l2) return l1 < l2 ? -1 : 1;
    for (size_t i = 0; i < l1; ++i) {
        int d1 = s1[b1 + i] - base1, d2 = s2[b2 + i] - base2;
        if (d1 != d2) return d1 < d2 ? -1 : 1;
    }
    return 0;
}

char16_t digitBase(char16_t c) {
    if (c >= u'0' && c <= u'9') return u'0';
    if (c >= u'０' && c <= u'９') return u'０';
    return 0;
}

}

std::u16string toU16(std::string const& utf8) {
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::u16string out(static_cast<size_t>(n), u'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), reinterpret_cast<wchar_t*>(out.data()), n);
    return out;
}

int naturalOrder(std::u16string const& a, std::u16string const& b) { return sign(a.compare(b)); }

int caseInsensitiveOrder(std::u16string const& a, std::u16string const& b) {
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        char16_t c1 = a[i], c2 = b[i];
        if (c1 == c2) continue;
        c1 = toUpper(c1);
        c2 = toUpper(c2);
        if (c1 == c2) continue;
        c1 = toLower(c1);
        c2 = toLower(c2);
        if (c1 != c2) return c1 < c2 ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

U16Comparator makeCollator(std::string const& localeTag, CollatorStrength strength) {
    std::wstring locale;
    if (localeTag != "sys" && !localeTag.empty()) {
        auto u = toU16(localeTag);
        locale.assign(u.begin(), u.end());
    }
    DWORD flags = 0;
    switch (strength) {
    case CollatorStrength::PRIMARY:
        flags = NORM_IGNORECASE | NORM_IGNORENONSPACE | NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH;
        break;
    case CollatorStrength::SECONDARY:
        flags = NORM_IGNORECASE | NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH;
        break;
    default:
        flags = 0;
        break;
    }
    bool identical = strength == CollatorStrength::IDENTICAL;
    return [locale, flags, identical](std::u16string const& a, std::u16string const& b) -> int {
        LPCWSTR name = locale.empty() ? LOCALE_NAME_USER_DEFAULT : locale.c_str();
        int     r    = CompareStringEx(name, flags, reinterpret_cast<LPCWCH>(a.data()), static_cast<int>(a.size()), reinterpret_cast<LPCWCH>(b.data()), static_cast<int>(b.size()), nullptr, nullptr, 0);
        if (r == 0) {
            r = CompareStringEx(LOCALE_NAME_USER_DEFAULT, flags, reinterpret_cast<LPCWCH>(a.data()), static_cast<int>(a.size()), reinterpret_cast<LPCWCH>(b.data()), static_cast<int>(b.size()), nullptr, nullptr, 0);
        }
        int res = r == CSTR_LESS_THAN ? -1 : (r == CSTR_GREATER_THAN ? 1 : 0);
        if (res == 0 && identical) res = naturalOrder(a, b);
        return res;
    };
}

U16Comparator makeLogicalStringComparator(U16Comparator textComparator) {
    return [text = std::move(textComparator)](std::u16string const& str1, std::u16string const& str2) -> int {
        size_t           index1 = 0, index2 = 0;
        std::vector<int> minus1, minus2;
        while (true) {
            {
                size_t s1 = index1, s2 = index2;
                while (index1 < str1.size() && str1[index1] == u'-') ++index1;
                while (index2 < str2.size() && str2[index2] == u'-') ++index2;
                if (s1 != index1 || s2 != index2) {
                    minus1.push_back(static_cast<int>(index1 - s1));
                    minus2.push_back(static_cast<int>(index2 - s2));
                }
            }
            if (index1 >= str1.size() || index2 >= str2.size()) {
                if (index1 < str1.size()) return 1;
                if (index2 < str2.size()) return -1;
                if (int r = compareIntLists(minus1, minus2); r != 0) return r;
                return naturalOrder(str1, str2);
            }
            char16_t d1 = digitBase(str1[index1]);
            char16_t d2 = digitBase(str2[index2]);
            if (d1 != 0 && d2 != 0) {
                size_t s1 = index1, s2 = index2;
                while (index1 < str1.size() && str1[index1] >= d1 && str1[index1] <= d1 + 9) ++index1;
                while (index2 < str2.size() && str2[index2] >= d2 && str2[index2] <= d2 + 9) ++index2;
                if (int r = compareDigits(str1, s1, index1, d1, str2, s2, index2, d2); r != 0) return r;
            } else {
                if (int r = text(std::u16string(1, str1[index1]), std::u16string(1, str2[index2])); r != 0) return r;
                ++index1;
                ++index2;
            }
        }
    };
}

}
