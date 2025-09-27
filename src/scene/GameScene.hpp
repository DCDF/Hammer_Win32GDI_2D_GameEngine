#pragma once
#include "../Scene.h"
#include "../GDI.h"
#include <vector>
#include <memory>
#include "../Role.h"
#include "../Camera.h"
#include <ctime>
#include "../Audios.h"
#include "../KV.h"
#include "../Input.h"
#include "../role/LaoA.hpp"
#include "../role/Zombie.hpp"
#include "../role/MountKnight.hpp"
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

    double zombieTime = 0;
    int max = 0;

public:
    void beforeEnter() override
    {
        Audios::bg(301);

        roleVec.emplace_back(std::make_unique<MountKnight>(150, GAME_LINE - 200));
        role = roleVec.back().get();
        roleVec.emplace_back(std::make_unique<LaoA>(150, GAME_LINE));
        Camera::setTarget(role);
    }

    void onKeyDown(int key) override
    {

        if (role->ground && Input::IsKeyDown(' '))
        {
            role->jump();
        }
        else if (Input::IsKeyDown('R'))
        {
            max = 10;
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

        GDI::imageWorld(101, 0, 0);
        // auto collisionList = QuadTree::WORLD->collisionListCache[role->rect.get()];
        // int count = 0;
        // for (auto &info : collisionList)
        // {
        //     count++;
        //     uint64_t pairId = (role->id < info->id)
        //                           ? (static_cast<uint64_t>(role->id) << 32) | info->id
        //                           : (static_cast<uint64_t>(info->id) << 32) | role->id;
        //     auto each = QuadTree::WORLD->collisionCache[pairId].get();
        //     GDI::text(L"from " + std::to_wstring(each->dir), GAME_OFFSET_X + 120, count * 40);
        // }
        GDI::text(L"flag " + std::to_wstring(role->flag), 60, 60);
        for (auto &role : roleVec)
        {
            role->render();
        }
        // int testId = 24;
        // auto testRect = QuadTree::WORLD->cache[testId];
        // GDI::text(L"debugRect " + std::to_wstring(testRect->x) + L"," + std::to_wstring(testRect->y) + L"," + std::to_wstring(testRect->w) + L"," + std::to_wstring(testRect->h), GAME_OFFSET_X + 120, 100);
        // GDI::text(L"myRect " + std::to_wstring(role->rect->x) + L"," + std::to_wstring(role->rect->y) + L"," + std::to_wstring(role->rect->w) + L"," + std::to_wstring(role->rect->h), GAME_OFFSET_X + 120, 130);
        // GDI::text(L"ground " + std::to_wstring(role->ground), GAME_OFFSET_X + 120, 130);
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

        zombieTime -= deltaTime;

        if (zombieTime <= 0 && max > 0)
        {
            max--;
            roleVec.emplace_back(std::make_unique<Zombie>(role->x, GAME_LINE));
            zombieTime = 0.5;
        }

        for (auto &role : roleVec)
        {

            role->tick(deltaTime);
        }
    }
};
