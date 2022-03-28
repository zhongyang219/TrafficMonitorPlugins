#include "pch.h"
#include "DataManager.h"
#include "resource.h"

#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>

std::mutex g_weather_update_nutex;

SConfiguration::SConfiguration() :
    m_wit(EWeatherInfoType::WEATHER_REALTIME),
    m_show_weather_icon(true),
    m_show_weather_in_tooltips(true),
    m_show_brief_rt_weather_info(false),
    m_show_weather_alerts(true),
    m_show_brief_weather_alert_info(true)
{}

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
    HDC hDC = ::GetDC(HWND_DESKTOP);
    m_dpi = ::GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(HWND_DESKTOP, hDC);
}

CDataManager::~CDataManager()
{
    SaveConfigs();
}

const CDataManager& CDataManager::Instance()
{
    return m_instance;
}

CDataManager& CDataManager::InstanceRef()
{
    return m_instance;
}

const CString& CDataManager::StringRes(UINT id)
{
    if (m_string_resources.count(id) == 0)
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        m_string_resources[id].LoadString(id);
    }

    return m_string_resources[id];
}

int CDataManager::DPI(int pixel) const
{
    return m_dpi * pixel / 96;
}

float CDataManager::DPIF(float pixel) const
{
    return m_dpi * pixel / 96;
}

int CDataManager::RDPI(int pixel) const
{
    return pixel * 96 / m_dpi;
}

void CDataManager::SetCurrentCityInfo(SCityInfo info)
{
    m_currentCityInfo = info;
}

const SCityInfo& CDataManager::GetCurrentCityInfo() const
{
    return m_currentCityInfo;
}

void CDataManager::_updateWeather(WeatherInfoUpdatedCallback callback)
{
    std::lock_guard<std::mutex> guard(g_weather_update_nutex);

    bool flag{ false };
    int query_times{ 0 };

    do 
    {
        if (query_times > 0)
            std::this_thread::sleep_for(std::chrono::seconds(1));

        flag = query::QueryRealTimeWeather(GetCurrentCityInfo().CityNO, m_rt_weather);

        ++query_times;
    } while (!flag && query_times < 3);

    flag = false;
    query_times = 0;

    do 
    {
        if (query_times > 0)
            std::this_thread::sleep_for(std::chrono::seconds(1));

        flag = query::QueryWeatherAlerts(GetCurrentCityInfo().CityNO, m_weather_alerts);

        ++query_times;
    } while (!flag && query_times < 3);

    flag = false;
    query_times = 0;

    do 
    {
        if (query_times > 0)
            std::this_thread::sleep_for(std::chrono::seconds(1));

        flag = query::QueryForecastWeather(GetCurrentCityInfo().CityNO, m_weather_today, m_weather_tommrow);

        ++query_times;
    } while (!flag && query_times < 3);

    if (callback != nullptr)
    {
        callback(GetTooptipInfo());
    }
}

void CDataManager::UpdateWeather(WeatherInfoUpdatedCallback callback /* = nullptr */)
{
    std::thread t(&CDataManager::_updateWeather, this, callback);
    t.detach();
}

std::wstring CDataManager::GetWeatherTemperature() const
{
    switch (m_config.m_wit)
    {
    case EWeatherInfoType::WEATHER_REALTIME:
        if (m_config.m_show_weather_icon)
            return m_rt_weather.GetTemperature();
        else
            return m_rt_weather.Weather + L" " + m_rt_weather.GetTemperature();

    case EWeatherInfoType::WEATHER_TODAY:
        if (m_config.m_show_weather_icon)
            return m_weather_today.GetTemperature();
        else
            return m_weather_today.ToString();

    case EWeatherInfoType::WEATHER_TOMMROW:
        if (m_config.m_show_weather_icon)
            return m_weather_tommrow.GetTemperature();
        else
            return m_weather_tommrow.ToString();

    default:
        return L"-";
    }
}

std::wstring CDataManager::GetRealTimeWeatherInfo(bool brief) const
{
    return m_rt_weather.ToString(brief);
}

std::wstring CDataManager::GetWeatherAlertsInfo(bool brief) const
{
    if (m_weather_alerts.empty())
    {
        return std::wstring();
    }
    else
    {
        std::wstringstream wss;

        for (const auto &alert : m_weather_alerts)
            wss << L"[!] " << alert.ToString(brief) << std::endl;

        return wss.str();
    }
}

std::wstring CDataManager::GetWeatherInfo() const
{
    std::wstringstream wss;

    wss << L"今天: " << m_weather_today.ToString() << std::endl
        << L"明天: " << m_weather_tommrow.ToString();

    return wss.str();
}

std::wstring CDataManager::GetTooptipInfo() const
{
    std::wstringstream wss;

    wss << m_currentCityInfo.CityName << L" " << GetRealTimeWeatherInfo(m_config.m_show_brief_rt_weather_info) << std::endl;
    if (m_config.m_show_weather_alerts)
        wss << GetWeatherAlertsInfo(m_config.m_show_brief_weather_alert_info);
    wss << GetWeatherInfo();

    return wss.str();
}

SConfiguration& CDataManager::GetConfig()
{
    return m_config;
}

const SConfiguration& CDataManager::GetConfig() const
{
    return m_config;
}

void CDataManager::LoadConfigs(const std::wstring &cfg_dir)
{
    const auto &ch = cfg_dir.back();
    if (ch == L'\\' || ch == L'/')
        m_config_file_path = cfg_dir + L"WeatherPro.ini";
    else
        m_config_file_path = cfg_dir + L"\\WeatherPro.ini";

    auto cfg_int_val_getter = [this](const wchar_t *key, int default_val) {
        int val = GetPrivateProfileInt(L"config", key, default_val, m_config_file_path.c_str());
        return val;
    };

    auto cfg_str_val_getter = [this](const wchar_t *key, const wchar_t *default_val) {
        wchar_t buffer[32]{ 0 };
        GetPrivateProfileString(L"config", key, default_val, buffer, 32, m_config_file_path.c_str());
        return std::wstring{ buffer };
    };

    m_currentCityInfo.CityNO = cfg_str_val_getter(L"city_no", L"101010100");
    m_currentCityInfo.CityName = cfg_str_val_getter(L"city_name", L"北京");

    m_config.m_wit = static_cast<EWeatherInfoType>(cfg_int_val_getter(L"wit", 0));
    m_config.m_show_weather_icon = (cfg_int_val_getter(L"show_weather_icon", 1) != 0);
    m_config.m_show_weather_in_tooltips = (cfg_int_val_getter(L"show_weather_in_tooltips", 1) != 0);
    m_config.m_show_brief_rt_weather_info = (cfg_int_val_getter(L"show_brief_rt_weather_info", 0) != 0);
    m_config.m_show_weather_alerts = (cfg_int_val_getter(L"show_weather_alerts", 1) != 0);
    m_config.m_show_brief_weather_alert_info = (cfg_int_val_getter(L"show_brief_weather_alert_info", 1) != 0);
}

void CDataManager::SaveConfigs() const
{
    auto cfg_int_val_writter = [this](const wchar_t *key, int value) {
        wchar_t buffer[16]{ 0 };

        swprintf_s(buffer, L"%d", value);
        WritePrivateProfileString(L"config", key, buffer, m_config_file_path.c_str());
    };

    auto cfg_str_val_writter = [this](const wchar_t *key, const wchar_t *value) {
        WritePrivateProfileString(L"config", key, value, m_config_file_path.c_str());
    };

    cfg_str_val_writter(L"city_no", m_currentCityInfo.CityNO.c_str());
    cfg_str_val_writter(L"city_name", m_currentCityInfo.CityName.c_str());

    cfg_int_val_writter(L"wit", static_cast<int>(m_config.m_wit));
    cfg_int_val_writter(L"show_weather_icon", m_config.m_show_weather_icon ? 1 : 0);
    cfg_int_val_writter(L"show_weather_in_tooltips", m_config.m_show_weather_in_tooltips ? 1 : 0);
    cfg_int_val_writter(L"show_brief_rt_weather_info", m_config.m_show_brief_rt_weather_info ? 1 : 0);
    cfg_int_val_writter(L"show_weather_alerts", m_config.m_show_weather_alerts ? 1 : 0);
    cfg_int_val_writter(L"show_brief_weather_alert_info", m_config.m_show_brief_weather_alert_info ? 1 : 0);
}
