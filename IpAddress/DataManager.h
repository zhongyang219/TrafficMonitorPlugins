#pragma once
#include <string>
#include <map>
#include "resource.h"
#include "AdapterCommon.h"

#define g_data CDataManager::Instance()


struct SettingData
{
    std::wstring current_connection_name;         //选择的网络连接名称
};

class CDataManager
{
private:
    CDataManager();
    ~CDataManager();

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
    const std::vector<NetWorkConection>& GetAllConnections() const;

    SettingData m_setting_data;

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    int m_dpi{ 96 };

    std::vector<NetWorkConection> m_connections;    //所有网络连接
};
