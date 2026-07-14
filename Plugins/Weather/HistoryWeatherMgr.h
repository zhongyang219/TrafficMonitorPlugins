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

    struct Date
    {
        int year;
        int month;
        int day;

        bool operator==(const Date& another) const;
        bool operator<(const Date& another) const;
    };

    void AddWeatherInfo(Date date, const WeatherInfo& weather_info);
    void AddWeatherInfo(yyjson_val* forecast);
    bool IsWeatherExist(const Date& date) const;

    struct HistoryWeather
    {
        CString type;
        CString high_temp;
        CString low_temp;
        CString wind;

        bool IsValid() const;
    };

    const std::map<Date, HistoryWeather>& GetHistoryWeather() const;

    static CString GetTemperatureString(const CString& low_temp, const CString& high_temp);

private:
    std::map<Date, HistoryWeather> m_history_weather_list;
};

