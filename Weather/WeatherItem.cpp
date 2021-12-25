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
    return false;
}

int CWeatherItem::GetItemWidth() const
{
    return 120;
}

void CWeatherItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
}
