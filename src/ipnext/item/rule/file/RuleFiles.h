// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ipnext/item/rule/Rule.h"

namespace inventory_utils::ipnext::item::rule::file {

class SelfReferenceException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class MissingParameterException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class NoSuchElementException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class SubRuleDefinition {
public:
    SubRuleDefinition(std::string prefix, std::string name, std::vector<std::pair<std::string, std::string>> arguments);
    [[nodiscard]] RulePtr toRule() const;

    std::string                                      prefix;
    std::string                                      name;
    std::vector<std::pair<std::string, std::string>> arguments;
};

class RuleDefinition {
public:
    enum class Status { LAZY, INITIALIZING, SUCCESS, FAILED };

    RuleDefinition(std::string ruleName, std::vector<SubRuleDefinition> subRules) : ruleName(std::move(ruleName)), mSubRules(std::move(subRules)) {}

    [[nodiscard]] RulePtr createCustomRule();
    [[nodiscard]] Status  status() const { return mStatus; }

    std::string ruleName;

private:
    std::vector<SubRuleDefinition>      mSubRules;
    Status                              mStatus = Status::LAZY;
    std::optional<std::vector<RulePtr>> mRuleList;
};

class RuleFile {
public:
    RuleFile(std::string fileName, std::string content, bool fromUserInput) : fileName(std::move(fileName)), mContent(std::move(content)), mFromUserInput(fromUserInput) {}
    void parseContent();

    std::string                                                       fileName;
    std::map<std::string, std::vector<std::shared_ptr<RuleDefinition>>> rulesMap;
    std::vector<std::string>                                          ruleOrder;

private:
    std::string mContent;
    bool        mFromUserInput;
};

class RuleFileRegister {
public:
    static void reloadRuleFiles(std::vector<std::shared_ptr<RuleFile>> ruleFiles, bool fromUserInput);
    static RulePtr getCustomRuleOrEmpty(std::string const& ruleName);
    static RulePtr getCustomRule(std::string const& ruleName, bool fromUserInput);
    static RulePtr getNativeRule(std::string const& ruleName);
    static Parameter const* getParameter(std::string const& parameterName);
    static std::set<std::string> const& loadedFileNames();
};

std::vector<std::string> preprocessRules(std::vector<std::string> const& lines);
void setWildcardCandidates(std::set<std::string> candidates);
bool wildcardMatches(std::string const& pattern, std::string const& text);

void tellPlayer(std::string const& message);

}
