#include "pch.h"
#include "KeyboardIndicatorItem.h"
#include "DataManager.h"
#include "KeyboardIndicator.h"
const wchar_t* INDICATOR_CAPS_LOCK = L"Caps";
const wchar_t* INDICATOR_NUM_LOCK = L"Num";
const wchar_t* INDICATOR_SCROLL_LOCK = L"ScrLk";


const wchar_t* CKeyboardIndicatorItem::GetItemName() const
{
    return g_data.StringRes(IDS_PLUGIN_ITEM_NAME);
}

const wchar_t* CKeyboardIndicatorItem::GetItemId() const
{
    return L"3db99u17";
}

const wchar_t* CKeyboardIndicatorItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CKeyboardIndicatorItem::GetItemValueText() const
{
    return L"";
}

const wchar_t* CKeyboardIndicatorItem::GetItemValueSampleText() const
{
    return L"";
}

bool CKeyboardIndicatorItem::IsCustomDraw() const
{
    return true;
}

int CKeyboardIndicatorItem::GetItemWidthEx(void * hDC) const
{
    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    //设置字体
    CFont* pFont = (CFont*)&m_font;
    CFont* pOldFont = pDC->SelectObject(pFont);
    int width = 0;
    if (g_data.m_setting_data.show_caps_lock)
        width += (pDC->GetTextExtent(INDICATOR_CAPS_LOCK).cx + g_data.DPI(3));
    if (g_data.m_setting_data.show_num_lock)
        width += (pDC->GetTextExtent(INDICATOR_NUM_LOCK).cx + g_data.DPI(3));
    if (g_data.m_setting_data.show_scroll_lock)
        width += (pDC->GetTextExtent(INDICATOR_SCROLL_LOCK).cx + g_data.DPI(3));
    //恢复字体
    pDC->SelectObject(pOldFont);
    return width;
}

static void DrawRectOutLine(CDC* pDC, CRect rect, COLORREF color)	//绘制矩形边框
{
    CPen aPen, * pOldPen;
    aPen.CreatePen(PS_SOLID, g_data.DPI(1), color);
    pOldPen = pDC->SelectObject(&aPen);
    CBrush* pOldBrush{ dynamic_cast<CBrush*>(pDC->SelectStockObject(NULL_BRUSH)) };

    pDC->Rectangle(rect);
    pDC->SelectObject(pOldPen);
    pDC->SelectObject(pOldBrush);       // Restore the old brush
    aPen.DeleteObject();
}

static void DrawIndicator(CDC* pDC, CRect& rect, const wchar_t* text, COLORREF color)
{
    //根据文本宽度设置矩形的宽度
    rect.right = rect.left + pDC->GetTextExtent(text).cx + g_data.DPI(2);
    //绘制边框
    DrawRectOutLine(pDC, rect, color);
    //绘制文本
    pDC->SetTextColor(color);
    pDC->DrawText(text, rect, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    //绘制完成后将矩形的左边框移动到右边框
    rect.MoveToX(rect.right + g_data.DPI(1));
}

void CKeyboardIndicatorItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    //矩形区域
    CRect rect(CPoint(x, y), CSize(w, h));
    //绘图颜色
    COLORREF color_enable;
    COLORREF color_disable;
    if (dark_mode)
    {
        color_enable = RGB(255, 255, 255);
        color_disable = RGB(145, 145, 145);
    }
    else
    {
        color_enable = RGB(0, 0, 0);
        color_disable = RGB(140, 140, 140);
    }

    //设置字体
    if (m_font.GetSafeHandle() == NULL)
        m_font.CreatePointFont(70, _T("Segoe UI"), pDC);
    pDC->SelectObject(&m_font);

    int item_height = g_data.DPI(14);
    CRect rect_indicator{ rect };
    rect_indicator.top += (rect.Height() - item_height) / 2;
    rect_indicator.bottom = rect_indicator.top + item_height;
    //绘制Caps Lock
    if (g_data.m_setting_data.show_caps_lock)
    {
        COLORREF color = CKeyboardIndicator::IsCapsLockOn() ? color_enable : color_disable;
        DrawIndicator(pDC, rect_indicator, INDICATOR_CAPS_LOCK, color);
    }
    //绘制num lock
    if (g_data.m_setting_data.show_num_lock)
    {
        COLORREF color = CKeyboardIndicator::IsNumLockOn() ? color_enable : color_disable;
        DrawIndicator(pDC, rect_indicator, INDICATOR_NUM_LOCK, color);
    }
    //绘制scroll lock
    if (g_data.m_setting_data.show_scroll_lock)
    {
        COLORREF color = CKeyboardIndicator::IsScrollLockOn() ? color_enable : color_disable;
        DrawIndicator(pDC, rect_indicator, INDICATOR_SCROLL_LOCK, color);
    }
}
