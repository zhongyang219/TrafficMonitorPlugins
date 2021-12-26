#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>
#include <sstream>
#include <Gdiplus.h>
#pragma comment(lib,"GdiPlus.lib")

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
    //初始化DPI
    HDC hDC = ::GetDC(HWND_DESKTOP);
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(HWND_DESKTOP, hDC);

    //初始化GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
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
    m_setting_data.battery_type = static_cast<BatteryType>(GetPrivateProfileInt(L"config", L"battery_type", static_cast<int>(BatteryType::NUMBER_BESIDE_ICON), config_path.c_str()));
    m_setting_data.show_battery_in_tooltip = (GetPrivateProfileInt(L"config", L"show_battery_in_tooltip", 1, config_path.c_str()) != 0);
}

void CDataManager::SaveConfig() const
{
    std::wstring config_path = m_module_path + L".ini";
    WritePrivateProfileInt(L"config", L"battery_type", static_cast<int>(m_setting_data.battery_type), config_path.c_str());
    WritePrivateProfileInt(L"config", L"show_battery_in_tooltip", m_setting_data.show_battery_in_tooltip, config_path.c_str());
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


std::wstring CDataManager::GetBatteryString() const
{
    if (m_sysPowerStatus.BatteryFlag == 128)
        return L"N/A";
    else
        return std::to_wstring(m_sysPowerStatus.BatteryLifePercent) + L" %";
}

COLORREF CDataManager::GetBatteryColor() const
{
    switch (m_sysPowerStatus.BatteryFlag)
    {
    case 2:
        return BATTERY_COLOR_LOW;
    case 4:
        return BATTERY_COLOR_CRITICAL;
    default:
        break;
    }
    return BATTERY_COLOR_HIGH;
}

const std::wstring& CDataManager::GetModulePath() const
{
    return m_module_path;
}
