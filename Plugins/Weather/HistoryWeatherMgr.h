#pragma once
#include <map>
#include "utilities/JsonHelper.h"
#include "CommonData.h"

class CHistoryWeatherMgr
{
public:
    CHistoryWeatherMgr() {}
    bool Save() const;
    bool Load();

    void AddWeatherInfo(CTime date, const WeatherInfo& weather_info);
    void AddWeatherInfo(yyjson_val* forecast);

    struct Date
    {
        int year;
        int month;
        int day;

        bool operator==(const Date& another) const;
        bool operator<(const Date& another) const;
    };

    struct HistoryWeather
    {
        CString type;
        int high_temp{};
        int low_temp{};
        CString wind;
    };

    const std::map<Date, HistoryWeather>& GetHistoryWeather() const;

private:
    std::map<Date, HistoryWeather> m_history_weather_list;
};

