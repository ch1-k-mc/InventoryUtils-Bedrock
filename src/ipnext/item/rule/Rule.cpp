// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/item/rule/Rule.h"

#include "ipnext/item/rule/parameter/NativeParameters.h"

namespace inventory_utils::ipnext::item::rule {

void ArgumentMap::defineParametersFrom(ArgumentMap const& other) {
    for (auto const& name : other.mOrder) {
        mDefaults[name] = other.mDefaults.at(name);
        if (std::find(mOrder.begin(), mOrder.end(), name) == mOrder.end()) mOrder.push_back(name);
    }
}

std::any const* ArgumentMap::rawGet(std::string const& name) const {
    if (auto it = mValues.find(name); it != mValues.end()) return &it->second;
    auto const& d = mDefaults.at(name);
    return d ? &*d : nullptr;
}

bool ArgumentMap::trySetArgument(Parameter const& p, std::string const& argument) {
    if (!contains(p.name)) return false;
    auto v = p.argumentType->parse(argument);
    if (!v) return false;
    mValues[p.name] = std::move(*v);
    return true;
}

void ArgumentMap::setArgumentsFrom(ArgumentMap const& other) {
    for (auto const& name : other.mOrder) {
        if (!contains(name)) continue;
        if (auto const* v = other.rawGet(name)) mValues[name] = *v;
    }
}

std::vector<std::string> ArgumentMap::missingParameters() const {
    std::vector<std::string> out;
    for (auto const& name : mOrder) {
        if (!mDefaults.at(name) && !mValues.contains(name)) out.push_back(name);
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> ArgumentMap::dumpAsPairList() const {
    std::vector<std::pair<std::string, std::string>> out;
    for (auto const& name : mOrder) {
        auto const* v = rawGet(name);
        if (!v) {
            out.emplace_back(name, "null");
            continue;
        }
        auto const* p = parameter::NativeParameters::find(name);
        out.emplace_back(name, p ? p->argumentType->toString(*v) : std::string("?"));
    }
    return out;
}

RulePtr const& EmptyRule::instance() {
    static RulePtr const r = std::make_shared<EmptyRule>();
    return r;
}

BaseRule::BaseRule() {
    mArgs.defineParameter(parameter::reverse(), false);
    mArgs.defineParameter(parameter::sub_rule(), EmptyRule::instance());
}

int BaseRule::compare(ItemType const& a, ItemType const& b) {
    if (!mLazyCompare) {
        int  mul          = mArgs.get<bool>(parameter::reverse()) ? -1 : 1;
        bool noSub        = mArgs.isDefaultValue(parameter::sub_rule());
        auto comparator   = mComparator;
        if (noSub) {
            mLazyCompare = [comparator, mul](ItemType const& x, ItemType const& y) { return comparator(x, y) * mul; };
        } else {
            RulePtr sub  = mArgs.get<RulePtr>(parameter::sub_rule());
            mLazyCompare = [comparator, mul, sub](ItemType const& x, ItemType const& y) {
                int r = comparator(x, y);
                if (r != 0) return r * mul;
                return sub->compare(x, y);
            };
        }
    }
    return mLazyCompare(a, b);
}

int compareByRules(std::vector<RulePtr> const& rules, ItemType const& a, ItemType const& b) {
    for (auto const& r : rules) {
        if (int c = r->compare(a, b); c != 0) return c;
    }
    return 0;
}

CustomRule::CustomRule(std::vector<RulePtr> subRules) : mSubRules(std::move(subRules)) {
    setComparator([this](ItemType const& a, ItemType const& b) { return compareByRules(mSubRules, a, b); });
}

namespace {
thread_local ItemBucketView const* gCountSource = nullptr;
}

CountSourceScope::CountSourceScope(ItemBucketView const* source) : mPrev(gCountSource) { gCountSource = source; }
CountSourceScope::~CountSourceScope() { gCountSource = mPrev; }
ItemBucketView const* CountSourceScope::current() { return gCountSource; }

}
