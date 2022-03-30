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
    std::wstring GetTemperature() const;
};

struct SWeatherAlert
{
    std::wstring Type;
    std::wstring Level;
    std::wstring BriefMessage;
    std::wstring DetailedMessage;
    std::wstring PublishTime;

    std::wstring ToString(bool brief) const;
};

using WeatherAlertList = std::vector<SWeatherAlert>;

struct SWeatherInfo
{
    std::wstring TemperatureDay;
    std::wstring TemperatureNight;
    std::wstring WeatherDay;
    std::wstring WeatherNight;
    std::wstring WeatherCodeDay;
    std::wstring WeatherCodeNight;

    std::wstring GetTemperature() const;
    std::wstring ToString() const;
};

namespace query
{
    bool QueryCityInfo(const std::wstring &q_name, CityInfoList &city_list);

    bool QueryRealTimeWeather(const std::wstring &code, SRealTimeWeather &weather);
    bool QueryWeatherAlerts(const std::wstring &code, WeatherAlertList &alerts);
    bool QueryForecastWeather(const std::wstring &code, SWeatherInfo &weather_td, SWeatherInfo &weather_tm);
}
