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

    virtual void onCollision(Role *other, DirectType dir) override
    {
        if (dir == DirectType::TOP && other->y <= y - h + 3)
        {
            other->otherLine = y - h;
        }
        other->flag += 1;
    }
    virtual void onCollisioning(Role *other, DirectType dir) override
    {
        if (dir == DirectType::TOP && other->y <= y - h + 3)
        {
            other->otherLine = y - h;
        }
    }
    virtual void onCollisionOut(Role *other) override
    {
        other->flag -= 1;
        // 重置为默认地面线
        other->otherLine = 0;
    }
};