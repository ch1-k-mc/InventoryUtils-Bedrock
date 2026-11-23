// SPDX-License-Identifier: AGPL-3.0-or-later
// Based on Inventory-Profiles-Next-Mod/IPN (all-in-one @bb3acc6, AGPL-3.0-or-later), ported to C++ and modified.
// Copyright (c) 2019-2020 jsnimda, (c) 2021-2022 Plamen K. Kosseff and contributors
#include "ipnext/inventory/sandbox/diffcalculator/DiffCalculator.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <functional>
#include <map>
#include <optional>
#include <unordered_set>
#include <vector>

#include "ipnext/config/Configs.h"
#include "ipnext/Log.h"

namespace inventory_utils::ipnext::inventory::sandbox::diffcalculator {

using data::ItemBucket;
using item::ItemStack;

namespace util {

int calcRank(int n, int g) {
    if (n == g) return 0;
    if (n + 1 == g) return 10;
    if (g == 0) return 11;
    if (n / 2 == g) return 12;
    if (n < g) return 20;
    if (g == 1) return 30;
    if (n / 2 + 1 == g) return 31;
    if (n / 4 == g) return 32;
    return 40;
}

int clickCountLowerBound(int n, int g) {
    switch (calcRank(n, g) / 10) {
    case 0:
        return 0;
    case 1:
    case 2:
        return 1;
    default:
        return 2;
    }
}

static int clickCountUpperBoundNGreaterThanG(int n, int g);

int clickCountUpperBound(int n, int g) {
    switch (calcRank(n, g) / 10) {
    case 0:
        return 0;
    case 1:
        return 1;
    case 2:
        return g - n;
    case 3:
        return 2;
    default:
        return clickCountUpperBoundNGreaterThanG(n, g);
    }
}

static int clickCountUpperBoundNGreaterThanG(int n, int g) { return 1 + std::min(clickCountUpperBound(0, g), clickCountUpperBound(n / 2, g)); }

int clickCountSingleSlotToLess(int from, int to) {
    if (from < to) throw std::runtime_error("from < to");
    if (from == to) return 0;
    int r = from / 2;
    return 1 + (r <= to ? to - r : to);
}

bool canRight(int from, int to) {
    if (from < to) throw std::runtime_error("from < to");
    return from / 2 <= to;
}

}

using namespace util;

namespace {

constexpr int MAX_CLICK_BOUND = 100000;

class DiffCalculatorInstance {
public:
    DiffCalculatorInstance(ContainerSandbox& sandbox, ItemTracker goal) : mSandbox(sandbox), mGoal(std::move(goal)) {
        if (now().slots.size() != mGoal.slots.size()) throw std::runtime_error("Unequal sandbox and goal slot size");
        if (!(data::collect(now()) == data::collect(mGoal))) throw std::runtime_error("Unequal sandbox and goal item counts");
        if (!mGoal.thrownItems.containsAll(now().thrownItems)) throw std::runtime_error("Impossible. Thrown items cannot be reverted");
    }
    virtual ~DiffCalculatorInstance() = default;
    virtual void run()                = 0;

protected:
    [[nodiscard]] ItemTracker const& now() const { return mSandbox.items(); }
    [[nodiscard]] ItemTracker const& goalTracker() const { return mGoal; }
    [[nodiscard]] ItemStack const&   cursorNow() const { return now().cursor; }
    [[nodiscard]] ItemStack const&   cursorGoal() const { return mGoal.cursor; }

    void increaseLoopCount() {
        int count = std::max(++mLoopCounter, mSandbox.clickCount());
        if (count > MAX_CLICK_BOUND) throw std::runtime_error("Infinity loop detected");
    }

    struct Slot {
        DiffCalculatorInstance* self;
        int                     slotIndex;
        [[nodiscard]] ItemStack const& now() const { return self->now().slots[slotIndex]; }
        [[nodiscard]] ItemStack const& goal() const { return self->mGoal.slots[slotIndex]; }
        [[nodiscard]] bool             equals() const { return now() == goal(); }
        [[nodiscard]] bool             equalsType() const { return now().itemType == goal().itemType; }
        [[nodiscard]] bool             bothEmpty() const { return now().isEmpty() && goal().isEmpty(); }
        [[nodiscard]] int              n() const { return now().count; }
        [[nodiscard]] int              g() const { return goal().count; }
        void                           leftClick() const { self->mSandbox.leftClick(slotIndex); }
        void                           rightClick() const { self->mSandbox.rightClick(slotIndex); }
        void                           repeatRightClick(int times) const {
            for (int i = 0; i < times; ++i) rightClick();
        }
    };

    [[nodiscard]] Slot slot(int index) { return {this, index}; }
    [[nodiscard]] int  size() const { return static_cast<int>(now().slots.size()); }

    template <class P>
    std::vector<Slot> filtered(P predicate) {
        std::vector<Slot> out;
        for (int i = 0; i < size(); ++i) {
            Slot s{this, i};
            if (predicate(s)) out.push_back(s);
        }
        return out;
    }

    ContainerSandbox& mSandbox;
    ItemTracker       mGoal;

private:
    int mLoopCounter = 0;
};

template <class T, class F>
std::optional<T> minBy(std::vector<T> const& list, F key) {
    std::optional<T> best;
    long long        bestKey = 0;
    for (auto const& e : list) {
        long long k = key(e);
        if (!best || k < bestKey) {
            best    = e;
            bestKey = k;
        }
    }
    return best;
}

template <class T, class F>
std::optional<T> maxBy(std::vector<T> const& list, F key) {
    std::optional<T> best;
    long long        bestKey = 0;
    for (auto const& e : list) {
        long long k = key(e);
        if (!best || k > bestKey) {
            best    = e;
            bestKey = k;
        }
    }
    return best;
}

class SimpleDiffCalculatorInstance : public DiffCalculatorInstance {
public:
    SimpleDiffCalculatorInstance(ContainerSandbox& sandbox, ItemTracker goal) : DiffCalculatorInstance(sandbox, std::move(goal)) {
        if (!cursorGoal().isEmpty()) throw std::runtime_error("non empty goal cursor is not supported");
        mToBeThrown = goalTracker().thrownItems - now().thrownItems;
        for (auto& s : filtered([](Slot const& s) { return !s.equals(); })) mNonEquals.push_back(s);
    }

    void run() override {
        while (!mNonEquals.empty() || !(now() == goalTracker())) {
            if (shouldStop()) return;
            increaseLoopCount();
            if (cursorNow().isEmpty()) grabAnything();
            else handleCursor();
            std::erase_if(mNonEquals, [](Slot const& s) { return s.equals(); });
        }
    }

protected:
    bool mUntilEqualsTypeOnly = false;

    [[nodiscard]] bool shouldStop() {
        return mUntilEqualsTypeOnly && filtered([](Slot const& s) { return !s.equalsType() && !s.now().isEmpty(); }).empty() && mToBeThrown.isEmpty();
    }

    [[nodiscard]] bool canThrowAll(Slot const& s) const { return !s.now().isEmpty() && mToBeThrown.contains(s.now()); }
    [[nodiscard]] bool canThrowType(Slot const& s) const { return !s.now().isEmpty() && mToBeThrown.contains(s.now().itemType); }
    [[nodiscard]] int  toBeThrownCount(Slot const& s) const { return std::min(mToBeThrown.count(s.now().itemType), s.now().count); }
    [[nodiscard]] bool canThrowSplit(Slot const& s) const { return !s.now().isEmpty() && (s.n() - s.n() / 2) <= toBeThrownCount(s); }
    [[nodiscard]] int  canThrowCount(Slot const& s) const { return canThrowAll(s) ? s.n() : s.n() - s.n() / 2; }
    [[nodiscard]] static bool nowLessThanEqualGoal(Slot const& s) { return s.equalsType() && s.n() <= s.g(); }
    [[nodiscard]] static bool nowMoreThanGoal(Slot const& s) { return s.equalsType() && s.n() > s.g(); }
    [[nodiscard]] static int  nowGoalCount(Slot const& s) { return s.equalsType() ? s.n() : 0; }
    [[nodiscard]] static int  nowGoalRemaining(Slot const& s) { return s.g() - nowGoalCount(s); }
    [[nodiscard]] int         withCursorNowCount(Slot const& s) const {
        return cursorNow().itemType == s.goal().itemType ? std::min(nowGoalCount(s) + cursorNow().count, s.goal().itemType.maxCount()) : 0;
    }

    template <class P>
    static std::vector<Slot> filteredOf(std::vector<Slot> const& list, P predicate, bool skipEquals = true, bool skipEmptyNow = false, bool skipEmptyGoal = false) {
        std::vector<Slot> out;
        for (auto const& s : list) {
            bool skip = (skipEquals && s.equals()) || (skipEmptyNow && s.now().isEmpty()) || (skipEmptyGoal && s.goal().isEmpty());
            if (!skip && predicate(s)) out.push_back(s);
        }
        return out;
    }

    void grabAnything() {
        if (!mToBeThrown.isEmpty()) return grabToBeThrown();
        grabOtherThing();
    }

    void grabToBeThrown() {
        auto list = filteredOf(mNonEquals, [&](Slot const& s) { return (canThrowAll(s) || canThrowSplit(s)) && !nowLessThanEqualGoal(s); });
        if (list.empty()) list = filteredOf(mNonEquals, [&](Slot const& s) { return canThrowAll(s) || canThrowSplit(s); });
        if (auto s = maxBy(list, [&](Slot const& x) { return canThrowCount(x); })) {
            if (canThrowAll(*s)) s->leftClick();
            else s->rightClick();
            return;
        }
        std::vector<Slot> types;
        for (auto const& s : mNonEquals) {
            if (canThrowType(s)) types.push_back(s);
        }
        if (auto s = minBy(types, [](Slot const& x) { return x.n(); })) return s->rightClick();
        throw std::runtime_error("should not reach here");
    }

    void grabOtherThing() {
        auto list = filteredOf(mNonEquals, [](Slot const& s) { return !s.equalsType(); }, true, true);
        if (auto s = minBy(list, [](Slot const& x) { return x.n(); })) return s->leftClick();
        if (mUntilEqualsTypeOnly) throw std::runtime_error("until equals type only");
        auto more = filteredOf(mNonEquals, [](Slot const& s) { return nowMoreThanGoal(s); }, true, true);
        if (auto s = minBy(more, [](Slot const& x) { return clickCountSingleSlotToLess(x.n(), x.g()); })) {
            if (canRight(s->n(), s->g())) s->rightClick();
            else s->leftClick();
            return;
        }
        throw std::runtime_error("should not reach here");
    }

    void handleCursor() {
        if (!mToBeThrown.isEmpty()) throwCursor();
        if (cursorNow().isEmpty()) return;
        handleCursorNoNeedThrow();
    }

    void throwCursor() {
        if (mToBeThrown.contains(cursorNow())) {
            mToBeThrown.remove(cursorNow());
            mSandbox.leftClickOutside();
        } else if (mToBeThrown.contains(cursorNow().itemType)) {
            int throwCount = mToBeThrown.count(cursorNow().itemType);
            mToBeThrown.remove(cursorNow());
            for (int i = 0; i < throwCount; ++i) mSandbox.rightClickOutside();
        }
    }

    void handleCursorNoNeedThrow() {
        auto candidate = filteredOf(mNonEquals, [&](Slot const& s) { return cursorNow().itemType == s.goal().itemType; }, true, false, true);
        {
            auto list = filteredOf(candidate, [&](Slot const& s) { return withCursorNowCount(s) <= s.g(); });
            if (auto s = minBy(list, [&](Slot const& x) { return x.g() - withCursorNowCount(x); })) return s->leftClick();
        }
        {
            auto list = filteredOf(candidate, [](Slot const& s) { return !s.equalsType() && !s.now().isEmpty(); });
            if (auto s = minBy(list, [&](Slot const& x) { return x.g() - withCursorNowCount(x); })) return s->leftClick();
        }
        auto remaining = filteredOf(candidate, [](Slot const& s) { return nowGoalRemaining(s) > 0; });
        if (mUntilEqualsTypeOnly) {
            if (auto s = maxBy(remaining, [](Slot const& x) { return nowGoalRemaining(x); })) return s->leftClick();
            throw std::runtime_error("should not reach here");
        }
        if (auto s = minBy(remaining, [](Slot const& x) { return nowGoalRemaining(x); })) {
            int r = nowGoalRemaining(*s);
            if (!(r < cursorNow().count)) throw std::runtime_error("check failed: nowGoalRemaining < cursorNow.count");
            return s->repeatRightClick(r);
        }
        throw std::runtime_error("should not reach here");
    }

    ItemBucket        mToBeThrown;
    std::vector<Slot> mNonEquals;
};

namespace single_type {

enum class Button { LEFT, RIGHT };

struct TSlot {
    int  n = 0, g = 0;
    bool operator==(TSlot const&) const = default;
    [[nodiscard]] bool isGoal() const { return n == g; }
    [[nodiscard]] int  rank() const { return calcRank(n, g); }
    [[nodiscard]] std::pair<TSlot, int> click(int c, Button button, int maxCount) const {
        if (c == 0) {
            if (button == Button::LEFT) return {TSlot{0, g}, n};
            return {TSlot{n / 2, g}, n - n / 2};
        }
        int nAfter = button == Button::LEFT ? std::min(n + c, maxCount) : n + 1;
        return {TSlot{nAfter, g}, c - (nAfter - n)};
    }
};

struct SlotBucket {
    std::vector<std::pair<TSlot, int>> entries;
    void                               add(TSlot s) {
        for (auto& e : entries) {
            if (e.first == s) {
                ++e.second;
                return;
            }
        }
        entries.emplace_back(s, 1);
    }
    void remove(TSlot s) {
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (it->first == s) {
                if (--it->second <= 0) entries.erase(it);
                return;
            }
        }
    }
    bool operator==(SlotBucket const& o) const {
        if (entries.size() != o.entries.size()) return false;
        for (auto const& e : entries) {
            bool found = false;
            for (auto const& f : o.entries) {
                if (f.first == e.first) {
                    found = f.second == e.second;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    }
    [[nodiscard]] size_t hash() const {
        size_t h = 0;
        for (auto const& e : entries) h += (static_cast<size_t>(e.first.n) * 31 + static_cast<size_t>(e.first.g)) ^ static_cast<size_t>(e.second);
        return h;
    }
};

struct Click {
    int    clickIndex;
    TSlot  slot;
    Button button;
};

struct Node {
    int                maxCount = 64;
    SlotBucket         identities;
    int                c        = 0;
    bool               skipThis = false;
    std::vector<Click> clicks;

    [[nodiscard]] bool isGoal() const {
        if (c != 0) return false;
        return std::all_of(identities.entries.begin(), identities.entries.end(), [](auto const& e) { return e.first.isGoal(); });
    }
    [[nodiscard]] int clickCount() const { return static_cast<int>(clicks.size()); }
    [[nodiscard]] int lowerBound() const {
        int s = 0;
        for (auto const& [slot, count] : identities.entries) s += clickCountLowerBound(slot.n, slot.g) * count;
        return s;
    }
    [[nodiscard]] int upperBound() const {
        int s = 0;
        for (auto const& [slot, count] : identities.entries) s += clickCountUpperBound(slot.n, slot.g) * count;
        return s;
    }
    [[nodiscard]] int fScore() const { return clickCount() + lowerBound(); }
    [[nodiscard]] int lowerTotal() const { return clickCount() + lowerBound(); }
    [[nodiscard]] int upperTotal() const { return clickCount() + upperBound(); }

    [[nodiscard]] bool banRight(TSlot const& slot) const { return c == 1 || (c == 0 && slot.n == 1) || (c != 0 && maxCount - slot.n == 1); }

    [[nodiscard]] Node copyByAddClick(TSlot const& slot, Button button) const {
        auto [after, cAfter] = slot.click(c, button, maxCount);
        Node r               = *this;
        r.skipThis           = false;
        r.clicks.push_back({clickCount(), slot, button});
        r.c = cAfter;
        r.identities.remove(slot);
        r.identities.add(after);
        return r;
    }

    [[nodiscard]] std::vector<Node> neighbor() const {
        std::vector<Node> result;
        bool              doneRank10 = false;
        for (auto const& [slot, count] : identities.entries) {
            if (slot.isGoal()) continue;
            if (c == 0 && slot.n == 0) continue;
            if (c != 0 && slot.n == maxCount) continue;
            result.push_back(copyByAddClick(slot, Button::LEFT));
            result.back().skipThis = true;
            size_t leftIndex       = result.size() - 1;
            result.push_back(copyByAddClick(slot, Button::RIGHT));
            result.back().skipThis = true;
            size_t rightIndex      = result.size() - 1;
            int    rank            = slot.rank();
            if (rank == 10 && c == 0) continue;
            if ((rank == 11 || rank == 12) && c != 0) continue;
            if ((rank == 30 || rank == 31) && c != 0) continue;
            if (rank != 12 && (rank != 10 || banRight(slot))) {
                if (rank == 10) {
                    if (!doneRank10) {
                        doneRank10                   = true;
                        result[leftIndex].skipThis = false;
                    }
                } else {
                    result[leftIndex].skipThis = false;
                }
            }
            if (!banRight(slot) && rank != 30) {
                if (rank == 10) {
                    if (!doneRank10) {
                        doneRank10                    = true;
                        result[rightIndex].skipThis = false;
                    }
                } else {
                    result[rightIndex].skipThis = false;
                }
            }
        }
        return result;
    }

    bool operator==(Node const& o) const { return c == o.c && identities == o.identities; }
};

struct NodeHash {
    size_t operator()(Node const& n) const { return static_cast<size_t>(n.c) * 31 + n.identities.hash(); }
};

struct NodeLess {
    bool operator()(Node const& a, Node const& b) const {
        int fa = a.fScore(), fb = b.fScore();
        if (fa != fb) return fa < fb;
        return a.upperTotal() < b.upperTotal();
    }
};

constexpr int MAX_LOOP = 100000;

std::vector<Click> solve(Node const& start) {
    std::unordered_set<Node, NodeHash> closedSet;
    std::map<Node, Node, NodeLess>     openSet;
    openSet.emplace(start, start);
    int minUpper           = start.upperTotal();
    int minLowerOfMinUpper = start.lowerTotal();
    int loopCounter        = 0;
    NodeLess less;
    while (!openSet.empty()) {
        if (++loopCounter > MAX_LOOP) throw std::runtime_error("Too many loop");
        Node x = openSet.begin()->first;
        if (x.isGoal()) return x.clicks;
        openSet.erase(openSet.begin());
        closedSet.insert(x);
        if (x.lowerTotal() > minUpper || (x.lowerTotal() == minUpper && (minUpper > minLowerOfMinUpper || x.upperTotal() > x.lowerTotal())))
            throw std::runtime_error("emm x.lowerTotal > minUpper etc");
        for (auto& y : x.neighbor()) {
            if (closedSet.contains(y)) continue;
            auto it = openSet.find(y);
            if (it == openSet.end() || less(y, it->second)) {
                if (y.lowerTotal() > minUpper || (y.lowerTotal() == minUpper && (minUpper > minLowerOfMinUpper || y.upperTotal() > y.lowerTotal())) || y.skipThis) {
                    if (it != openSet.end()) openSet.erase(it);
                    closedSet.insert(y);
                    continue;
                }
                if (it != openSet.end()) it->second = y;
                else openSet.emplace(y, y);
                if (y.upperTotal() < minUpper) {
                    minUpper           = y.upperTotal();
                    minLowerOfMinUpper = y.lowerTotal();
                } else if (y.upperTotal() == minUpper && y.lowerTotal() < minLowerOfMinUpper) {
                    minLowerOfMinUpper = y.lowerTotal();
                }
            }
        }
    }
    throw std::runtime_error("solve failure");
}

}

class ScoreBasedDualDiffCalculatorInstance : public SimpleDiffCalculatorInstance {
public:
    ScoreBasedDualDiffCalculatorInstance(ContainerSandbox& sandbox, ItemTracker goal)
    : SimpleDiffCalculatorInstance(sandbox, std::move(goal)),
      mStatGoal(goalTracker().slots) {
        mUntilEqualsTypeOnly = true;
    }

    void run() override {
        SimpleDiffCalculatorInstance::run();
        if (!cursorNow().isEmpty()) doItemType(cursorNow().itemType);
        for (auto const& t : mStatGoal.itemTypes()) doItemType(t);
    }

private:
    void doItemType(item::ItemType const& itemType) {
        auto const* entry = mStatGoal.group(itemType);
        if (!entry) throw std::out_of_range("statGoal has no item type");
        std::vector<int> indices;
        for (int i : entry->slotIndices) {
            if (!slot(i).equals()) indices.push_back(i);
        }
        if (indices.empty()) return;
        single_type::Node start;
        start.maxCount = itemType.maxCount();
        start.c        = cursorNow().count;
        for (int i : indices) start.identities.add(toSlot(i));
        auto clicks = single_type::solve(start);
        for (auto const& click : clicks) {
            auto it = std::find_if(indices.begin(), indices.end(), [&](int i) { return toSlot(i) == click.slot; });
            if (it == indices.end()) throw std::runtime_error("target slot not found");
            if (click.button == single_type::Button::LEFT) slot(*it).leftClick();
            else slot(*it).rightClick();
        }
    }

    single_type::TSlot toSlot(int index) { return {slot(index).n(), slot(index).g()}; }

    data::ItemStat mStatGoal;
};

class GenericDiffCalculatorInstance : public DiffCalculatorInstance {
public:
    using DiffCalculatorInstance::DiffCalculatorInstance;

    void run() override {
        if (now() == goalTracker()) return;
        ItemTracker goal = intermediateGoalTracker();
        auto        type = config::Debugs::DIFF_CALCULATOR.getOptionListValue();
        if (type == "SCORE_BASED_SINGLE") throw std::runtime_error("todo");
        if (type == "SCORE_BASED_DUAL") ScoreBasedDualDiffCalculatorInstance(mSandbox, goal).run();
        else SimpleDiffCalculatorInstance(mSandbox, goal).run();
        if (!(now() == goal)) throw std::runtime_error("ContainerSandbox actual result by " + type + " Diff Calculator not same as goal");
        if (!cursorGoal().isEmpty()) postRun();
    }

private:
    ItemTracker intermediateGoalTracker() {
        if (cursorGoal().isEmpty()) return goalTracker();
        ItemTracker t = goalTracker().copyAsMutable();
        t.cursor.transferTo(t.slots[slotForEnoughRoom()]);
        if (!t.cursor.isEmpty()) throw std::runtime_error("goal cursor still not empty");
        return t;
    }

    int slotForEnoughRoom() {
        if (mSlotForEnoughRoom) return *mSlotForEnoughRoom;
        auto a = filtered([&](Slot const& s) { return s.now().itemType == cursorGoal().itemType && s.goal().isEmpty(); });
        if (auto s = minBy(a, [&](Slot const& x) { return calcRank(x.n(), cursorGoal().count); })) return *(mSlotForEnoughRoom = s->slotIndex);
        auto b = filtered([&](Slot const& s) {
            return s.goal().itemType == cursorGoal().itemType && (s.g() + cursorGoal().count) <= s.goal().itemType.maxCount();
        });
        if (auto s = minBy(b, [&](Slot const& x) { return clickCountSingleSlotToLess(x.g() + cursorGoal().count, x.g()); }))
            return *(mSlotForEnoughRoom = s->slotIndex);
        throw NoRoomException("No room to arrange items. You may try to clear cursor.");
    }

    void postRun() {
        Slot s = slot(slotForEnoughRoom());
        if (s.n() == s.g()) return;
        if (canRight(s.n(), s.g())) s.rightClick();
        else s.leftClick();
        s.repeatRightClick(s.g() - s.n());
    }

    std::optional<int> mSlotForEnoughRoom;
};

}

void DiffCalculator::apply(ContainerSandbox& sandbox, ItemTracker const& goalTracker) {
    auto start = std::chrono::steady_clock::now();
    try {
        GenericDiffCalculatorInstance(sandbox, goalTracker).run();
    } catch (...) {
        auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        Log::debug("diff brain costs {} ms (failed)", ms);
        throw;
    }
    auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    Log::debug("diff brain costs {} ms", ms);
}

}
