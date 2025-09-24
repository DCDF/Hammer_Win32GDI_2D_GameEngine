#pragma once
#include "../Role.h"

class PlatForm : public Role
{
public:
    PlatForm(int resId, int x, int y, int imgW, int imgH, int row, int col, int w, int h) : Role(resId, x, y, imgW, imgH, row, col, w, h)
    {
        name = L"板板";
        gravity = 0;
    }
};