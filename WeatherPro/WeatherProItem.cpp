#include "pch.h"
#include "WeatherProItem.h"
#include "DataManager.h"

const wchar_t* CWeatherProItem::GetItemName() const
{
    return CDataManager::Instance().StringRes(IDS_WEATHER_PRO);
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
        auto icon_size = CDataManager::Instance().DPI(16);
        auto hIcon = CDataManager::InstanceRef().GetIcon();
        CPoint icon_pos{ rect.TopLeft() };
        icon_pos.x += CDataManager::Instance().DPI(2);
        icon_pos.y += (rect.Height() - icon_size) / 2;
        DrawIconEx(pDC->GetSafeHdc(), icon_pos.x, icon_pos.y, hIcon, icon_size, icon_size, 0, NULL, DI_NORMAL);

        rect.left += CDataManager::Instance().DPI(20);
    }

    pDC->DrawText(CDataManager::Instance().GetWeatherTemperature().c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}
