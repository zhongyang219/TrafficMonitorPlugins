#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>
#include <sstream>

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
    //初始化DPI
    HDC hDC = ::GetDC(HWND_DESKTOP);
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(HWND_DESKTOP, hDC);

    m_config_dir = L".\\";
    m_log_path = m_config_dir + L"GP.dll.log";
}

CDataManager::~CDataManager()
{
    SaveConfig();
}

CDataManager& CDataManager::Instance()
{
    return m_instance;
}

void CDataManager::ResetText()
{
    //m_gupiao_info_arr = GupiaoInfo();
    m_gupiao_info_map.clear();
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
    //CCommon::WriteLog(m_config_path.c_str(), g_data.m_log_path.c_str());
    CString codeStr{};
    ::GetPrivateProfileString(L"config", L"gp_code", L"", codeStr.GetBuffer(MAX_PATH), MAX_PATH, m_config_path.c_str());
    m_setting_data.setupByCodeStr(codeStr);
    //Log2("LoadConfig: %s -> %d\n", m_setting_data.m_all_gp_code_str, m_setting_data.m_gp_codes.size());
    //::GetPrivateProfileString(L"config", L"licence", L"", m_setting_data.m_licence.GetBuffer(MAX_PATH), MAX_PATH, m_config_path.c_str());
    m_setting_data.m_full_day = (GetPrivateProfileInt(L"config", L"full_day", 1, m_config_path.c_str()) != 0);
}

void CDataManager::SaveConfig()
{
    if (!m_config_path.empty())
    {
        m_setting_data.updateAllCodeStr();
        ::WritePrivateProfileString(L"config", L"gp_code", m_setting_data.m_all_gp_code_str, m_config_path.c_str());
        //::WritePrivateProfileString(L"config", L"licence", m_setting_data.m_licence, m_config_path.c_str());
        ::WritePrivateProfileInt(L"config", L"full_day", m_setting_data.m_full_day, m_config_path.c_str());
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

std::wstring GupiaoInfo::ToString() const
{
    std::wstringstream wss;
    wss << p << ' ' << pc;
    return wss.str();
}

GupiaoInfo& CDataManager::GetGPInfo(CString key)
{
    return m_gupiao_info_map[key];
}

void SettingData::updateAllCodeStr()
{
    if (m_gp_codes.empty())
    {
        m_all_gp_code_str.ReleaseBuffer();
        int len = m_all_gp_code_str.GetLength();
        m_all_gp_code_str.Delete(0, len);
        return;
    }
    m_all_gp_code_str = CCommon::vectorJoinString(m_gp_codes, ",");
}

void SettingData::setupByCodeStr(CString codeStr)
{
    vector<string> codes = CCommon::split(CCommon::UnicodeToStr(codeStr), ',');
    if (codes.size() > 0)
    {
        m_all_gp_code_str = codeStr;
        m_gp_codes.clear();
        for (string item : codes)
        {
            m_gp_codes.push_back(item.c_str());
        }
    }
}
