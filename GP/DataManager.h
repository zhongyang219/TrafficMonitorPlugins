#pragma once
#include <string>
#include <map>
#include "resource.h"

#define g_data CDataManager::Instance()


struct SettingData
{
    CString m_all_gp_code_str{};
    vector<CString> m_gp_codes; // 代码
    bool m_full_day{}; // 全天更新

    void setupByCodeStr(CString codeStr);
    void updateAllCodeStr();
};

// GP显示数据
struct GupiaoInfo
{
    std::wstring pc = L"--%";
    std::wstring p = L"--";
    std::wstring name = L"";
    std::wstring ToString() const;
};

class CDataManager
{
private:
    CDataManager();
    ~CDataManager();

public:
    static CDataManager& Instance();

    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig();
    const CString& StringRes(UINT id);      //根据资源id获取一个字符串资源
    void DPIFromWindow(CWnd* pWnd);
    int DPI(int pixel);
    float DPIF(float pixel);
    int RDPI(int pixel);
    HICON GetIcon(UINT id);

    void ResetText();
    SettingData m_setting_data;

    GupiaoInfo& GetGPInfo(CString key);

    std::map<CString, GupiaoInfo> m_gupiao_info_map;

    std::wstring m_log_path;
    std::wstring m_config_dir;

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    int m_dpi{ 96 };
};
