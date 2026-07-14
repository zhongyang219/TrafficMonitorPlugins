#include "pch.h"
#include "HistoryWeatherMgr.h"
#include "DataManager.h"
#include <string>
#include "../utilities/Common.h"
#include "Weather.h"


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

void CHistoryWeatherMgr::AddWeatherInfo(Date date, const WeatherInfo& weather_info)
{
    //先获取原有天气
    HistoryWeather weather = m_history_weather_list[date];
    weather.type = weather_info.m_type.c_str();
    if (!weather_info.m_high.empty())
        weather.high_temp = weather_info.m_high.c_str();
    if (!weather_info.m_low.empty())
        weather.low_temp = weather_info.m_low.c_str();
    weather.wind = weather_info.m_wind.c_str();
    m_history_weather_list[date] = weather;
}

void CHistoryWeatherMgr::AddWeatherInfo(yyjson_val* forecast)
{
    if (forecast != nullptr)
    {
        //解析日期
        Date date{};
        std::string str_date = utilities::JsonHelper::GetJsonString(forecast, "date");
        std::vector<std::string> vec_date;
        utilities::StringHelper::StringSplit(str_date, '-', vec_date);
        if (vec_date.size() < 3)
            return;
        date.year = atoi(vec_date[0].c_str());
        date.month = atoi(vec_date[1].c_str());
        date.day = atoi(vec_date[2].c_str());

        //解析天气
        WeatherInfo info;
        CWeather::ParseWeatherInfo(info, forecast);

        //如果当前日期的天气已存在且解析到的天气中缺失日间温度，则不添加到历史天气中
        if (IsWeatherExist(date) && info.m_high.empty())
            return;

        AddWeatherInfo(date, info);
    }
}

bool CHistoryWeatherMgr::IsWeatherExist(const Date& date) const
{
    auto iter = m_history_weather_list.find(date);
    if (iter == m_history_weather_list.end())
        return false;
    if (!iter->second.IsValid())
        return false;
    return true;
}

const std::map<CHistoryWeatherMgr::Date, CHistoryWeatherMgr::HistoryWeather>& CHistoryWeatherMgr::GetHistoryWeather() const
{
    return m_history_weather_list;
}

CString CHistoryWeatherMgr::GetTemperatureString(const CString& low_temp, const CString& high_temp)
{
    if (low_temp.IsEmpty())
        return high_temp + _T("℃");
    if (high_temp.IsEmpty())
        return low_temp + _T("℃");

    CString str_temp;
    str_temp.Format(_T("%s~%s℃"), low_temp.GetString(), high_temp.GetString());
    return str_temp;
}

bool CHistoryWeatherMgr::HistoryWeather::IsValid() const
{
    return !type.IsEmpty() && !high_temp.IsEmpty() && !low_temp.IsEmpty();
}
