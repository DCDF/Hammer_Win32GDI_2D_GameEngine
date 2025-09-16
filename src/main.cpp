#include "PC.h"
#include "Draw.h"
#include "Audio.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    PC pc(hInstance, 640, 160, "test");
    pc.show();

    Draw draw(pc);

    draw.showFPS(true);

    Audio_bg(301);

    int count = 0;

    Draw::GridSprite heroSprite(201, 64, 64, 10, 8, -32, -64);
    int frame = 0;
    while (pc.tick())
    {
        draw.begin();

        draw.image(101, 0, 0, true);
        draw.drawGridSprite(heroSprite, frame, 0, 128, true);
        draw.rect(0, 0, 10, 10);  // 默认绿色填充矩形
        draw.text(L"XX", 10, 30); // 默认红色、字号16、居中

        draw.end();

        // frame = (frame + 1) % (heroSprite.cols * heroSprite.rows);
    }
}
