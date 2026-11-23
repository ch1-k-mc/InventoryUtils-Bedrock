// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/libIPN (master @89a2efa, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "libipn/util/IndentedDataFileParser.h"

#include <algorithm>
#include <cctype>

#include "ipnext/Log.h"

namespace inventory_utils::libipn::util {

namespace {

bool isWs(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::string indentOf(std::string const& s) {
    auto it = std::find_if(s.begin(), s.end(), [](char c) { return !isWs(c); });
    return std::string(s.begin(), it);
}
bool hasIndent(std::string const& s) { return !s.empty() && isWs(s[0]); }
bool isCommentOrBlank(std::string const& s) {
    auto it = std::find_if(s.begin(), s.end(), [](char c) { return !isWs(c); });
    if (it == s.end()) return true;
    return std::string(it, s.end()).rfind("//", 0) == 0;
}

struct ErrorLine {
    int         lineNumber;
    std::string text;
};

class Parser {
public:
    Parser(std::vector<std::string> const& lines, std::string fileName, int maxDepth) : mFileName(std::move(fileName)), mMaxDepth(maxDepth) {
        bool dropping = true;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (isCommentOrBlank(lines[i])) continue;
            if (dropping && hasIndent(lines[i])) {
                mErrors.push_back({static_cast<int>(i + 1), "unexpected indent"});
                continue;
            }
            dropping = false;
            mFiltered.emplace_back(static_cast<int>(i + 1), lines[i]);
        }
    }

    std::shared_ptr<IndentedData> parse() {
        auto root = std::make_shared<IndentedData>();
        for (auto const& [n, t] : mFiltered) {
            auto d        = std::make_shared<IndentedData>();
            d->lineNumber = n;
            d->rawText    = t;
            d->text       = t;
            root->subData.push_back(d);
        }
        root->maxDepth = mMaxDepth;
        deep(*root);
        std::sort(mErrors.begin(), mErrors.end(), [](ErrorLine const& a, ErrorLine const& b) { return a.lineNumber < b.lineNumber; });
        for (auto const& e : mErrors) ipnext::Log::warn("Indent Error while parsing {}: {} at line {}", mFileName, e.text, e.lineNumber);
        return root;
    }

private:
    void deep(IndentedData& d) {
        if (d.maxDepth == 0) return;
        if (d.subData.empty()) return;
        trimFirstIndent(d);
        auto copy = d.subData;
        d.subData.clear();
        for (auto& data : copy) {
            if (hasIndent(data->text) && !d.subData.empty()) d.subData.back()->subData.push_back(data);
            else d.subData.push_back(data);
        }
        for (auto& sub : d.subData) {
            sub->maxDepth = d.maxDepth > 0 ? d.maxDepth - 1 : d.maxDepth;
            deep(*sub);
        }
    }

    void trimFirstIndent(IndentedData& d) {
        std::string indent = indentOf(d.subData.front()->text);
        std::erase_if(d.subData, [&](std::shared_ptr<IndentedData> const& data) {
            bool retain = data->text.rfind(indent, 0) == 0;
            if (retain) data->text = data->text.substr(indent.size());
            else mErrors.push_back({data->lineNumber, "indent mismatch"});
            return !retain;
        });
    }

    std::string                              mFileName;
    int                                      mMaxDepth;
    std::vector<std::pair<int, std::string>> mFiltered;
    std::vector<ErrorLine>                   mErrors;
};

}

std::vector<IndentedLine> IndentedData::lines() const {
    std::vector<IndentedLine> out{asLine()};
    auto                      sub = subLines();
    out.insert(out.end(), sub.begin(), sub.end());
    return out;
}

std::vector<IndentedLine> IndentedData::subLines() const {
    std::vector<IndentedLine> out;
    for (auto const& s : subData) {
        auto l = s->lines();
        out.insert(out.end(), l.begin(), l.end());
    }
    return out;
}

std::shared_ptr<IndentedData> IndentedDataFileParser::parse(std::vector<std::string> const& lines, std::string const& fileName, int maxDepth) {
    return Parser(lines, fileName, maxDepth).parse();
}

std::shared_ptr<IndentedData> IndentedDataFileParser::parse(std::string const& text, std::string const& fileName, int maxDepth) {
    return parse(splitLines(text), fileName, maxDepth);
}

std::vector<std::string> splitLines(std::string const& text) {
    std::vector<std::string> out;
    std::string              cur;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\r') {
            out.push_back(cur);
            cur.clear();
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
        } else if (c == '\n') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

}
