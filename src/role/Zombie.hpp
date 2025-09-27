#pragma once
#include "LaoA.hpp"
#include "KV.h"
class Zombie : public LaoA
{
public:
    double think = 0.5;
    int dir = 0;
    int speed = 0;
    bool revertSpeed = false;
    Zombie(int x, int y) : LaoA(x, y)
    {
        speed = rand() % 50 + 10;
    }
    void tick(double deltaTime) override
    {
        think -= deltaTime;
        if (think <= 0)
        {
            revertSpeed = false;
            dir = rand() % 2;
            int addMax = 700;
            double addThink = rand() % addMax / addMax;
            think = 0.3 + addThink / 1000.0;
            if (preVec->k == 0)
            {
                int rd = rand() % 10;
                if (rd == 0)
                {
                    jump();
                }
                else if (rd > 8)
                {
                    revertSpeed = true;
                }
            }
        }
        handVec->k = revertSpeed ? speed : -speed;
        Role::tick(deltaTime);
    }
};