// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "ipnext/item/rule/Rule.h"

namespace inventory_utils::libipn::util {
struct IndentedData;
}

namespace inventory_utils::ipnext::item::rule::file {
class RuleDefinition;
class SubRuleDefinition;
}

namespace inventory_utils::ipnext::parser {

class SyntaxErrorException : public std::runtime_error {
public:
    SyntaxErrorException(int line_, int pos_, std::string msg_) : std::runtime_error(msg_), line(line_), pos(pos_), msg(std::move(msg_)) {}
    int         line;
    int         pos;
    std::string msg;
};

struct RuleParser {
    static item::rule::file::RuleDefinition parseRuleDefinition(libipn::util::IndentedData const& data);
    static std::vector<item::rule::file::SubRuleDefinition> parseSubRule(std::string const& content);
    static item::rule::file::RuleDefinition                 parseRuleDefinition(std::string const& content);
};

struct TemporaryRuleParser {
    static item::rule::RulePtr parse(std::string const& content);
    static void                onReload();
};

}
