#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "Role.h" // QTree.h 仍然需要包含 Role.h 的完整定义

// 碰撞方向枚举
enum class DirectType
{
    NONE,
    TOP,
    BOTTOM,
    LEFT,
    RIGHT
};

// 内部使用的矩形结构
struct Rect
{
    int x, y, w, h;
    bool intersects(const Rect &other) const;
};

// 内部存储的对象数据
struct ObjectData
{
    int id;
    Rect rect;
    Role *role;
};

// 四叉树节点
class Node
{
public:
    Node(int level, Rect bounds);
    void clear();
    void insert(ObjectData *obj);
    void retrieve(std::vector<ObjectData *> &returnObjects, const Rect &rect);

private:
    void split();
    int getIndex(const Rect &rect);

    int m_level;
    Rect m_bounds;
    std::vector<ObjectData *> m_objects;
    std::unique_ptr<Node> m_children[4];

    const int MAX_OBJECTS = 10;
    const int MAX_LEVELS = 5;
};

// 静态四叉树工具类
class QTree
{
public:
    // 初始化四叉树，定义世界边界和最大深度
    static void init(int worldX, int worldY, int worldWidth, int worldHeight);

    // 清空整个四叉树和所有对象
    static void clear();

    // 更新或插入一个对象
    static void update(int roleId, int x, int y, int w, int h, Role *role);

    // 移除一个对象
    static void remove(int roleId);

    // 核心方法：每帧调用一次，检测所有碰撞并触发回调
    static void updateCollisions();

    // (可选) 获取与指定对象碰撞的所有对象信息
    static std::vector<std::pair<Role *, DirectType>> getCollisions(int roleId);

private:
    // --- 关键改动 ---
    // 重命名函数以提高可读性
    static DirectType getCollisionDirection(const Rect &r1, const Rect &r2);

    static std::unique_ptr<Node> m_root;
    // 使用ID快速查找对象数据
    static std::unordered_map<int, ObjectData> m_allObjects;

    // 碰撞状态管理
    // Key: roleId, Value: 与之碰撞的roleId集合
    using CollisionSet = std::unordered_set<int>;
    static std::unordered_map<int, CollisionSet> m_currentCollisions;
    static std::unordered_map<int, CollisionSet> m_previousCollisions;
};