#pragma once
#include <windows.h>
#include <unordered_map>
#include <unordered_set>
#include <string>

class Input
{
public:
    // 初始化输入系统
    static void Initialize(HWND hwnd);
    // 更新输入状态（应在每帧开始时调用）
    static void Update();
    // 清理资源
    static void Shutdown();

    // 键盘查询接口
    static bool IsKeyDown(int keyCode);
    static bool IsKeyPressed(int keyCode);
    static bool IsKeyReleased(int keyCode);
    static const std::unordered_set<int> &GetPressedKeys();

    // 鼠标查询接口
    static bool IsMouseButtonDown(int button);
    static bool IsMouseButtonPressed(int button);
    static bool IsMouseButtonReleased(int button);
    static POINT GetMousePosition();
    static POINT GetMouseDelta();
    static int GetMouseWheelDelta();

    // 处理窗口消息（在窗口过程中调用）
    static void ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 输入状态存储（按你的要求）
    static std::unordered_map<int, bool> keys;
    static POINT point;
    static int mouseButtons[3];

private:
    static HWND m_hwnd;
    static std::unordered_map<int, bool> m_prevKeys;
    static std::unordered_set<int> m_pressedKeys;

    // 鼠标状态
    static POINT m_prevMousePos;
    static POINT m_mouseDelta;
    static int m_mouseWheelDelta;
    static int m_prevMouseButtons[3];

    // 处理鼠标滚轮消息
    static void HandleMouseWheel(WPARAM wParam);
};