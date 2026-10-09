#pragma once
#include <windows.h>
#include <commctrl.h>
#include <vector>

// Tooltip activation is shared across DLLs via a window property. Two open
// plugin popups must not re-enable the host tooltip when only one closes.
class PluginTooltipGuard
{
public:
    void Acquire(HWND owner)
    {
        Release();
        if (!IsWindow(owner)) return;
        Context context{ owner, this };
        EnumThreadWindows(GetWindowThreadProcessId(owner, nullptr), Collect,
            reinterpret_cast<LPARAM>(&context));
    }

    void Release()
    {
        for (HWND tooltip : m_tooltips)
        {
            if (!IsWindow(tooltip)) continue;
            RemoveWindowSubclass(tooltip, SuppressShow, reinterpret_cast<UINT_PTR>(this));
            const auto count = reinterpret_cast<ULONG_PTR>(GetPropW(tooltip, Property()));
            if (count > 1) SetPropW(tooltip, Property(), reinterpret_cast<HANDLE>(count - 1));
            else if (count == 1)
            {
                RemovePropW(tooltip, Property());
                SendMessageW(tooltip, TTM_ACTIVATE, TRUE, 0);
            }
        }
        m_tooltips.clear();
    }

private:
    static LRESULT CALLBACK SuppressShow(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
    {
        if (message == WM_WINDOWPOSCHANGING)
            reinterpret_cast<WINDOWPOS*>(lp)->flags &= ~SWP_SHOWWINDOW;
        // Discard pending tooltip timers while a plugin detail is open.
        if (message == WM_TIMER) return 0;
        return DefSubclassProc(window, message, wp, lp);
    }
    struct Context { HWND owner; PluginTooltipGuard* guard; };
    static const wchar_t* Property() { return L"TrafficMonitor.PluginTooltipSuppressionCount"; }
    static BOOL CALLBACK Collect(HWND window, LPARAM param)
    {
        auto& context = *reinterpret_cast<Context*>(param);
        wchar_t class_name[64]{};
        GetClassNameW(window, class_name, 64);
        if (lstrcmpiW(class_name, L"tooltips_class32") != 0) return TRUE;
        bool matches = GetWindow(window, GW_OWNER) == context.owner;
        const int tools = static_cast<int>(SendMessageW(window, TTM_GETTOOLCOUNT, 0, 0));
        for (int index = 0; !matches && index < tools; ++index)
        {
            TOOLINFOW info{ TTTOOLINFOW_V1_SIZE };
            if (SendMessageW(window, TTM_ENUMTOOLSW, index, reinterpret_cast<LPARAM>(&info)))
                matches = info.hwnd == context.owner;
        }
        if (matches)
        {
            const auto count = reinterpret_cast<ULONG_PTR>(GetPropW(window, Property()));
            if (SetPropW(window, Property(), reinterpret_cast<HANDLE>(count + 1)))
            {
                context.guard->m_tooltips.push_back(window);
                SetWindowSubclass(window, SuppressShow, reinterpret_cast<UINT_PTR>(context.guard), 0);
                SendMessageW(window, TTM_POP, 0, 0);
                SendMessageW(window, TTM_ACTIVATE, FALSE, 0);
            }
        }
        return TRUE;
    }
    std::vector<HWND> m_tooltips;
};
