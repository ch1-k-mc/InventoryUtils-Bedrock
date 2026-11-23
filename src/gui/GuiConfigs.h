// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#pragma once

#include <string>
#include <vector>

#include "gui/ConfigGuiBase.h"

namespace inventory_utils::gui {

class GuiConfigs : public ConfigGuiBase {
public:
    GuiConfigs();

    void        initGui(Ui2d& ui) override;
    std::string title() const override;
    char const* name() const override { return "GuiConfigs"; }
    bool        mouseScrolled(float mx, float my, int delta) override;
    void        drawContents(Ui2d& ui, float mx, float my) override;
    void        drawOverlay(Ui2d& ui, float mx, float my) override;
    bool        mouseClicked(float mx, float my, int button) override;

    [[nodiscard]] static int  selectedIndex();
    static void               setMod(int mod);
    [[nodiscard]] static char const* modId();
    [[nodiscard]] std::string selectedName() const;
    [[nodiscard]] static std::string const& searchText();

private:
    struct Section {
        std::string name;
        std::string label;
        std::vector<malilib::config::IConfigBase*> options;
    };
    struct Entry {
        enum class Kind { GAP, CATEGORY, OPTION, BUTTON } kind;
        int                           height = 0;
        std::string                   text;
        malilib::config::IConfigBase* option = nullptr;
        std::string                   type;
        std::string                   key;
        std::string                   label;
        std::string                   desc;
        bool                          ipn = false;
        float                         top    = 0;
    };
    struct Anchor {
        std::string text;
        float       scrollTo = 0;
        float       x = 0, y = 0, w = 0;
    };

    void buildSections();
    void buildEntries();
    void addEntryWidgets(Ui2d& ui, Entry const& e, float x, float y, float w, bool active);
    [[nodiscard]] bool matches(Entry const& e) const;
    [[nodiscard]] std::string valueText(Entry const& e) const;

    std::vector<Section> mSections;
    std::vector<Entry>   mEntries;
    std::vector<Anchor>  mAnchors;
    float                mContentHeight = 0;
    float                mListLeft = 0, mListTop = 0, mListRight = 0, mListBottom = 0, mContentTop = 0;
    TextField*           mSearch = nullptr;
    std::vector<std::pair<float, float>> mDimmed;
};

}
