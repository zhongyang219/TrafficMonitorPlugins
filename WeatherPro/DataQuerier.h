#pragma once

#include <vector>
#include <string>

struct SCityInfo
{
    std::wstring CityNO;
    std::wstring CityName;
    std::wstring CityAdministrativeOwnership;
};

using CityInfoList = std::vector<SCityInfo>;

struct SRealTimeWeather
{
    std::wstring Temperature;
    std::wstring UpdateTime;
    std::wstring Weather;
    std::wstring WeatherCode;
    std::wstring AqiPM25;
    std::wstring WindDirection;
    std::wstring WindStrength;

    std::wstring ToString(bool brief) const;
};

struct SWeatherAlert
{
    std::wstring Type;
    std::wstring Level;
    std::wstring BriefMessage;
    std::wstring DetailedMessage;
    std::wstring PublishTime;

    std::wstring ToString(bool Brief) const;
};

using WeatherAlertList = std::vector<SWeatherAlert>;

namespace query
{
    bool QueryCityInfo(const std::wstring &q_name, CityInfoList &city_list);

    bool QueryRealTimeWeather(const std::wstring &city_code, SRealTimeWeather &weather);
    bool QueryWeatherAlerts(const std::wstring &city_code, WeatherAlertList &alerts);
}
