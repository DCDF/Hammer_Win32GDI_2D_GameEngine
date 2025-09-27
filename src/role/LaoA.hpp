#pragma once
#include "../Role.h"

class LaoA : public Role
{
public:
    LaoA(int x, int y, int resId = 201, int imgW = 64, int imgH = 64, int row = 10, int col = 8, int w = 20, int h = 38) : Role(x, y, resId, imgW, imgH, row, col, w, h)
    {
        name = L"牢a";
        face = false;
        setProps(PropModel::roleProps(
            {
                {PropType::MAX_HP, 200},
                {PropType::HP, 200},
                {PropType::ATK, 15},
            }));

        addAnimation("idle", 0, 7, true, {});
        addAnimation("move", 7, 8, true, {});
        play("idle");
    }

    virtual bool isRight() override
    {
        return scaleX < 0;
    }
};