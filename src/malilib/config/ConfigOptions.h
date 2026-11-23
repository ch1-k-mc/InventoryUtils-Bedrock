// SPDX-License-Identifier: LGPL-3.0-only
// Based on sakura-ryoko/malilib (26.3 @b043d04, LGPL-3.0), ported to C++ and modified.
// Copyright (C) masa and contributors
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "malilib/hotkeys/KeybindMulti.h"

namespace inventory_utils::malilib::config {

enum class ConfigType { BOOLEAN, INTEGER, STRING, STRING_LIST, OPTION_LIST, HOTKEY };

class IConfigBase {
public:
    explicit IConfigBase(std::string name) : mName(std::move(name)) {}
    virtual ~IConfigBase() = default;

    [[nodiscard]] std::string const& getName() const { return mName; }
    [[nodiscard]] virtual ConfigType getType() const = 0;

    IConfigBase& apply(std::string prefix) {
        mTranslationPrefix = std::move(prefix);
        return *this;
    }
    [[nodiscard]] std::string getCommentKey() const { return mTranslationPrefix + ".comment." + mName; }
    [[nodiscard]] std::string getPrettyNameKey() const { return mTranslationPrefix + ".prettyName." + mName; }
    [[nodiscard]] std::string getNameKey() const { return mTranslationPrefix + ".name." + mName; }

    virtual void                         setValueFromJson(nlohmann::json const& element) = 0;
    [[nodiscard]] virtual nlohmann::json getAsJson() const                                 = 0;
    [[nodiscard]] virtual bool           isModified() const                                = 0;
    virtual void                         resetToDefault()                                  = 0;
    [[nodiscard]] virtual std::string getStringValue() const              = 0;
    virtual bool                      setValueFromString(std::string const& value) = 0;

    void setValueChangeCallback(std::function<void(IConfigBase&)> cb) { mOnChange = std::move(cb); }

protected:
    void onValueChanged() {
        if (mOnChange) mOnChange(*this);
    }

private:
    std::string                       mName;
    std::string                       mTranslationPrefix;
    std::function<void(IConfigBase&)> mOnChange;
};

class ConfigBoolean : public IConfigBase {
public:
    ConfigBoolean(std::string name, bool defaultValue) : IConfigBase(std::move(name)), mDefault(defaultValue), mValue(defaultValue) {}
    [[nodiscard]] ConfigType getType() const override { return ConfigType::BOOLEAN; }
    [[nodiscard]] bool       getBooleanValue() const { return mValue; }
    void                     setBooleanValue(bool v);
    void                     toggleBooleanValue() { setBooleanValue(!mValue); }
    void                     setValueFromJson(nlohmann::json const& e) override;
    [[nodiscard]] nlohmann::json getAsJson() const override { return mValue; }
    [[nodiscard]] bool           isModified() const override { return mValue != mDefault; }
    void                         resetToDefault() override { setBooleanValue(mDefault); }
    [[nodiscard]] std::string    getStringValue() const override { return mValue ? "true" : "false"; }
    bool                         setValueFromString(std::string const& v) override;

private:
    bool mDefault;
    bool mValue;
};

class ConfigInteger : public IConfigBase {
public:
    ConfigInteger(std::string name, int defaultValue, int minValue, int maxValue)
    : IConfigBase(std::move(name)),
      mDefault(defaultValue),
      mValue(defaultValue),
      mMin(minValue),
      mMax(maxValue) {}
    [[nodiscard]] ConfigType getType() const override { return ConfigType::INTEGER; }
    [[nodiscard]] int        getIntegerValue() const { return mValue; }
    [[nodiscard]] int        getMinIntegerValue() const { return mMin; }
    [[nodiscard]] int        getMaxIntegerValue() const { return mMax; }
    void                     setIntegerValue(int v);
    void                     setValueFromJson(nlohmann::json const& e) override;
    [[nodiscard]] nlohmann::json getAsJson() const override { return mValue; }
    [[nodiscard]] bool           isModified() const override { return mValue != mDefault; }
    void                         resetToDefault() override { setIntegerValue(mDefault); }
    [[nodiscard]] std::string    getStringValue() const override { return std::to_string(mValue); }
    bool                         setValueFromString(std::string const& v) override;

private:
    int mDefault, mValue, mMin, mMax;
};

class ConfigString : public IConfigBase {
public:
    ConfigString(std::string name, std::string defaultValue) : IConfigBase(std::move(name)), mDefault(defaultValue), mValue(std::move(defaultValue)) {}
    [[nodiscard]] ConfigType getType() const override { return ConfigType::STRING; }
    void                     setValueFromJson(nlohmann::json const& e) override;
    [[nodiscard]] nlohmann::json getAsJson() const override { return mValue; }
    [[nodiscard]] bool           isModified() const override { return mValue != mDefault; }
    void                         resetToDefault() override { setValueFromString(mDefault); }
    [[nodiscard]] std::string    getStringValue() const override { return mValue; }
    bool                         setValueFromString(std::string const& v) override;

private:
    std::string mDefault, mValue;
};

class ConfigStringList : public IConfigBase {
public:
    ConfigStringList(std::string name, std::vector<std::string> defaultValue) : IConfigBase(std::move(name)), mDefault(defaultValue), mValue(std::move(defaultValue)) {}
    [[nodiscard]] ConfigType                      getType() const override { return ConfigType::STRING_LIST; }
    [[nodiscard]] std::vector<std::string> const& getStrings() const { return mValue; }
    void                                          setStrings(std::vector<std::string> v);
    void                                          setValueFromJson(nlohmann::json const& e) override;
    [[nodiscard]] nlohmann::json                  getAsJson() const override { return mValue; }
    [[nodiscard]] bool                            isModified() const override { return mValue != mDefault; }
    void                                          resetToDefault() override { setStrings(mDefault); }
    [[nodiscard]] std::string                     getStringValue() const override;
    bool                                          setValueFromString(std::string const& v) override;

private:
    std::vector<std::string> mDefault, mValue;
};

class ConfigOptionList : public IConfigBase {
public:
    ConfigOptionList(std::string name, std::vector<std::string> values, std::string defaultValue)
    : IConfigBase(std::move(name)),
      mValues(std::move(values)),
      mDefault(defaultValue),
      mValue(std::move(defaultValue)) {}
    [[nodiscard]] ConfigType                      getType() const override { return ConfigType::OPTION_LIST; }
    [[nodiscard]] std::string const&              getOptionListValue() const { return mValue; }
    [[nodiscard]] std::vector<std::string> const& getValues() const { return mValues; }
    void                                          cycle(bool forward);
    void                                          setValueFromJson(nlohmann::json const& e) override;
    [[nodiscard]] nlohmann::json                  getAsJson() const override { return mValue; }
    [[nodiscard]] bool                            isModified() const override { return mValue != mDefault; }
    void                                          resetToDefault() override { setValueFromString(mDefault); }
    [[nodiscard]] std::string                     getStringValue() const override { return mValue; }
    bool                                          setValueFromString(std::string const& v) override;

private:
    std::vector<std::string> mValues;
    std::string              mDefault, mValue;
};

class ConfigHotkey : public IConfigBase {
public:
    ConfigHotkey(std::string name, std::string defaultStorageString, hotkeys::KeybindSettings const& settings = hotkeys::KeybindSettings::DEFAULT)
    : IConfigBase(std::move(name)),
      mKeybind(hotkeys::KeybindMulti::fromStorageString(defaultStorageString, settings)) {}
    [[nodiscard]] ConfigType             getType() const override { return ConfigType::HOTKEY; }
    [[nodiscard]] hotkeys::KeybindMulti& getKeybind() { return mKeybind; }
    [[nodiscard]] hotkeys::KeybindMulti const& getKeybind() const { return mKeybind; }
    void                                 setValueFromJson(nlohmann::json const& e) override;
    [[nodiscard]] nlohmann::json         getAsJson() const override { return mKeybind.getAsJson(); }
    [[nodiscard]] bool                   isModified() const override { return mKeybind.isModified() || mKeybind.areSettingsModified(); }
    void                                 resetToDefault() override;
    [[nodiscard]] std::string            getStringValue() const override { return mKeybind.getStringValue(); }
    bool                                 setValueFromString(std::string const& v) override;

private:
    hotkeys::KeybindMulti mKeybind;
};

void readConfigBase(nlohmann::json const& root, std::string const& category, std::vector<IConfigBase*> const& options);
void writeConfigBase(nlohmann::json& root, std::string const& category, std::vector<IConfigBase*> const& options);

}
