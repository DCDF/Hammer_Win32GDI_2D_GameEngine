#pragma once
#include "../Role.h"

class PlatForm : public Role
{
public:
    PlatForm(int resId, int x, int y, int imgW, int imgH, int row, int col, int w, int h) : Role(resId, x, y, imgW, imgH, row, col, w, h)
    {
        name = L"板板";
        gravity = 0;
    }

    virtual void onCollision(Role *other, Direction dir) override
    {
        if (int(dir) == int(Direction::UP))
        { 
            other->y = y;
            other->line = static_cast<int>(y - h/2);
        }
        other->flag += 1;
    }
    virtual void onCollisioning(Role *other, Direction dir) override
    {
        if (int(dir) == int(Direction::UP))
        { 
            other->line = y;
        }
    }
    virtual void onCollisionOut(Role *other) override
    {
        bool a = false;
        other->flag -= 1;
    }
};