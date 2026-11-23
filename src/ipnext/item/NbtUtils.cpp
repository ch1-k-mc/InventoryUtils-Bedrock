// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/item/NbtUtils.h"

#include <algorithm>

#include "mc/deps/nbt/ByteArrayTag.h"
#include "mc/deps/nbt/ByteTag.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include "mc/deps/nbt/DoubleTag.h"
#include "mc/deps/nbt/FloatTag.h"
#include "mc/deps/nbt/Int64Tag.h"
#include "mc/deps/nbt/IntArrayTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/ShortTag.h"
#include "mc/deps/nbt/StringTag.h"

#include "ipnext/item/ItemType.h"

namespace inventory_utils::ipnext::item::NbtUtils {

namespace {

int sign(int v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

template <class T>
int cmp(T const& a, T const& b) {
    return a < b ? -1 : (b < a ? 1 : 0);
}

template <class T, class F>
int compareLists(std::vector<T> const& a, std::vector<T> const& b, F compare) {
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        int c = compare(a[i], b[i]);
        if (c != 0) return c;
    }
    return cmp(a.size(), b.size());
}

std::string stripQuotes(std::string s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
    return s;
}

std::vector<std::string> sortedKeys(CompoundTag const& t) {
    std::vector<std::string> keys;
    for (auto const& [k, v] : t.mTags) keys.emplace_back(k);
    std::sort(keys.begin(), keys.end());
    return keys;
}

}

bool isNumber(Tag const& t) {
    auto id = t.getId();
    return id == Tag::Byte || id == Tag::Short || id == Tag::Int || id == Tag::Int64 || id == Tag::Float || id == Tag::Double;
}
bool isString(Tag const& t) { return t.getId() == Tag::String; }
bool isCompound(Tag const& t) { return t.getId() == Tag::Compound; }
bool isList(Tag const& t) {
    auto id = t.getId();
    return id == Tag::ByteArray || id == Tag::List || id == Tag::IntArray;
}

double asDouble(Tag const& t) {
    switch (t.getId()) {
    case Tag::Byte:
        return static_cast<ByteTag const&>(t).data;
    case Tag::Short:
        return static_cast<ShortTag const&>(t).data;
    case Tag::Int:
        return static_cast<IntTag const&>(t).data;
    case Tag::Int64:
        return static_cast<double>(static_cast<Int64Tag const&>(t).data);
    case Tag::Float:
        return static_cast<FloatTag const&>(t).data;
    case Tag::Double:
        return static_cast<DoubleTag const&>(t).data;
    default:
        return 0;
    }
}

std::string asString(Tag const& t) {
    if (t.getId() == Tag::String) return static_cast<std::string const&>(static_cast<StringTag const&>(t));
    return t.toSnbt(SnbtFormat::Minimize, 0);
}

std::vector<Tag const*> asList(Tag const& t) {
    std::vector<Tag const*> out;
    if (t.getId() == Tag::List) {
        for (auto const& e : static_cast<ListTag const&>(t)) {
            if (e) out.push_back(e.get());
        }
    }
    return out;
}

int compareNbt(CompoundTag const* a, CompoundTag const* b) {
    bool b1 = a == nullptr, b2 = b == nullptr;
    if (b1 != b2) return b1 ? -1 : 1;
    if (!a || !b) return 0;
    auto keys1 = sortedKeys(*a);
    auto keys2 = sortedKeys(*b);
    return compareLists(keys1, keys2, [&](std::string const& k1, std::string const& k2) {
        if (int c = k1.compare(k2); c != 0) return sign(c);
        return compareTag((*a)[k1].get(), (*b)[k2].get());
    });
}

int compareTag(Tag const& a, Tag const& b) {
    if (isNumber(a) && isNumber(b)) return cmp(asDouble(a), asDouble(b));
    if (isCompound(a) && isCompound(b)) return compareNbt(&static_cast<CompoundTag const&>(a), &static_cast<CompoundTag const&>(b));
    if (a.getId() == Tag::List && b.getId() == Tag::List) {
        return compareLists(asList(a), asList(b), [](Tag const* x, Tag const* y) { return compareTag(*x, *y); });
    }
    if (isString(a) && isString(b)) return sign(stripQuotes(asString(a)).compare(stripQuotes(asString(b))));
    return sign(asString(a).compare(asString(b)));
}

TagPtr parseNbtOrEmpty(std::string const& nbt) {
    if (auto v = CompoundTagVariant::parse(nbt)) return TagPtr(v->get().copy().release());
    return std::make_shared<CompoundTag>();
}

namespace {

bool sameType(Tag const& a, Tag const& b) {
    return (isList(a) && isList(b)) || (isString(a) && isString(b)) || (isCompound(a) && isCompound(b)) || (isNumber(a) && isNumber(b));
}

bool listContainsAll(Tag const& b, Tag const& a) {
    if (a.getId() != Tag::List || b.getId() != Tag::List) return a.equals(b);
    auto la = asList(a), lb = asList(b);
    return std::all_of(la.begin(), la.end(), [&](Tag const* x) {
        return std::any_of(lb.begin(), lb.end(), [&](Tag const* y) { return x->equals(*y); });
    });
}

bool innerMatch(Tag const& criteria, Tag const& target) {
    if (isCompound(criteria)) {
        if (!isCompound(target)) return false;
        auto const& c = static_cast<CompoundTag const&>(criteria);
        auto const& t = static_cast<CompoundTag const&>(target);
        for (auto const& [k, v] : c.mTags) {
            if (!t.contains(k)) return false;
            if (!innerMatch(v.get(), t[k].get())) return false;
        }
        return true;
    }
    if (criteria.getId() == Tag::List && target.getId() == Tag::List) {
        auto lc = asList(criteria), lt = asList(target);
        if (lc.empty()) return lt.empty();
        return std::all_of(lc.begin(), lc.end(), [&](Tag const* x) {
            return std::any_of(lt.begin(), lt.end(), [&](Tag const* y) { return innerMatch(*x, *y); });
        });
    }
    return criteria.equals(target);
}

}

bool matchNbtNoExtra(Tag const* a, Tag const* b) {
    if (!a) return true;
    if (!b) return false;
    if (!sameType(*a, *b)) return false;
    if (isNumber(*a)) return asDouble(*a) == asDouble(*b);
    if (isString(*a)) return asString(*a) == asString(*b);
    if (isList(*a)) return asList(*a).size() == asList(*b).size() && listContainsAll(*a, *b);
    auto const& ca = static_cast<CompoundTag const&>(*a);
    auto const& cb = static_cast<CompoundTag const&>(*b);
    if (ca.empty() || cb.empty()) return ca.empty() && cb.empty();
    return ca.equals(cb);
}

bool matchNbt(Tag const* a, Tag const* b) {
    if (!a) return true;
    if (!b) return false;
    if (!sameType(*a, *b)) return false;
    if (isNumber(*a)) return asDouble(*a) == asDouble(*b);
    if (isString(*a)) return asString(*a) == asString(*b);
    if (isList(*a)) return listContainsAll(*b, *a);
    auto const& ca = static_cast<CompoundTag const&>(*a);
    auto const& cb = static_cast<CompoundTag const&>(*b);
    if (ca.empty() && cb.empty()) return true;
    if (cb.empty()) return false;
    return innerMatch(ca, cb);
}

Tag const* componentNbt(ItemType const& itemType, std::string const& componentId) {
    CompoundTag const* tag = itemType.tag();
    if (!tag) return nullptr;
    std::string key = componentId;
    if (auto p = key.find(':'); p != std::string::npos) key = key.substr(p + 1);
    if (key.empty() || componentId == "ipn:none") return tag;
    if (!tag->contains(key)) return nullptr;
    return &(*tag)[key].get();
}

int compareComponents(ItemType const& a, ItemType const& b) {
    CompoundTag const* t1 = a.tag();
    CompoundTag const* t2 = b.tag();
    size_t             s1 = t1 ? t1->size() : 0;
    size_t             s2 = t2 ? t2->size() : 0;
    if (t1 && t2) {
        for (auto const& key : sortedKeys(*t1)) {
            if (!t2->contains(key)) continue;
            if (int c = compareTag((*t1)[key].get(), (*t2)[key].get()); c != 0) return c;
        }
    }
    return cmp(s1, s2);
}

std::optional<NbtPath> NbtPath::of(std::string const& path) {
    NbtPath p;
    p.mText  = path;
    size_t i = 0;
    while (i < path.size()) {
        char ch = path[i];
        if (ch == '.') {
            ++i;
            continue;
        }
        if (ch == '[') {
            auto close = path.find(']', i);
            if (close == std::string::npos) return std::nullopt;
            std::string inner = path.substr(i + 1, close - i - 1);
            Node        n;
            if (inner.empty()) {
                n.kind = Node::Kind::All;
            } else {
                try {
                    n.kind  = Node::Kind::Index;
                    n.index = std::stoi(inner);
                } catch (...) {
                    return std::nullopt;
                }
            }
            p.mNodes.push_back(n);
            i = close + 1;
            continue;
        }
        Node n;
        if (ch == '"') {
            auto close = path.find('"', i + 1);
            if (close == std::string::npos) return std::nullopt;
            n.key = path.substr(i + 1, close - i - 1);
            i     = close + 1;
        } else {
            size_t j = i;
            while (j < path.size() && path[j] != '.' && path[j] != '[') ++j;
            n.key = path.substr(i, j - i);
            i     = j;
        }
        p.mNodes.push_back(n);
    }
    if (p.mNodes.empty()) return std::nullopt;
    return p;
}

std::vector<Tag const*> NbtPath::getTagsFor(Tag const& root) const {
    std::vector<Tag const*> current{&root};
    for (auto const& node : mNodes) {
        std::vector<Tag const*> next;
        for (auto const* t : current) {
            if (node.kind == Node::Kind::Key) {
                if (!isCompound(*t)) continue;
                auto const& c = static_cast<CompoundTag const&>(*t);
                if (c.contains(node.key)) next.push_back(&c[node.key].get());
            } else {
                auto list = asList(*t);
                if (node.kind == Node::Kind::All) {
                    next.insert(next.end(), list.begin(), list.end());
                } else {
                    int idx = node.index < 0 ? static_cast<int>(list.size()) + node.index : node.index;
                    if (idx >= 0 && idx < static_cast<int>(list.size())) next.push_back(list[idx]);
                }
            }
        }
        current = std::move(next);
        if (current.empty()) break;
    }
    return current;
}

std::vector<Tag const*> NbtPath::getTags(ItemType const& itemType, std::string const& componentId) const {
    Tag const* root = componentNbt(itemType, componentId);
    if (!root) return {};
    return getTagsFor(*root);
}

}
