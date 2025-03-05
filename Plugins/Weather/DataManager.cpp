#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>
#include <sstream>
#include "utilities/IniHelper.h"
#include "utilities/Common.h"

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
    //初始天气图标ID
    m_weather_icon_id[L"小雨"] = IDI_LIGHT_RAIN;
    m_weather_icon_id[L"小到中雨"] = IDI_LIGHT_RAIN;
    m_weather_icon_id[L"中雨"] = IDI_MODERATE_RAIN;
    m_weather_icon_id[L"中到大雨"] = IDI_MODERATE_RAIN;
    m_weather_icon_id[L"大雨"] = IDI_HEAVY_RAIN;
    m_weather_icon_id[L"大到暴雨"] = IDI_HEAVY_RAIN;
    m_weather_icon_id[L"暴雨"] = IDI_RAINSTORM;
    m_weather_icon_id[L"暴雨到大暴雨"] = IDI_RAINSTORM;
    m_weather_icon_id[L"大暴雨"] = IDI_HEAVY_STORM;
    m_weather_icon_id[L"大暴雨到特大暴雨"] = IDI_HEAVY_STORM;
    m_weather_icon_id[L"特大暴雨"] = IDI_HEAVY_STORM;
    m_weather_icon_id[L"冻雨"] = IDI_HAIL;
    m_weather_icon_id[L"阵雨"] = IDI_SHOWER;
    m_weather_icon_id[L"雷阵雨"] = IDI_THUNDERSHOWER;
    m_weather_icon_id[L"雨夹雪"] = IDI_RAIN_AND_SNOW;
    m_weather_icon_id[L"雷阵雨伴有冰雹"] = IDI_THUNDERSTORM;
    m_weather_icon_id[L"小雪"] = IDI_LIGHT_SNOW;
    m_weather_icon_id[L"小到中雪"] = IDI_LIGHT_SNOW;
    m_weather_icon_id[L"中雪"] = IDI_MODERATE_SNOW;
    m_weather_icon_id[L"中到大雪"] = IDI_MODERATE_SNOW;
    m_weather_icon_id[L"大雪"] = IDI_HEAVY_SNOW;
    m_weather_icon_id[L"大到暴雪"] = IDI_HEAVY_SNOW;
    m_weather_icon_id[L"暴雪"] = IDI_SNOWSTORM;
    m_weather_icon_id[L"阵雪"] = IDI_SNOW_SHOWER;
    m_weather_icon_id[L"晴"] = IDI_SUNNY;
    m_weather_icon_id[L"多云"] = IDI_CLOUDY2;
    m_weather_icon_id[L"阴"] = IDI_CLOUDY;
    m_weather_icon_id[L"强沙尘暴"] = IDI_SANDSTORM;
    m_weather_icon_id[L"扬沙"] = IDI_DUST;
    m_weather_icon_id[L"沙尘暴"] = IDI_SANDSTORM;
    m_weather_icon_id[L"浮尘"] = IDI_DUST2;
    m_weather_icon_id[L"雾"] = IDI_FOG;
    m_weather_icon_id[L"霾"] = IDI_SMOG;
    //初始化DPI
    HDC hDC = ::GetDC(HWND_DESKTOP);
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(HWND_DESKTOP, hDC);

    g_data.m_weather_info[WEATHER_CURRENT].is_cur_weather = true;
}

CDataManager::~CDataManager()
{
    SaveConfig();
}

CDataManager& CDataManager::Instance()
{
    return m_instance;
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
    m_config_dir = config_dir;
    m_config_path = config_dir + L"Weather.ini";
    utilities::CIniHelper ini(m_config_path);
    m_setting_data.m_city_index = ini.GetInt(L"config", L"city", 101);
    m_setting_data.m_weather_selected = static_cast<WeahterSelected>(ini.GetInt(L"config", L"weather_selected", 0));
    m_setting_data.m_show_weather_in_tooltips = ini.GetBool(L"config", L"show_weather_in_tooltips", true);
    m_setting_data.m_use_weather_icon = ini.GetBool(L"config", L"use_weather_icon", true);
    m_setting_data.auto_locate = ini.GetBool(L"config", L"auto_locate", true);
}

void CDataManager::SaveConfig() const
{
    if (!m_config_path.empty())
    {
        utilities::CIniHelper ini(m_config_path);
        ini.WriteInt(L"config", L"city", m_setting_data.m_city_index);
        ini.WriteInt(L"config", L"weather_selected", m_setting_data.m_weather_selected);
        ini.WriteBool(L"config", L"show_weather_in_tooltips", m_setting_data.m_show_weather_in_tooltips);
        ini.WriteBool(L"config", L"use_weather_icon", m_setting_data.m_use_weather_icon);
        ini.WriteBool(L"config", L"auto_locate", m_setting_data.auto_locate);
        ini.Save();
    }
}

const CString& CDataManager::StringRes(UINT id)
{
    auto iter = m_string_table.find(id);
    if (iter != m_string_table.end())
    {
        return iter->second;
    }
    else
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        m_string_table[id].LoadString(id);
        return m_string_table[id];
    }
}

void CDataManager::DPIFromWindow(CWnd* pWnd)
{
    CWindowDC dc(pWnd);
    HDC hDC = dc.GetSafeHdc();
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
}

int CDataManager::DPI(int pixel)
{
    return m_dpi * pixel / 96;
}

float CDataManager::DPIF(float pixel)
{
    return m_dpi * pixel / 96;
}

int CDataManager::RDPI(int pixel)
{
    return pixel * 96 / m_dpi;
}

HICON CDataManager::GetIcon(UINT id)
{
    auto iter = m_icons.find(id);
    if (iter != m_icons.end())
    {
        return iter->second;
    }
    else
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);;
        m_icons[id] = hIcon;
        return hIcon;
    }
}

CityCodeItem CDataManager::CurCity() const
{
    if (m_setting_data.m_city_index >= 0 && m_setting_data.m_city_index < static_cast<int>(CityCode.size()))
        return CityCode[m_setting_data.m_city_index];
    else
        return CityCodeItem();
}

void CDataManager::ResetText()
{
    m_weather_info[m_setting_data.m_weather_selected] = WeatherInfo();
}

HICON CDataManager::GetWeatherIcon(const std::wstring weather_type)
{
    UINT id{};
    auto iter = m_weather_icon_id.find(weather_type);
    if (iter != m_weather_icon_id.end())
    {
        id = iter->second;
    }
    else
    {
        id = IDI_UNKOWN_WEATHER;
    }
    //获取当前时间
    SYSTEMTIME cur_time{};
    GetLocalTime(&cur_time);
    if (cur_time.wHour >= 18 || cur_time.wHour < 6)        //如果是晚上
    {
        if (id == IDI_SUNNY)
            id = IDI_SUNNY_NIGHT;
        if (id == IDI_CLOUDY2)
            id = IDI_CLOUDY_NIGHT;
    }
    return GetIcon(id);
}

CDataManager::WeatherInfo& CDataManager::GetWeather()
{
    return m_weather_info[m_setting_data.m_weather_selected];
}

CString CDataManager::GetUpdateTimeAsString()
{
    // 获取当前时间
    CTime now = CTime::GetCurrentTime();

    // 计算日期差（以天为单位）
    CTimeSpan span = now - m_update_time;
    int daysDiff = static_cast<int>(span.GetDays());

    // 格式化日期部分
    CString strDate;
    if (daysDiff == 0)
    {
        strDate = StringRes(IDS_TODAY_WEATHER);
    }
    else if (daysDiff == 1)
    {
        strDate = StringRes(IDS_YESTERDAY);
    }
    else if (daysDiff > 1)
    {
        strDate = utilities::StringHelper::StringFormat(StringRes(IDS_DAYS_AGO).GetString(), { daysDiff }).c_str();
    }
    else if (daysDiff < 0)
    {
        strDate = utilities::StringHelper::StringFormat(StringRes(IDS_DAYS_LATER).GetString(), { -daysDiff }).c_str();
    }

    // 格式化时间部分（固定为 hh:mm）
    CString strTime = m_update_time.Format(_T("%H:%M"));

    // 组合日期和时间
    CString strResult;
    strResult.Format(_T("%s %s"), strDate.GetString(), strTime.GetString());

    return strResult;
}

CString CDataManager::GetPM25AsString() const
{
    wchar_t buff[32];
    swprintf_s(buff, L"%g", m_pm2_5);
    return CString(buff);
}

std::wstring CDataManager::WeatherInfo::ToString() const
{
    std::wstringstream wss;
    if (m_low.empty() || is_cur_weather)
        wss << m_type << ' ' << m_high;
    else if (m_high.empty())
        wss << m_type << ' ' << m_low;
    else
        wss << m_type << ' ' << m_low << '~' << m_high;
    wss << ' ' << m_wind;
    return wss.str();
}

std::wstring CDataManager::WeatherInfo::ToStringTemperature() const
{
    std::wstringstream wss;
    if (m_low.empty() || is_cur_weather)
        wss << m_high;
    else if (m_high.empty())
        wss << m_low;
    else
        wss << m_low << '~' << m_high;
    return wss.str();
}
