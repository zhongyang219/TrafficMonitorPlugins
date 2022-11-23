#pragma once
#include <string>
#include <map>
#include "DateTimeFormatHelper.h"

#define g_data CDataManager::Instance()

struct SettingData
{
    //bool show_second{};
    //bool show_label_text{};
    std::wstring time_format;
    std::wstring date_format;
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
    int DPI(int pixel);
    HICON GetIcon(UINT id);

public:
    std::wstring m_cur_time;
    std::wstring m_cur_date;
    //SYSTEMTIME m_system_time;
    SettingData m_setting_data;
    CDataTimeFormatHelper m_format_helper;

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    int m_dpi{ 96 };
    std::map<UINT, HICON> m_icons;
};
