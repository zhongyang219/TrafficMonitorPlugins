#include "pch.h"
#include "CommonData.h"
#include <sstream>


std::wstring WeatherInfo::WeatherInfo::ToString() const
{
    std::wstringstream wss;
    if (m_low.empty() || is_cur_weather)
        wss << m_type << ' ' << m_high;
    else if (m_high.empty())
        wss << m_type << ' ' << m_low;
    else
        wss << m_type << ' ' << m_low << '~' << m_high;
    wss << ' ' << m_wind;
    return wss.str();
}

std::wstring WeatherInfo::WeatherInfo::ToStringTemperature() const
{
    std::wstringstream wss;
    if (m_low.empty() || is_cur_weather)
        wss << m_high;
    else if (m_high.empty())
        wss << m_low;
    else
        wss << m_low << '~' << m_high;
    return wss.str();
}
