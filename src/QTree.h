#ifndef QTREE_H
#define QTREE_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <algorithm>
// 碰撞方向枚举
enum class Direction
{
    NONE,
    UP,
    DOWN,
    LEFT,
    RIGHT
};
class Role;
// 碰撞信息结构
struct CollisionInfo
{
    int otherId;
    Direction dir;

    CollisionInfo(int id, Direction d) : otherId(id), dir(d) {}
};

// 物体数据结构
struct Object
{
    int id;
    int x, y, w, h;
    Role *role;

    Object(int id, int x, int y, int w, int h, Role *role)
        : id(id), x(x), y(y), w(w), h(h), role(role) {}
};

// 四叉树节点
class QuadNode
{
public:
    QuadNode(int x, int y, int width, int height, int level = 0, int maxLevels = 5, int capacity = 4);

    bool insert(Object *object);
    bool remove(int id);
    void update(Object *object, std::unordered_set<int> &movedObjects, bool &needsReinsertion); // 修改方法签名
    void clear();

    void getPotentialCollisions(Object *object, std::unordered_set<int> &potentialCollisions);
    void getAllObjects(std::vector<Object *> &objects);

private:
    void split();
    bool contains(Object *object) const;
    int getIndex(Object *object) const;

    int x, y, width, height;
    int level;
    int maxLevels;
    int capacity;

    std::vector<Object *> objects;
    std::unique_ptr<QuadNode> children[4];
    bool hasChildren;
};

// 静态四叉树管理器
class QTree
{
public:
    // 初始化四叉树
    static void init(int width, int height, int maxLevels = 5, int nodeCapacity = 4);

    // 核心接口
    static void insert(int id, int x, int y, int w, int h, Role *role = nullptr);
    static void update(int id, int x, int y, int w, int h);
    static void remove(int id);
    static void clear();

    // 获取碰撞信息
    static std::vector<CollisionInfo> getCollision(int id);

    // 更新碰撞状态（需要在每帧结束时调用）
    static void updateCollisions();

    // 碰撞回调设置
    static void setOnCollision(std::function<void(int, int, Direction)> callback);
    static void setOnCollisionOut(std::function<void(int, int)> callback);
    static void setOnCollisioning(std::function<void(int, int, Direction)> callback);

private:
    static Direction calculateDirection(const Object &a, const Object &b);

    // 静态成员
    static std::unique_ptr<QuadNode> root;
    static std::unordered_map<int, std::unique_ptr<Object>> objects;

    // 碰撞状态跟踪
    static std::unordered_map<int, std::unordered_map<int, Direction>> currentCollisions;
    static std::unordered_map<int, std::unordered_map<int, Direction>> previousCollisions;

    // 回调函数
    static std::function<void(int, int, Direction)> onCollision;
    static std::function<void(int, int)> onCollisionOut;
    static std::function<void(int, int, Direction)> onCollisioning;

    // 需要检查碰撞的物体ID集合
    static std::unordered_set<int> objectsToCheck;

    // 初始化标志
    static bool initialized;
};

#endif // QTREE_H