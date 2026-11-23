// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ipnext/item/ItemType.h"

namespace inventory_utils::ipnext::item::rule {

class Rule;
using RulePtr = std::shared_ptr<Rule>;

class ArgumentType {
public:
    virtual ~ArgumentType()                                                         = default;
    [[nodiscard]] virtual std::string             toString(std::any const& v) const = 0;
    [[nodiscard]] virtual std::optional<std::any> parse(std::string const& argument) const = 0;
};

struct Parameter {
    std::string         name;
    ArgumentType const* argumentType = nullptr;
};

class ArgumentMap {
public:
    template <class T>
    void defineParameter(Parameter const& p, T defaultValue) {
        mDefaults[p.name] = std::any(std::move(defaultValue));
        if (std::find(mOrder.begin(), mOrder.end(), p.name) == mOrder.end()) mOrder.push_back(p.name);
    }
    void defineParameter(Parameter const& p) {
        mDefaults[p.name] = std::nullopt;
        if (std::find(mOrder.begin(), mOrder.end(), p.name) == mOrder.end()) mOrder.push_back(p.name);
    }
    void defineParametersFrom(ArgumentMap const& other);

    template <class T>
    [[nodiscard]] T const& get(Parameter const& p) const {
        if (auto it = mValues.find(p.name); it != mValues.end()) return *std::any_cast<T>(&it->second);
        return *std::any_cast<T>(&*mDefaults.at(p.name));
    }
    [[nodiscard]] bool isDefaultValue(Parameter const& p) const { return !mValues.contains(p.name); }

    bool trySetArgument(Parameter const& p, std::string const& argument);
    void setArgumentsFrom(ArgumentMap const& other);

    [[nodiscard]] bool                     contains(std::string const& name) const { return mDefaults.contains(name); }
    [[nodiscard]] std::vector<std::string> missingParameters() const;
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> dumpAsPairList() const;

private:
    [[nodiscard]] std::any const* rawGet(std::string const& name) const;

    std::map<std::string, std::optional<std::any>> mDefaults;
    std::map<std::string, std::any>                mValues;
    std::vector<std::string>                       mOrder;
};

class Rule {
public:
    virtual ~Rule() = default;
    [[nodiscard]] virtual ArgumentMap& arguments()                                   = 0;
    [[nodiscard]] virtual int          compare(ItemType const& a, ItemType const& b) = 0;
    [[nodiscard]] bool less(ItemType const& a, ItemType const& b) { return compare(a, b) < 0; }
};

class EmptyRule : public Rule {
public:
    static RulePtr const& instance();
    ArgumentMap&          arguments() override { return mArgs; }
    int                   compare(ItemType const&, ItemType const&) override { return 0; }

private:
    ArgumentMap mArgs;
};

class BaseRule : public Rule {
public:
    using Comparator = std::function<int(ItemType const&, ItemType const&)>;

    BaseRule();
    ArgumentMap& arguments() final { return mArgs; }
    int          compare(ItemType const& a, ItemType const& b) final;

    void setComparator(Comparator c) { mComparator = std::move(c); }

protected:
    ArgumentMap mArgs;
    Comparator  mComparator = [](ItemType const&, ItemType const&) { return 0; };

private:
    std::function<int(ItemType const&, ItemType const&)> mLazyCompare;
};

class MutableEmptyRule : public BaseRule {};

class CustomRule : public BaseRule {
public:
    explicit CustomRule(std::vector<RulePtr> subRules);

private:
    std::vector<RulePtr> mSubRules;
};

int compareByRules(std::vector<RulePtr> const& rules, ItemType const& a, ItemType const& b);

class ItemBucketView {
public:
    virtual ~ItemBucketView()                                              = default;
    [[nodiscard]] virtual int accumulateCount(ItemType const& element) const = 0;
};
class CountSourceScope {
public:
    explicit CountSourceScope(ItemBucketView const* source);
    ~CountSourceScope();
    static ItemBucketView const* current();

private:
    ItemBucketView const* mPrev;
};

}
