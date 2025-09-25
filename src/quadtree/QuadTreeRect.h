#pragma once
#include <unordered_set>
#include <unordered_map>
#include <functional>

#include "QuadTreeCollisionInfo.h"

class QuadTreeRect
{
public:
    QuadTreeRect(float x, float y, float w, float h, int id = 0, void *val = nullptr);
    QuadTreeRect(const QuadTreeRect &other);
    int id;
    float x, y, w, h, centerX, centerY;
    float lastUpdateX, lastUpdateY;
    bool updateFlag = false;
    static const float UPDATE_THRESHOLD;
    QuadTreeRect *parent;
    void *val;

    std::function<void(void *, int, bool)> onCollisionCallBack;
    std::function<void(void *, int, bool)> onCollisioningCallBack;
    std::function<void(void *, bool)> onCollisionOutCallBack;
    bool contains(QuadTreeRect *other);

    bool inBound(float centerX, float centerY);
    bool needReInsert();
    bool update();
    int getDir(QuadTreeRect *other);
};