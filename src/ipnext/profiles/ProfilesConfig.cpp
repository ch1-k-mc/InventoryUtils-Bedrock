// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/profiles/ProfilesConfig.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>

#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/StringTag.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/enchanting/Enchant.h"

#include "ipnext/Log.h"
#include "ipnext/item/rule/parameter/NativeParameters.h"
#include "ipnext/item/rule/file/RuleFiles.h"

namespace inventory_utils::ipnext::profiles {

namespace {

std::string stripNamespace(std::string const& s) {
    auto p = s.find(':');
    return p == std::string::npos ? s : s.substr(p + 1);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string bedrockEnchantName(std::string const& javaName) {
    std::string n = lower(stripNamespace(javaName));
    if (n == "binding_curse") return "binding";
    if (n == "vanishing_curse") return "vanishing";
    if (n == "sweeping_edge" || n == "sweeping") return "sweeping_edge";
    return n;
}

std::string enchantNameOf(int id) {
    auto& list = Enchant::mEnchants();
    if (id < 0 || id >= static_cast<int>(list.size()) || !list[id]) return {};
    return lower(list[id]->mStringId->getString());
}

int levelOf(Tag const& t) { return static_cast<int>(item::NbtUtils::asDouble(t)); }

bool matchComponent(ProfileComponentData const& ct, item::ItemType const& type) {
    std::string id = lower(stripNamespace(ct.id));
    if (id == "enchantments" || id == "stored_enchantments" || id == "ench") {
        auto const& have = type.enchantments();
        if (!ct.componentNbt) return !have.empty();
        auto cond = item::NbtUtils::parseNbtOrEmpty(*ct.componentNbt);
        std::map<std::string, int> haveByName;
        for (auto const& e : have) haveByName[enchantNameOf(e.id)] = e.level;
        if (cond->getId() == Tag::List) {
            for (auto const* t : item::NbtUtils::asList(*cond)) {
                if (!haveByName.contains(bedrockEnchantName(item::NbtUtils::asString(*t)))) return false;
            }
            return true;
        }
        if (cond->getId() == Tag::Compound) {
            auto const& c = static_cast<CompoundTag const&>(*cond);
            for (auto const& [k, v] : c.mTags) {
                auto it = haveByName.find(bedrockEnchantName(std::string(k)));
                if (it == haveByName.end() || it->second != levelOf(v.get())) return false;
            }
            return true;
        }
        return false;
    }
    if (id == "potion_contents") {
        if (type.potionName().empty()) return false;
        if (!ct.componentNbt) return true;
        auto cond = item::NbtUtils::parseNbtOrEmpty(*ct.componentNbt);
        std::vector<std::string> names;
        if (cond->getId() == Tag::List) {
            for (auto const* t : item::NbtUtils::asList(*cond)) names.push_back(item::NbtUtils::asString(*t));
        } else {
            names.push_back(item::NbtUtils::asString(*cond));
        }
        std::string potion = lower(type.potionName());
        return std::any_of(names.begin(), names.end(), [&](std::string const& n) { return lower(stripNamespace(n)) == potion; });
    }
    Tag const* component = item::NbtUtils::componentNbt(type, ct.id);
    if (!ct.componentNbt) return component != nullptr;
    if (!component) return false;
    auto cond = item::NbtUtils::parseNbtOrEmpty(*ct.componentNbt);
    return item::NbtUtils::matchNbt(cond.get(), component);
}

enum class T { WORD, QUOTED, LPAREN, RPAREN, LBRACK, RBRACK, COMMA, ARROW, END };

struct Token {
    T           type;
    std::string text;
    int         line;
};

std::vector<Token> tokenize(std::string const& s) {
    std::vector<Token> out;
    size_t             i    = 0;
    int                line = 1;
    while (i < s.size()) {
        char c = s[i];
        if (c == '\n') {
            ++line;
            ++i;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n') ++i;
            continue;
        }
        if (c == '-' && i + 1 < s.size() && s[i + 1] == '>') {
            out.push_back({T::ARROW, "->", line});
            i += 2;
            continue;
        }
        if (c == '(' || c == ')' || c == '[' || c == ']' || c == ',') {
            T t = c == '(' ? T::LPAREN : c == ')' ? T::RPAREN : c == '[' ? T::LBRACK : c == ']' ? T::RBRACK : T::COMMA;
            out.push_back({t, std::string(1, c), line});
            ++i;
            continue;
        }
        if (c == '"') {
            size_t j = i + 1;
            while (j < s.size() && s[j] != '"') {
                if (s[j] == '\\') ++j;
                if (j < s.size() && s[j] == '\n') ++line;
                ++j;
            }
            if (j >= s.size()) throw ProfileSyntaxError(line, "unterminated string");
            out.push_back({T::QUOTED, s.substr(i, j - i + 1), line});
            i = j + 1;
            continue;
        }
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            size_t j = i;
            while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_' || s[j] == '-')) ++j;
            out.push_back({T::WORD, s.substr(i, j - i), line});
            i = j;
            continue;
        }
        throw ProfileSyntaxError(line, std::string("token recognition error at: '") + c + "'");
    }
    out.push_back({T::END, "", line});
    return out;
}

class Parser {
public:
    explicit Parser(std::vector<Token> t) : mT(std::move(t)) {}

    std::vector<ProfileData> script() {
        std::vector<ProfileData> out;
        while (peek().type != T::END) out.push_back(profile());
        return out;
    }

private:
    Token const& peek(size_t k = 0) const { return mT[std::min(mI + k, mT.size() - 1)]; }
    Token const& next() { return mT[mI++]; }
    [[noreturn]] void fail(std::string const& msg) const { throw ProfileSyntaxError(peek().line, msg + " at '" + peek().text + "'"); }
    bool isWord(char const* w, size_t k = 0) const { return peek(k).type == T::WORD && peek(k).text == w; }
    bool isSlotName(size_t k = 0) const { return peek(k).type == T::WORD && slotIdFromName(peek(k).text).has_value(); }

    ProfileData profile() {
        if (!isWord("profile")) fail("expecting 'profile'");
        next();
        if (peek().type != T::WORD) fail("expecting profile name");
        ProfileData p;
        p.name = next().text;
        if (isWord("activate")) {
            next();
            auto id = peek().type == T::WORD ? slotIdFromName(peek().text) : std::nullopt;
            if (!id || invSlotOf(*id) < 0 || invSlotOf(*id) > 8) fail("expecting HOT1..HOT9");
            next();
            p.active = *id;
        }
        while (isSlotName()) p.slots.push_back(slotDef());
        return p;
    }

    ProfileSlot slotDef() {
        ProfileSlot s;
        s.id = *slotIdFromName(next().text);
        if (isWord("order")) {
            next();
            if (peek().type != T::QUOTED) fail("expecting order string");
            std::string o = unescape(next().text);
            if (!o.empty() && o.front() == '@') o.erase(0, 1);
            s.order = o;
        }
        while (peek().type == T::QUOTED) s.items.push_back(itemDef());
        return s;
    }

    ProfileItemData itemDef() {
        ProfileItemData d;
        std::string     raw = unescape(next().text);
        if (!raw.empty() && raw.front() == '#') {
            d.itemId.kind  = ProfileItemId::Kind::Tag;
            d.itemId.value = item::rule::parameter::parseIdentifier(raw.substr(1));
        } else if (raw.find('*') != std::string::npos) {
            d.itemId.kind  = ProfileItemId::Kind::Wildcard;
            d.itemId.value = raw;
        } else {
            d.itemId.kind  = ProfileItemId::Kind::Id;
            d.itemId.value = item::rule::parameter::parseIdentifier(raw);
            if (d.itemId.value.empty()) d.itemId.value = raw;
        }
        if (peek().type == T::LPAREN) {
            next();
            if (peek().type != T::QUOTED) fail("expecting custom name string");
            d.customName = unescape(next().text);
            if (peek().type != T::RPAREN) fail("expecting ')'");
            next();
        }
        if (peek().type == T::ARROW) {
            next();
            if (peek().type != T::LBRACK) fail("expecting '['");
            next();
            std::vector<ProfileComponentData> comps;
            while (true) {
                if (peek().type != T::QUOTED) fail("expecting component id");
                ProfileComponentData c;
                c.id = item::rule::parameter::parseIdentifier(unescape(next().text));
                if (peek().type == T::LPAREN) {
                    next();
                    if (peek().type != T::QUOTED) fail("expecting component nbt string");
                    c.componentNbt = unescape(next().text);
                    if (peek().type != T::RPAREN) fail("expecting ')'");
                    next();
                }
                comps.push_back(std::move(c));
                if (peek().type == T::COMMA) {
                    next();
                    continue;
                }
                break;
            }
            if (peek().type != T::RBRACK) fail("expecting ']'");
            next();
            d.components = std::move(comps);
        }
        return d;
    }

    std::vector<Token> mT;
    size_t             mI = 0;
};

}

std::optional<ProfileSlotId> slotIdFromName(std::string const& n) {
    static std::map<std::string, ProfileSlotId> const m{
        {"HOT1", ProfileSlotId::HOT1}, {"HOT2", ProfileSlotId::HOT2}, {"HOT3", ProfileSlotId::HOT3}, {"HOT4", ProfileSlotId::HOT4},
        {"HOT5", ProfileSlotId::HOT5}, {"HOT6", ProfileSlotId::HOT6}, {"HOT7", ProfileSlotId::HOT7}, {"HOT8", ProfileSlotId::HOT8},
        {"HOT9", ProfileSlotId::HOT9}, {"FEET", ProfileSlotId::FEET}, {"LEGS", ProfileSlotId::LEGS}, {"CHEST", ProfileSlotId::CHEST},
        {"HEAD", ProfileSlotId::HEAD}, {"OFFHAND", ProfileSlotId::OFFHAND},
    };
    auto it = m.find(n);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

std::string slotIdName(ProfileSlotId id) {
    switch (id) {
    case ProfileSlotId::HOT1: return "HOT1";
    case ProfileSlotId::HOT2: return "HOT2";
    case ProfileSlotId::HOT3: return "HOT3";
    case ProfileSlotId::HOT4: return "HOT4";
    case ProfileSlotId::HOT5: return "HOT5";
    case ProfileSlotId::HOT6: return "HOT6";
    case ProfileSlotId::HOT7: return "HOT7";
    case ProfileSlotId::HOT8: return "HOT8";
    case ProfileSlotId::HOT9: return "HOT9";
    case ProfileSlotId::FEET: return "FEET";
    case ProfileSlotId::LEGS: return "LEGS";
    case ProfileSlotId::CHEST: return "CHEST";
    case ProfileSlotId::HEAD: return "HEAD";
    case ProfileSlotId::OFFHAND: return "OFFHAND";
    default: return "NONE";
    }
}

int invSlotOf(ProfileSlotId id) {
    switch (id) {
    case ProfileSlotId::HOT1: return 0;
    case ProfileSlotId::HOT2: return 1;
    case ProfileSlotId::HOT3: return 2;
    case ProfileSlotId::HOT4: return 3;
    case ProfileSlotId::HOT5: return 4;
    case ProfileSlotId::HOT6: return 5;
    case ProfileSlotId::HOT7: return 6;
    case ProfileSlotId::HOT8: return 7;
    case ProfileSlotId::HOT9: return 8;
    case ProfileSlotId::HEAD: return 36;
    case ProfileSlotId::CHEST: return 37;
    case ProfileSlotId::LEGS: return 38;
    case ProfileSlotId::FEET: return 39;
    case ProfileSlotId::OFFHAND: return 40;
    default: return -1;
    }
}

ProfileSlotId slotIdOfInvSlot(int invSlot) {
    for (int i = static_cast<int>(ProfileSlotId::HOT1); i <= static_cast<int>(ProfileSlotId::OFFHAND); ++i) {
        if (invSlotOf(static_cast<ProfileSlotId>(i)) == invSlot) return static_cast<ProfileSlotId>(i);
    }
    return ProfileSlotId::NONE;
}

bool ProfileItemId::test(item::ItemType const& type) const {
    if (type.isEmpty()) return false;
    switch (kind) {
    case Kind::Id: return type.itemId() == value;
    case Kind::Tag: {
        auto const* item = type.vanillaStack().mItem.get();
        return item && item->hasTag(HashedString(value));
    }
    default: {
        std::string pattern = value.find(':') == std::string::npos ? "minecraft:" + value : value;
        return item::rule::file::wildcardMatches(pattern, type.itemId());
    }
    }
}

std::string ProfileItemId::toString() const { return kind == Kind::Tag ? "#" + value : value; }

bool ProfileItemData::match(item::ItemType const& type) const {
    bool nameOk = customName.empty() || customName == type.customName();
    if (!itemId.test(type) || !nameOk) return false;
    if (!components || components->empty()) return true;
    return std::all_of(components->begin(), components->end(), [&](ProfileComponentData const& c) { return matchComponent(c, type); });
}

std::string escape(std::string const& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        out += c;
    }
    return out + "\"";
}

std::string unescape(std::string const& q) {
    if (q.size() < 2 || q.front() != '"' || q.back() != '"') return q;
    std::string out;
    for (size_t i = 1; i + 1 < q.size(); ++i) {
        char c = q[i];
        if (c == '\\' && i + 2 < q.size()) {
            char n = q[++i];
            switch (n) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                if (i + 4 < q.size()) {
                    unsigned cp = std::stoul(q.substr(i + 1, 4), nullptr, 16);
                    i += 4;
                    if (cp < 0x80) out += static_cast<char>(cp);
                    else if (cp < 0x800) {
                        out += static_cast<char>(0xC0 | (cp >> 6));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (cp >> 12));
                        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                }
                break;
            }
            default: out += n; break;
            }
            continue;
        }
        out += c;
    }
    return out;
}

std::string ProfileComponentData::toString() const {
    if (!componentNbt) return escape(id);
    return escape(id) + "(" + escape(*componentNbt) + ")";
}

std::string ProfileItemData::toString() const {
    std::string s = customName.empty() ? escape(itemId.toString()) : escape(itemId.toString()) + "(" + escape(customName) + ")";
    if (!components || components->empty()) return s;
    s += " -> [";
    for (size_t i = 0; i < components->size(); ++i) s += (i ? ", " : "") + (*components)[i].toString();
    return s + "]";
}

std::string ProfileSlot::toString() const {
    std::string s = slotIdName(id);
    if (order) s += " order " + escape("@" + *order);
    for (auto const& it : items) s += "\n\t\t" + it.toString();
    return s;
}

std::string ProfileData::toString() const {
    std::string s = active != ProfileSlotId::NONE ? "profile " + name + " activate " + slotIdName(active) : "profile " + name;
    s += "\n";
    for (size_t i = 0; i < slots.size(); ++i) s += (i ? "\n\t" : "\t") + slots[i].toString();
    return s + "\n";
}

std::vector<ProfileData> ProfilesConfig::getProfiles(std::string const& content) { return Parser(tokenize(content)).script(); }

std::string ProfilesConfig::asString(std::vector<ProfileData> const& profiles) {
    std::string s;
    for (auto const& p : profiles) s += p.toString() + "\n";
    return s;
}

}
