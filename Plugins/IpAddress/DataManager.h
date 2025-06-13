#pragma once
#include <chrono>
#include <string>
#include <map>
#include <memory>
#include "resource.h"
#include "AdapterCommon.h"
#include "IExternalIpProvider.h"
#include "Common.h"

#define g_data CDataManager::Instance()


    struct SettingData
    {
        std::wstring current_connection_name;
        int ip_query_interval{ 60 };
        std::wstring ip_provider_name;
    };

class CDataManager
{
private:
    CDataManager();
    ~CDataManager();

    static UINT ExternalIpUpdateThread(LPVOID dwUser);

public:
    static CDataManager& Instance();

    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig() const;
    const CString& StringRes(UINT id);      //根据资源id获取一个字符串资源
    void DPIFromWindow(CWnd* pWnd);
    int DPI(int pixel);
    float DPIF(float pixel);
    int RDPI(int pixel);
    HICON GetIcon(UINT id);

    void UpdateConnections();
    bool GetLocalIPv4Address(std::wstring& ipv4address);
    bool GetExternalIPv4Address(std::wstring& ipv4address);
    void ForceRefreshExternalIp();
    const std::map<std::wstring, NetWorkConection>& GetAllConnections() const;
    const std::map<std::wstring, std::unique_ptr<IExternalIpProvider>>& GetIpProviders() const;


    SettingData m_setting_data;

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    int m_dpi{ 96 };

    std::map<std::wstring, NetWorkConection> m_connections;    //所有网络连接
    std::map<std::wstring, std::unique_ptr<IExternalIpProvider>> m_ip_providers;
    std::wstring m_external_ip;
    std::chrono::steady_clock::time_point m_last_ip_query_time;
    bool m_is_thread_running{};
};
