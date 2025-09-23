#include "QuadTree.h"

std::unordered_map<int, QuadTreeRect *> QuadTree::cache;

QuadTree::QuadTree(QuadTreeRect bound, int capacity)
{
    root = std::make_unique<QuadTreeNode>(bound, capacity);
}

bool QuadTree::insert(QuadTreeRect &val)
{
    //入缓存 todo
    return root->insert(val);
}

std::vector<QuadTreeRect> QuadTree::query(QuadTreeRect &range)
{
    std::vector<QuadTreeRect> result;
    root->query(range, result);
    return result;
}

bool QuadTree::remove(int id)
{
    // 删除缓存 todo
    // todo 缓存获取
    QuadTreeRect rt{0, 0, 0, 0, id};
    return root->remove(rt);
}