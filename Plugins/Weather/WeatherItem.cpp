#include "pch.h"
#include "WeatherItem.h"
#include "DataManager.h"
#include "Weather.h"
#include <algorithm>
#undef min
#undef max

const wchar_t* CWeatherItem::GetItemName() const
{
    return g_data.StringRes(IDS_WEATHER);
}

const wchar_t* CWeatherItem::GetItemId() const
{
    return L"NdKZEf39";
}

const wchar_t* CWeatherItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CWeatherItem::GetItemValueText() const
{
    return L"";
}

const wchar_t* CWeatherItem::GetItemValueSampleText() const
{
    const CDataManager::WeatherInfo& weather_info{ g_data.GetWeather() };
    if (g_data.m_setting_data.m_use_weather_icon)
    {
        if (weather_info.is_cur_weather)
            return L"20℃";
        else
            return L"20~20℃";
    }
    else
    {
        if (weather_info.is_cur_weather)
            return L"多云 20℃";
        else
            return L"多云 20~20℃";
    }
}

bool CWeatherItem::IsCustomDraw() const
{
    return true;
}

int CWeatherItem::GetItemWidthEx(void* hDC) const
{
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    if (g_data.m_setting_data.m_use_weather_icon)
    {
        return g_data.DPI(20) + std::max(pDC->GetTextExtent(g_data.GetWeather().ToStringTemperature().c_str()).cx, pDC->GetTextExtent(GetItemValueSampleText()).cx);
    }
    else
    {
        return std::max(pDC->GetTextExtent(g_data.GetWeather().ToString().c_str()).cx, pDC->GetTextExtent(GetItemValueSampleText()).cx);
    }
}

void CWeatherItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    //矩形区域
    CRect rect(CPoint(x, y), CSize(w, h));
    if (g_data.m_setting_data.m_use_weather_icon)
    {
        //绘制天气图标
        const int icon_size{ g_data.DPI(16) };
        HICON hIcon = g_data.GetWeatherIcon(g_data.GetWeather().m_type);
        CPoint icon_point{ rect.TopLeft() };
        icon_point.x = rect.left + g_data.DPI(2);
        icon_point.y = rect.top + (rect.Height() - icon_size) / 2;
        ::DrawIconEx(pDC->GetSafeHdc(), icon_point.x, icon_point.y, hIcon, icon_size, icon_size, 0, NULL, DI_NORMAL);
        //绘制天气文本
        CRect rc_text{ rect };
        rc_text.left += (icon_size + g_data.DPI(4));
        pDC->DrawText(g_data.GetWeather().ToStringTemperature().c_str(), rc_text, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    else
    {
        pDC->DrawText(g_data.GetWeather().ToString().c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

int CWeatherItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    //CWnd* pWnd = CWnd::FromHandle((HWND)hWnd);
    //if (type == IPluginItem::MT_RCLICKED)
    //{
    //    CWeather::Instance().ShowContextMenu(pWnd);
    //    return 1;
    //}
    return 0;
}
