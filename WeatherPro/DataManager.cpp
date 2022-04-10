#include "pch.h"
#include "DataManager.h"
#include "resource.h"

#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <set>

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

const CString& CDataManager::StringRes(UINT id) const
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

    RefreshWeatherInfoCache();

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
    return m_weather_info_cache.WeatherTemperature;
}

std::wstring CDataManager::GetRealTimeWeatherInfo() const
{
    return m_weather_info_cache.RealTimeWeatherInfo;
}

std::wstring CDataManager::GetWeatherAlertsInfo() const
{
    return m_weather_info_cache.WeatherAlersInfo;
}

std::wstring CDataManager::GetWeatherInfo() const
{
    return m_weather_info_cache.WeatherInfo;
}

std::wstring CDataManager::GetTooptipInfo() const
{
    return m_weather_info_cache.TooltipInfo;
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

HICON CDataManager::_getIconByID(UINT id)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);

    return hIcon;
}

UINT CDataManager::_getIconIdBlue(const std::wstring& code) const
{
    static const std::map<std::wstring, UINT> dmap{
        {L"d00", IDI_ICON_B_D00},
        {L"d01", IDI_ICON_B_D01},
        {L"d03", IDI_ICON_B_D03},
        {L"d13", IDI_ICON_B_D13},
        {L"n00", IDI_ICON_B_N00},
        {L"n01", IDI_ICON_B_N01},
        {L"n03", IDI_ICON_B_N03},
        {L"n13", IDI_ICON_B_N13},
        {L"02", IDI_ICON_B_A02},
        {L"04", IDI_ICON_B_A04},
        {L"05", IDI_ICON_B_A05},
        {L"06", IDI_ICON_B_A06},
        {L"07", IDI_ICON_B_A07},
        {L"08", IDI_ICON_B_A08},
        {L"09", IDI_ICON_B_A09},
        {L"10", IDI_ICON_B_A10},
        {L"11", IDI_ICON_B_A11},
        {L"12", IDI_ICON_B_A12},
        {L"14", IDI_ICON_B_A14},
        {L"15", IDI_ICON_B_A15},
        {L"16", IDI_ICON_B_A16},
        {L"17", IDI_ICON_B_A17},
        {L"18", IDI_ICON_B_A18},
        {L"19", IDI_ICON_B_A19},
        {L"20", IDI_ICON_B_A20},
        {L"29", IDI_ICON_B_A29},
        {L"30", IDI_ICON_B_A30},
        {L"31", IDI_ICON_B_A31},
        {L"53", IDI_ICON_B_A53},
        {L"54", IDI_ICON_B_A54},
        {L"55", IDI_ICON_B_A55},
        {L"56", IDI_ICON_B_A56},
        {L"99", IDI_ICON_B_A99},
        {L"21", IDI_ICON_B_A08},
        {L"22", IDI_ICON_B_A09},
        {L"23", IDI_ICON_B_A10},
        {L"24", IDI_ICON_B_A11},
        {L"25", IDI_ICON_B_A12},
        {L"26", IDI_ICON_B_A15},
        {L"27", IDI_ICON_B_A16},
        {L"28", IDI_ICON_B_A17},
        {L"32", IDI_ICON_B_A18},
        {L"49", IDI_ICON_B_A18},
        {L"57", IDI_ICON_B_A18},
        {L"58", IDI_ICON_B_A18},
        {L"97", IDI_ICON_B_A08},
        {L"98", IDI_ICON_B_A15},
        {L"301", IDI_ICON_B_A08},
        {L"302", IDI_ICON_B_A15},
    };

    auto itr = dmap.find(code);
    if (itr != dmap.end())
        return itr->second;
    else if (code.size() > 1)
    {
        auto itr2 = dmap.find(code.substr(1));
        if (itr2 != dmap.end())
            return itr2->second;
    }

    return IDI_ICON_B_A99;
}

HICON CDataManager::GetIcon() const
{
    return m_weather_info_cache.Icon;
}

HICON CDataManager::_getIcon()
{
    switch (m_config.m_wit)
    {
    default:
    case EWeatherInfoType::WEATHER_REALTIME:
        return _getIconByCode(m_rt_weather.WeatherCode);

    case EWeatherInfoType::WEATHER_TODAY:
        return _getIconByCode(m_weather_today.WeatherCodeDay);

    case EWeatherInfoType::WEATHER_TOMMROW:
        return _getIconByCode(m_weather_tommrow.WeatherCodeDay);
    }
}

HICON CDataManager::_getIconByCode(const std::wstring& w_code)
{
    if (w_code.empty()) return nullptr;

    auto icon_id = _getIconIdBlue(w_code);

    if (m_icons.find(icon_id) == m_icons.end())
    {
        m_icons[icon_id] = _getIconByID(icon_id);
    }

    return m_icons[icon_id];
}

std::wstring CDataManager::_getWeatherTemperature() const
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

std::wstring CDataManager::_getRealTimeWeatherInfo(bool brief) const
{
    return m_rt_weather.ToString(brief);
}

std::wstring CDataManager::_getWeatherAlertsInfo(bool brief) const
{
    if (m_weather_alerts.empty())
    {
        return std::wstring();
    }
    else
    {
        std::wstringstream wss;

        for (const auto& alert : m_weather_alerts)
            wss << L"[!] " << alert.ToString(brief) << std::endl;

        return wss.str();
    }
}

std::wstring CDataManager::_getWeatherInfo() const
{
    std::wstringstream wss;

    wss << std::wstring(StringRes(IDS_TODAY)) << L": " << m_weather_today.ToString() << std::endl
        << std::wstring(StringRes(IDS_TOMORROW)) << L": " << m_weather_tommrow.ToString();

    return wss.str();
}

std::wstring CDataManager::_getTooptipInfo() const
{
    std::wstringstream wss;

    wss << m_currentCityInfo.CityName << L" " << _getRealTimeWeatherInfo(m_config.m_show_brief_rt_weather_info) << std::endl;
    if (m_config.m_show_weather_alerts)
        wss << _getWeatherAlertsInfo(m_config.m_show_brief_weather_alert_info);
    wss << _getWeatherInfo();

    return wss.str();
}

void CDataManager::RefreshWeatherInfoCache()
{
    m_weather_info_cache.WeatherTemperature = _getWeatherTemperature();
    m_weather_info_cache.RealTimeWeatherInfo = _getRealTimeWeatherInfo(m_config.m_show_brief_rt_weather_info);
    m_weather_info_cache.WeatherAlersInfo = _getWeatherAlertsInfo(m_config.m_show_brief_weather_alert_info);
    m_weather_info_cache.WeatherInfo = _getWeatherInfo();
    m_weather_info_cache.TooltipInfo = _getTooptipInfo();
    m_weather_info_cache.Icon = _getIcon();
}
