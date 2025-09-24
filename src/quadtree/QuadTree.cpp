#include "QuadTree.h"
#include <iostream>

std::unique_ptr<QuadTree> QuadTree::WORLD = nullptr;

QuadTree::QuadTree(QuadTreeRect bound, int capacity, int depth)
{
    root = std::make_unique<QuadTreeNode>(bound, capacity, depth);
}

bool QuadTree::insert(QuadTreeRect *val)
{
    val->update();
    curUpdates[val->id] = val;
    cache[val->id] = val;
    return root->insert(val);
}

std::unordered_set<QuadTreeRect *> QuadTree::query(QuadTreeRect *range)
{
    std::unordered_set<QuadTreeRect *> result;
    root->query(range, result);
    return result; // 返回值（C++11的RVO会优化这个过程，避免拷贝开销）
}

// 真正需要移除时调用,会清理碰撞缓存,如果只是移动更新的,不需要此接口,走节点删除和插入,并更新
bool QuadTree::remove(int id)
{
    auto cacheIt = cache.find(id);
    if (cacheIt == cache.end())
    {
        return false;
    }
    QuadTreeRect *itemToRemove = cacheIt->second;
    if (itemToRemove != nullptr)
    {
        // 创建 collisioning 的副本以避免在遍历时修改原始集合
        auto collisioningCopy = itemToRemove->collisioning;
        for (auto other : collisioningCopy)
        {
            // 检查 other 是否仍然在缓存中
            if (cache.find(other->id) != cache.end())
            {
                other->collisioning.erase(itemToRemove);
                other->collisionDir.erase(itemToRemove);
                if (other->collisioning.empty())
                {
                    collisionRects.erase(other);
                }
            }
        }
        itemToRemove->collisioning.clear();
        root->remove(itemToRemove);
    }
    collisionRects.erase(itemToRemove);
    curUpdates.erase(id);
    cache.erase(cacheIt);
    return true;
}

void QuadTree::update(QuadTreeRect *val)
{

    // 不需要更新,没达到阈值
    if (!val->update())
    {
        return;
    }
    if (val->needReInsert())
    {
        reinserts.emplace(val);
    }
    else
    {
        curUpdates[val->id] = val;
    }
}

void QuadTree::tick(double dt)
{
    for (auto item : reinserts)
    {
        root->remove(item);
        root->insert(item);
        curUpdates[item->id] = item;
    }

    for (auto const &[id, val] : curUpdates)
    {
        auto preCollision = val->collisioning;
        auto newCollisions = query(val);

        // 新碰撞检查
        for (auto other : newCollisions)
        {
            if (other == val)
                continue;
            uint64_t pairId = (val->id < other->id)
                                  ? (static_cast<uint64_t>(val->id) << 32) | other->id
                                  : (static_cast<uint64_t>(other->id) << 32) | val->id;
            if (processedPairs.count(pairId))
                continue;
            processedPairs.emplace(pairId);
            // 新碰撞
            if (preCollision.find(other) == preCollision.end())
            {
                val->onCollision(other, val->getDir(other));
                other->onCollision(val, other->getDir(val));
                other->collisioning.emplace(val);
                collisionRects.emplace(other);
                collisionRects.emplace(val);
            }
        }
        // 碰撞失效检查
        for (auto other : preCollision)
        {
            if (other == val)
                continue;
            if (newCollisions.find(other) == newCollisions.end())
            {
                val->onCollisionOut(other);
                val->collisioning.erase(other);
                val->collisionDir.erase(other);

                other->onCollisionOut(val);
                other->collisioning.erase(val);
                other->collisionDir.erase(val);

                if (val->collisioning.empty())
                {
                    collisionRects.erase(val);
                }
                if (other->collisioning.empty())
                {
                    collisionRects.erase(other);
                }
            }
        }
        val->collisioning = newCollisions;
    }

    for (auto item : collisionRects)
    {
        for (auto other : item->collisioning)
        {
            if (item == other)
                continue;
            int dir1 = item->getDir(other);
            item->onCollisioning(other, dir1);
            item->collisionDir[other] = dir1;
            int dir2 = other->getDir(item);
            other->onCollisioning(item, dir2);
            other->collisionDir[item] = dir2;
        }
    }

    processedPairs.clear();
    reinserts.clear();
    curUpdates.clear();
}
