#pragma once
#include "QuadTreeNode.h"
#include <unordered_map>
class QuadTree
{
public:
    QuadTree(QuadTreeRect bound, int capacity);

    static std::unordered_map<int, QuadTreeRect *> cache;

    // 根节点
    std::unique_ptr<QuadTreeNode> root;

    // 插入
    bool insert(QuadTreeRect &val);

    // 查询
    std::vector<QuadTreeRect> query(QuadTreeRect &range);

    // 删除
    bool remove(int id);
};