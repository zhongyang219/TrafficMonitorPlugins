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

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
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
    m_setting_data.battery_type = static_cast<BatteryType>(GetPrivateProfileInt(L"config", L"battery_type", static_cast<int>(BatteryType::NUMBER_BESIDE_ICON), m_config_path.c_str()));
    m_setting_data.show_battery_in_tooltip = (GetPrivateProfileInt(L"config", L"show_battery_in_tooltip", 1, m_config_path.c_str()) != 0);
    m_setting_data.show_percent = (GetPrivateProfileInt(L"config", L"show_percent", 1, m_config_path.c_str()) != 0);
    m_setting_data.show_charging_animation = (GetPrivateProfileInt(L"config", L"show_charging_animation", 0, m_config_path.c_str()) != 0);
}

void CDataManager::SaveConfig() const
{
    if (!m_config_path.empty())
    {
        WritePrivateProfileInt(L"config", L"battery_type", static_cast<int>(m_setting_data.battery_type), m_config_path.c_str());
        WritePrivateProfileInt(L"config", L"show_battery_in_tooltip", m_setting_data.show_battery_in_tooltip, m_config_path.c_str());
        WritePrivateProfileInt(L"config", L"show_percent", m_setting_data.show_percent, m_config_path.c_str());
        WritePrivateProfileInt(L"config", L"show_charging_animation", m_setting_data.show_charging_animation, m_config_path.c_str());
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
        HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);
        m_icons[id] = hIcon;
        return hIcon;
    }
}


bool CDataManager::IsAcOnline() const
{
    return m_sysPowerStatus.ACLineStatus == 1;
}

bool CDataManager::IsCharging() const
{
    return m_sysPowerStatus.BatteryFlag == 8;
}

std::wstring CDataManager::GetBatteryString() const
{
    if (m_sysPowerStatus.BatteryFlag == 128)
    {
        return L"N/A";
    }
    else
    {
        std::wstring str = std::to_wstring(m_sysPowerStatus.BatteryLifePercent);
        if (m_setting_data.show_percent)
        {
            if (m_sysPowerStatus.BatteryLifePercent < 100)
                str.push_back(L' ');
            str.push_back(L'%');
        }
        return str;
    }
}

COLORREF CDataManager::GetBatteryColor() const
{
    if (m_sysPowerStatus.BatteryLifePercent < 20)
        return BATTERY_COLOR_CRITICAL;
    else if (m_sysPowerStatus.BatteryLifePercent < 60)
        return BATTERY_COLOR_LOW;
    else
        return BATTERY_COLOR_HIGH;
}
