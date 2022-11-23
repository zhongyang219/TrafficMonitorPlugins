
// PluginTester.h : PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
    #error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"		// 主符号
#include "CommonData.h"

// CPluginTesterApp: 
// 有关此类的实现，请参阅 PluginTester.cpp
//

class CPluginTesterApp : public CWinApp
{
public:
    CPluginTesterApp();
    int DPI(int pixel);
    static CString LoadText(UINT id);
    HICON GetIcon(UINT id);

    Language m_language;
    std::wstring m_config_path;
    HICON m_plugin_icon;

private:
    void LoadConfig();
    void SaveConfig() const;

private:
    int m_dpi{ 96 };
    std::map<UINT, HICON> m_icons;

// 重写
public:
    virtual BOOL InitInstance();

// 实现

    DECLARE_MESSAGE_MAP()
    afx_msg void OnHelp();
};

extern CPluginTesterApp theApp;