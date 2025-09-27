#include "PC.h"
#include "GDI.h"
#include <chrono>
#include "Camera.h"
#include "Scene.h"
#include "scene/StartScene.hpp"
#include "scene/GameScene.hpp"
#include "Input.h"
#include <iostream>

extern int GAME_WIDTH;
extern int GAME_HEIGHT;
extern int GAME_LINE;
extern int GAME_OFFSET_X;
extern int GAME_OFFSET_Y;

// 使用 steady_clock（单调时钟，不受系统时间调整影响）
using Clock = std::chrono::high_resolution_clock; // 或 steady_clock
using TimePoint = std::chrono::time_point<Clock>;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    PC pc(hInstance, GAME_WIDTH, GAME_HEIGHT, "Hammer");
    pc.show();

    Input::Initialize(pc.window());
    GDI::init(pc.window());

    TimePoint prev_time = Clock::now();

    int tickCount = 0;
    int fps = 0;
    float secondsFps = 1.0;
    bool running = true;
    std::wstring fpsText = L"0";

    QuadTree::WORLD = std::make_unique<QuadTree>(QuadTreeRect(-100, -100, WORLD_RIGHT * 1.5, GAME_HEIGHT * 1.5), 4);

    // 设置碰撞回调

    Scene::change(std::make_unique<GameScene>());
    while (running)
    {
        if (!pc.tick())
            break;
        TimePoint current_time = Clock::now();
        // 计算帧间隔 dt（秒，浮点型）
        float dt = std::chrono::duration<float>(current_time - prev_time).count();
        prev_time = current_time;

        secondsFps -= dt;
        tickCount++;
        if (secondsFps <= 0)
        {
            secondsFps = 1;
            fpsText = L" FPS:" + std::to_wstring(fps);
            fps = tickCount;
            tickCount = 0;
        }
        if (dt > 0.33)
        {
            dt = 0.33;
        }
        QuadTree::WORLD->tick(dt);
        Input::Update();
        Scene::curScene->tick(dt);
        GDI::begin(dt);
        GAME_OFFSET_X = Camera::getOffsetX();
        GAME_OFFSET_Y = Camera::getOffsetY();
        GDI::setCamera(GAME_OFFSET_X, GAME_OFFSET_Y);
        Scene::curScene->render();
        GDI::tick(dt);

        GDI::setCamera(0, 0);
        Scene::curScene->renderGlobal();
        GDI::text(fpsText, 10, 10);
        GDI::flush(dt);
    }
    // 保证场景内对象清理在tree之前
    Scene::change(nullptr);
    GDI::end();
    Input::Shutdown();
    return 0;
}