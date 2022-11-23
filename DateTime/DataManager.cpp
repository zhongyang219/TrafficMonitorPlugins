#include "pch.h"
#include "DataManager.h"

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
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
    //m_setting_data.show_second = GetPrivateProfileInt(_T("config"), _T("show_second"), 0, m_config_path.c_str());
    //m_setting_data.show_label_text = GetPrivateProfileInt(_T("config"), _T("show_label_text"), 1, m_config_path.c_str());
    TCHAR buf[256];
    GetPrivateProfileString(_T("config"), _T("time_format"), _T("HH:mm:ss"), buf, 255, m_config_path.c_str());
    m_setting_data.time_format = buf;
    GetPrivateProfileString(_T("config"), _T("date_format"), _T("yyyy/MM/dd"), buf, 255, m_config_path.c_str());
    m_setting_data.date_format = buf;
}

static void WritePrivateProfileInt(const wchar_t* app_name, const wchar_t* key_name, int value, const wchar_t* file_path)
{
    wchar_t buff[16];
    swprintf_s(buff, L"%d", value);
    WritePrivateProfileString(app_name, key_name, buff, file_path);
}

void CDataManager::SaveConfig() const
{
    //WritePrivateProfileInt(_T("config"), _T("show_second"), m_setting_data.show_second, m_config_path.c_str());
    //WritePrivateProfileInt(_T("config"), _T("show_label_text"), m_setting_data.show_label_text, config_path.c_str());
    WritePrivateProfileString(_T("config"), _T("time_format"), m_setting_data.time_format.c_str(), m_config_path.c_str());
    WritePrivateProfileString(_T("config"), _T("date_format"), m_setting_data.date_format.c_str(), m_config_path.c_str());
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
        HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);
        m_icons[id] = hIcon;
        return hIcon;
    }
}
