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
    auto it = cache[id];
    auto list = collisionListCache[it];
    for (auto &item : list)
    {
        collisionListCache[item].erase(it);
    }
    cache.erase(id);
    collisionListCache.erase(it);
    curUpdates.erase(id);
    reinserts.erase(it);
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
        auto preCollision = collisionListCache[val];
        auto newCollisions = query(val);
        // 排除自身
        newCollisions.erase(val);
        // 新碰撞检查
        for (auto other : newCollisions)
        {
            uint64_t pairId = (val->id < other->id)
                                  ? (static_cast<uint64_t>(val->id) << 32) | other->id
                                  : (static_cast<uint64_t>(other->id) << 32) | val->id;
            if (processedPairs.count(pairId))
                continue;
            processedPairs.emplace(pairId);
            // 这里做个优化,变化后先遍历的碰撞则是主动碰撞,另外个是被动碰撞
            if (preCollision.find(other) == preCollision.end())
            {
                //  新碰撞
                // int dir = val->getDir(other);
                int dir = 0;
                auto newInfo = std::make_unique<QuadTreeCollisionInfo>();
                auto info = newInfo.get();
                collisionCache[pairId] = std::move(newInfo);
                info->from = val;
                info->to = other;
                info->dir = dir;

                val->onCollisionCallBack(other, dir, true);
                other->onCollisionCallBack(val, dir, false);

                collisionListCache[val].emplace(other);
                collisionListCache[other].emplace(val);
            }
            else
            {
                // 旧碰撞,将主动方更新
                auto info = collisionCache.find(pairId)->second.get();
                info->from = val;
                info->to = other;
                info->dir = val->getDir(other);
            }
        }

        // 碰撞失效检查
        for (auto other : preCollision)
        {
            if (newCollisions.find(other) == newCollisions.end())
            {

                uint64_t pairId = (val->id < other->id)
                                      ? (static_cast<uint64_t>(val->id) << 32) | other->id
                                      : (static_cast<uint64_t>(other->id) << 32) | val->id;
                auto info = collisionCache.find(pairId)->second.get();

                collisionListCache[val].erase(other);
                collisionListCache[other].erase(val);
                bool from = info->from == val;

                val->onCollisionOutCallBack(other, from);
                other->onCollisionOutCallBack(val, !from);

                collisionCache.erase(pairId);
            }
        }
        collisionListCache[val] = newCollisions;
    }

    for (auto const &[pairId, info] : collisionCache)
    {
        info->from->onCollisioningCallBack(info->to, info->dir, true);
        info->to->onCollisioningCallBack(info->from, info->dir, false);
    }

    processedPairs.clear();
    reinserts.clear();
    curUpdates.clear();
}
