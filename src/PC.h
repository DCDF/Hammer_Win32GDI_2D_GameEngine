#pragma once
#include <windows.h>

class PC
{
public:
    PC(HINSTANCE hInstance, int width, int height, const char *title);
    ~PC();

    HWND window() const { return hwnd_; }
    HDC dc() const { return backDC_; }

    void show();
    bool tick();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_;
    HDC backDC_;
    HBITMAP backBuffer_;
    int width_, height_;
};