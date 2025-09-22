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
#include <vector>
#include "GameScene.hpp"

extern int GAME_WIDTH;
extern int GAME_HEIGHT;
class StartScene : public Scene
{

protected:
    std::vector<double> hitArr;
    int index = 0;
    double pass = 0.0;
    double shanTime = 0;
    double resetTime = 44;

    double fontHit = 0;
    std::wstring *text = nullptr;
public:
    void beforeEnter() override
    {
    }

    void enter() override
    {

        Audios::bg(302);

        text = std::move(new std::wstring(L"press any key!"));
        hitArr.clear();
        hitArr.push_back(2.9);
        hitArr.push_back(4.3);
        hitArr.push_back(4.5);
        hitArr.push_back(5.9);
        hitArr.push_back(7.1);
        hitArr.push_back(7.3);
        hitArr.push_back(7.4);
        hitArr.push_back(45);
        pass = 0.0;
    }

    void exit() override
    {
    }

    void render() override
    {
        if(index >= 6){
            GDI::image(402, 320, 0, GAME_HEIGHT, GAME_HEIGHT);
        }
        GDI::image(401, 0, 0, GAME_HEIGHT, GAME_HEIGHT);
        if(fontHit > 0.2){
            GDI::text(*text, GAME_WIDTH/2 - 90,GAME_HEIGHT - 50,20.0,Gdiplus::Color::WhiteSmoke);
        }
        if(shanTime > 0){
            GDI::rect(0, 0, GAME_WIDTH, GAME_HEIGHT, Gdiplus::Color(180, 255, 255, 255));
        }
    }

    void renderGlobal() override
    {
    }

    void tick(double deltaTime) override
    {
        pass += deltaTime;
        shanTime -= deltaTime;
        fontHit -= deltaTime;
        if(fontHit <= 0){
            if(index < 6){
                fontHit = 0.4;
            }else{
                fontHit = 0.3;
            }
        }
        if (index < hitArr.size() && pass > hitArr[index])
        {
            index++;
            shanTime = 0.1;
        }
        if(pass >= resetTime){
            index = 0;
            pass = 0.0;
            Audios::stopBg();
            Audios::bg(302);
        }
        if(Input::GetPressedKeys().size() > 0){
            Scene::change(std::make_unique<GameScene>());
        }
    }
};
