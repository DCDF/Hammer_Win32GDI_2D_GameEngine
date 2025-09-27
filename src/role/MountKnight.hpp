#pragma once
#include "../Role.h"
#include "../PropModel.h"
#include "../KV.h"

class MountKnight : public Role
{
public:
    MountKnight(int x, int y, int resId = 202, int imgW = 128, int imgH = 111, int row = 10, int col = 6, int w = 50, int h = 45) : Role(x, y, resId, imgW, imgH, row, col, w, h)
    {
        name = L"牢a";

        setProps(PropModel::roleProps(
            {
                {PropType::MAX_HP, 200},
                {PropType::HP, 200},
                {PropType::ATK, 15},
            }));

        addAnimation("idle", 0, 10, true, {});
        addAnimation("move", 16, 7, true, {});
        play("idle");
    }
    virtual void onCollisioning(Role *other, int dir, bool from) override
    {
        if (from)
        {
        }
        else
        {
            Role::onCollisioning(other, dir, from);
        }

        float offset = 3;
        float dis = std::abs(other->y - y);
        if ((other->y < y && dis >= h - offset) || (other->y > y && dis >= other->h - offset))
        {
        }
        else
        {
            if (other->x > x)
            {
                other->otherVec->k = 100;
                other->lockHandVec->k = -1;
            }
            else if (other->x < x)
            {
                other->otherVec->k = -100;
                other->lockHandVec->k = 1;
            }
        }
    }
};
