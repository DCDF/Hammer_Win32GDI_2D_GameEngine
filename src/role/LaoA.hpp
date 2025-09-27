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
        float leftX = x - w / 2;
        float otherLeftX = other->x - other->w / 2;
        float offset = 3;

        if (std::abs(other->y - y) >= other->h / 2 + h / 2 - offset)
        {
            // 上/下
            if (other->y < y)
            {
                // 上
                other->otherLine = y - h;
            }
            else
            {
                other->lockHandVec->p = 1;
                // 下
            }
        }
        else
        {
            other->lockHandVec->k = otherLeftX > leftX ? -1 : 1;
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