#pragma once
#include "QuadTreeNode.h"
#include <unordered_map>
#include <unordered_set>
class QuadTree
{
public:
    static std::unique_ptr<QuadTree> WORLD;
    QuadTree(QuadTreeRect bound, int capacity, int depth = 0);

    // 根节点
    std::unique_ptr<QuadTreeNode> root;

    // 插入
    bool insert(QuadTreeRect *val);

    // 查询
    std::unordered_set<QuadTreeRect *> query(QuadTreeRect *range);

    // 物体位置大小变化时会手动调用
    void update(QuadTreeRect *val);

    // 删除
    bool remove(int id);

    // 每帧检查需要更新的对象并触发已碰撞对象的碰撞中接口
    void tick(double dt);
    // 记录两两碰撞时的来源方
    std::unordered_map<uint64_t, std::unique_ptr<QuadTreeCollisionInfo>> collisionCache;
    std::unordered_map<QuadTreeRect *, std::unordered_set<QuadTreeRect *>> collisionListCache;
    std::unordered_map<int, QuadTreeRect *> cache;
    std::unordered_map<int, QuadTreeRect *> curUpdates;
    std::unordered_set<QuadTreeRect *> reinserts;
    std::unordered_set<uint64_t> processedPairs;
private:
};