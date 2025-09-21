#include "QTree.h"
#include <iostream>
#include <cmath>
#include "Role.h"

// 初始化静态成员
std::unique_ptr<QuadNode> QTree::root = nullptr;
std::unordered_map<int, std::unique_ptr<Object>> QTree::objects;
std::unordered_map<int, std::unordered_map<int, Direction>> QTree::currentCollisions;
std::unordered_map<int, std::unordered_map<int, Direction>> QTree::previousCollisions;
std::function<void(int, int, Direction)> QTree::onCollision = nullptr;
std::function<void(int, int)> QTree::onCollisionOut = nullptr;
std::function<void(int, int, Direction)> QTree::onCollisioning = nullptr;
std::unordered_set<int> QTree::objectsToCheck;
bool QTree::initialized = false;

// QuadNode 实现
QuadNode::QuadNode(int x, int y, int width, int height, int level, int maxLevels, int capacity)
    : x(x), y(y), width(width), height(height), level(level), maxLevels(maxLevels), capacity(capacity), hasChildren(false)
{
}

bool QuadNode::insert(Object *object)
{
    if (!contains(object))
    {
        return false;
    }

    if (!hasChildren)
    {
        if (objects.size() < capacity || level >= maxLevels)
        {
            objects.push_back(object);
            return true;
        }

        // 需要分裂
        split();
    }

    // 尝试插入到子节点
    int index = getIndex(object);
    if (index != -1)
    {
        return children[index]->insert(object);
    }

    // 如果对象跨越多个子节点，留在当前节点
    objects.push_back(object);
    return true;
}

bool QuadNode::remove(int id)
{
    // 在当前节点查找
    auto it = std::find_if(objects.begin(), objects.end(),
                           [id](Object *obj)
                           { return obj->id == id; });

    if (it != objects.end())
    {
        objects.erase(it);
        return true;
    }

    // 在子节点中查找
    if (hasChildren)
    {
        for (int i = 0; i < 4; i++)
        {
            if (children[i]->remove(id))
            {
                return true;
            }
        }
    }

    return false;
}

void QuadNode::update(Object *object, std::unordered_set<int> &movedObjects, bool &needsReinsertion)
{
    // 检查对象是否仍在当前节点范围内
    if (this->contains(object))
    {
        // 如果在子节点中，更新子节点
        if (this->hasChildren)
        {
            int index = this->getIndex(object);
            if (index != -1)
            {
                this->children[index]->update(object, movedObjects, needsReinsertion);
                return;
            }
        }

        // 对象在当前节点，标记需要检查碰撞
        movedObjects.insert(object->id);
    }
    else
    {
        // 对象已移出当前节点范围，需要重新插入到树中
        this->remove(object->id);
        needsReinsertion = true;
    }
}

void QuadNode::clear()
{
    objects.clear();
    if (hasChildren)
    {
        for (int i = 0; i < 4; i++)
        {
            children[i]->clear();
        }
        hasChildren = false;
    }
}

void QuadNode::getPotentialCollisions(Object *object, std::unordered_set<int> &potentialCollisions)
{
    // 添加当前节点的对象
    for (auto obj : objects)
    {
        if (obj->id != object->id)
        {
            potentialCollisions.insert(obj->id);
        }
    }

    // 递归检查子节点
    if (hasChildren)
    {
        int index = getIndex(object);
        if (index != -1)
        {
            children[index]->getPotentialCollisions(object, potentialCollisions);
        }
        else
        {
            // 对象跨越多个子节点，检查所有相关子节点
            for (int i = 0; i < 4; i++)
            {
                if (children[i]->contains(object))
                {
                    children[i]->getPotentialCollisions(object, potentialCollisions);
                }
            }
        }
    }
}

void QuadNode::getAllObjects(std::vector<Object *> &allObjects)
{
    allObjects.insert(allObjects.end(), objects.begin(), objects.end());

    if (hasChildren)
    {
        for (int i = 0; i < 4; i++)
        {
            children[i]->getAllObjects(allObjects);
        }
    }
}

void QuadNode::split()
{
    if (hasChildren)
        return;

    int childWidth = width / 2;
    int childHeight = height / 2;

    children[0] = std::make_unique<QuadNode>(x, y, childWidth, childHeight, level + 1, maxLevels, capacity);
    children[1] = std::make_unique<QuadNode>(x + childWidth, y, childWidth, childHeight, level + 1, maxLevels, capacity);
    children[2] = std::make_unique<QuadNode>(x, y + childHeight, childWidth, childHeight, level + 1, maxLevels, capacity);
    children[3] = std::make_unique<QuadNode>(x + childWidth, y + childHeight, childWidth, childHeight, level + 1, maxLevels, capacity);

    hasChildren = true;

    // 将当前节点的对象重新分配到子节点
    std::vector<Object *> objectsToRedistribute;
    objectsToRedistribute.swap(objects);

    for (auto obj : objectsToRedistribute)
    {
        int index = getIndex(obj);
        if (index != -1)
        {
            children[index]->insert(obj);
        }
        else
        {
            objects.push_back(obj);
        }
    }
}

bool QuadNode::contains(Object *object) const
{
    return object->x >= x &&
           object->x + object->w <= x + width &&
           object->y >= y &&
           object->y + object->h <= y + height;
}

int QuadNode::getIndex(Object *object) const
{
    if (!hasChildren)
        return -1;

    int verticalMidpoint = x + width / 2;
    int horizontalMidpoint = y + height / 2;

    // 检查对象完全在哪个象限
    bool topHalf = object->y < horizontalMidpoint && object->y + object->h < horizontalMidpoint;
    bool bottomHalf = object->y > horizontalMidpoint;
    bool leftHalf = object->x < verticalMidpoint && object->x + object->w < verticalMidpoint;
    bool rightHalf = object->x > verticalMidpoint;

    if (leftHalf)
    {
        if (topHalf)
            return 0;
        else if (bottomHalf)
            return 2;
    }
    else if (rightHalf)
    {
        if (topHalf)
            return 1;
        else if (bottomHalf)
            return 3;
    }

    // 对象跨越多个象限
    return -1;
}

// QTree 静态方法实现
void QTree::init(int width, int height, int maxLevels, int nodeCapacity)
{
    root = std::make_unique<QuadNode>(0, 0, width, height, 0, maxLevels, nodeCapacity);
    clear();
    initialized = true;
}

void QTree::insert(int id, int x, int y, int w, int h, Role *role)
{
    if (!initialized)
        return;

    auto newObj = std::make_unique<Object>(id, x, y, w, h, role);
    if (root->insert(newObj.get()))
    {
        objects[id] = std::move(newObj);
        objectsToCheck.insert(id);
    }
}

void QTree::update(int id, int x, int y, int w, int h)
{
    if (!initialized)
        return;

    auto it = objects.find(id);
    if (it != objects.end())
    {
        // 更新对象位置和尺寸
        it->second->x = x;
        it->second->y = y;
        it->second->w = w;
        it->second->h = h;

        // 标记需要检查碰撞
        objectsToCheck.insert(id);

        // 更新四叉树中的对象位置
        bool needsReinsertion = false;
        root->update(it->second.get(), objectsToCheck, needsReinsertion);

        // 如果需要重新插入，先删除再插入
        if (needsReinsertion)
        {
            root->remove(id);
            if (root->insert(it->second.get()))
            {
                objectsToCheck.insert(id);
            }
        }
    }
}

void QTree::remove(int id)
{
    if (!initialized)
        return;

    auto it = objects.find(id);
    if (it != objects.end())
    {
        root->remove(id);
        objects.erase(it);

        // 从碰撞状态中移除
        currentCollisions.erase(id);
        previousCollisions.erase(id);

        // 从其他对象的碰撞状态中移除该对象
        for (auto &pair : currentCollisions)
        {
            pair.second.erase(id);
        }
        for (auto &pair : previousCollisions)
        {
            pair.second.erase(id);
        }
    }
}

void QTree::clear()
{
    if (root)
    {
        root->clear();
    }
    objects.clear();
    currentCollisions.clear();
    previousCollisions.clear();
    objectsToCheck.clear();
}

std::vector<CollisionInfo> QTree::getCollision(int id)
{
    std::vector<CollisionInfo> result;

    if (!initialized)
        return result;

    auto it = currentCollisions.find(id);
    if (it != currentCollisions.end())
    {
        for (const auto &collision : it->second)
        {
            result.emplace_back(collision.first, collision.second);
        }
    }

    return result;
}

void QTree::updateCollisions()
{
    if (!initialized)
        return;

    // 交换当前和之前的碰撞状态
    std::swap(previousCollisions, currentCollisions);
    currentCollisions.clear();

    // 检查需要更新的对象的碰撞
    std::unordered_set<int> processedObjects;
    for (int id : objectsToCheck)
    {
        auto it = objects.find(id);
        if (it == objects.end())
            continue;

        Object *obj = it->second.get();
        processedObjects.insert(id);

        // 获取可能碰撞的对象
        std::unordered_set<int> potentialCollisions;
        root->getPotentialCollisions(obj, potentialCollisions);

        // 检查实际碰撞
        for (int otherId : potentialCollisions)
        {
            auto otherIt = objects.find(otherId);
            if (otherIt == objects.end())
                continue;

            Object *other = otherIt->second.get();

            // 检查AABB碰撞
            if (obj->x < other->x + other->w &&
                obj->x + obj->w > other->x &&
                obj->y < other->y + other->h &&
                obj->y + obj->h > other->y)
            {

                // 计算碰撞方向
                Direction dir = calculateDirection(*obj, *other);
                Direction otherDir;
                switch (dir)
                {
                case Direction::UP:
                    otherDir = Direction::DOWN;
                    break;
                case Direction::DOWN:
                    otherDir = Direction::UP;
                    break;
                case Direction::LEFT:
                    otherDir = Direction::RIGHT;
                    break;
                case Direction::RIGHT:
                    otherDir = Direction::LEFT;
                }
                // 记录碰撞
                currentCollisions[id][otherId] = dir;
                currentCollisions[otherId][id] =
                    (dir == Direction::UP) ? Direction::DOWN : (dir == Direction::DOWN) ? Direction::UP
                                                           : (dir == Direction::LEFT)   ? Direction::RIGHT
                                                           : (dir == Direction::RIGHT)  ? Direction::LEFT
                                                                                        : Direction::NONE;

                // 标记被动碰撞的物体也需要处理
                processedObjects.insert(otherId);

                // 检查碰撞状态变化并触发回调
                auto prevIt = previousCollisions.find(id);
                bool wasColliding = prevIt != previousCollisions.end() &&
                                    prevIt->second.find(otherId) != prevIt->second.end();

                if (!wasColliding)
                {
                    // 新的碰撞
                    if (onCollision)
                    {
                        onCollision(id, otherId, dir);
                    }
                    obj->role->onCollision(other->role, dir);
                    // other->role->onCollision(obj->role, dir);
                }
                else
                {
                    // 持续碰撞
                    if (onCollisioning)
                    {
                        onCollisioning(id, otherId, dir);
                    }
                    obj->role->onCollisioning(other->role, dir);
                    // other->role->onCollisioning(obj->role, dir);
                }
            }
        }
    }

    // 检查碰撞结束（需要检查所有之前有碰撞的对象）
    for (const auto &pair : previousCollisions)
    {
        int id = pair.first;
        for (const auto &collision : pair.second)
        {
            int otherId = collision.first;
            // 如果当前帧没有这个碰撞，触发碰撞结束回调
            auto currentIt = currentCollisions.find(id);
            if (currentIt == currentCollisions.end() ||
                currentIt->second.find(otherId) == currentIt->second.end())
            {
                // 碰撞结束
                if (onCollisionOut)
                {
                    onCollisionOut(id, otherId);
                }
                auto it = objects.find(id);
                if (it != objects.end())
                {
                    auto ot = objects.find(id);
                    if (ot != objects.end())
                    {
                        it->second.get()->role->onCollisionOut(ot->second.get()->role);
                        // ot->second.get()->role->onCollisionOut(it->second.get()->role);
                    }
                }
            }
        }
    }

    objectsToCheck = processedObjects;
}

Direction QTree::calculateDirection(const Object &a, const Object &b)
{
    // 计算中心点
    float aCenterX = static_cast<float>(a.x) + static_cast<float>(a.w) / 2.0f;
    float aCenterY = static_cast<float>(a.y) + static_cast<float>(a.h) / 2.0f;
    float bCenterX = static_cast<float>(b.x) + static_cast<float>(b.w) / 2.0f;
    float bCenterY = static_cast<float>(b.y) + static_cast<float>(b.h) / 2.0f;

    // 计算重叠量
    float overlapLeft = static_cast<float>(a.x + a.w) - static_cast<float>(b.x);
    float overlapRight = static_cast<float>(b.x + b.w) - static_cast<float>(a.x);
    float overlapTop = static_cast<float>(a.y + a.h) - static_cast<float>(b.y);
    float overlapBottom = static_cast<float>(b.y + b.h) - static_cast<float>(a.y);

    // 找出最小重叠方向
    float minOverlap = std::min({overlapLeft, overlapRight, overlapTop, overlapBottom});

    if (minOverlap == overlapLeft)
        return Direction::LEFT;
    else if (minOverlap == overlapRight)
        return Direction::RIGHT;
    else if (minOverlap == overlapTop)
        return Direction::UP;
    else
        return Direction::DOWN;
}

void QTree::setOnCollision(std::function<void(int, int, Direction)> callback)
{
    onCollision = callback;
}

void QTree::setOnCollisionOut(std::function<void(int, int)> callback)
{
    onCollisionOut = callback;
}

void QTree::setOnCollisioning(std::function<void(int, int, Direction)> callback)
{
    onCollisioning = callback;
}