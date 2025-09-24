#include "QuadTreeRect.h"
#include <memory>
// 更新阈值
const float QuadTreeRect::UPDATE_THRESHOLD = 2.0;
QuadTreeRect::QuadTreeRect(float x, float y, float w, float h, int id, void *val) : x(x), y(y), w(w), h(h), id(id), val(val), parent(nullptr), centerX(x + w / 2), centerY(y + h / 2), lastUpdateX(x), lastUpdateY(y)
{
}

QuadTreeRect::QuadTreeRect(const QuadTreeRect &other)
    : x(other.x), y(other.y), w(other.w), h(other.h),
      id(other.id), val(other.val), parent(other.parent),
      centerX(other.centerX), centerY(other.centerY) {}

bool QuadTreeRect::contains(QuadTreeRect *other)
{
    return !(x + w < other->x || x > other->x + other->w || y + h < other->y || y > other->y + other->h);
}

bool QuadTreeRect::inBound(float centerX, float centerY)
{
    return (x <= centerX && centerX <= x + w && y <= centerY && centerY <= y + h);
}

bool QuadTreeRect::needReInsert()
{
    if (parent == nullptr)
        return true;
    return !parent->inBound(centerX, centerY);
}

bool QuadTreeRect::update()
{
    centerX = x + w / 2;
    centerY = y + h / 2;
    if (std::abs(x - lastUpdateX) > UPDATE_THRESHOLD || std::abs(y - lastUpdateY) > UPDATE_THRESHOLD)
    {
        lastUpdateX = x;
        lastUpdateY = y;
        return true;
    }
    return false;
}

void QuadTreeRect::onCollision(QuadTreeRect *other, int dir)
{
    onCollisionCallBack(other, dir);
}

void QuadTreeRect::onCollisionOut(QuadTreeRect *other)
{
    onCollisionOutCallBack(other);
}

void QuadTreeRect::onCollisioning(QuadTreeRect *other, int dir)
{
    onCollisioningCallBack(other, dir);
}

int QuadTreeRect::getDir(QuadTreeRect *other)
{
    int dir = 0; // todo 0左 1右 2上 3下
    float dx = centerX - other->centerX;
    float dy = centerY - other->centerY;
    if (std::abs(dx) > std::abs(dy))
    {
        dir = (dx > 0) ? 0 : 1; // 左或右
    }
    else
    {
        dir = (dy > 0) ? 2 : 3; // 上或下
    }
    return dir;
}