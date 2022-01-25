#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>
#include <sstream>

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
}

CDataManager::~CDataManager()
{
    SaveConfig();
}

CDataManager& CDataManager::Instance()
{
    return m_instance;
}

static void WritePrivateProfileInt(const wchar_t* app_name, const wchar_t* key_name, int value, const wchar_t* file_path)
{
    wchar_t buff[16];
    swprintf_s(buff, L"%d", value);
    WritePrivateProfileString(app_name, key_name, buff, file_path);
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
    m_config_dir = config_dir;
    //获取模块的路径
    HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);
    std::wstring module_path = path;
    m_config_path = module_path;
    if (!config_dir.empty())
    {
        size_t index = module_path.find_last_of(L"\\/");
        //模块的文件名
        std::wstring module_file_name = module_path.substr(index + 1);
        m_config_path = config_dir + module_file_name;
    }
    m_config_path += L".ini";
    m_setting_data.m_city_index = GetPrivateProfileInt(L"config", L"city", 101, m_config_path.c_str());
    m_setting_data.m_weather_selected = static_cast<WeahterSelected>(GetPrivateProfileInt(L"config", L"weather_selected", 0, m_config_path.c_str()));
    m_setting_data.m_show_weather_in_tooltips = (GetPrivateProfileInt(L"config", L"show_weather_in_tooltips", 1, m_config_path.c_str()) != 0);
    m_setting_data.m_use_weather_icon = (GetPrivateProfileInt(L"config", L"use_weather_icon", 1, m_config_path.c_str()) != 0);
}

void CDataManager::SaveConfig() const
{
    if (!m_config_path.empty())
    {
        WritePrivateProfileInt(L"config", L"city", m_setting_data.m_city_index, m_config_path.c_str());
        WritePrivateProfileInt(L"config", L"weather_selected", m_setting_data.m_weather_selected, m_config_path.c_str());
        WritePrivateProfileInt(L"config", L"show_weather_in_tooltips", m_setting_data.m_show_weather_in_tooltips, m_config_path.c_str());
        WritePrivateProfileInt(L"config", L"use_weather_icon", m_setting_data.m_use_weather_icon, m_config_path.c_str());
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

std::wstring CDataManager::WeatherInfo::ToString() const
{
    std::wstringstream wss;
    if (m_low.empty())
        wss << m_type << ' ' << m_high;
    else if (m_high.empty())
        wss << m_type << ' ' << m_low;
    else
        wss << m_type << ' ' << m_low << '~' << m_high;
    return wss.str();
}

std::wstring CDataManager::WeatherInfo::ToStringTemperature() const
{
    std::wstringstream wss;
    if (m_low.empty())
        wss << m_high;
    else if (m_high.empty())
        wss << m_low;
    else
        wss << m_low << '~' << m_high;
    return wss.str();
}
