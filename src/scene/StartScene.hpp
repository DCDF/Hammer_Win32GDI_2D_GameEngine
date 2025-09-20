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

extern int GAME_WIDTH;
extern int GAME_HEIGHT;
class StartScene : public Scene
{

protected:
    std::vector<double> hitArr;
    int index = 0;
    double pass = 0.0;
    double shanTime = 0;
public:
    void beforeEnter() override
    {
    }

    void enter() override
    {

        Audios::bg(302);

        hitArr.clear();
        hitArr.push_back(1.85);
        hitArr.push_back(3.5);
        hitArr.push_back(4.0);
        pass = 0.0;
    }

    void exit() override
    {
    }

    void render() override
    {
        if(shanTime > 0){
        }
    }

    void renderGlobal() override
    {
    }

    void tick(double deltaTime) override
    {
        pass += deltaTime;
        shanTime -= deltaTime;
        if (index < hitArr.size() && pass > hitArr[index])
        {
            index++;
            shanTime = 0.3;
        }
        if (Input::IsKeyDown('A'))
        {
        }
    }
};
