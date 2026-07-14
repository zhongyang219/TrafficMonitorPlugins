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

    struct HistoryWeather
    {
        CString type;
        CString high_temp;
        CString low_temp;
        CString wind;
        CString city;

        bool IsValid() const;
    };

    //保存每个城市每个日期的历史天气
    using HistoryWeahterMap = std::map<CString, std::map<Date, HistoryWeather>>;

    void AddWeatherInfo(const CString& city, Date date, const WeatherInfo& weather_info);
    void AddWeatherInfo(const CString& city, yyjson_val* forecast);
    bool IsWeatherExist(const CString& city, const Date& date) const;
    const HistoryWeahterMap& GetHistoryWeather() const;

    static CString GetTemperatureString(const CString& low_temp, const CString& high_temp);

private:
    HistoryWeahterMap m_history_weather_list;
};

