// SPDX-License-Identifier: AGPL-3.0-only
#include "platform/screen/ScreenLayout.h"

#include <string_view>
#include <vector>

#include "mc/client/gui/VisibilityFlag.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenView.h"

#include "nlohmann/json.hpp"

namespace inventory_utils::platform::screen {

namespace {

bool isVisible(UIControl const& c) { return c.mVisible != ui::VisibilityFlag::None && c.mIsVisibleInTree; }

GuiArea areaOf(UIControl const& c) {
    glm::vec2 pos  = c.mCachedPosition;
    glm::vec2 size = c.mSize;
    return {static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(size.x), static_cast<int>(size.y)};
}

UIControl* findByName(UIControl& root, std::string_view name, int maxNodes) {
    std::vector<UIControl*> queue{&root};
    size_t                  head = 0;
    int                     seen = 0;
    while (head < queue.size() && seen < maxNodes) {
        UIControl* c = queue[head++];
        ++seen;
        if (!isVisible(*c)) continue;
        if (c->mName.get() == name) return c;
        for (auto& child : c->mChildren.get()) {
            if (child) queue.push_back(child.get());
        }
    }
    return nullptr;
}

UIControl* findLargestPanel(UIControl& root, int screenW, int screenH, int maxNodes) {
    std::vector<UIControl*> queue{&root};
    size_t                  head = 0;
    int                     seen = 0;
    UIControl*              best = nullptr;
    int                     bestArea = 0;
    while (head < queue.size() && seen < maxNodes) {
        UIControl* c = queue[head++];
        ++seen;
        if (!isVisible(*c)) continue;
        GuiArea a = areaOf(*c);
        if (a.width >= 60 && a.height >= 60 && a.width <= screenW * 9 / 10 && a.height <= screenH * 9 / 10 && a.width * a.height > bestArea) {
            best     = c;
            bestArea = a.width * a.height;
        }
        for (auto& child : c->mChildren.get()) {
            if (child) queue.push_back(child.get());
        }
    }
    return best;
}

}

std::optional<GuiArea> getGuiArea(ScreenView& view, std::string const& screenName, int screenWidth, int screenHeight) {
    VisualTree* tree = view.mVisualTree.get();
    if (!tree) return std::nullopt;
    UIControl* root = tree->mRootControl.get();
    if (!root) return std::nullopt;
    UIControl* panel = nullptr;
    if (screenName == "inventory_screen" || screenName == "crafting_screen") {
        panel = findByName(*root, "player_inventory", 4000);
        if (!panel && screenName == "inventory_screen") return std::nullopt;
        if (!panel) panel = findByName(*root, "content_stack_panel", 4000);
    }
    if (!panel) panel = findLargestPanel(*root, screenWidth, screenHeight, 4000);
    if (!panel) return std::nullopt;
    GuiArea a = areaOf(*panel);
    if (a.width <= 0 || a.height <= 0) return std::nullopt;
    return a;
}

std::string dumpControlsJson(ScreenView& view, std::string const& filter, int maxNodes) {
    nlohmann::json out = nlohmann::json::array();
    VisualTree*    tree = view.mVisualTree.get();
    if (!tree || !tree->mRootControl.get()) return out.dump();
    struct Item {
        UIControl*  c;
        int         depth;
        std::string parent;
    };
    std::vector<Item> queue{{tree->mRootControl.get(), 0, ""}};
    size_t            head = 0;
    int               seen = 0;
    while (head < queue.size() && seen < maxNodes) {
        Item it = queue[head++];
        ++seen;
        if (!isVisible(*it.c)) continue;
        std::string name = it.c->mName.get();
        if (filter.empty() || name.find(filter) != std::string::npos) {
            GuiArea a = areaOf(*it.c);
            out.push_back({{"name", name}, {"parent", it.parent}, {"depth", it.depth}, {"x", a.left}, {"y", a.top}, {"w", a.width}, {"h", a.height}});
        }
        for (auto& child : it.c->mChildren.get()) {
            if (child) queue.push_back({child.get(), it.depth + 1, name});
        }
    }
    return out.dump();
}

namespace {

std::optional<std::pair<std::string, int>> namedSlot(std::string const& name, std::string const& parent) {
    if (name == "head_grid_item") return std::make_pair(std::string("armor_items"), 0);
    if (name == "chest_grid_item" && parent == "armor_grid") return std::make_pair(std::string("armor_items"), 1);
    if (name == "legs_grid_item") return std::make_pair(std::string("armor_items"), 2);
    if (name == "feet_grid_item") return std::make_pair(std::string("armor_items"), 3);
    if (name == "offhand_grid_item") return std::make_pair(std::string("offhand_items"), 0);
    if (name == "crafting_input_item_top_left") return std::make_pair(std::string("crafting_input_items"), 0);
    if (name == "crafting_input_item_top_right") return std::make_pair(std::string("crafting_input_items"), 1);
    if (name == "crafting_input_item_bot_left") return std::make_pair(std::string("crafting_input_items"), 2);
    if (name == "crafting_input_item_bot_right") return std::make_pair(std::string("crafting_input_items"), 3);
    if (name == "work_bench_output") return std::make_pair(std::string("crafting_output_items"), 0);
    return std::nullopt;
}

std::optional<std::string> gridCollection(std::string const& name, std::string const& parent) {
    if (name == "grid_item_for_inventory" || parent == "inventory_grid") return std::string("inventory_items");
    if (name == "grid_item_for_hotbar" || parent == "hotbar_grid") return std::string("hotbar_items");
    if (parent == "armor_grid" || name == "grid_item_for_recipe_book") return std::nullopt;
    if (name == "chest_grid_item") return std::string("container_items");
    return std::nullopt;
}

}

SlotRects collectSlotRects(ScreenView& view, int maxNodes) {
    SlotRects   out;
    VisualTree* tree = view.mVisualTree.get();
    if (!tree || !tree->mRootControl.get()) return out;
    struct Item {
        UIControl*  c;
        std::string parent;
    };
    std::vector<Item>          queue{{tree->mRootControl.get(), ""}};
    std::map<std::string, int> counters;
    size_t                     head = 0;
    int                        seen = 0;
    while (head < queue.size() && seen < maxNodes) {
        Item it = queue[head++];
        ++seen;
        if (!isVisible(*it.c)) continue;
        std::string name = it.c->mName.get();
        GuiArea     a    = areaOf(*it.c);
        if (auto named = namedSlot(name, it.parent)) {
            out[*named] = {a.left, a.top, a.width, a.height};
        } else if (auto grid = gridCollection(name, it.parent)) {
            int index                 = counters[*grid]++;
            out[{*grid, index}]       = {a.left, a.top, a.width, a.height};
            continue;
        }
        for (auto& child : it.c->mChildren.get()) {
            if (child) queue.push_back({child.get(), name});
        }
    }
    return out;
}

std::vector<SlotRect> collectNamedRects(ScreenView& view, std::string_view name, int maxNodes) {
    std::vector<SlotRect> out;
    VisualTree*           tree = view.mVisualTree.get();
    if (!tree || !tree->mRootControl.get()) return out;
    std::vector<UIControl*> queue{tree->mRootControl.get()};
    size_t                  head = 0;
    int                     seen = 0;
    while (head < queue.size() && seen < maxNodes) {
        UIControl* c = queue[head++];
        ++seen;
        if (!isVisible(*c)) continue;
        if (c->mName.get() == name) {
            GuiArea a = areaOf(*c);
            out.push_back({a.left, a.top, a.width, a.height});
        }
        for (auto& child : c->mChildren.get()) {
            if (child) queue.push_back(child.get());
        }
    }
    return out;
}

}
