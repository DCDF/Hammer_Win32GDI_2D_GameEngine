#include "QTree.h"
#include "Role.h" // 确保 cpp 文件包含 Role.h
#include <algorithm>

// 初始化静态成员变量
std::unique_ptr<Node> QTree::m_root = nullptr;
std::unordered_map<int, ObjectData> QTree::m_allObjects;
std::unordered_map<int, QTree::CollisionSet> QTree::m_currentCollisions;
std::unordered_map<int, QTree::CollisionSet> QTree::m_previousCollisions;

// --- Rect Methods ---
bool Rect::intersects(const Rect &other) const
{
    return (x <= other.x + other.w &&
            x + w >= other.x &&
            y <= other.y + other.h &&
            y + h >= other.y);
}

// --- Node Methods (无改动) ---
Node::Node(int level, Rect bounds) : m_level(level), m_bounds(bounds)
{
    for (int i = 0; i < 4; ++i)
    {
        m_children[i] = nullptr;
    }
}

void Node::clear()
{
    m_objects.clear();
    for (int i = 0; i < 4; ++i)
    {
        if (m_children[i] != nullptr)
        {
            m_children[i]->clear();
            m_children[i] = nullptr;
        }
    }
}

void Node::split()
{
    int subWidth = m_bounds.w / 2;
    int subHeight = m_bounds.h / 2;
    int x = m_bounds.x;
    int y = m_bounds.y;

    m_children[0] = std::make_unique<Node>(m_level + 1, Rect{x + subWidth, y, subWidth, subHeight});
    m_children[1] = std::make_unique<Node>(m_level + 1, Rect{x, y, subWidth, subHeight});
    m_children[2] = std::make_unique<Node>(m_level + 1, Rect{x, y + subHeight, subWidth, subHeight});
    m_children[3] = std::make_unique<Node>(m_level + 1, Rect{x + subWidth, y + subHeight, subWidth, subHeight});
}

int Node::getIndex(const Rect &rect)
{
    int index = -1;
    double verticalMidpoint = m_bounds.x + (m_bounds.w / 2.0);
    double horizontalMidpoint = m_bounds.y + (m_bounds.h / 2.0);

    bool topQuadrant = (rect.y < horizontalMidpoint && rect.y + rect.h < horizontalMidpoint);
    bool bottomQuadrant = (rect.y > horizontalMidpoint);

    if (rect.x < verticalMidpoint && rect.x + rect.w < verticalMidpoint)
    {
        if (topQuadrant)
        {
            index = 1;
        }
        else if (bottomQuadrant)
        {
            index = 2;
        }
    }
    else if (rect.x > verticalMidpoint)
    {
        if (topQuadrant)
        {
            index = 0;
        }
        else if (bottomQuadrant)
        {
            index = 3;
        }
    }
    return index;
}

void Node::insert(ObjectData *obj)
{
    if (m_children[0] != nullptr)
    {
        int index = getIndex(obj->rect);
        if (index != -1)
        {
            m_children[index]->insert(obj);
            return;
        }
    }

    m_objects.push_back(obj);

    if (m_objects.size() > MAX_OBJECTS && m_level < MAX_LEVELS)
    {
        if (m_children[0] == nullptr)
        {
            split();
        }
        int i = 0;
        while (i < m_objects.size())
        {
            int index = getIndex(m_objects[i]->rect);
            if (index != -1)
            {
                m_children[index]->insert(m_objects[i]);
                m_objects.erase(m_objects.begin() + i);
            }
            else
            {
                i++;
            }
        }
    }
}

void Node::retrieve(std::vector<ObjectData *> &returnObjects, const Rect &rect)
{
    int index = getIndex(rect);
    if (index != -1 && m_children[0] != nullptr)
    {
        m_children[index]->retrieve(returnObjects, rect);
    }

    returnObjects.insert(returnObjects.end(), m_objects.begin(), m_objects.end());
}

// --- QTree Methods ---
void QTree::init(int worldX, int worldY, int worldWidth, int worldHeight)
{
    m_root = std::make_unique<Node>(0, Rect{worldX, worldY, worldWidth, worldHeight});
}

void QTree::clear()
{
    if (m_root)
    {
        m_root->clear();
    }
    m_allObjects.clear();
    m_currentCollisions.clear();
    m_previousCollisions.clear();
}

void QTree::update(int roleId, int x, int y, int w, int h, Role *role)
{
    m_allObjects[roleId] = {roleId, {x, y, w, h}, role};
}

void QTree::remove(int roleId)
{
    m_allObjects.erase(roleId);
    // 同时从碰撞状态中移除，确保能触发onCollisionOut
    m_currentCollisions.erase(roleId);
    m_previousCollisions.erase(roleId);
}

// --- 关键改动 ---
// 实现重命名后的函数
DirectType QTree::getCollisionDirection(const Rect &r1, const Rect &r2)
{
    float w = 0.5f * (r1.w + r2.w);
    float h = 0.5f * (r1.h + r2.h);
    float dx = (r1.x + r1.w / 2.0f) - (r2.x + r2.w / 2.0f);
    float dy = (r1.y + r1.h / 2.0f) - (r2.y + r2.h / 2.0f);

    float wy = w * dy;
    float hx = h * dx;

    if (wy > hx)
    {
        if (wy > -hx)
        {
            return DirectType::TOP; // r1 is on top of r2
        }
        else
        {
            return DirectType::RIGHT; // r1 is to the right of r2
        }
    }
    else
    {
        if (wy > -hx)
        {
            return DirectType::LEFT; // r1 is to the left of r2
        }
        else
        {
            return DirectType::BOTTOM; // r1 is below r2
        }
    }
}

void QTree::updateCollisions()
{
    if (!m_root)
        return;

    // 1. 备份上一帧的碰撞状态
    m_previousCollisions = m_currentCollisions;
    m_currentCollisions.clear();

    // 2. 清空并重建四叉树
    m_root->clear();
    for (auto &pair : m_allObjects)
    {
        m_root->insert(&pair.second);
    }

    // 3. 检测本帧的所有碰撞
    std::vector<ObjectData *> candidates;
    for (auto const &[idA, objA] : m_allObjects)
    {
        candidates.clear();
        m_root->retrieve(candidates, objA.rect);

        for (ObjectData *objB : candidates)
        {
            if (objA.id == objB->id || objA.id > objB->id)
            {
                continue;
            }

            if (objA.rect.intersects(objB->rect))
            {
                m_currentCollisions[objA.id].insert(objB->id);
                m_currentCollisions[objB->id].insert(objA.id);
            }
        }
    }

    // 4. 比较新旧状态，触发回调
    std::unordered_set<int> checkedIds;
    auto process_id = [&](int id)
    {
        if (checkedIds.count(id))
            return;
        checkedIds.insert(id);

        if (m_allObjects.find(id) == m_allObjects.end()) return; // 对象可能已被移除
        Role *roleA = m_allObjects.at(id).role;
        const auto &rectA = m_allObjects.at(id).rect;

        const auto prev_iter = m_previousCollisions.find(id);
        const auto curr_iter = m_currentCollisions.find(id);

        CollisionSet emptySet;
        const CollisionSet &prevSet = (prev_iter != m_previousCollisions.end()) ? prev_iter->second : emptySet;
        const CollisionSet &currSet = (curr_iter != m_currentCollisions.end()) ? curr_iter->second : emptySet;

        for (int otherId : currSet)
        {
            if (m_allObjects.find(otherId) == m_allObjects.end()) continue;
            Role *roleB = m_allObjects.at(otherId).role;
            const auto &rectB = m_allObjects.at(otherId).rect;
            // --- 关键改动 ---
            // 调用重命名后的函数
            DirectType dir = getCollisionDirection(rectA, rectB);

            if (prevSet.find(otherId) == prevSet.end())
            {
                roleA->onCollision(roleB, dir);
            }
            else
            {
                roleA->onCollisioning(roleB, dir);
            }
        }

        for (int otherId : prevSet)
        {
            if (m_allObjects.find(otherId) == m_allObjects.end()) continue;
            if (currSet.find(otherId) == currSet.end())
            {
                Role *roleB = m_allObjects.at(otherId).role;
                roleA->onCollisionOut(roleB);
            }
        }
    };

    for (auto const &[id, colls] : m_previousCollisions)
        process_id(id);
    for (auto const &[id, colls] : m_currentCollisions)
        process_id(id);
}

std::vector<std::pair<Role *, DirectType>> QTree::getCollisions(int roleId)
{
    std::vector<std::pair<Role *, DirectType>> result;
    if (m_currentCollisions.count(roleId))
    {
        const Rect &rectA = m_allObjects.at(roleId).rect;
        for (int otherId : m_currentCollisions.at(roleId))
        {
            const Rect &rectB = m_allObjects.at(otherId).rect;
            Role *roleB = m_allObjects.at(otherId).role;
             // --- 关键改动 ---
            // 调用重命名后的函数
            result.push_back({roleB, getCollisionDirection(rectA, rectB)});
        }
    }
    return result;
}