// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#include "malilib/config/ConfigOptions.h"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace inventory_utils::malilib::config {

void ConfigBoolean::setBooleanValue(bool v) {
    if (v == mValue) return;
    mValue = v;
    onValueChanged();
}

void ConfigBoolean::setValueFromJson(nlohmann::json const& e) {
    if (e.is_boolean()) setBooleanValue(e.get<bool>());
}

bool ConfigBoolean::setValueFromString(std::string const& v) {
    if (v == "true" || v == "1" || v == "on") setBooleanValue(true);
    else if (v == "false" || v == "0" || v == "off") setBooleanValue(false);
    else return false;
    return true;
}

void ConfigInteger::setIntegerValue(int v) {
    v = std::clamp(v, mMin, mMax);
    if (v == mValue) return;
    mValue = v;
    onValueChanged();
}

void ConfigInteger::setValueFromJson(nlohmann::json const& e) {
    if (e.is_number()) setIntegerValue(e.get<int>());
}

bool ConfigInteger::setValueFromString(std::string const& v) {
    int  x  = 0;
    auto rs = std::from_chars(v.data(), v.data() + v.size(), x);
    if (rs.ec != std::errc() || rs.ptr != v.data() + v.size()) return false;
    setIntegerValue(x);
    return true;
}

void ConfigString::setValueFromJson(nlohmann::json const& e) {
    if (e.is_string()) setValueFromString(e.get<std::string>());
}

bool ConfigString::setValueFromString(std::string const& v) {
    if (v != mValue) {
        mValue = v;
        onValueChanged();
    }
    return true;
}

void ConfigStringList::setStrings(std::vector<std::string> v) {
    if (v == mValue) return;
    mValue = std::move(v);
    onValueChanged();
}

void ConfigStringList::setValueFromJson(nlohmann::json const& e) {
    if (!e.is_array()) return;
    std::vector<std::string> v;
    for (auto const& x : e) {
        if (x.is_string()) v.push_back(x.get<std::string>());
    }
    setStrings(std::move(v));
}

std::string ConfigStringList::getStringValue() const {
    std::string s;
    for (size_t i = 0; i < mValue.size(); ++i) {
        if (i > 0) s += ",";
        s += mValue[i];
    }
    return s;
}

bool ConfigStringList::setValueFromString(std::string const& v) {
    std::vector<std::string> out;
    std::stringstream        ss(v);
    std::string              part;
    while (std::getline(ss, part, ',')) {
        if (!part.empty()) out.push_back(part);
    }
    setStrings(std::move(out));
    return true;
}

void ConfigOptionList::cycle(bool forward) {
    auto it = std::find(mValues.begin(), mValues.end(), mValue);
    if (it == mValues.end() || mValues.empty()) return;
    long i = static_cast<long>(it - mValues.begin());
    long n = static_cast<long>(mValues.size());
    i      = forward ? (i + 1) % n : (i - 1 + n) % n;
    setValueFromString(mValues[static_cast<size_t>(i)]);
}

void ConfigOptionList::setValueFromJson(nlohmann::json const& e) {
    if (e.is_string()) setValueFromString(e.get<std::string>());
}

bool ConfigOptionList::setValueFromString(std::string const& v) {
    auto it = std::find_if(mValues.begin(), mValues.end(), [&](std::string const& x) {
        return x.size() == v.size() && std::equal(x.begin(), x.end(), v.begin(), [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    });
    if (it == mValues.end()) return false;
    if (*it != mValue) {
        mValue = *it;
        onValueChanged();
    }
    return true;
}

void ConfigHotkey::setValueFromJson(nlohmann::json const& e) {
    auto old = mKeybind.getStringValue();
    mKeybind.setValueFromJson(e);
    if (old != mKeybind.getStringValue()) onValueChanged();
}

void ConfigHotkey::resetToDefault() {
    mKeybind.resetToDefault();
    mKeybind.resetSettingsToDefaults();
    onValueChanged();
}

bool ConfigHotkey::setValueFromString(std::string const& v) {
    auto old = mKeybind.getStringValue();
    mKeybind.setValueFromString(v);
    if (old != mKeybind.getStringValue()) onValueChanged();
    return true;
}

void readConfigBase(nlohmann::json const& root, std::string const& category, std::vector<IConfigBase*> const& options) {
    if (!root.is_object() || !root.contains(category) || !root[category].is_object()) return;
    auto const& obj = root[category];
    for (auto* option : options) {
        if (obj.contains(option->getName())) option->setValueFromJson(obj[option->getName()]);
    }
}

void writeConfigBase(nlohmann::json& root, std::string const& category, std::vector<IConfigBase*> const& options) {
    nlohmann::json obj = nlohmann::json::object();
    for (auto* option : options) obj[option->getName()] = option->getAsJson();
    root[category] = obj;
}

}
