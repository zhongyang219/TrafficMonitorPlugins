#include "pch.h"
#include "BatteryItem.h"
#include "DataManager.h"
#include "DrawCommon.h"
#include "GdiPlusTool.h"

const wchar_t* CBatteryItem::GetItemName() const
{
    return g_data.StringRes(IDS_BATTERY);
}

const wchar_t* CBatteryItem::GetItemId() const
{
    return L"b5R30ITQ";
}

const wchar_t* CBatteryItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CBatteryItem::GetItemValueText() const
{
    static std::wstring battery_str;
    battery_str = g_data.GetBatteryString();
    return battery_str.c_str();
}

const wchar_t* CBatteryItem::GetItemValueSampleText() const
{
    if (g_data.m_setting_data.show_percent)
        return L"100%";
    else
        return L"100";
}

bool CBatteryItem::IsCustomDraw() const
{
    return g_data.m_setting_data.battery_type != BatteryType::NUMBER;
}

//int CBatteryItem::GetItemWidth() const
//{
//    return g_data.RDPI(m_item_width) + 2;
//}

int CBatteryItem::GetItemWidthEx(void * hDC) const
{
    CDC* pDC = CDC::FromHandle((HDC)hDC);

    CString sample_str;
    if (g_data.m_setting_data.show_percent)
        sample_str = _T("100%");
    else
        sample_str = _T("100");

    switch (g_data.m_setting_data.battery_type)
    {
    case BatteryType::NUMBER:
        return pDC->GetTextExtent(sample_str).cx;
    case BatteryType::ICON:
        return g_data.DPI(20);
    case BatteryType::NUMBER_BESIDE_ICON:
        return g_data.DPI(20) + pDC->GetTextExtent(sample_str).cx;
    }
    return g_data.DPI(20) + pDC->GetTextExtent(sample_str).cx;
}

void CBatteryItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    //矩形区域
    CRect rect(CPoint(x, y), CSize(w, h));
    //绘制电池图标
    HICON hIcon = (dark_mode ? g_data.GetIcon(IDI_BATTERY_LIGHT) : g_data.GetIcon(IDI_BATTERY_DARK));
    const int icon_size{ g_data.DPI(16) };
    CPoint icon_point{ rect.TopLeft() };
    icon_point.x = rect.left + g_data.DPI(2);
    icon_point.y = rect.top + (rect.Height() - icon_size) / 2;
    ::DrawIconEx(pDC->GetSafeHdc(), icon_point.x, icon_point.y, hIcon, icon_size, icon_size, 0, NULL, DI_NORMAL);
    //填充电量指示
    if (g_data.m_sysPowerStatus.BatteryFlag != 128 && g_data.m_sysPowerStatus.BatteryLifePercent > 0 && g_data.m_sysPowerStatus.BatteryLifePercent <= 100)
    {
        Gdiplus::RectF rc_indicater;
        rc_indicater.X = icon_point.x + g_data.DPIF(1);
        rc_indicater.Y = icon_point.y + g_data.DPIF(6);
        int percent = g_data.m_sysPowerStatus.BatteryLifePercent;
        //显示充电动画
        if (g_data.m_setting_data.show_charging_animation && g_data.IsAcOnline() && g_data.m_sysPowerStatus.BatteryLifePercent < 100)
        {
            static int display_percent = g_data.m_sysPowerStatus.BatteryLifePercent;
            display_percent += 4;
            if (display_percent > 100)
                display_percent = g_data.m_sysPowerStatus.BatteryLifePercent;
            percent = display_percent;
        }
        float indicater_width = g_data.DPIF(11.7) * percent / 100;
        rc_indicater.Width = indicater_width;
        rc_indicater.Height = g_data.DPIF(3.7);
        CDrawCommon drawer;
        drawer.Create(pDC);
        drawer.DrawRoundRect(rc_indicater, CGdiPlusTool::COLORREFToGdiplusColor(g_data.GetBatteryColor()), g_data.DPI(1));
    }
    //绘制电池数值
    if (g_data.m_setting_data.battery_type == BatteryType::NUMBER_BESIDE_ICON)
    {
        CRect rc_text{ rect };
        rc_text.left = rect.left + icon_size + g_data.DPI(4);
        std::wstring battery_str{ g_data.GetBatteryString() };
        pDC->DrawText(battery_str.c_str(), rc_text, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}
