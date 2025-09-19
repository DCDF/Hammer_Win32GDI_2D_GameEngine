#pragma once
#include "../Scene.h"
#include "../GDI.h"
#include <vector>
#include "../Role.h"
#include "../Camera.h"
#include <ctime>
#include "../Audios.h"
#include "../KV.h"
#include "../PropModel.h"
#include "../Input.h"

extern int GAME_WIDTH;
extern int GAME_HEIGHT;
extern int GAME_OFFSET_X;
extern int GAME_LINE;
class GameScene : public Scene
{

protected:
    std::unique_ptr<Role> role;
    std::unique_ptr<Role> shuiyue;
    int floorX = 0;

public:
    void beforeEnter() override
    {
        Audios::bg(301);

        role = std::make_unique<Role>(201, 320, GAME_LINE, 64, 64, 10, 8, 32, 32);
        role->face = false;
        role->addAnimation("idle", 0, 7, true, {});
        role->addAnimation("move", 7, 8, true, {});
        role->play("idle");
        role->setFace(true);
        role->initProp(PropModel::roleProps({{PropType::MAX_HP, 200},
                                             {PropType::HP, 200},
                                             {PropType::ATK, 15}}));
        role->name = L"牢a";
        Camera::setTarget(role.get());
    }

    void enter() override
    {
    }

    void exit() override
    {
    }

    void render() override
    {
        if (GAME_OFFSET_X > floorX)
        {
            if (GAME_OFFSET_X > floorX + GAME_WIDTH)
            {
                floorX += GAME_WIDTH;
            }
            GDI::image(101, floorX + GAME_WIDTH, 0, GAME_WIDTH, GAME_HEIGHT);
        }

        if (GAME_OFFSET_X < floorX)
        {
            if (GAME_OFFSET_X < floorX - GAME_WIDTH)
            {
                floorX -= GAME_WIDTH;
            }
            GDI::image(101, floorX - GAME_WIDTH, 0, GAME_WIDTH, GAME_HEIGHT);
        }
        GDI::image(101, floorX, 0, GAME_WIDTH, GAME_HEIGHT);
        role->render();
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

        if (role->ground && Input::IsKeyDown(' '))
        {
            role->upSpeed = 200;
        }

        role->tick(deltaTime);
    }
};
