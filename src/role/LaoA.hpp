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
};