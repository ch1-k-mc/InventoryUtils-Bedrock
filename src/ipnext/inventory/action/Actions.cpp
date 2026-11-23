// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/inventory/action/Actions.h"

#include <algorithm>
#include <random>
#include <set>
#include <stdexcept>

#include "ipnext/config/Configs.h"
#include "itemscroller/config/Configs.h"

namespace inventory_utils::ipnext::inventory::action {

using data::ItemBucket;
using data::ItemStat;

PostAction postActionFromName(std::string const& name) {
    static std::vector<std::string> const names{"NONE", "GROUP_IN_ROWS", "GROUP_IN_COLUMNS", "DISTRIBUTE_EVENLY", "SHUFFLE", "FILL_ONE", "REVERSE"};
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) return static_cast<PostAction>(i);
    }
    return PostAction::NONE;
}

namespace {

std::mt19937& rng() {
    static std::mt19937 r{std::random_device{}()};
    return r;
}

int randomInt(int from, int to) {
    if (to < from) throw std::invalid_argument("Random range is empty");
    return std::uniform_int_distribution<int>(from, to)(rng());
}

void restockFromStacks(ItemStack& self, std::vector<ItemStack*> const& source) {
    if (self.isEmpty() || self.isFull()) return;
    for (auto* s : source) {
        s->transferTo(self);
        if (self.isFull()) return;
    }
}

void moveTo(ItemStack& self, std::vector<ItemStack*> const& destination, bool skipEmpty) {
    if (self.isEmpty()) return;
    for (auto* d : destination) {
        if (skipEmpty && d->isEmpty()) continue;
        self.transferTo(*d);
        if (self.isEmpty()) return;
    }
}

void moveTo(ItemStack& self, std::vector<ItemStack*> const& destination) {
    moveTo(self, destination, true);
    moveTo(self, destination, false);
}

void refillIfNeeded(ItemStack& self, std::vector<ItemStack*> const& destination) {
    for (auto* target : destination) {
        int needs = target->itemType.maxCount() - target->count;
        self.transferNTo(*target, needs);
    }
}

bool containsType(std::vector<ItemType> const& types, ItemType const& probe) {
    return std::any_of(types.begin(), types.end(), [&](ItemType const& t) { return probe == t; });
}

void writeTo(std::vector<ItemStack> const& source, std::vector<ItemStack*> const& destination) {
    for (size_t i = 0; i < destination.size(); ++i) {
        ItemStack s       = i < source.size() ? source[i] : ItemStack();
        destination[i]->itemType = s.itemType;
        destination[i]->count    = s.count;
    }
}

std::vector<ItemStack> flatten(std::vector<std::pair<ItemType, std::vector<int>>> const& list, std::optional<size_t> resultSize) {
    std::vector<ItemStack> a;
    for (auto const& [type, counts] : list) {
        for (int c : counts) a.emplace_back(type, c);
    }
    if (resultSize && a.size() < *resultSize) a.resize(*resultSize);
    return a;
}

class BucketView : public item::rule::ItemBucketView {
public:
    explicit BucketView(ItemBucket const& b) : mBucket(b) {}
    int accumulateCount(ItemType const& element) const override { return mBucket.accumulateCount(element); }

private:
    ItemBucket const& mBucket;
};

int compareWithItemScrollerOptions(item::rule::Rule& rule, ItemType const& a, ItemType const& b) {
    namespace G = itemscroller::config::Generic;
    bool boxA = a.isShulker(), boxB = b.isShulker();
    if (G::SORT_SHULKER_BOXES_AT_END.getBooleanValue() && boxA != boxB) return boxA ? 1 : -1;
    bool bundleA = a.isBundle(), bundleB = b.isBundle();
    if (G::SORT_BUNDLES_AT_END.getBooleanValue() && bundleA != bundleB) return bundleA ? 1 : -1;
    if (boxA && boxB) {
        int flip = G::SORT_SHULKER_BOXES_INVERTED.getBooleanValue() ? -1 : 1;
        int ca = a.containedItemCount(), cb = b.containedItemCount();
        if (ca != cb) return (ca < cb ? -1 : 1) * flip;
    }
    if (bundleA && bundleB) {
        int flip = G::SORT_BUNDLES_INVERTED.getBooleanValue() ? -1 : 1;
        int ca = a.containedItemCount(), cb = b.containedItemCount();
        if (ca != cb) return (ca < cb ? -1 : 1) * flip;
    }
    return rule.compare(a, b);
}

std::vector<ItemStack> sortItems(std::vector<ItemStack> const& list, item::rule::Rule& rule) {
    std::vector<std::pair<int, ItemStack>> overStacked;
    std::vector<ItemStack>                 rest;
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].overstacked()) overStacked.emplace_back(static_cast<int>(i), list[i]);
    }
    for (auto const& s : list) {
        bool isOver = std::any_of(overStacked.begin(), overStacked.end(), [&](auto const& p) { return p.second == s; });
        if (!isOver) rest.push_back(s);
    }
    ItemBucket bucket = data::collect(rest);
    auto       types  = bucket.elementSet();
    {
        BucketView                     view(bucket);
        item::rule::CountSourceScope   scope(&view);
        std::stable_sort(types.begin(), types.end(), [&](ItemType const& a, ItemType const& b) { return compareWithItemScrollerOptions(rule, a, b) < 0; });
    }
    std::vector<std::pair<ItemType, std::vector<int>>> packed;
    for (auto const& t : types) packed.emplace_back(t, pack(bucket.count(t), t.maxCount()));
    auto sorted = flatten(packed, list.size());
    for (auto const& [i, s] : overStacked) sorted.insert(sorted.begin() + std::min<size_t>(i, sorted.size()), s);
    return sorted;
}

std::vector<ItemStack> applyPostAction(std::vector<ItemStack> const& list, PostAction postAction, bool isRectangular, int width, int height) {
    switch (postAction) {
    case PostAction::NONE:
        return list;
    case PostAction::GROUP_IN_ROWS:
        return isRectangular ? PostActions::groupInRows(list, width, height) : list;
    case PostAction::GROUP_IN_COLUMNS:
        return isRectangular ? PostActions::groupInColumns(list, width, height) : list;
    case PostAction::DISTRIBUTE_EVENLY:
        return spreadSlot(list);
    case PostAction::SHUFFLE: {
        auto r = spreadItemCount(spreadSlot(list));
        std::shuffle(r.begin(), r.end(), rng());
        return r;
    }
    case PostAction::FILL_ONE:
        return fillOne(spreadSlot(list));
    case PostAction::REVERSE: {
        auto r = list;
        std::reverse(r.begin(), r.end());
        return r;
    }
    }
    return list;
}

struct Constraint {
    int min, max;
};

std::vector<int> randfixedsum(std::vector<Constraint> const& nums, int sum) {
    int totalMin = 0, totalMax = 0;
    for (auto const& c : nums) totalMin += c.min, totalMax += c.max;
    if (!(totalMin <= sum && totalMax >= sum)) throw std::invalid_argument("impossible sum");
    if (nums.empty()) return {};
    if (nums.size() == 1) return {sum};
    std::vector<Constraint> a(nums.begin(), nums.begin() + static_cast<long>(nums.size() / 2));
    std::vector<Constraint> b(nums.begin() + static_cast<long>(a.size()), nums.end());
    int aMinT = 0, aMaxT = 0, bMinT = 0, bMaxT = 0;
    for (auto const& c : a) aMinT += c.min, aMaxT += c.max;
    for (auto const& c : b) bMinT += c.min, bMaxT += c.max;
    int aMinSum = std::max(sum - bMaxT, aMinT);
    int aMaxSum = std::min(sum - bMinT, aMaxT);
    int aSum    = randomInt(aMinSum, aMaxSum);
    auto ra     = randfixedsum(a, aSum);
    auto rb     = randfixedsum(b, sum - aSum);
    ra.insert(ra.end(), rb.begin(), rb.end());
    return ra;
}

template <class F>
std::vector<ItemStack> writeEachItemType(std::vector<ItemStack> const& list, F itemCounts) {
    ItemStat               stat(list);
    std::vector<ItemStack> result(list.size());
    for (auto const& entry : stat.groupEntries()) {
        auto   counts = itemCounts(entry);
        size_t n      = std::min(counts.size(), entry.slotIndices.size());
        for (size_t i = 0; i < n; ++i) result[entry.slotIndices[i]] = ItemStack(entry.itemType, counts[i]);
    }
    return result;
}

using Pairs = std::vector<std::pair<ItemType, int>>;

class ColumnsCandidate {
public:
    struct Cell {
        int  room, rowIndex, columnIndex;
        bool occupied = false;
    };

    ColumnsCandidate(Pairs const& pairs, std::vector<int> columnWidths, int height) : mPairs(pairs), mColumnWidths(std::move(columnWidths)), mHeight(height) {
        for (int w : mColumnWidths) mWidth += w;
        for (size_t ci = 0; ci < mColumnWidths.size(); ++ci) {
            for (int ri = 0; ri < height; ++ri) mCells.push_back({mColumnWidths[ci], ri, static_cast<int>(ci)});
        }
        succeeded = true;
        for (size_t i = 0; i < mPairs.size(); ++i) {
            if (!addCellsForIndex(i)) {
                succeeded = false;
                break;
            }
        }
    }

    bool succeeded    = false;
    int  brokenGroups = 0;

    [[nodiscard]] std::vector<std::pair<ItemType, std::vector<int>>> apply() const {
        std::vector<std::pair<ItemType, std::vector<int>>> out;
        for (size_t i = 0; i < mPairs.size(); ++i) {
            std::vector<int> indices;
            for (int c : mEachCells[i]) {
                auto const& cell      = mCells[c];
                int         slotX     = 0;
                for (int k = 0; k < cell.columnIndex; ++k) slotX += mColumnWidths[k];
                int slotIndex = cell.rowIndex * mWidth + slotX;
                for (int r = 0; r < cell.room; ++r) indices.push_back(slotIndex + r);
            }
            out.emplace_back(mPairs[i].first, std::move(indices));
        }
        return out;
    }

private:
    bool addCellsForIndex(size_t index) {
        auto result = findCellsForRoom(mPairs[index].second);
        if (result.empty()) return false;
        for (int c : result) mCells[c].occupied = true;
        mEachCells.push_back(result);
        if (!connected(result)) ++brokenGroups;
        return true;
    }

    std::vector<int> findCellsForRoom(int slotCount) {
        if (!findEmptyCell()) return {};
        if (!mAllowBroken) {
            int initColumnIndex = mCells[mCellIndex].columnIndex;
            int totalRoom = 0, neededCells = 0;
            for (size_t i = mCellIndex; i < mCells.size(); ++i) {
                totalRoom += mCells[i].room;
                ++neededCells;
                if (totalRoom >= slotCount) {
                    if (neededCells > mHeight || mCells[i].columnIndex == initColumnIndex) {
                        std::vector<int> r;
                        for (int k = 0; k < neededCells; ++k) r.push_back(static_cast<int>(mCellIndex) + k);
                        return r;
                    }
                    mCellIndex = static_cast<size_t>((initColumnIndex + 1) * mHeight);
                    break;
                }
            }
        }
        std::vector<int> result;
        int              remaining = slotCount;
        while (remaining > 0) {
            if (!findEmptyCell()) return {};
            auto& cell = mCells[mCellIndex];
            remaining -= cell.room;
            result.push_back(static_cast<int>(mCellIndex));
            cell.occupied = true;
        }
        return result;
    }

    bool findEmptyCell() {
        while (mCellIndex < mCells.size()) {
            if (!mCells[mCellIndex].occupied) return true;
            ++mCellIndex;
        }
        if (mAllowBroken) return false;
        mAllowBroken = true;
        mCellIndex   = 0;
        return findEmptyCell();
    }

    [[nodiscard]] bool connected(std::vector<int> const& cells) const {
        if (cells.empty()) return true;
        std::set<std::pair<int, int>> active;
        for (int c : cells) active.insert({mCells[c].rowIndex, mCells[c].columnIndex});
        std::set<std::pair<int, int>> queue{*active.begin()};
        while (!queue.empty()) {
            auto p = *queue.begin();
            queue.erase(queue.begin());
            if (active.erase(p)) {
                queue.insert({p.first - 1, p.second});
                queue.insert({p.first + 1, p.second});
                queue.insert({p.first, p.second - 1});
                queue.insert({p.first, p.second + 1});
            }
        }
        return active.empty();
    }

    Pairs                         mPairs;
    std::vector<int>              mColumnWidths;
    int                           mHeight;
    int                           mWidth = 0;
    std::vector<Cell>             mCells;
    size_t                        mCellIndex   = 0;
    bool                          mAllowBroken = false;
    std::vector<std::vector<int>> mEachCells;
};

std::optional<std::vector<std::pair<ItemType, std::vector<int>>>> groupInColumnsCalc(Pairs const& pairs, int width, int height) {
    int minRows = static_cast<int>(pairs.size());
    if (minRows == 0) return std::nullopt;
    std::vector<ColumnsCandidate> list;
    for (int columnsCount = 1; columnsCount <= width; ++columnsCount) {
        if (minRows > height * columnsCount) continue;
        ColumnsCandidate cc(pairs, distribute(width, columnsCount), height);
        if (cc.succeeded) {
            if (cc.brokenGroups == 0) return cc.apply();
            list.push_back(std::move(cc));
        }
    }
    if (list.empty()) return std::nullopt;
    auto best = std::min_element(list.begin(), list.end(), [](auto const& a, auto const& b) { return a.brokenGroups < b.brokenGroups; });
    return best->apply();
}

std::vector<ItemStack> group(std::vector<ItemStack> const& slots, int width, int height, bool inRows) {
    if (slots.empty()) return slots;
    if (width * height != static_cast<int>(slots.size())) throw std::runtime_error("Area is not rectangular!");
    std::vector<std::pair<ItemType, std::vector<ItemStack>>> groups;
    for (auto const& s : slots) {
        if (s.isEmpty()) continue;
        auto it = std::find_if(groups.begin(), groups.end(), [&](auto const& g) { return s.itemType == g.first; });
        if (it == groups.end()) groups.push_back({s.itemType, {s}});
        else it->second.push_back(s);
    }
    Pairs pairs;
    for (auto const& [t, list] : groups) pairs.emplace_back(t, static_cast<int>(list.size()));
    std::optional<std::vector<std::pair<ItemType, std::vector<int>>>> result;
    if (!inRows) {
        result = groupInColumnsCalc(pairs, height, width);
        if (result) {
            for (auto& [t, indices] : *result) {
                for (auto& idx : indices) idx = (idx % height) * width + (idx / height);
            }
        }
    } else {
        result = groupInColumnsCalc(pairs, width, height);
    }
    if (!result) return slots;
    std::vector<ItemStack> ans(static_cast<size_t>(width * height));
    for (auto const& [type, slotIndices] : *result) {
        auto it = std::find_if(groups.begin(), groups.end(), [&](auto const& g) { return type == g.first; });
        if (it == groups.end()) continue;
        for (size_t index = 0; index < it->second.size() && index < slotIndices.size(); ++index) ans[slotIndices[index]] = it->second[index];
    }
    return ans;
}

}

void restockFrom(SubTracker& self, SubTracker& another) {
    auto anotherSlots = another.slots();
    for (auto* s : self.slots()) restockFromStacks(*s, anotherSlots);
}

void moveAllTo(SubTracker& self, SubTracker& another, bool skipEmpty) {
    auto anotherSlots = another.slots();
    for (auto* s : self.slots()) moveTo(*s, anotherSlots, skipEmpty);
}

void moveAllTo(SubTracker& self, SubTracker& another) {
    auto anotherSlots = another.slots();
    for (auto* s : self.slots()) moveTo(*s, anotherSlots);
}

void moveMatchTo(SubTracker& self, SubTracker& another) {
    auto anotherSlots = another.slots();
    if (config::ModSettings::IGNORE_DURABILITY.getBooleanValue()) {
        auto types = data::itemTypes(data::derefCopy(anotherSlots), true);
        for (auto* s : self.slots()) {
            if (containsType(types, s->itemType.copy(true))) moveTo(*s, anotherSlots);
        }
    } else {
        auto types = data::itemTypes(data::derefCopy(anotherSlots));
        for (auto* s : self.slots()) {
            if (containsType(types, s->itemType)) moveTo(*s, anotherSlots);
        }
    }
}

void moveFocusMatchTo(SubTracker& self, SubTracker& another, ItemType const& focusType) {
    auto anotherSlots = another.slots();
    auto moveType     = config::ModSettings::IGNORE_DURABILITY.getBooleanValue() ? focusType.copy(true) : focusType;
    for (auto* s : self.slots()) {
        if (moveType == s->itemType) moveTo(*s, anotherSlots);
    }
}

void refillStacksTo(SubTracker& self, SubTracker& another) {
    auto                    anotherSlots = another.slots();
    std::vector<ItemType>   anotherTypes;
    std::vector<ItemStack*> needRefill;
    for (auto* s : anotherSlots) {
        if (!s->isEmpty() || !s->isFull()) {
            if (!containsType(anotherTypes, s->itemType)) anotherTypes.push_back(s->itemType);
            needRefill.push_back(s);
        }
    }
    for (auto* s : self.slots()) {
        if (!containsType(anotherTypes, s->itemType)) continue;
        std::vector<ItemStack*> targets;
        for (auto* t : needRefill) {
            if (t->itemType == s->itemType && !t->isEmpty() && !t->isFull()) targets.push_back(t);
        }
        refillIfNeeded(*s, targets);
    }
}

void moveSingle(SubTracker& self, SubTracker& another) {
    auto slots = self.slots();
    if (slots.empty()) return;
    ItemStack& source = *slots[0];
    if (source.itemType.isEmpty()) return;
    auto       itemType     = source.itemType;
    auto       anotherSlots = another.slots();
    ItemStack* target       = nullptr;
    for (auto* s : anotherSlots) {
        if (s->itemType == itemType && !s->isEmpty() && !s->isFull()) {
            target = s;
            break;
        }
    }
    if (!target) {
        for (auto* s : anotherSlots) {
            if (s->isEmpty()) {
                target = s;
                break;
            }
        }
    }
    if (target) source.transferOneTo(*target);
}

void moveMatchCraftingTo(SubTracker& self, SubTracker& crafting) {
    auto&            main = crafting.mainTracker();
    std::vector<int> nonEmpty;
    for (auto const& [i, s] : crafting.indexedSlots()) {
        if (!s->isEmpty()) nonEmpty.push_back(i);
    }
    auto nonEmptyTracker = main.subTracker(nonEmpty);
    moveMatchTo(self, nonEmptyTracker);
    auto slots = nonEmptyTracker.slots();
    writeTo(distributeMonotonic(data::derefCopy(slots)), slots);
}

void sort(SubTracker& self, item::rule::Rule& sortingRule, PostAction postAction, bool isRectangular, int width, int height) {
    auto slots = self.slots();
    auto list  = data::derefCopy(slots);
    writeTo(applyPostAction(sortItems(list, sortingRule), postAction, isRectangular, width, height), slots);
}

std::vector<int> distribute(int sum, int listSize) {
    std::vector<int> out;
    if (listSize <= 0) return out;
    for (int i = 0; i < listSize; ++i) out.push_back(sum * (i + 1) / listSize - sum * i / listSize);
    return out;
}

std::vector<int> distributeMonotonic(int sum, int listSize) {
    std::vector<int> out;
    if (listSize <= 0) return out;
    int big = sum % listSize;
    for (int i = 0; i < big; ++i) out.push_back(sum / listSize + 1);
    for (int i = big; i < listSize; ++i) out.push_back(sum / listSize);
    return out;
}

std::vector<int> fillOne(int sum, int max, int listSize) {
    if (listSize * max == sum) return std::vector<int>(static_cast<size_t>(listSize), max);
    std::vector<int> out(static_cast<size_t>((sum - listSize) / (max - 1)), max);
    out.push_back((sum - listSize) % (max - 1) + 1);
    while (static_cast<int>(out.size()) < listSize) out.push_back(1);
    return out;
}

std::vector<int> pack(int sum, int max) {
    std::vector<int> out(static_cast<size_t>(sum / max), max);
    if (sum % max != 0) out.push_back(sum % max);
    return out;
}

std::vector<ItemStack> distributeMonotonic(std::vector<ItemStack> const& list) {
    return writeEachItemType(list, [](ItemStat::GroupEntry const& e) { return distributeMonotonic(e.itemCount, e.slotCount); });
}

std::vector<ItemStack> spreadSlot(std::vector<ItemStack> const& list, int emptySlot) {
    ItemStat stat(list);
    int      resultSlotCount = std::clamp(static_cast<int>(list.size()) - emptySlot, stat.totalMinSlotCount(), std::max(stat.totalMinSlotCount(), stat.totalMaxSlotCount()));
    auto const&             entries = stat.groupEntries();
    std::vector<Constraint> constraints;
    for (auto const& e : entries) constraints.push_back({e.minSlotCount(), e.itemCount});
    auto slotCounts = randfixedsum(constraints, resultSlotCount);
    std::vector<std::pair<ItemType, std::vector<int>>> each;
    for (size_t i = 0; i < entries.size(); ++i) each.emplace_back(entries[i].itemType, distributeMonotonic(entries[i].itemCount, slotCounts[i]));
    return flatten(each, list.size());
}

std::vector<ItemStack> spreadItemCount(std::vector<ItemStack> const& list) {
    return writeEachItemType(list, [](ItemStat::GroupEntry const& e) {
        std::vector<Constraint> cs(static_cast<size_t>(e.slotCount), Constraint{1, e.itemType.maxCount()});
        return randfixedsum(cs, e.itemCount);
    });
}

std::vector<ItemStack> fillOne(std::vector<ItemStack> const& list) {
    return writeEachItemType(list, [](ItemStat::GroupEntry const& e) { return fillOne(e.itemCount, e.itemType.maxCount(), e.slotCount); });
}

namespace PostActions {
std::vector<ItemStack> groupInRows(std::vector<ItemStack> const& slots, int width, int height) { return group(slots, width, height, true); }
std::vector<ItemStack> groupInColumns(std::vector<ItemStack> const& slots, int width, int height) { return group(slots, width, height, false); }
}

}
