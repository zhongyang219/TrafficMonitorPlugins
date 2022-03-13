#include "pch.h"
#include "WeatherProItem.h"

const wchar_t* CWeatherProItem::GetItemName() const
{
    static CString itemName;
    if (itemName.IsEmpty()) itemName.LoadStringW(IDS_WEATHER_PRO);

    return itemName;
}

const wchar_t* CWeatherProItem::GetItemId() const
{
    return L"FxOP34Lm";
}

const wchar_t* CWeatherProItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CWeatherProItem::GetItemValueText() const
{
    return L"";
}

const wchar_t* CWeatherProItem::GetItemValueSampleText() const
{
    return L"";
}

bool CWeatherProItem::IsCustomDraw() const
{
    return true;
}

int CWeatherProItem::GetItemWidthEx(void* hDC) const
{
    // todo
    return 0;
}

void CWeatherProItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    // todo
}
