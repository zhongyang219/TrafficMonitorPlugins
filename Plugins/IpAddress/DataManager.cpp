#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>
#include <sstream>
#include <chrono>
#include "../utilities/FilePathHelper.h"
#include "../utilities/IniHelper.h"
#include "IpSbProvider.h"
#include "TencentIpProvider.h"
#include "DummyIpProvider.h"

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
    m_ip_providers.push_back(std::make_unique<CDummyIpProvider>());
    m_ip_providers.push_back(std::make_unique<CIpSbProvider>());
    m_ip_providers.push_back(std::make_unique<CTencentIpProvider>());
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
    m_config_path = utilities::CFilePathHelper(m_config_path).GetFilePathWithoutExtension();
    m_config_path += L".ini";
    utilities::CIniHelper ini(m_config_path);
    m_setting_data.current_connection_name = ini.GetString(L"config", L"connection_name");
    m_setting_data.ip_query_interval = ini.GetInt(L"config", L"ip_query_interval", 60);
    m_setting_data.ip_provider_name = ini.GetString(L"config", L"ip_provider_name", L"None");
    if (m_setting_data.current_connection_name.empty() && !m_connections.empty())
    {
        m_setting_data.current_connection_name = m_connections[0].description;
    }

}

void CDataManager::SaveConfig() const
{
    if (!m_config_path.empty())
    {
        utilities::CIniHelper ini(m_config_path);
        ini.WriteString(L"config", L"connection_name", m_setting_data.current_connection_name);
        ini.WriteInt(L"config", L"ip_query_interval", m_setting_data.ip_query_interval);
        ini.WriteString(L"config", L"ip_provider_name", m_setting_data.ip_provider_name);
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
        HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);
        m_icons[id] = hIcon;
        return hIcon;
    }
}

void CDataManager::UpdateConnections()
{
    CAdapterCommon::GetAdapterInfo(m_connections);
}

bool CDataManager::GetLocalIPv4Address(std::wstring& ipv4address)
{
    for (const auto& item : m_connections)
    {
        if (item.description == m_setting_data.current_connection_name)
        {
            ipv4address = item.ip_address;
            return true;
        }
    }
    return false;
}

bool CDataManager::GetExternalIPv4Address(std::wstring& ipv4address)
{
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - m_last_ip_query_time).count() > m_setting_data.ip_query_interval)
    {
        m_last_ip_query_time = now;
        if (!m_is_thread_running)
            AfxBeginThread(ExternalIpUpdateThread, nullptr);
    }
    ipv4address = m_external_ip;
    return !m_external_ip.empty();
}

void CDataManager::ForceRefreshExternalIp()
{
    m_last_ip_query_time = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(0));
}

UINT CDataManager::ExternalIpUpdateThread(LPVOID dwUser)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CFlagLocker flag_locker(m_instance.m_is_thread_running);
    for (const auto& provider : m_instance.m_ip_providers)
    {
        if (provider->GetName() == m_instance.m_setting_data.ip_provider_name)
        {
            if (!provider->GetExternalIp(m_instance.m_external_ip))
            {
                m_instance.m_external_ip = L"<disconnected>";
            }
            break;
        }
    }
    return 0;
}

const std::vector<NetWorkConection>& CDataManager::GetAllConnections() const
{
    return m_connections;
}

const std::vector<std::unique_ptr<IExternalIpProvider>>& CDataManager::GetIpProviders() const
{
    return m_ip_providers;
}
