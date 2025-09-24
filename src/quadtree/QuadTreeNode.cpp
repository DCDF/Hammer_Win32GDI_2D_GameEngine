#include "QuadTreeNode.h"
#include <iostream>
QuadTreeNode::QuadTreeNode(QuadTreeRect rect, int capacity, int depth) : bound(rect), capacity(capacity), depth(depth)
{
}

bool QuadTreeNode::insert(QuadTreeRect *val)
{
    // 不在范围
    if (!bound.inBound(val->centerX, val->centerY))
    {
        return false;
    }

    // 容量未满||最大深度时,本节点消化
    if (vals.size() < capacity || depth >= MAX_DEPTH)
    {
        vals.push_back(val);
        val->parent = &bound;
        return true;
    }

    // 未到最大深度,一定能分裂
    if (!isSub)
    {
        sub();
        // 合并本次数据
        vals.push_back(val);
        // 将本节点内容插入子节点
        for (auto &p : vals)
        {
            // 尝试插入到四个子节点，理论上一定会成功一个
            if (nw->insert(p))
                continue;
            if (ne->insert(p))
                continue;
            if (sw->insert(p))
                continue;
            if (se->insert(p))
                continue;
            // 插入失败,不该发生
        }
        // 清空当前节点的点列表，现在点都存储在子节点中
        vals.clear();
        return true;
    }
    else
    {
        // 以分裂,则由子节点消化
        if (nw->insert(val))
            return true;
        if (ne->insert(val))
            return true;
        if (sw->insert(val))
            return true;
        if (se->insert(val))
            return true;
    }
    // 如果点未能插入任何子节点（理论上不应发生给定点在边界内）
    return false;
}

void QuadTreeNode::query(QuadTreeRect *range, std::unordered_set<QuadTreeRect *> &result)
{
    // 如果查询范围与本节点边界不相交
    if (!bound.contains(range))
    {
        return;
    }

    // 检查本节点存储的点是否在查询范围内
    for (auto &p : vals)
    {
        if (range->contains(p))
        {
            result.emplace(p);
        }
    }

    // 如果本节点有子节点，递归查询子节点
    if (isSub)
    {
        nw->query(range, result);
        ne->query(range, result);
        sw->query(range, result);
        se->query(range, result);
    }
}

bool QuadTreeNode::remove(QuadTreeRect *val)
{
    // 不在范围
    if (!bound.inBound(val->centerX, val->centerY))
    {
        return false;
    }
    // 尝试在当前节点的点列表中删除
    auto it = std::find_if(vals.begin(), vals.end(),
                           [val](QuadTreeRect *pp)
                           { return pp->id == val->id; });
    if (it != vals.end())
    {
        vals.erase(it);
        val->parent = nullptr;
        return true; // 在当前节点找到并删除
    }

    if (isSub)
    {
        if (nw->remove(val))
            return tryMergeChildren() || true;
        if (ne->remove(val))
            return tryMergeChildren() || true;
        if (sw->remove(val))
            return tryMergeChildren() || true;
        if (se->remove(val))
            return tryMergeChildren() || true;
    }

    return false;
}

void QuadTreeNode::sub()
{
    isSub = true;

    float x = bound.x;
    float y = bound.y;
    float w = bound.w / 2;
    float h = bound.h / 2;

    int newDepth = depth + 1;
    nw = std::make_unique<QuadTreeNode>(QuadTreeRect(x, y, w, h), capacity, newDepth);
    ne = std::make_unique<QuadTreeNode>(QuadTreeRect(x + w, y, w, h), capacity, newDepth);
    sw = std::make_unique<QuadTreeNode>(QuadTreeRect(x, y + h, w, h), capacity, newDepth);
    se = std::make_unique<QuadTreeNode>(QuadTreeRect(x + w, y + h, w, h), capacity, newDepth);
}

// 尝试合并子节点
bool QuadTreeNode::tryMergeChildren()
{
    // 检查所有子节点是否都是叶子节点，并且所有子节点的点数之和 <= capacity
    if (isSub && !nw->isSub && !ne->isSub && !sw->isSub && !se->isSub && (nw->vals.size() + ne->vals.size() + sw->vals.size() + se->vals.size()) <= capacity)
    {
        // 将所有子节点中的点合并到当前节点
        vals.insert(vals.end(), nw->vals.begin(), nw->vals.end());
        vals.insert(vals.end(), ne->vals.begin(), ne->vals.end());
        vals.insert(vals.end(), sw->vals.begin(), sw->vals.end());
        vals.insert(vals.end(), se->vals.begin(), se->vals.end());
        // 释放子节点
        nw.reset();
        ne.reset();
        sw.reset();
        se.reset();
        // 当前节点变回叶子节点
        isSub = false;

        return true;
    }
    // 不满足合并条件
    return false;
}