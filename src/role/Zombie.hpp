#pragma once
#include "LaoA.hpp"
#include "KV.h"
class Zombie : public LaoA
{
public:
    double think = 0.5;
    int dir = 0;

    Zombie(int resId, int x, int y, int imgW, int imgH, int row, int col, int w, int h) : LaoA(resId, x, y, imgW, imgH, row, col, w, h)
    {
    }
    void tick(double deltaTime) override
    {
        think -= deltaTime;
        if (think <= 0)
        {
            dir = rand() % 2;
            int addMax = 700;
            double addThink = rand() % addMax / addMax;
            think = 0.3 + addThink / 1000.0;
            if (preVec->k == 0)
            {
                if (rand() % 3 == 0)
                {
                    upSpeed = 250;
                }
            }
        }
        handVec->k = dir == 0 ? -30 : 30;
        Role::tick(deltaTime);
    }
};