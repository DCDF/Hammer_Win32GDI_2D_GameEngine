#pragma once
#include "Point.h"

class Rect
{
public:
    Rect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
    float x, y, w, h;

    bool contains(Point &pofloat)
    {
        return !(pofloat.x < x || pofloat.x > x + w || pofloat.y < y || pofloat.y > y + h);
    }

    bool contains(Rect &other)
    {
        return !(x + w < other.x || x > other.x + other.w || y + h < other.y || y > other.y + other.h);
    }
};