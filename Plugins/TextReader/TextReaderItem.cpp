#include "pch.h"
#include "TextReaderItem.h"
#include "DataManager.h"
#include "TextReader.h"

const wchar_t* CTextReaderItem::GetItemName() const
{
    return g_data.StringRes(IDS_PLUGIN_ITEM_NAME);
}

const wchar_t* CTextReaderItem::GetItemId() const
{
    //TODO: 在此返回插件的唯一ID，建议只包含字母和数字
    return L"W5XuBvH0";
}

const wchar_t* CTextReaderItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CTextReaderItem::GetItemValueText() const
{
    return L"";
}

const wchar_t* CTextReaderItem::GetItemValueSampleText() const
{
    return L"Text Reader Plugin";
}

bool CTextReaderItem::IsCustomDraw() const
{
    //TODO: 根据是否由插件自绘返回对应的值
    return true;
}


int CTextReaderItem::GetItemWidth() const
{
    return g_data.m_setting_data.window_width;
}

void CTextReaderItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    g_data.m_draw_width = w;
    static std::wstring text;
    bool hide{ g_data.m_setting_data.hide_when_lose_focus && !g_data.HasFocus() };   //窗口没有没有焦点时，显示文本消失
    if (g_data.m_boss_key_pressed || hide)                              //按下老板键，显示文本消失
    {
        text.clear();
    }
    else if (g_data.m_setting_data.current_position < g_data.GetTextContexts().size())
    {
        text = g_data.GetTextContexts().substr(g_data.m_setting_data.current_position, 100);
    }

    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);

    g_data.m_multi_line = (h > g_data.DPI(30));
    g_data.m_page_step = g_data.GetPageStep(pDC);

    //矩形区域
    CRect rect(CPoint(x, y), CSize(w, h));
    UINT flag{};
    if (g_data.IsMultiLine())
    {
        flag = DT_NOPREFIX | DT_WORDBREAK;
    }
    else
    {
        flag = DT_VCENTER | DT_NOPREFIX | DT_SINGLELINE;
    }
    pDC->DrawText(text.c_str(), rect, flag);

}

int CTextReaderItem::OnMouseEvent(MouseEventType type, int x, int y, void * hWnd, int flag)
{
    g_data.m_wnd = (HWND)hWnd;
    CWnd* pWnd = CWnd::FromHandle((HWND)hWnd);
    if (type == IPluginItem::MT_LCLICKED)
    {
        g_data.PageDown(g_data.m_page_step);
        return 1;
    }
    else if (type == IPluginItem::MT_DBCLICKED)
    {
        return 1;
    }
    else if (type == IPluginItem::MT_RCLICKED && g_data.m_setting_data.use_own_context_menu)
    {
        CTextReader::Instance().ShowContextMenu(pWnd);
        return 1;
    }
    if (g_data.m_setting_data.mouse_wheel_read)
    {
        int mouse_wheel_step;
        if (g_data.m_setting_data.mouse_wheel_read_page)
            mouse_wheel_step = g_data.m_page_step;
        else
            mouse_wheel_step = g_data.m_setting_data.mouse_wheel_charactors;

        if (type == IPluginItem::MT_WHEEL_UP)
        {
            g_data.PageUp(mouse_wheel_step);
            return 1;
        }
        else if (type == IPluginItem::MT_WHEEL_DOWN)
        {
            g_data.PageDown(mouse_wheel_step);
            return 1;
        }
    }

    return 0;
}

int CTextReaderItem::OnKeboardEvent(int key, bool ctrl, bool shift, bool alt, void* hWnd, int flag)
{
    g_data.m_wnd = (HWND)hWnd;
    if (key == VK_LEFT)
    {
        g_data.PageUp(1);
        return 1;
    }
    else if (key == VK_RIGHT)
    {
        g_data.PageDown(1);
        return 1;
    }
    else if (key == VK_UP)
    {
        g_data.PageUp();
        return 1;
    }
    else if (key == VK_DOWN)
    {
        g_data.PageDown();
    }
    else if (key == VK_SPACE)
    {
        g_data.m_boss_key_pressed = !g_data.m_boss_key_pressed;
    }
    return 0;
}
