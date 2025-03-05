#include "pch.h"
#include "LoudnessMeterItem.h"
#include "DataManager.h"
#include <afxdrawmanager.h>

CLoudnessMeterItem::CLoudnessMeterItem()
{
}

const wchar_t* CLoudnessMeterItem::GetItemName() const
{
    return g_data.StringRes(IDS_PLUGIN_ITEM_NAME);
}

const wchar_t* CLoudnessMeterItem::GetItemId() const
{
    return L"b5szf6zw";
}

const wchar_t* CLoudnessMeterItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CLoudnessMeterItem::GetItemValueText() const
{
    return L"";
}

const wchar_t* CLoudnessMeterItem::GetItemValueSampleText() const
{
    return L"000.00 dB";
}

void CLoudnessMeterItem::SetValue(const float value, float percent, DBState state)
{
    m_db = value;
    if (percent > 100)
        m_percent = 100;
    else if (percent < 0)
        m_percent = 0;
    else
        m_percent = percent;
    m_state = state;
}

bool CLoudnessMeterItem::IsCustomDraw() const
{
    return true;
}

int CLoudnessMeterItem::GetItemWidth() const
{
    return 50;
}
//
//static float dBToPercentage(float dB)
//{
//    const float minDB = -96.0f; // 最小分贝值
//    const float maxDB = 0.0f;   // 最大分贝值
//
//    if (dB < minDB) return 0.0f; // 低于最小分贝值，返回 0%
//    if (dB > maxDB) return 100.0f; // 高于最大分贝值，返回 100%
//
//    // 映射到 [0%, 100%]
//    return ((dB - minDB) / (maxDB - minDB)) * 100.0f;
//}

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

void CLoudnessMeterItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    //矩形区域
    CRect rect(CPoint(x, y), CSize(w, h));
    const int bar_height = (std::min)(h, g_data.DPI(10));

    CRect bar_rect{ rect };
    bar_rect.top += (rect.Height() - bar_height) / 2;
    bar_rect.bottom = bar_rect.top + bar_height;

    // 计算填充区域的宽度
    int fillWidth = static_cast<int>(bar_rect.Width() * m_percent / 100);

    // 定义颜色
    COLORREF color_low;     // 绿色
    COLORREF color_medium;  // 黄色
    COLORREF color_high;    // 红色
    if (dark_mode)
    {
        color_low = RGB(80, 143, 57); // 绿色
        color_medium = RGB(163, 131, 0); // 黄色
        color_high = RGB(144, 16, 0); // 红色
    }
    else
    {
        color_low = RGB(128, 194, 105);
        color_medium = RGB(255, 216, 58);
        color_high = RGB(255, 95, 74);
    }

    const int MEDIUM_PERCENT = 80;
    const int HIGH_PERCENT = 90;

    if (m_state != DB_INVALID)
    {
        COLORREF outline_color = dark_mode ? RGB(255, 255, 255) : 0;
        // 绘制边框
        DrawRectOutLine(pDC, bar_rect, outline_color);
    }
    if (m_state == DB_VALID && fillWidth > 0)
    {
        // 绘制填充区域
        CRect fill_rect = bar_rect;
        fill_rect.DeflateRect(g_data.DPI(1), g_data.DPI(1));
        //
        
        //设置剪辑区域
        CRect clip_rect{ fill_rect };
        clip_rect.right = clip_rect.left + fillWidth;
        CRgn clip_rgn;
        clip_rgn.CreateRectRgn(clip_rect.left, clip_rect.top, clip_rect.right, clip_rect.bottom);
        pDC->SelectClipRgn(&clip_rgn);

        //绘制渐变
        CDrawingManager draw_mgr(*pDC);
        CRect low_rect{ fill_rect };
        low_rect.right = low_rect.left + fill_rect.Width() * MEDIUM_PERCENT / 100;
        draw_mgr.FillGradient(low_rect, color_low, color_medium, FALSE, 50, 0);
        CRect high_rect{ fill_rect };
        high_rect.left = low_rect.right;
        draw_mgr.FillGradient(high_rect, color_medium, color_high, FALSE);

        pDC->SelectClipRgn(nullptr);
    }

    //绘制文本
    if (m_font.GetSafeHandle() == NULL)
        m_font.CreatePointFont(80, _T("Segoe UI"), pDC);
    pDC->SelectObject(&m_font);
    wchar_t buff[64]{};
    if (m_state == DB_INVALID)
        wcscpy_s(buff, L"--.-- db");
    else if (m_state == DB_MUTE)
        wcscpy_s(buff, L"mute");
    else
        swprintf_s(buff, L"%.2f dB", m_db);

    pDC->DrawText(buff, bar_rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

}
