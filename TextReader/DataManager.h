#pragma once
#include <string>
#include <map>
#include "resource.h"

#define g_data CDataManager::Instance()


struct SettingData
{
    //TODO: 在此添加选项设置的数据
    std::wstring file_path;     //文本文件路径
    int window_width;           //窗口宽度
    int current_position;       //当前阅读位置（字）
    bool enable_mulit_line;     //允许多行显示
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

    bool LoadTextContents(LPCTSTR file_path);
    const std::wstring& GetTextContexts() const;

    int GetPageStep(CDC* dc);      //获取翻页步长，并保存在m_page_step中

    void PageUp(int step);
    void PageDown(int step);

    bool IsMultiLine() const;

    SettingData m_setting_data;
    int m_page_step{ 1 };
    bool m_boss_key_pressed{ false };
    bool m_multi_line{ false };
    int m_draw_width{};

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    int m_dpi{ 96 };
    std::wstring m_text_contents;
};
