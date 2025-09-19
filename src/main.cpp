#include "PC.h"
#include "GDI.h"
// #include "Audio.h"
#include <chrono>
#include "Audios.h"

extern int GAME_WIDTH;
extern int GAME_HEIGHT;

// 使用 steady_clock（单调时钟，不受系统时间调整影响）
using Clock = std::chrono::high_resolution_clock; // 或 steady_clock
using TimePoint = std::chrono::time_point<Clock>;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    PC pc(hInstance, GAME_WIDTH, GAME_HEIGHT, "Hammer");
    pc.show();

    GDI::init(pc.window());
    Audios::bg(301);
    // Audio_bg(301);

    TimePoint prev_time = Clock::now();

    int tickCount = 0;
    int fps = 0;
    float secondsFps = 1.0;
    bool running = true;
    std::wstring fpsText = L"0";
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
        GDI::begin(dt);
        GDI::tick(dt);

        GDI::setCamera(-320, 0);

        GDI::text(fpsText, 10, 10);

        GDI::image(101, 0, 0, 640, 160);
        int next = 1;
        GDI::imageEx(201, 0, 96, 64, 64, true, 0, 0, 64, 64);
        GDI::imageEx(201, 0, 32, 64, 64, false, 0, 0, 64, 64);
        // GDI::imageEx(101, 140, 96, 64, 64, false, 576, 96, 64, 64);

        GDI::flush(dt);
    }

    GDI::end();
    // Audio_shutdown();
    return 0;
}