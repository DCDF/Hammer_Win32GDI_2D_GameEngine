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

        // for (int i = 0; i < 5; i++)
        // {
        //     roleVec.emplace_back(std::make_unique<LaoA>(201, rand() % WORLD_RIGHT, GAME_LINE, 64, 64, 10, 8, 20, 32));
        // }
        roleVec.emplace_back(std::make_unique<LaoA>(201, 150, GAME_LINE, 64, 64, 10, 8, 20, 32));

        Camera::setTarget(role);
    }

    void onKeyDown(int key) override
    {

        if (role->ground && Input::IsKeyDown(' '))
        {
            role->upSpeed = role->getProp(PropType::JUMP_SPEED);
        }
    }

    void enter() override
    {
    }

    void exit() override
    {
    }

    void render() override
    {

        // GDI::imageWorld(101, 0, 0);
        GDI::text(L"flag " + std::to_wstring(role->flag), 60, 60);
        for (auto &role : roleVec)
        {
            role->render();
        }
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
        for (auto &role : roleVec)
        {
            role->tick(deltaTime);
        }
    }
};
