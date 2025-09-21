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
        if (int(dir) == int(Direction::DOWN) && other->upSpeed <= 0)
        { 
        other->y = y - other->h;
        other->line = y - other->h;
        other->ground = true;
        other->downSpeed = 0;
        other->upSpeed = 0;
        }
        other->flag += 1;
    }
    virtual void onCollisioning(Role *other, Direction dir) override
    {
        if (int(dir) == int(Direction::DOWN)&& other->upSpeed <= 0)
        { 
        other->y = y - other->h;
        other->line = y - other->h;
        other->ground = true;
        other->downSpeed = 0;
        other->upSpeed = 0;
        }
    }
    virtual void onCollisionOut(Role *other) override
    {
       other->flag -= 1;
    // 重置为默认地面线
    }
};