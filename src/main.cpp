#include "PC.h"
// #include "Draw.h"
#include "Audio.h"
#include <windows.h>
#include "GDI.h"
static long long getTimestampQPC()
{
    LARGE_INTEGER frequency, time;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&time);
    return (long long)((time.QuadPart * 1000) / frequency.QuadPart);
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    PC pc(hInstance, 640, 160, "test");
    pc.show();

    GDI gdi(pc.window());

    gdi.init();
    // Draw draw(pc);
    // draw.showFPS(true);

    Audio_bg(301);

    int count = 0;

    // Draw::GridSprite heroSprite(201, 64, 64, 10, 8, -32, -64);
    int frame = 0;
    double frameDt = 100;
    long long pre = getTimestampQPC();
    while (pc.tick())
    {
        // draw.begin();

        // draw.image(101, 0, 0, true);
        // draw.drawGridSprite(heroSprite, frame, 50, 128, true);
        // draw.rect(0, 0, 10, 10);
        // draw.text(L"XX", 10, 30);

        // draw.end();
        gdi.tick();
        // long long time = getTimestampQPC();
        // long long dt = time - pre;
        // pre = time;
        // frameDt -= dt;
        // if (frameDt <= 0)
        // {
        //     frame = (frame + 1) % (heroSprite.cols * heroSprite.rows);
        //     frameDt = 200;
        // }
    }
    gdi.end();
}
