// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/parser/RuleParser.h"

#include <cctype>
#include <optional>

#include "ipnext/Log.h"
#include "ipnext/item/rule/file/RuleFiles.h"
#include "libipn/util/IndentedDataFileParser.h"

namespace inventory_utils::ipnext::parser {

using item::rule::file::RuleDefinition;
using item::rule::file::SubRuleDefinition;

namespace {

enum class Tok { AT, RuleName, REVERSE, DOUBLE_COLON, HASHTAG, NamespacedId, OPEN, NBT, Parameter, EQUAL, CLOSE, COMMA, Argument, END };

char const* tokName(Tok t) {
    switch (t) {
    case Tok::AT:
        return "'@'";
    case Tok::RuleName:
        return "RuleName";
    case Tok::REVERSE:
        return "'!'";
    case Tok::DOUBLE_COLON:
        return "'::'";
    case Tok::HASHTAG:
        return "'#'";
    case Tok::NamespacedId:
        return "NamespacedId";
    case Tok::OPEN:
        return "'('";
    case Tok::NBT:
        return "NBT";
    case Tok::Parameter:
        return "Parameter";
    case Tok::EQUAL:
        return "'='";
    case Tok::CLOSE:
        return "')'";
    case Tok::COMMA:
        return "','";
    case Tok::Argument:
        return "Argument";
    default:
        return "<EOF>";
    }
}

struct Token {
    Tok         type;
    std::string text;
    int         line;
    int         pos;
};

enum class Mode { Default, DeclareName, SubRule, SubRuleName, Args, Arg };

bool isIdChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
bool isWs(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

class Lexer {
public:
    Lexer(std::string const& s, Mode start) : mS(s) { mModes.push_back(start); }

    std::vector<Token> tokenize() {
        std::vector<Token> out;
        while (mI < mS.size()) {
            int  line = mLine, pos = mPos;
            Mode m    = mModes.back();
            switch (m) {
            case Mode::Default: {
                if (isWs(mS[mI])) {
                    skipWs();
                    break;
                }
                if (mS[mI] == '@') {
                    advance(1);
                    out.push_back({Tok::AT, "@", line, pos});
                    mModes.back() = Mode::DeclareName;
                    break;
                }
                error(line, pos);
            }
            case Mode::DeclareName: {
                size_t n = idLen(mI);
                if (n == 0) error(line, pos);
                out.push_back({Tok::RuleName, mS.substr(mI, n), line, pos});
                advance(n);
                mModes.back() = Mode::SubRule;
                break;
            }
            case Mode::SubRule: {
                char c = mS[mI];
                if (isWs(c)) {
                    skipWs();
                    break;
                }
                if (c == '!') {
                    advance(1);
                    out.push_back({Tok::REVERSE, "!", line, pos});
                    break;
                }
                if (c == '@') {
                    advance(1);
                    out.push_back({Tok::AT, "@", line, pos});
                    mModes.push_back(Mode::SubRuleName);
                    break;
                }
                if (c == ':' && mI + 1 < mS.size() && mS[mI + 1] == ':') {
                    advance(2);
                    out.push_back({Tok::DOUBLE_COLON, "::", line, pos});
                    mModes.push_back(Mode::SubRuleName);
                    break;
                }
                if (c == '#') {
                    advance(1);
                    out.push_back({Tok::HASHTAG, "#", line, pos});
                    break;
                }
                if (c == '(') {
                    advance(1);
                    out.push_back({Tok::OPEN, "(", line, pos});
                    mModes.push_back(Mode::Args);
                    break;
                }
                if (c == '{') {
                    size_t end = nested(mI);
                    if (end == std::string::npos) error(line, pos);
                    out.push_back({Tok::NBT, mS.substr(mI, end - mI), line, pos});
                    advance(end - mI);
                    break;
                }
                size_t n = idLen(mI);
                if (n > 0) {
                    size_t len = n;
                    if (mI + n < mS.size() && mS[mI + n] == ':') {
                        size_t m2 = idLen(mI + n + 1);
                        if (m2 > 0) len = n + 1 + m2;
                    }
                    out.push_back({Tok::NamespacedId, mS.substr(mI, len), line, pos});
                    advance(len);
                    break;
                }
                error(line, pos);
            }
            case Mode::SubRuleName: {
                size_t n = idLen(mI);
                if (n == 0) error(line, pos);
                out.push_back({Tok::RuleName, mS.substr(mI, n), line, pos});
                advance(n);
                mModes.pop_back();
                break;
            }
            case Mode::Args: {
                char c = mS[mI];
                if (isWs(c)) {
                    skipWs();
                    break;
                }
                if (c == '=') {
                    advance(1);
                    out.push_back({Tok::EQUAL, "=", line, pos});
                    mModes.push_back(Mode::Arg);
                    break;
                }
                if (c == ')') {
                    advance(1);
                    out.push_back({Tok::CLOSE, ")", line, pos});
                    mModes.pop_back();
                    break;
                }
                size_t n = idLen(mI);
                if (n == 0) error(line, pos);
                out.push_back({Tok::Parameter, mS.substr(mI, n), line, pos});
                advance(n);
                break;
            }
            case Mode::Arg: {
                char c = mS[mI];
                size_t argEnd = argument(mI);
                size_t argLen = argEnd == std::string::npos ? 0 : argEnd - mI;
                if (c == ')' && argLen <= 1) {
                    advance(1);
                    out.push_back({Tok::CLOSE, ")", line, pos});
                    mModes.pop_back();
                    if (!mModes.empty()) mModes.pop_back();
                    break;
                }
                if (c == ',' && argLen <= 1) {
                    advance(1);
                    out.push_back({Tok::COMMA, ",", line, pos});
                    mModes.pop_back();
                    break;
                }
                size_t wsLen = 0;
                while (mI + wsLen < mS.size() && isWs(mS[mI + wsLen])) ++wsLen;
                if (argLen > 0 && argLen >= wsLen) {
                    if (argLen == wsLen) {
                        skipWs();
                        break;
                    }
                    out.push_back({Tok::Argument, mS.substr(mI, argLen), line, pos});
                    advance(argLen);
                    break;
                }
                if (wsLen > 0) {
                    skipWs();
                    break;
                }
                error(line, pos);
            }
            }
        }
        out.push_back({Tok::END, "", mLine, mPos});
        return out;
    }

private:
    [[noreturn]] void error(int line, int pos) {
        throw SyntaxErrorException(line, pos, std::string("token recognition error at: '") + mS[mI] + "'");
    }

    void advance(size_t n) {
        for (size_t k = 0; k < n && mI < mS.size(); ++k, ++mI) {
            if (mS[mI] == '\n') {
                ++mLine;
                mPos = 0;
            } else {
                ++mPos;
            }
        }
    }
    void skipWs() {
        while (mI < mS.size() && isWs(mS[mI])) advance(1);
    }
    [[nodiscard]] size_t idLen(size_t i) const {
        size_t j = i;
        while (j < mS.size() && isIdChar(mS[j])) ++j;
        return j - i;
    }

    [[nodiscard]] size_t stringEnd(size_t i) const {
        char q = mS[i];
        size_t j = i + 1;
        while (j < mS.size()) {
            char c = mS[j];
            if (c == '\\') {
                j += 2;
                continue;
            }
            if (c == q) return j + 1;
            if (static_cast<unsigned char>(c) < 0x20) return std::string::npos;
            ++j;
        }
        return std::string::npos;
    }

    [[nodiscard]] size_t common(size_t i) const {
        char c = mS[i];
        if (c == '"' || c == '\'') return stringEnd(i);
        char close = c == '(' ? ')' : (c == '[' ? ']' : (c == '{' ? '}' : 0));
        if (!close) return std::string::npos;
        size_t j = i + 1;
        while (j < mS.size() && mS[j] != close) {
            size_t k = nestedElement(j);
            if (k == std::string::npos) return std::string::npos;
            j = k;
        }
        return j < mS.size() ? j + 1 : std::string::npos;
    }

    [[nodiscard]] size_t nestedElement(size_t i) const {
        char c = mS[i];
        if (c == '"' || c == '\'' || c == '(' || c == '[' || c == '{') return common(i);
        if (c == ')' || c == ']' || c == '}') return std::string::npos;
        size_t j = i;
        while (j < mS.size()) {
            char d = mS[j];
            if (d == '"' || d == '\'' || d == '{' || d == '[' || d == '(' || d == ')' || d == ']' || d == '}') break;
            ++j;
        }
        return j;
    }

    [[nodiscard]] size_t nested(size_t i) const { return common(i); }

    [[nodiscard]] size_t argument(size_t i) const {
        size_t j = i;
        while (j < mS.size()) {
            char c = mS[j];
            if (c == '"' || c == '\'' || c == '(' || c == '[' || c == '{') {
                size_t k = common(j);
                if (k == std::string::npos) break;
                j = k;
                continue;
            }
            if (c == ',' || c == ')' || c == ']' || c == '}') break;
            ++j;
        }
        return j == i ? std::string::npos : j;
    }

    std::string const& mS;
    size_t             mI    = 0;
    int                mLine = 1;
    int                mPos  = 0;
    std::vector<Mode>  mModes;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : mT(std::move(tokens)) {}

    RuleDefinition customRuleEOF() {
        expect(Tok::AT);
        std::string name = expect(Tok::RuleName).text;
        auto        subs = subRuleEOF();
        return RuleDefinition(name, std::move(subs));
    }

    std::vector<SubRuleDefinition> subRuleEOF() {
        std::vector<SubRuleDefinition> subs;
        subs.push_back(subRule());
        while (peek().type != Tok::END) subs.push_back(subRule());
        expect(Tok::END);
        return subs;
    }

private:
    Token const& peek() const { return mT[mI]; }
    Token const& expect(Tok t) {
        if (peek().type != t) {
            throw SyntaxErrorException(peek().line, peek().pos, std::string("mismatched input '") + peek().text + "' expecting " + tokName(t));
        }
        return mT[mI++];
    }
    bool accept(Tok t) {
        if (peek().type != t) return false;
        ++mI;
        return true;
    }

    SubRuleDefinition subRule() {
        std::vector<std::pair<std::string, std::string>> args;
        auto put = [&](std::string const& k, std::string const& v) {
            for (auto& a : args) {
                if (a.first == k) {
                    a.second = v;
                    return;
                }
            }
            args.emplace_back(k, v);
        };
        if (accept(Tok::REVERSE)) put("reverse", "true");
        std::string prefix = "::";
        std::string name;
        if (peek().type == Tok::AT || peek().type == Tok::DOUBLE_COLON) {
            if (peek().type == Tok::AT) prefix = "@";
            ++mI;
            name = expect(Tok::RuleName).text;
        } else {
            bool        tag = accept(Tok::HASHTAG);
            std::string id  = expect(Tok::NamespacedId).text;
            std::string itemOrTag = tag ? "tag" : "item";
            if (peek().type == Tok::NBT) {
                put("require_nbt", "required");
                put("nbt", mT[mI++].text);
            }
            put(itemOrTag + "_name", id);
            name = "is_" + itemOrTag;
        }
        if (peek().type == Tok::OPEN) {
            ++mI;
            if (!accept(Tok::CLOSE)) {
                do {
                    std::string p = expect(Tok::Parameter).text;
                    expect(Tok::EQUAL);
                    std::string a = expect(Tok::Argument).text;
                    put(p, a);
                } while (accept(Tok::COMMA));
                expect(Tok::CLOSE);
            }
        }
        return SubRuleDefinition(prefix, name, std::move(args));
    }

    std::vector<Token> mT;
    size_t             mI = 0;
};

}

RuleDefinition RuleParser::parseRuleDefinition(std::string const& content) {
    return Parser(Lexer(content, Mode::Default).tokenize()).customRuleEOF();
}

RuleDefinition RuleParser::parseRuleDefinition(libipn::util::IndentedData const& data) {
    auto        lines = data.lines();
    std::string text;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) text += "\n";
        text += lines[i].rawText;
    }
    try {
        return parseRuleDefinition(text);
    } catch (SyntaxErrorException const& e) {
        int line = (e.line - 1 >= 0 && e.line - 1 < static_cast<int>(lines.size())) ? lines[e.line - 1].lineNumber : -1;
        throw SyntaxErrorException(line, e.pos + 1, e.msg);
    }
}

std::vector<SubRuleDefinition> RuleParser::parseSubRule(std::string const& content) {
    return Parser(Lexer(content, Mode::SubRule).tokenize()).subRuleEOF();
}

namespace {
std::map<std::string, item::rule::RulePtr>& tempCache() {
    static std::map<std::string, item::rule::RulePtr> m;
    return m;
}
}

item::rule::RulePtr TemporaryRuleParser::parse(std::string const& content) {
    if (auto it = tempCache().find(content); it != tempCache().end()) return it->second;
    std::vector<SubRuleDefinition> subRules;
    try {
        subRules = RuleParser::parseSubRule(content);
    } catch (SyntaxErrorException const& e) {
        Log::warn("Syntax error in '{}'", content);
        Log::warn("  > at: {}:{} {}", e.line, e.pos, e.msg);
        return item::rule::EmptyRule::instance();
    } catch (std::exception const& e) {
        Log::warn("Error in '{}': {}", content, e.what());
        return item::rule::EmptyRule::instance();
    }
    try {
        std::vector<item::rule::RulePtr> rules;
        for (auto& s : subRules) rules.push_back(s.toRule());
        auto result          = std::make_shared<item::rule::CustomRule>(std::move(rules));
        tempCache()[content] = result;
        return result;
    } catch (std::exception const& e) {
        Log::warn("Error in '{}'", content);
        Log::warn("  > {}", e.what());
        return item::rule::EmptyRule::instance();
    }
}

void TemporaryRuleParser::onReload() { tempCache().clear(); }

}
