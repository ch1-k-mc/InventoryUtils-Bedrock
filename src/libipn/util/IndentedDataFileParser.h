// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/libIPN (master @89a2efa, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <memory>
#include <string>
#include <vector>

namespace inventory_utils::libipn::util {

struct IndentedLine {
    int         lineNumber = -1;
    std::string rawText;
    std::string text;
};

struct IndentedData {
    int                                        lineNumber = -1;
    std::string                                rawText;
    std::string                                text;
    std::vector<std::shared_ptr<IndentedData>> subData;
    int                                        maxDepth = -1;

    [[nodiscard]] IndentedLine              asLine() const { return {lineNumber, rawText, text}; }
    [[nodiscard]] std::vector<IndentedLine> lines() const;
    [[nodiscard]] std::vector<IndentedLine> subLines() const;
};

class IndentedDataFileParser {
public:
    static std::shared_ptr<IndentedData> parse(std::vector<std::string> const& lines, std::string const& fileName = "<unknown file>", int maxDepth = -1);
    static std::shared_ptr<IndentedData> parse(std::string const& text, std::string const& fileName = "<unknown file>", int maxDepth = -1);
};

std::vector<std::string> splitLines(std::string const& text);

}
