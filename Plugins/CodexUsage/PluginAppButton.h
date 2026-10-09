#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>

namespace PluginAppButton
{
    inline RECT Bounds(HWND popup, int dpi)
    {
        RECT client{}; GetClientRect(popup, &client);
        return { client.right - MulDiv(192, dpi, 96), MulDiv(8, dpi, 96),
            client.right - MulDiv(40, dpi, 96), MulDiv(34, dpi, 96) };
    }
    inline void Draw(HWND popup, HDC dc, int dpi, bool codex)
    {
        RECT button = Bounds(popup, dpi);
        DrawFrameControl(dc, &button, DFC_BUTTON, DFCS_BUTTONPUSH);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(45, 49, 54));
        DrawTextW(dc, codex ? L"打开 Codex App" : L"打开 Qoder App", -1, &button,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    inline bool Click(HWND popup, int dpi, LPARAM position, bool codex)
    {
        const POINT point{ static_cast<short>(LOWORD(position)), static_cast<short>(HIWORD(position)) };
        const RECT button = Bounds(popup, dpi);
        if (!PtInRect(&button, point)) return false;
        HINSTANCE result{};
        if (codex)
            result = ShellExecuteW(popup, L"open", L"explorer.exe",
                L"shell:AppsFolder\\OpenAI.Codex_2p2nqsd0c76g0!App", nullptr, SW_SHOWNORMAL);
        else
        {
            wchar_t local[MAX_PATH]{};
            const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
            if (length > 0 && length < MAX_PATH)
            {
                const std::wstring path = std::wstring(local) + L"\\Programs\\Qoder\\Qoder.exe";
                result = ShellExecuteW(popup, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
        if (reinterpret_cast<INT_PTR>(result) <= 32)
            MessageBoxW(popup, L"无法打开应用，请确认已安装。", L"TrafficMonitor", MB_OK | MB_ICONWARNING);
        else ShowWindow(popup, SW_HIDE);
        return true;
    }
}
