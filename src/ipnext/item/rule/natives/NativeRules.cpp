// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/item/rule/natives/NativeRules.h"

#include <algorithm>

#include "mc/deps/nbt/CompoundTag.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"

#include "ipnext/Log.h"
#include "ipnext/item/NbtUtils.h"
#include "ipnext/item/rule/parameter/NativeParameters.h"
#include "libipn/util/StringComparators.h"

namespace inventory_utils::ipnext::item::rule::natives {

using namespace parameter;
using libipn::util::U16Comparator;

namespace {

template <class T, class MatchBy, class Same>
int compareByMatch(T const& v1, T const& v2, MatchBy matchBy, Match match, Same bothSameCompare) {
    bool b1 = matchBy(v1), b2 = matchBy(v2);
    if (b1 == b2) return bothSameCompare(v1, v2);
    return multiplier(match) * (b1 ? -1 : 1);
}

int compareByMatchSeparate(ItemType const& v1, ItemType const& v2, bool b1, bool b2, Match match, RulePtr const& matchRule, RulePtr const& notMatchRule) {
    if (b1 == b2) return b1 ? matchRule->compare(v1, v2) : notMatchRule->compare(v1, v2);
    return multiplier(match) * (b1 ? -1 : 1);
}

bool isBlank(std::string const& s) {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
}

std::string gameLanguageTag() {
    try {
        auto lang = getI18n().getCurrentLanguage();
        std::string code = *lang->mCode;
        std::replace(code.begin(), code.end(), '_', '-');
        return code;
    } catch (...) {
        return "sys";
    }
}

class StringBasedRule : public BaseRule {
public:
    std::function<std::string(ItemType const&)> valueOf = [](ItemType const&) { return std::string(); };

    StringBasedRule() {
        mArgs.defineParameter(blank_string(), Match::LAST);
        mArgs.defineParameter(string_compare(), StringCompare::LOCALE);
        mArgs.defineParameter(locale(), std::string("mc"));
        mArgs.defineParameter(strength(), Strength::PRIMARY);
        mArgs.defineParameter(logical(), true);
        setComparator([this](ItemType const& a, ItemType const& b) { return compareString(valueOf(a), valueOf(b)); });
    }

    int compareString(std::string const& s1, std::string const& s2) {
        return compareByMatch(s1, s2, isBlank, mArgs.get<Match>(blank_string()), [this](std::string const& a, std::string const& b) {
            return lazyCompareString()(libipn::util::toU16(a), libipn::util::toU16(b));
        });
    }

private:
    U16Comparator const& lazyCompareString() {
        if (!mStringComparator) {
            U16Comparator raw;
            switch (mArgs.get<StringCompare>(string_compare())) {
            case StringCompare::UNICODE_:
                raw = libipn::util::naturalOrder;
                break;
            case StringCompare::IGNORE_CASE:
                raw = libipn::util::caseInsensitiveOrder;
                break;
            default: {
                std::string tag = mArgs.get<std::string>(locale());
                if (tag == "mc") tag = gameLanguageTag();
                std::replace(tag.begin(), tag.end(), '_', '-');
                raw = libipn::util::makeCollator(tag, static_cast<libipn::util::CollatorStrength>(mArgs.get<Strength>(strength())));
                break;
            }
            }
            mStringComparator = mArgs.get<bool>(logical()) ? libipn::util::makeLogicalStringComparator(raw) : raw;
        }
        return mStringComparator;
    }

    U16Comparator mStringComparator;
};

class NumberBasedRule : public BaseRule {
public:
    std::function<double(ItemType const&)> valueOf = [](ItemType const&) { return 0.0; };

    NumberBasedRule() {
        mArgs.defineParameter(number_order(), NumberOrder::ASCENDING);
        setComparator([this](ItemType const& a, ItemType const& b) { return compareNumberValue(valueOf(a), valueOf(b)); });
    }
    int compareNumberValue(double a, double b) { return compareNumber(mArgs.get<NumberOrder>(number_order()), a, b); }
};

class BooleanBasedRule : public BaseRule {
public:
    std::function<bool(ItemType const&)> valueOf = [](ItemType const&) { return false; };

    BooleanBasedRule() {
        mArgs.defineParameter(match(), Match::FIRST);
        mArgs.defineParameter(sub_rule_match(), EmptyRule::instance());
        mArgs.defineParameter(sub_rule_not_match(), EmptyRule::instance());
        setComparator([this](ItemType const& a, ItemType const& b) {
            return compareByMatchSeparate(a, b, valueOf(a), valueOf(b), mArgs.get<Match>(match()), mArgs.get<RulePtr>(sub_rule_match()), mArgs.get<RulePtr>(sub_rule_not_match()));
        });
    }

    void andValue(std::function<bool(ItemType const&)> extra) {
        auto old = valueOf;
        valueOf  = [extra = std::move(extra), old](ItemType const& t) { return extra(t) && old(t); };
    }
};

class SimpleParameterBasedRule : public BooleanBasedRule {
public:
    SimpleParameterBasedRule() {
        mArgs.defineParameter(component_id(), std::string("ipn:none"));
        mArgs.defineParameter(nbt(), NbtUtils::TagPtr(NbtUtils::parseNbtOrEmpty("{}")));
        mArgs.defineParameter(allow_extra(), true);
        valueOf = [this](ItemType const& t) {
            static NbtUtils::TagPtr const empty = NbtUtils::parseNbtOrEmpty("{}");
            Tag const* value                    = NbtUtils::componentNbt(t, mArgs.get<std::string>(component_id()));
            if (!value) value = empty.get();
            auto const& pattern = mArgs.get<NbtUtils::TagPtr>(nbt());
            return mArgs.get<bool>(allow_extra()) ? NbtUtils::matchNbt(pattern.get(), value) : NbtUtils::matchNbtNoExtra(pattern.get(), value);
        };
    }
};

class MatchNbtRule : public BooleanBasedRule {
public:
    MatchNbtRule() {
        mArgs.defineParameter(component_id());
        mArgs.defineParameter(nbt(), NbtUtils::TagPtr(NbtUtils::parseNbtOrEmpty("{}")));
        mArgs.defineParameter(allow_extra(), true);
        valueOf = [this](ItemType const& t) {
            Tag const*  value   = NbtUtils::componentNbt(t, mArgs.get<std::string>(component_id()));
            auto const& pattern = mArgs.get<NbtUtils::TagPtr>(nbt());
            return mArgs.get<bool>(allow_extra()) ? NbtUtils::matchNbt(pattern.get(), value) : NbtUtils::matchNbtNoExtra(pattern.get(), value);
        };
    }
};

class ByNbtRule : public BaseRule {
public:
    ByNbtRule() {
        mArgs.defineParameter(component_id());
        mArgs.defineParameter(nbt_path());
        mArgs.defineParameter(not_found(), Match::LAST);
        mArgs.defineParametersFrom(mDummyString.arguments());
        mArgs.defineParametersFrom(mDummyNumber.arguments());
        setComparator([this](ItemType const& a, ItemType const& b) { return compareByNbtPath(a, b); });
    }

private:
    int compareByNbtPath(ItemType const& a, ItemType const& b) {
        if (!mInitialized) {
            mInitialized = true;
            mDummyString.arguments().setArgumentsFrom(mArgs);
            mDummyNumber.arguments().setArgumentsFrom(mArgs);
        }
        auto const& path = mArgs.get<NbtUtils::NbtPath>(nbt_path());
        auto const& comp = mArgs.get<std::string>(component_id());
        auto        t1   = path.getTags(a, comp);
        auto        t2   = path.getTags(b, comp);
        if (t1.size() > 1 || t2.size() > 1) Log::warn("given nbt path produce more than one result. currently support only the first result");
        Tag const* tag1 = t1.empty() ? nullptr : t1.front();
        Tag const* tag2 = t2.empty() ? nullptr : t2.front();
        bool       b1 = tag1 == nullptr, b2 = tag2 == nullptr;
        if (b1 != b2) return multiplier(mArgs.get<Match>(not_found())) * (b1 ? -1 : 1);
        if (!tag1 || !tag2) return 0;
        return compareTag(*tag1, *tag2);
    }

    int compareTag(Tag const& t1, Tag const& t2) {
        if (NbtUtils::isNumber(t1) && NbtUtils::isNumber(t2)) return mDummyNumber.compareNumberValue(NbtUtils::asDouble(t1), NbtUtils::asDouble(t2));
        if (NbtUtils::isCompound(t1) && NbtUtils::isCompound(t2)) return NbtUtils::compareNbt(&static_cast<CompoundTag const&>(t1), &static_cast<CompoundTag const&>(t2));
        if (t1.getId() == Tag::List && t2.getId() == Tag::List) {
            auto l1 = NbtUtils::asList(t1), l2 = NbtUtils::asList(t2);
            size_t n = std::min(l1.size(), l2.size());
            for (size_t i = 0; i < n; ++i) {
                if (int c = compareTag(*l1[i], *l2[i]); c != 0) return c;
            }
            return l1.size() == l2.size() ? 0 : (l1.size() < l2.size() ? -1 : 1);
        }
        return mDummyString.compareString(NbtUtils::asString(t1), NbtUtils::asString(t2));
    }

    StringBasedRule mDummyString;
    NumberBasedRule mDummyNumber;
    bool            mInitialized = false;
};

int compareEnchantments(std::vector<EnchantmentEntry> a, std::vector<EnchantmentEntry> b) {
    auto byId = [](EnchantmentEntry const& x, EnchantmentEntry const& y) { return x.id < y.id; };
    std::stable_sort(a.begin(), a.end(), byId);
    std::stable_sort(b.begin(), b.end(), byId);
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        if (a[i].id != b[i].id) return a[i].id - b[i].id;
        if (a[i].level != b[i].level) return b[i].level - a[i].level;
    }
    return static_cast<int>(b.size()) - static_cast<int>(a.size());
}

class EnchantmentsOrder : public BaseRule {
public:
    EnchantmentsOrder() {
        setComparator([](ItemType const& a, ItemType const& b) {
            auto const& ea = a.enchantments();
            auto const& eb = b.enchantments();
            if (!ea.empty() || !eb.empty()) return compareEnchantments(ea, eb);
            return 0;
        });
    }
};

class NbtComparatorRule : public BaseRule {
public:
    NbtComparatorRule() {
        mArgs.defineParameter(component_id());
        setComparator([this](ItemType const& a, ItemType const& b) {
            auto const& id = mArgs.get<std::string>(component_id());
            Tag const*  va = NbtUtils::componentNbt(a, id);
            Tag const*  vb = NbtUtils::componentNbt(b, id);
            if (!va || !vb) return va == vb ? 0 : (va ? -1 : 1);
            return NbtUtils::compareTag(*va, *vb);
        });
    }
};

class AllComponentsRule : public BaseRule {
public:
    AllComponentsRule() {
        setComparator([](ItemType const& a, ItemType const& b) { return NbtUtils::compareComponents(a, b); });
    }
};

class PotionEffectRule : public BaseRule {
public:
    PotionEffectRule() {
        setComparator([](ItemType const& a, ItemType const& b) {
            auto const& la = a.comparablePotionEffects();
            auto const& lb = b.comparablePotionEffects();
            size_t      n  = std::min(la.size(), lb.size());
            for (size_t i = 0; i < n; ++i) {
                if (int c = la[i].compareTo(lb[i]); c != 0) return c;
            }
            return la.size() == lb.size() ? 0 : (la.size() < lb.size() ? -1 : 1);
        });
    }
};

using Factory = std::function<RulePtr()>;

template <class R, class V>
Factory typeRule(V valueOf, std::function<void(R&)> post = {}) {
    return [valueOf, post]() -> RulePtr {
        auto r     = std::make_shared<R>();
        r->valueOf = valueOf;
        if (post) post(*r);
        return r;
    };
}

std::map<std::string, Factory> const& nativeMap() {
    static std::map<std::string, Factory> const m = [] {
        std::map<std::string, Factory> m;
        m["none"] = [] { return std::make_shared<MutableEmptyRule>(); };
        m["display_name"]    = typeRule<StringBasedRule>([](ItemType const& t) { return t.displayName(); });
        m["custom_name"]     = typeRule<StringBasedRule>([](ItemType const& t) { return t.customName(); });
        m["translated_name"] = typeRule<StringBasedRule>([](ItemType const& t) { return t.translatedName(); });
        auto unicode         = std::function<void(StringBasedRule&)>([](StringBasedRule& r) { r.arguments().defineParameter(string_compare(), StringCompare::UNICODE_); });
        m["translation_key"] = typeRule<StringBasedRule>([](ItemType const& t) { return t.translationKey(); }, unicode);
        m["item_id"]         = typeRule<StringBasedRule>([](ItemType const& t) { return t.itemId(); }, unicode);
        m["potion_name"]     = typeRule<StringBasedRule>([](ItemType const& t) { return t.potionName(); }, unicode);
        auto descending = std::function<void(NumberBasedRule&)>([](NumberBasedRule& r) { r.arguments().defineParameter(number_order(), NumberOrder::DESCENDING); });
        m["raw_id"]                    = typeRule<NumberBasedRule>([](ItemType const& t) { return static_cast<double>(t.rawId()); });
        m["creative_menu_group_index"] = typeRule<NumberBasedRule>([](ItemType const& t) { return static_cast<double>(t.groupIndex()); });
        m["search_tab_index"]          = typeRule<NumberBasedRule>([](ItemType const& t) { return static_cast<double>(t.searchTabIndex()); });
        m["damage"]                    = typeRule<NumberBasedRule>([](ItemType const& t) { return static_cast<double>(t.damage()); });
        m["max_damage"]                = typeRule<NumberBasedRule>([](ItemType const& t) { return static_cast<double>(t.maxDamage()); });
        m["durability"]                = typeRule<NumberBasedRule>([](ItemType const& t) { return static_cast<double>(t.durability()); });
        m["enchantments_score"]        = typeRule<NumberBasedRule>([](ItemType const& t) { return t.enchantmentsScore(); }, descending);
        m["accumulated_count"]         = typeRule<NumberBasedRule>(
            [](ItemType const& t) {
                auto const* source = CountSourceScope::current();
                return source ? static_cast<double>(source->accumulateCount(t.copy(true))) : 0.0;
            },
            descending
        );
        m["has_custom_name"]     = typeRule<BooleanBasedRule>([](ItemType const& t) { return t.hasCustomName(); });
        m["is_damageable"]       = typeRule<BooleanBasedRule>([](ItemType const& t) { return t.isDamageable(); });
        m["component_match_nbt"] = [] { return std::make_shared<MatchNbtRule>(); };
        m["is_tag"]              = [] {
            auto r = std::make_shared<SimpleParameterBasedRule>();
            r->arguments().defineParameter(tag_name());
            r->arguments().defineParameter(require_nbt(), RequireNbt::NOT_REQUIRED);
            auto* raw = r.get();
            r->andValue([raw](ItemType const& t) {
                return requireNbtMatch(raw->arguments().get<RequireNbt>(require_nbt()), t) && raw->arguments().get<ItemTypeMatcher>(tag_name()).match(t);
            });
            return RulePtr(r);
        };
        m["is_item"] = [] {
            auto r = std::make_shared<SimpleParameterBasedRule>();
            r->arguments().defineParameter(item_name());
            r->arguments().defineParameter(require_nbt(), RequireNbt::NOT_REQUIRED);
            auto* raw = r.get();
            r->andValue([raw](ItemType const& t) {
                return requireNbtMatch(raw->arguments().get<RequireNbt>(require_nbt()), t) && raw->arguments().get<ItemTypeMatcher>(item_name()).match(t);
            });
            return RulePtr(r);
        };
        m["component_by_nbt"]           = [] { return std::make_shared<ByNbtRule>(); };
        m["component_nbt_comparator"]   = [] { return std::make_shared<NbtComparatorRule>(); };
        m["potion_effect"]              = [] { return std::make_shared<PotionEffectRule>(); };
        m["components_comparator"]      = [] { return std::make_shared<AllComponentsRule>(); };
        m["enchantments_tooltip_order"] = [] { return std::make_shared<EnchantmentsOrder>(); };
        return m;
    }();
    return m;
}

}

RulePtr NativeRules::create(std::string const& name) {
    auto const& m  = nativeMap();
    auto        it = m.find(name);
    return it == m.end() ? nullptr : it->second();
}

std::vector<std::string> NativeRules::names() {
    std::vector<std::string> out;
    for (auto const& [k, v] : nativeMap()) out.push_back(k);
    return out;
}

}
