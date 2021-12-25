#include "pch.h"
#include "Weather.h"
#include "Common.h"
#include <fstream>
#include "DataManager.h"
#include "yyjson/yyjson.h"
#include "OptionsDlg.h"
#include <sstream>

CWeather CWeather::m_instance;

CWeather::CWeather()
{
}

CWeather& CWeather::Instance()
{
    return m_instance;
}

UINT CWeather::ThreadCallback(LPVOID dwUser)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CFlagLocker flag_locker(m_instance.m_is_thread_runing);

    std::wstring url{ L"http://t.weather.itboy.net/api/weather/city/" };
    url += g_data.CurCity().code;
    std::string weather_data;
    if (CCommon::GetURL(url, weather_data))
    {
        m_instance.ParseJsonData(weather_data);
    }
    return 0;
}

void ParseWeatherInfo(CDataManager::WeatherInfo& weather_info, yyjson_val* forecast)
{
    weather_info.m_type = CCommon::StrToUnicode(yyjson_get_str(yyjson_obj_get(forecast, "type")), true);
    weather_info.m_high = CCommon::StrToUnicode(yyjson_get_str(yyjson_obj_get(forecast, "high")), true);
    weather_info.m_low = CCommon::StrToUnicode(yyjson_get_str(yyjson_obj_get(forecast, "low")), true);

    //去掉前面的“高温”和“低温”
    weather_info.m_high = weather_info.m_high.substr(2);
    weather_info.m_low = weather_info.m_low.substr(2);
}

void CWeather::ParseJsonData(std::string json_data)
{
    //int a = 0;
    std::ofstream stream{ g_data.GetModulePath() + L".log" };
    stream << json_data;

    g_data.ResetText();

    yyjson_doc* doc = yyjson_read(json_data.c_str(), json_data.size(), 0);
    if (doc != nullptr)
    {
        yyjson_val* root = yyjson_doc_get_root(doc);
        yyjson_val* data = yyjson_obj_get(root, "data");
        yyjson_val* forecast_arr = yyjson_obj_get(data, "forecast");
        yyjson_val* forecast_today = yyjson_arr_get_first(forecast_arr);
        yyjson_val* forecast_tommorrow = yyjson_arr_get(forecast_arr, 1);
        ParseWeatherInfo(g_data.m_weather_info[WEATHER_TODAY], forecast_today);
        ParseWeatherInfo(g_data.m_weather_info[WEATHER_TOMMORROW], forecast_tommorrow);

        const CDataManager::WeatherInfo& weather_today{ g_data.m_weather_info[WEATHER_TODAY] };
        const CDataManager::WeatherInfo& weather_tomorrow{ g_data.m_weather_info[WEATHER_TOMMORROW] };
        std::wstringstream wss;
        wss << g_data.StringRes(IDS_WEATHER).GetString() << std::endl
            << g_data.StringRes(IDS_TODAY_WEATHER).GetString() << L": " << weather_today.ToString() << std::endl
            << g_data.StringRes(IDS_TOMMORROW_WEATHER).GetString() << L": " << weather_tomorrow.ToString();
        m_tooltop_info = wss.str();

        yyjson_doc_free(doc);
    }
}

IPluginItem* CWeather::GetItem(int index)
{
    switch (index)
    {
    case 0:
        return &m_item;
    default:
        break;
    }
    return nullptr;
}

const wchar_t* CWeather::GetTooltipInfo()
{
    if (g_data.m_setting_data.m_show_weather_in_tooltips)
        return m_tooltop_info.c_str();
    else
        return L"";
}

void CWeather::DataRequired()
{
    static int last_hour{ -1 };
    SYSTEMTIME system_time{};
    GetLocalTime(&system_time);
    //每隔一小时获取一次天气
    if (system_time.wHour != last_hour)
    {
        last_hour = system_time.wHour;
        SendWetherInfoQequest();
    }

}

ITMPlugin::OptionReturn CWeather::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* pParent = CWnd::FromHandle((HWND)hParent);
    g_data.DPIFromWindow(pParent);
    COptionsDlg dlg(pParent);
    dlg.m_data = g_data.m_setting_data;
    if (dlg.DoModal() == IDOK)
    {
        bool city_changed{ g_data.m_setting_data.m_city_index != dlg.m_data.m_city_index };
        g_data.m_setting_data = dlg.m_data;
        if (city_changed)
        {
            CWeather::Instance().SendWetherInfoQequest();   //城市改变后，重新发送天气请求
        }

        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

const wchar_t* CWeather::GetInfo(PluginInfoIndex index)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    static CString str;
    switch (index)
    {
    case TMI_NAME:
        str.LoadString(IDS_PLUGIN_NAME);
        return str.GetString();
    case TMI_DESCRIPTION:
        str.LoadString(IDS_PLUGIN_DESCRIPTION);
        return str.GetString();
    case TMI_AUTHOR:
        return L"zhongyang219";
    case TMI_COPYRIGHT:
        return L"Copyright (C) by Zhong Yang 2021";
    case TMI_VERSION:
        return L"1.0";
    default:
        break;
    }
    return L"";
}

void CWeather::SendWetherInfoQequest()
{
    if (!m_is_thread_runing)    //确保线程已退出
        AfxBeginThread(ThreadCallback, nullptr);
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CWeather::Instance();
}
