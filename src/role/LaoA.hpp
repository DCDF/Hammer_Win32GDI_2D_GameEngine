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

    virtual void onCollision(Role *other, int dir, bool from) override
    {
        other->flag += 1;
        if (from)
            return;
        onCollisioning(other, dir, from);
    }
    virtual void onCollisioning(Role *other, int dir, bool from) override
    {
        if (from)
            return;

        if (dir == 0)
        {
            auto value = other->w / 2 + w / 2;
            if ((other->x - value) < x)
            {
                other->x = (x + value);
            }
            other->lockHandVec->k = -1;
        }
        else if (dir == 1)
        {
            auto value = other->w / 2 + w / 2;
            if ((other->x + value) > x)
            {
                other->x = (x - value);
            }

            other->lockHandVec->k = 1;
        }
        else if (dir == 3)
        {
            if (other->y <= y - h + 3)
            {
                other->y = (y - h);
                other->otherLine = y - h;
            }
        }
    }
    virtual void onCollisionOut(Role *other, bool from) override
    {
        other->flag -= 1;
        other->lockHandVec->clear();
        if (other->flag == 0)
        {
            other->otherLine = 0;
        }
    }
};