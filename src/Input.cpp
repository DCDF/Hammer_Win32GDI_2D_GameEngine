#include "Input.h"
#include <windowsx.h>
#include "Scene.h"
// 静态成员初始化
HWND Input::m_hwnd = nullptr;
std::unordered_map<int, bool> Input::keys;
std::unordered_map<int, bool> Input::m_prevKeys;
std::unordered_set<int> Input::m_pressedKeys;
POINT Input::point = {0, 0};
POINT Input::m_prevMousePos = {0, 0};
POINT Input::m_mouseDelta = {0, 0};
int Input::mouseButtons[3] = {0};
int Input::m_prevMouseButtons[3] = {0};
int Input::m_mouseWheelDelta = 0;

void Input::Initialize(HWND hwnd)
{
    m_hwnd = hwnd;

    // 初始化输入状态
    keys.clear();
    m_prevKeys.clear();
    m_pressedKeys.clear();

    // 初始化鼠标状态
    GetCursorPos(&point);
    ScreenToClient(m_hwnd, &point);
    m_prevMousePos = point;

    ZeroMemory(mouseButtons, sizeof(mouseButtons));
    ZeroMemory(m_prevMouseButtons, sizeof(m_prevMouseButtons));
}

void Input::Update()
{
    // 保存上一帧的状态
    m_prevKeys = keys;
    m_prevMousePos = point;
    for (int i = 0; i < 3; ++i)
    {
        m_prevMouseButtons[i] = mouseButtons[i];
    }

    // 更新鼠标位置
    POINT screenPos;
    GetCursorPos(&screenPos);
    point = screenPos;
    ScreenToClient(m_hwnd, &point);

    // 计算鼠标增量
    m_mouseDelta.x = point.x - m_prevMousePos.x;
    m_mouseDelta.y = point.y - m_prevMousePos.y;

    // 重置鼠标滚轮增量（每帧重置）
    m_mouseWheelDelta = 0;
}

void Input::Shutdown()
{
    keys.clear();
    m_prevKeys.clear();
    m_pressedKeys.clear();
}

void Input::ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_MOUSEMOVE:
        // 鼠标移动已在 Update() 中处理
        break;

    case WM_LBUTTONDOWN:
        mouseButtons[0] = 1;
        break;

    case WM_LBUTTONUP:
        mouseButtons[0] = 0;
        break;

    case WM_RBUTTONDOWN:
        mouseButtons[1] = 1;
        break;

    case WM_RBUTTONUP:
        mouseButtons[1] = 0;
        break;

    case WM_MBUTTONDOWN:
        mouseButtons[2] = 1;
        break;

    case WM_MBUTTONUP:
        mouseButtons[2] = 0;
        break;

    case WM_MOUSEWHEEL:
        HandleMouseWheel(wParam);
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        keys[wParam] = true;
        m_pressedKeys.insert(wParam);
        Scene::curScene->onKeyDown(wParam);
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        keys[wParam] = false;
        m_pressedKeys.erase(wParam);
        Scene::curScene->onKeyUp(wParam);
        break;
    }
}

void Input::HandleMouseWheel(WPARAM wParam)
{
    // 获取滚轮增量（120 为一个标准滚轮刻度）
    m_mouseWheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
}

bool Input::IsKeyDown(int keyCode)
{
    auto it = keys.find(keyCode);
    return it != keys.end() && it->second;
}

bool Input::IsKeyPressed(int keyCode)
{
    bool current = IsKeyDown(keyCode);
    bool previous = m_prevKeys.find(keyCode) != m_prevKeys.end() && m_prevKeys[keyCode];
    return current && !previous;
}

bool Input::IsKeyReleased(int keyCode)
{
    bool current = IsKeyDown(keyCode);
    bool previous = m_prevKeys.find(keyCode) != m_prevKeys.end() && m_prevKeys[keyCode];
    return !current && previous;
}

const std::unordered_set<int> &Input::GetPressedKeys()
{
    return m_pressedKeys;
}

bool Input::IsMouseButtonDown(int button)
{
    if (button < 0 || button >= 3)
        return false;
    return mouseButtons[button] != 0;
}

bool Input::IsMouseButtonPressed(int button)
{
    if (button < 0 || button >= 3)
        return false;
    return mouseButtons[button] && !m_prevMouseButtons[button];
}

bool Input::IsMouseButtonReleased(int button)
{
    if (button < 0 || button >= 3)
        return false;
    return !mouseButtons[button] && m_prevMouseButtons[button];
}

POINT Input::GetMousePosition()
{
    return point;
}

POINT Input::GetMouseDelta()
{
    return m_mouseDelta;
}

int Input::GetMouseWheelDelta()
{
    return m_mouseWheelDelta;
}