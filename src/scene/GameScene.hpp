#pragma once
#include "../Scene.h"
#include "../GDI.h"
#include <vector>
#include "../Role.h"
#include "../Camera.h"
#include <ctime>
#include "../Audios.h"
#include "../KV.h"
#include "../Input.h"
#include "../QTree.h"
#include "../role/LaoA.hpp"
#include "../role/PlatForm.hpp"

extern int GAME_WIDTH;
extern int GAME_HEIGHT;
extern int GAME_OFFSET_X;
extern int GAME_LINE;
extern int WORLD_LEFT;
extern int WORLD_RIGHT;
class GameScene : public Scene
{

protected:
    std::vector<std::unique_ptr<Role>> roleVec;
    Role *role;
    int floorX = 0;

public:
    void beforeEnter() override
    {
        Audios::bg(301);

        roleVec.emplace_back(std::make_unique<LaoA>(201, 64, GAME_LINE, 64, 64, 10, 8, 20, 32));
        role = roleVec.back().get();
        // roleVec.emplace_back(std::make_unique<LaoA>(201, 128, GAME_LINE, 64, 64, 10, 8, 20, 32));
        // roleVec.emplace_back(std::make_unique<LaoA>(201, 192, GAME_LINE, 64, 64, 10, 8, 20, 32));
        // roleVec.emplace_back(std::make_unique<LaoA>(201, 200, GAME_LINE, 64, 64, 10, 8, 20, 32));
        // roleVec.emplace_back(std::make_unique<LaoA>(201, 250, GAME_LINE, 64, 64, 10, 8, 20, 32));
        // roleVec.emplace_back(std::make_unique<LaoA>(201, 300, GAME_LINE, 64, 64, 10, 8, 20, 32));
        
        roleVec.emplace_back(std::make_unique<PlatForm>(201, 400, 300, 64, 64, 10, 8, 20, 32));

        Camera::setTarget(role);
    }

    void enter() override
    {
    }

    void exit() override
    {
    }

    void render() override
    {
        // if (GAME_OFFSET_X > floorX)
        // {
        //     if (GAME_OFFSET_X > floorX + GAME_WIDTH)
        //     {
        //         floorX += GAME_WIDTH;
        //     }
        //     GDI::image(101, floorX + GAME_WIDTH, GAME_HEIGHT - 160, GAME_WIDTH, 160);
        // }

        // if (GAME_OFFSET_X < floorX)
        // {
        //     if (GAME_OFFSET_X < floorX - GAME_WIDTH)
        //     {
        //         floorX -= GAME_WIDTH;
        //     }
        //     GDI::image(101, floorX - GAME_WIDTH, GAME_HEIGHT - 160, GAME_WIDTH, 160);
        // }
        // GDI::image(101, floorX,GAME_HEIGHT - 160, GAME_WIDTH, 160);
        GDI::imageWorld(101, 0, 0);
        int count = 0;
        for (auto &info : QTree::getCollision(role->id))
        {
            GDI::text(std::to_wstring(info.otherId) + L"方向" + std::to_wstring(int(info.dir)), 320, count++ * 20);
        }
        count = 0;
        for (auto &role : roleVec)
        {
            role->render();
            if (!role->outSide)
            {
                count++;
            }
        }
        GDI::text(L"渲染个数" + std::to_wstring(count), GAME_OFFSET_X + 80, 50);
        GDI::text(L"ground" + std::to_wstring(role->ground) + L" line"+ std::to_wstring(role->line) + L" flag"+std::to_wstring(role->flag), GAME_OFFSET_X + 160, 50);
    }

    void renderGlobal() override
    {
    }

    void tick(double deltaTime) override
    {
        if (Input::IsKeyDown('A'))
        {
            role->handVec->k -= 100;
        }
        else if (Input::IsKeyDown('D'))
        {
            role->handVec->k += 100;
        }
        else if (Input::IsKeyDown('F'))
        {
            GAME_LINE = 50;
        }
        else if (Input::IsKeyDown('G'))
        {
            GAME_LINE = 100;
        }

        if (role->ground && Input::IsKeyDown(' '))
        {
            role->upSpeed = role->getProp(PropType::JUMP_SPEED);
        }

        for (auto &role : roleVec)
        {
            role->tick(deltaTime);
        }
    }
};
