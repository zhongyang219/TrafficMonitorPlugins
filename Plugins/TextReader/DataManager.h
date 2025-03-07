#pragma once
#include <string>
#include <map>
#include "resource.h"
#include "ChapterParser.h"
#include "BookmarkMgr.h"

#define g_data CDataManager::Instance()


struct SettingData
{
    //TODO: 在此添加选项设置的数据
    std::wstring file_path;     //文本文件路径
    int window_width;           //窗口宽度
    int current_position;       //当前阅读位置（字）
    bool show_in_tooltips;         //在鼠标提示中显示
    bool enable_mulit_line;     //允许多行显示
    bool hide_when_lose_focus;    //失去焦点时不显示
    bool auto_read;             //自动阅读
    int auto_read_timer_interval;   //自动阅读的时间间隔
    bool auto_decode_base64{};  //自动对base64编码的文本文件解码
    bool use_own_context_menu{};    //使用独立的右键菜单
    bool restart_at_end{};      //阅读到结束时自动返回开始
    bool auto_reload_when_file_changed{};   //文件更改时自动重新加载
    bool mouse_wheel_read{};    //使用鼠标滚轮阅读
    int mouse_wheel_charactors{ 1 };    //鼠标滚轮一次阅读字数
    bool mouse_wheel_read_page{};   //鼠标滚轮一次阅读整页
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

    void PageUp(int step = 0);
    void PageDown(int step = 0);
    void AddBookmark();

    bool IsMultiLine() const;
    bool HasFocus() const;

    CChapterParser& GetChapter();
    const std::set<int>& GetBookmark();

    void SaveReadPosition();

    SettingData m_setting_data;
    int m_page_step{ 1 };
    bool m_boss_key_pressed{ false };
    bool m_multi_line{ false };
    int m_draw_width{};
    HWND m_wnd{};
    int m_position_modified{ false };

    void CheckFileChange();

private:
    unsigned __int64 m_file_last_modified{};    //打开文件的上次修改时间

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    int m_dpi{ 96 };
    std::wstring m_text_contents;
    CChapterParser m_chapter_parser{ m_text_contents };
    CBookmarkMgr m_bookmark_mgr;
};
