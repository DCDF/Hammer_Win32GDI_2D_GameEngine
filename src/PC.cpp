#include "PC.h"

PC::PC(HINSTANCE hInstance, int width, int height, const char* title)
    : hwnd_(nullptr), width_(width), height_(height) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;  // 直接关联静态成员函数
    wc.hInstance = hInstance;
    wc.lpszClassName = "PCWindowClass";
    RegisterClass(&wc);

    RECT rc = {0, 0, width_, height_};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int winWidth = rc.right - rc.left;
    int winHeight = rc.bottom - rc.top;

    hwnd_ = CreateWindowEx(0, "PCWindowClass", title,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        winWidth, winHeight, nullptr, nullptr, hInstance, nullptr);
}

PC::~PC() {
    if (hwnd_) DestroyWindow(hwnd_);
}

void PC::show() {
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
}

bool PC::tick() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

// 静态窗口过程
LRESULT CALLBACK PC::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}