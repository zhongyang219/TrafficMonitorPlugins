#include "pch.h"
#include "WeatherProItem.h"
#include "DataManager.h"

const wchar_t* CWeatherProItem::GetItemName() const
{
    return CDataManager::InstanceRef().StringRes(IDS_WEATHER_PRO);
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
    CDC* pDC = CDC::FromHandle((HDC)hDC);

    auto icon_width = CDataManager::Instance().GetConfig().m_show_weather_icon ? CDataManager::Instance().DPI(20) : 0;

    return icon_width + pDC->GetTextExtent(CDataManager::Instance().GetWeatherTemperature().c_str()).cx;
}

void CWeatherProItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    CDC* pDC = CDC::FromHandle((HDC)hDC);

    CRect rect(CPoint(x, y), CSize(w, h));

    if (CDataManager::Instance().GetConfig().m_show_weather_icon)
    {
        // todo: draw icon

        rect.left += CDataManager::Instance().DPI(20);
    }

    pDC->DrawText(CDataManager::Instance().GetWeatherTemperature().c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}
