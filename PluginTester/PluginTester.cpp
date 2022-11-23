
// PluginTester.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "PluginTester.h"
#include "PluginTesterDlg.h"
#include "../utilities/IniHelper.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CPluginTesterApp

BEGIN_MESSAGE_MAP(CPluginTesterApp, CWinApp)
    ON_COMMAND(ID_HELP, &CPluginTesterApp::OnHelp)
END_MESSAGE_MAP()


// CPluginTesterApp 构造

CPluginTesterApp::CPluginTesterApp()
{
    // TODO: 在此处添加构造代码，
    // 将所有重要的初始化放置在 InitInstance 中
}


int CPluginTesterApp::DPI(int pixel)
{
    return m_dpi * pixel / 96;
}

CString CPluginTesterApp::LoadText(UINT id)
{
    CString str;
    str.LoadString(id);
    return str;
}

HICON CPluginTesterApp::GetIcon(UINT id)
{
    auto iter = m_icons.find(id);
    if (iter != m_icons.end())
    {
        return iter->second;
    }
    else
    {
        HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);
        m_icons[id] = hIcon;
        return hIcon;
    }
}

void CPluginTesterApp::LoadConfig()
{
    utilities::CIniHelper ini(m_config_path);
    m_language = static_cast<Language>(ini.GetInt(L"config", L"language"));
}

void CPluginTesterApp::SaveConfig() const
{
    utilities::CIniHelper ini(m_config_path);
    ini.WriteInt(L"config", L"language", static_cast<int>(m_language));
    ini.Save();
}

// 唯一的一个 CPluginTesterApp 对象

CPluginTesterApp theApp;


// CPluginTesterApp 初始化

BOOL CPluginTesterApp::InitInstance()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    m_config_path = std::wstring(path) + L".ini";

    LoadConfig();

    //初始化界面语言
    switch (m_language)
    {
    case Language::ENGLISH:
        SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
        break;
    case Language::SIMPLIFIED_CHINESE:
        SetThreadUILanguage(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED));
        break;
    default:
        break;
    }

    CWinApp::InitInstance();


    // 创建 shell 管理器，以防对话框包含
    // 任何 shell 树视图控件或 shell 列表视图控件。
    CShellManager *pShellManager = new CShellManager;

    // 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
    CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

    // 标准初始化
    // 如果未使用这些功能并希望减小
    // 最终可执行文件的大小，则应移除下列
    // 不需要的特定初始化例程
    // 更改用于存储设置的注册表项
    // TODO: 应适当修改该字符串，
    // 例如修改为公司或组织名
    //SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

    //初始化DPI
    HDC hDC = ::GetDC(HWND_DESKTOP);
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(HWND_DESKTOP, hDC);

    m_plugin_icon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_PLUGIN), IMAGE_ICON, DPI(16), DPI(16), 0);

    CPluginTesterDlg dlg;
    m_pMainWnd = &dlg;
    INT_PTR nResponse = dlg.DoModal();
    if (nResponse == IDOK)
    {
        // TODO: 在此放置处理何时用
        //  “确定”来关闭对话框的代码
    }
    else if (nResponse == IDCANCEL)
    {
        // TODO: 在此放置处理何时用
        //  “取消”来关闭对话框的代码
    }
    else if (nResponse == -1)
    {
        TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
        TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
    }

    // 删除上面创建的 shell 管理器。
    if (pShellManager != NULL)
    {
        delete pShellManager;
    }

    SaveConfig();

    // 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
    //  而不是启动应用程序的消息泵。
    return FALSE;
}



void CPluginTesterApp::OnHelp()
{
    ShellExecute(NULL, _T("open"), _T("https://github.com/zhongyang219/TrafficMonitorPlugins/wiki/%E6%8F%92%E4%BB%B6%E6%B5%8B%E8%AF%95%E5%99%A8"), NULL, NULL, SW_SHOW);
}
