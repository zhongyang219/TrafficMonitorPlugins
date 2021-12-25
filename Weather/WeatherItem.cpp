#include "pch.h"
#include "WeatherItem.h"
#include "DataManager.h"

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
    static std::wstring str_value;
    str_value = g_data.GetWeather().ToString();
    return str_value.c_str();
}

const wchar_t* CWeatherItem::GetItemValueSampleText() const
{
    return L"小雨 10℃~20℃";
}

bool CWeatherItem::IsCustomDraw() const
{
    return g_data.m_setting_data.m_use_weather_icon;
}

int CWeatherItem::GetItemWidth() const
{
    return 80;
}

void CWeatherItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    //矩形区域
    CRect rect(CPoint(x, y), CSize(w, h));
    //绘制天气图标
    HICON hIcon = g_data.GetWeatherIcon(g_data.GetWeather().m_type);
    const int icon_size{ g_data.DPI(16) };
    CPoint icon_point{ rect.TopLeft() };
    icon_point.x = rect.left + g_data.DPI(2);
    icon_point.y = rect.top + (rect.Height() - icon_size) / 2;
    ::DrawIconEx(pDC->GetSafeHdc(), icon_point.x, icon_point.y, hIcon, icon_size, icon_size, 0, NULL, DI_NORMAL);
    //绘制天气文本
    CRect rc_text{ rect };
    rc_text.left = rect.left + icon_size + g_data.DPI(4);
    pDC->SetTextColor(g_data.m_value_text_color);
    pDC->DrawText(g_data.GetWeather().ToStringTemperature().c_str(), rc_text, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}
