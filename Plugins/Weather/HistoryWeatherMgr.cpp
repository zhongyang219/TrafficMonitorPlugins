#include "pch.h"
#include "HistoryWeatherMgr.h"
#include "DataManager.h"
#include <string>
#include "../utilities/Common.h"


bool CHistoryWeatherMgr::Date::operator==(const Date& another) const
{
    return year == another.year && month == another.month && day == another.day;
}

bool CHistoryWeatherMgr::Date::operator<(const Date& another) const
{
    if (year != another.year)
        return year < another.year;
    if (month != another.month)
        return month < another.month;
    if (day != another.day)
        return day < another.day;
    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHistoryWeatherMgr::Save() const
{
    std::wstring data_path = g_data.m_config_dir + L"History_weather.dat";
    // 打开或者新建文件
    CFile file;
    BOOL bRet = file.Open(data_path.c_str(),
        CFile::modeCreate | CFile::modeWrite);
    if (!bRet)      // 打开文件失败
        return false;
    // 构造CArchive对象
    CArchive ar(&file, CArchive::store);
    // 写版本号
    const int version{ 0 };
    ar << version;
    // 写数据
    ar << static_cast<int>(m_history_weather_list.size());
    for (const auto& item : m_history_weather_list)
    {
        ar << item.first.year
            << item.first.month
            << item.first.day
            << item.second.type
            << item.second.high_temp
            << item.second.low_temp
            << item.second.wind
            ;
    }
    // 关闭CArchive对象
    ar.Close();
    // 关闭文件
    file.Close();
    return true;
}

bool CHistoryWeatherMgr::Load()
{
    std::wstring data_path = g_data.m_config_dir + L"History_weather.dat";
    CFile file;
    // 打开文件
    if (file.Open(data_path.c_str(), CFile::modeRead))
    {
        // 构造CArchive对象
        CArchive ar(&file, CArchive::load);
        // 读数据
        int version{}, size{};
        CString temp_str;
        try
        {
            ar >> version;
            ar >> size;
            for (int i{}; i < size; ++i)
            {
                Date date{};
                ar >> date.year;
                ar >> date.month;
                ar >> date.day;
                HistoryWeather list_item{};
                ar >> list_item.type;
                ar >> list_item.high_temp;
                ar >> list_item.low_temp;
                ar >> list_item.wind;
                // 插入m_list
                m_history_weather_list[date] = list_item;
            }
        }
        catch (CArchiveException* exception)
        {
            // 捕获序列化时出现的异常
        }
        // 关闭对象
        ar.Close();
        // 关闭文件
        file.Close();
        return true;
    }
    return false;
}

void CHistoryWeatherMgr::AddWeatherInfo(CTime date, const WeatherInfo& weather_info)
{
    Date _date{};
    _date.year = date.GetYear();
    _date.month = date.GetMonth();
    _date.day = date.GetDay();
    HistoryWeather weather;
    weather.type = weather_info.m_type.c_str();
    weather.high_temp = _wtoi(weather_info.m_high.c_str());
    weather.low_temp = _wtoi(weather_info.m_low.c_str());
    weather.wind = weather_info.m_wind.c_str();
    m_history_weather_list[_date] = weather;
}

void CHistoryWeatherMgr::AddWeatherInfo(yyjson_val* forecast)
{
    if (forecast != nullptr)
    {
        //解析日期
        Date date{};
        std::string str_date = utilities::JsonHelper::GetJsonString(forecast, "ymd");
        std::vector<std::string> vec_date;
        utilities::StringHelper::StringSplit(str_date, '-', vec_date);
        if (vec_date.size() > 0)
            date.year = atoi(vec_date[0].c_str());
        if (vec_date.size() > 1)
            date.month = atoi(vec_date[1].c_str());
        if (vec_date.size() > 2)
            date.day = atoi(vec_date[2].c_str());

        //解析天气
        HistoryWeather weather;
        weather.type = utilities::JsonHelper::GetJsonWString(forecast, "type").c_str();
        weather.high_temp = _wtoi(utilities::JsonHelper::GetJsonWString(forecast, "high").substr(2).c_str());
        weather.low_temp = _wtoi(utilities::JsonHelper::GetJsonWString(forecast, "low").substr(2).c_str());
        //风向和风力
        std::wstring fx = utilities::JsonHelper::GetJsonWString(forecast, "fx");
        std::wstring fl = utilities::JsonHelper::GetJsonWString(forecast, "fl");
        weather.wind = (fx + L' ' + fl).c_str();
        m_history_weather_list[date] = weather;
    }
}

const std::map<CHistoryWeatherMgr::Date, CHistoryWeatherMgr::HistoryWeather>& CHistoryWeatherMgr::GetHistoryWeather() const
{
    return m_history_weather_list;
}
