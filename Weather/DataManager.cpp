#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
    //获取模块的路径
    HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);
    m_module_path = path;
    //从配置文件读取配置
    LoadConfig();
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

void CDataManager::LoadConfig()
{
    std::wstring config_path = m_module_path + L".ini";
    m_setting_data.m_city_index = GetPrivateProfileInt(L"config", L"city", 101, config_path.c_str());
    m_setting_data.m_weather_selected = static_cast<WeahterSelected>(GetPrivateProfileInt(L"config", L"weather_selected", 0, config_path.c_str()));
}

void CDataManager::SaveConfig() const
{
    std::wstring config_path = m_module_path + L".ini";
    WritePrivateProfileInt(L"config", L"city", m_setting_data.m_city_index, config_path.c_str());
    WritePrivateProfileInt(L"config", L"weather_selected", m_setting_data.m_weather_selected, config_path.c_str());
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

CDataManager::WeatherInfo& CDataManager::GetWeather()
{
    return m_weather_info[m_setting_data.m_weather_selected];
}

const std::wstring& CDataManager::GetModulePath() const
{
    return m_module_path;
}
