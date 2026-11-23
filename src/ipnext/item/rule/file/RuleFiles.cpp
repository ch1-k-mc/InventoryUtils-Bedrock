// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/item/rule/file/RuleFiles.h"

#include <algorithm>

#include "ipnext/Log.h"
#include "ipnext/item/rule/natives/NativeRules.h"
#include "ipnext/item/rule/parameter/NativeParameters.h"
#include "ipnext/parser/RuleParser.h"
#include "libipn/util/IndentedDataFileParser.h"
#include "platform/util/Messages.h"

namespace inventory_utils::ipnext::item::rule::file {

namespace {

std::string trim(std::string const& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string ordinalName(int n) {
    int m100 = n % 100, m10 = n % 10;
    if (m100 >= 11 && m100 <= 13) return std::to_string(n) + "th";
    if (m10 == 1) return std::to_string(n) + "st";
    if (m10 == 2) return std::to_string(n) + "nd";
    if (m10 == 3) return std::to_string(n) + "rd";
    return std::to_string(n) + "th";
}

}

void tellPlayer(std::string const& message) { platform::util::chat(message); }

SubRuleDefinition::SubRuleDefinition(std::string prefix_, std::string name_, std::vector<std::pair<std::string, std::string>> arguments_)
: prefix(std::move(prefix_)),
  name(std::move(name_)) {
    for (auto& [a, b] : arguments_) arguments.emplace_back(trim(a), trim(b));
}

RulePtr SubRuleDefinition::toRule() const {
    std::string identifier = prefix + name;
    RulePtr     rule;
    if (prefix == "@") rule = RuleFileRegister::getCustomRule(name, false);
    else if (prefix == "::") rule = RuleFileRegister::getNativeRule(name);
    if (!rule) throw NoSuchElementException("rule " + identifier + " is broken rule or does not exist");
    for (auto const& [param, arg] : arguments) {
        if (!rule->arguments().contains(param)) {
            Log::warn("Rule {} has no parameter {}", identifier, param);
            continue;
        }
        auto const* parameter = RuleFileRegister::getParameter(param);
        if (!parameter) throw std::runtime_error(identifier + " defined nonexistent parameter " + param);
        if (!rule->arguments().trySetArgument(*parameter, arg)) Log::warn("Failed to parse argument '{}' for parameter {}", arg, param);
    }
    auto missing = rule->arguments().missingParameters();
    if (!missing.empty()) {
        std::string joined;
        for (size_t i = 0; i < missing.size(); ++i) joined += (i ? ", " : "") + missing[i];
        throw MissingParameterException("required parameter is missing: " + joined);
    }
    return rule;
}

RulePtr RuleDefinition::createCustomRule() {
    if (mStatus == Status::INITIALIZING) {
        mStatus = Status::FAILED;
        throw SelfReferenceException("loop (?) detected");
    }
    if (mStatus == Status::LAZY) {
        mStatus = Status::INITIALIZING;
        std::vector<RulePtr> list;
        for (auto const& s : mSubRules) list.push_back(s.toRule());
        mRuleList = std::move(list);
        mStatus   = Status::SUCCESS;
    }
    if (!mRuleList) return nullptr;
    return std::make_shared<CustomRule>(*mRuleList);
}

void RuleFile::parseContent() {
    Log::trace("[-] Parsing file {}", fileName);
    auto data = libipn::util::IndentedDataFileParser::parse(preprocessRules(libipn::util::splitLines(mContent)), fileName);
    for (auto const& sub : data->subData) {
        try {
            auto def = std::make_shared<RuleDefinition>(parser::RuleParser::parseRuleDefinition(*sub));
            if (!rulesMap.contains(def->ruleName)) ruleOrder.push_back(def->ruleName);
            rulesMap[def->ruleName].push_back(def);
        } catch (parser::SyntaxErrorException const& e) {
            tellPlayer("Loading sort rules failed!");
            tellPlayer("Syntax error in '" + fileName + "' (" + sub->text + ")");
            tellPlayer("  > at: " + std::to_string(e.line) + ":" + std::to_string(e.pos) + " " + e.msg);
            Log::warn("Syntax error in '{}' ({})", fileName, sub->text);
            Log::warn("  > at: {}:{} {}", e.line, e.pos, e.msg);
        } catch (std::exception const& e) {
            Log::warn("Error in '{}' ({}): {}", fileName, sub->text, e.what());
        }
    }
    (void)mFromUserInput;
}

namespace {

std::set<std::string>                               gLoadedFileNames;
std::vector<std::shared_ptr<RuleFile>>              gRuleFiles;
std::map<std::string, std::shared_ptr<RuleDefinition>> gCachedRules;
std::vector<std::string>                            gNames;

std::shared_ptr<RuleDefinition> findUsableRule(std::string const& ruleName, std::vector<std::shared_ptr<RuleDefinition>> const& list, std::string const& fileName, bool fromUserInput) {
    int count = static_cast<int>(list.size());
    for (auto it = list.rbegin(); it != list.rend(); ++it, --count) {
        auto const& def = *it;
        try {
            def->createCustomRule();
            if (def->status() == RuleDefinition::Status::SUCCESS) return def;
            Log::error("interesting rule @{}#{} (at file {})", ruleName, count, fileName);
        } catch (std::exception const& e) {
            if (fromUserInput) {
                tellPlayer("Failed to process Rule '@" + ruleName + "'");
                tellPlayer(std::string("  > ") + e.what());
            }
            Log::warn("Error in {} '@{}' (at file {})", ordinalName(count), ruleName, fileName);
            Log::warn("  > {}", e.what());
        }
    }
    return nullptr;
}

std::shared_ptr<RuleDefinition> searchCustomRule(std::string const& ruleName, bool fromUserInput) {
    for (auto it = gRuleFiles.rbegin(); it != gRuleFiles.rend(); ++it) {
        auto& ruleFile = **it;
        auto  found    = ruleFile.rulesMap.find(ruleName);
        if (found == ruleFile.rulesMap.end()) continue;
        auto list = found->second;
        auto def  = findUsableRule(ruleName, list, ruleFile.fileName, fromUserInput);
        ruleFile.rulesMap.erase(ruleName);
        if (def) {
            gLoadedFileNames.insert(ruleFile.fileName);
            return def;
        }
    }
    return nullptr;
}

std::shared_ptr<RuleDefinition> searchAndPutCustomRule(std::string const& ruleName, bool fromUserInput) {
    auto def = searchCustomRule(ruleName, fromUserInput);
    if (!gCachedRules.contains(ruleName)) gCachedRules[ruleName] = def;
    std::erase_if(gRuleFiles, [](std::shared_ptr<RuleFile> const& f) { return f->rulesMap.empty(); });
    return def;
}

}

void RuleFileRegister::reloadRuleFiles(std::vector<std::shared_ptr<RuleFile>> ruleFiles, bool fromUserInput) {
    for (auto& f : ruleFiles) {
        if (fromUserInput) tellPlayer("Parsing custom rules file " + f->fileName);
        f->parseContent();
    }
    gLoadedFileNames.clear();
    gRuleFiles.clear();
    gCachedRules.clear();
    gNames.clear();
    gRuleFiles = std::move(ruleFiles);
    for (auto const& f : gRuleFiles) {
        for (auto const& name : f->ruleOrder) {
            if (std::find(gNames.begin(), gNames.end(), name) != gNames.end()) {
                if (fromUserInput) tellPlayer("Rule @" + name + " already defined in another file. Overriding with file " + f->fileName);
                Log::info("Rule @{} overrode by file {}", name, f->fileName);
            } else {
                gNames.push_back(name);
            }
        }
    }
    for (auto const& name : gNames) {
        if (!getCustomRule(name, fromUserInput)) {
            if (fromUserInput) tellPlayer("Failed to process Rule '@" + name + "'");
            Log::debug("rule @{} failed to parse", name);
        }
    }
}

RulePtr RuleFileRegister::getCustomRuleOrEmpty(std::string const& ruleName) {
    if (auto r = getCustomRule(ruleName, false)) return r;
    Log::warn("Rule @{} not found", ruleName);
    return EmptyRule::instance();
}

RulePtr RuleFileRegister::getCustomRule(std::string const& ruleName, bool fromUserInput) {
    std::shared_ptr<RuleDefinition> def;
    if (auto it = gCachedRules.find(ruleName); it != gCachedRules.end()) def = it->second;
    else def = searchAndPutCustomRule(ruleName, fromUserInput);
    return def ? def->createCustomRule() : nullptr;
}

RulePtr RuleFileRegister::getNativeRule(std::string const& ruleName) { return natives::NativeRules::create(ruleName); }

Parameter const* RuleFileRegister::getParameter(std::string const& parameterName) { return parameter::NativeParameters::find(parameterName); }

std::set<std::string> const& RuleFileRegister::loadedFileNames() { return gLoadedFileNames; }

namespace {

std::set<std::string> gCandidates;

std::string getIndent(std::string const& s) {
    std::string out;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '!') out += c;
    }
    return out;
}

std::string concatRule(std::string const& start, std::string const& rest) {
    bool blank = std::all_of(start.begin(), start.end(), [](unsigned char c) { return std::isspace(c); });
    return blank ? rest : start + " " + rest;
}

struct ScriptItem {
    bool                     isList = false;
    std::string              str;
    std::vector<std::string> list;
};

void explode(std::string const& start, std::vector<ScriptItem> const& script, size_t from, std::vector<std::string>& target) {
    if (from >= script.size()) return;
    auto const& head = script[from];
    if (!head.isList) {
        auto root = concatRule(start, head.str);
        if (from + 1 < script.size()) explode(root, script, from + 1, target);
        else target.push_back(root);
    } else {
        for (auto const& e : head.list) {
            auto root = concatRule(start, e);
            if (from + 1 < script.size()) explode(root, script, from + 1, target);
            else target.push_back(root);
        }
    }
}

std::vector<ScriptItem> collapseStrings(std::vector<ScriptItem> const& in) {
    std::vector<ScriptItem> res;
    std::string             acc;
    for (auto const& it : in) {
        if (!it.isList) {
            acc = acc + " " + it.str;
        } else {
            res.push_back({false, acc, {}});
            res.push_back(it);
            acc.clear();
        }
    }
    bool blank = std::all_of(acc.begin(), acc.end(), [](unsigned char c) { return std::isspace(c); });
    if (!blank) res.push_back({false, acc, {}});
    return res;
}

std::vector<std::string> processWildcards(std::string const& line) {
    if (line.find('*') == std::string::npos && line.find('?') == std::string::npos) return {line};
    std::string indent = getIndent(line);
    std::string body   = line.rfind(indent, 0) == 0 ? line.substr(indent.size()) : line;
    std::vector<ScriptItem> script;
    std::string             cur;
    auto                    flush = [&]() {
        std::string wc = cur;
        cur.clear();
        if (wc.find('*') != std::string::npos || wc.find('?') != std::string::npos) {
            std::string toExpand = wc.find(':') == std::string::npos ? "minecraft:" + wc : wc;
            ScriptItem  item{true, {}, {}};
            for (auto const& c : gCandidates) {
                if (wildcardMatches(toExpand, c)) item.list.push_back(c);
            }
            script.push_back(std::move(item));
        } else if (!std::all_of(wc.begin(), wc.end(), [](unsigned char c) { return std::isspace(c); })) {
            script.push_back({false, wc, {}});
        }
    };
    for (char c : body) {
        if (c == ' ' || c == '\t') flush();
        else cur += c;
    }
    flush();
    script = collapseStrings(script);
    std::vector<std::string> collect;
    explode("", script, 0, collect);
    std::vector<std::string> res;
    for (auto const& c : collect) res.push_back(indent + c);
    if (res.empty()) {
        std::string stripped;
        for (char c : line) {
            if (c != '*' && c != '?') stripped += c;
        }
        res.push_back(stripped);
    }
    return res;
}

}

void setWildcardCandidates(std::set<std::string> candidates) { gCandidates = std::move(candidates); }

bool wildcardMatches(std::string const& pattern, std::string const& text) {
    size_t p = 0, t = 0, star = std::string::npos, mark = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            mark = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++mark;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

std::vector<std::string> preprocessRules(std::vector<std::string> const& lines) {
    std::vector<std::string> res;
    for (auto const& l : lines) {
        auto e = processWildcards(l);
        res.insert(res.end(), e.begin(), e.end());
    }
    return res;
}

}
