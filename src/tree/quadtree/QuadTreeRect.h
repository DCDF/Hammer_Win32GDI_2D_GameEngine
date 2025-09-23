#pragma once

class QuadTreeRect
{
public:
    QuadTreeRect(float x, float y, float w, float h, int id = 0, void *val = nullptr) : x(x), y(y), w(w), h(h), id(id), val(val) {}
    int id;
    float x, y, w, h;
    void *val;
    bool contains(QuadTreeRect &other)
    {
        return !(x + w < other.x || x > other.x + other.w || y + h < other.y || y > other.y + other.h);
    }
};