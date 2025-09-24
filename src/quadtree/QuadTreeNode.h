#pragma once
#include <vector>
#include <memory>
#include <unordered_set>

#include "QuadTreeRect.h"
class QuadTreeNode
{
public:
    QuadTreeNode(QuadTreeRect rect, int capacity, int depth = 0);

    // 范围
    QuadTreeRect bound;
    // 数据
    std::vector<QuadTreeRect *> vals;
    // 分裂
    bool isSub = false;
    // 容量
    int capacity;
    // 深度
    int depth;
    // 最大深度,达到后不再分裂
    static const int MAX_DEPTH = 8;
    // 西北
    std::unique_ptr<QuadTreeNode> nw;
    // 东北
    std::unique_ptr<QuadTreeNode> ne;
    // 西南
    std::unique_ptr<QuadTreeNode> sw;
    // 东南
    std::unique_ptr<QuadTreeNode> se;

    // 插入
    bool insert(QuadTreeRect *val);

    // 查找
    void query(QuadTreeRect *range, std::unordered_set<QuadTreeRect *> &result);

    // 删除
    bool remove(QuadTreeRect *val);
    // 分裂
    void sub();

private:
    bool tryMergeChildren();
};