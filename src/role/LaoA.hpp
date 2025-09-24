#pragma once
#include "../Role.h"

class LaoA : public Role
{
public:
    LaoA(int resId, int x, int y, int imgW, int imgH, int row, int col, int w, int h) : Role(resId, x, y, imgW, imgH, row, col, w, h)
    {
        name = L"牢a";

        setProps(PropModel::roleProps(
            {
                {PropType::MAX_HP, 200},
                {PropType::HP, 200},
                {PropType::ATK, 15},
            }));
        face = false;

        addAnimation("idle", 0, 7, true, {});
        addAnimation("move", 7, 8, true, {});
        play("idle");
    }

    virtual void onCollision(Role *other, int dir) override
    {
        if (dir == 2 && other->y <= y - h + 3)
        {
            other->otherLine = y - h;
        }
        else if (dir == 1)
        {
            other->lockHandVec->k = -1;
        }
        else if (dir == 0)
        {
            other->lockHandVec->k = 1;
        }
        other->flag += 1;
    }
    virtual void onCollisioning(Role *other, int dir) override
    {
        if (dir == 2 && other->y <= y - h + 3)
        {
            other->otherLine = y - h;
        }
    }
    virtual void onCollisionOut(Role *other) override
    {
        other->flag -= 1;
        other->otherLine = 0;
        other->lockHandVec->clear();
    }
};