#include "PC.h"
#include "Draw.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    PC pc(hInstance, 640, 160, "test");
    pc.show();

    Draw draw(pc);

    draw.showFPS(true);

    while (pc.tick())
    {
        draw.begin();

        draw.image(101, 0, 0, true);
        // draw.drawGridSprite(heroSprite, frame, 100, 100);
        draw.rect(50, 50, 100, 50);          // 默认绿色填充矩形
        draw.text(L"Hello World", 400, 300); // 默认红色、字号16、居中

        draw.end();

        // frame = (frame + 1) % (heroSprite.cols * heroSprite.rows);
    }
}
