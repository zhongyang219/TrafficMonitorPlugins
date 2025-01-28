
// PluginTester.cpp : ����Ӧ�ó��������Ϊ��
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


// CPluginTesterApp ����

CPluginTesterApp::CPluginTesterApp()
{
    // TODO: �ڴ˴���ӹ�����룬
    // ��������Ҫ�ĳ�ʼ�������� InitInstance ��
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

// Ψһ��һ�� CPluginTesterApp ����

CPluginTesterApp theApp;


// CPluginTesterApp ��ʼ��

BOOL CPluginTesterApp::InitInstance()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    m_config_path = std::wstring(path) + L".ini";

    LoadConfig();

    //��ʼ����������
    switch (m_language)
    {
    case Language::ENGLISH:
        SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
        break;
    case Language::SIMPLIFIED_CHINESE:
        SetThreadUILanguage(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED));
        break;
    case Language::RUSSIAN:
        SetThreadUILanguage(MAKELANGID(LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA));
        break;
    default:
        break;
    }

    CWinApp::InitInstance();


    // ���� shell ���������Է��Ի������
    // �κ� shell ����ͼ�ؼ��� shell �б���ͼ�ؼ���
    CShellManager *pShellManager = new CShellManager;

    // ���Windows Native���Ӿ����������Ա��� MFC �ؼ�����������
    CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

    // ��׼��ʼ��
    // ���δʹ����Щ���ܲ�ϣ����С
    // ���տ�ִ���ļ��Ĵ�С����Ӧ�Ƴ�����
    // ����Ҫ���ض���ʼ������
    // �������ڴ洢���õ�ע�����
    // TODO: Ӧ�ʵ��޸ĸ��ַ�����
    // �����޸�Ϊ��˾����֯��
    //SetRegistryKey(_T("Ӧ�ó��������ɵı���Ӧ�ó���"));

    //��ʼ��DPI
    HDC hDC = ::GetDC(HWND_DESKTOP);
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(HWND_DESKTOP, hDC);

    m_plugin_icon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_PLUGIN), IMAGE_ICON, DPI(16), DPI(16), 0);

    CPluginTesterDlg dlg;
    m_pMainWnd = &dlg;
    INT_PTR nResponse = dlg.DoModal();
    if (nResponse == IDOK)
    {
        // TODO: �ڴ˷��ô����ʱ��
        //  ��ȷ�������رնԻ���Ĵ���
    }
    else if (nResponse == IDCANCEL)
    {
        // TODO: �ڴ˷��ô����ʱ��
        //  ��ȡ�������رնԻ���Ĵ���
    }
    else if (nResponse == -1)
    {
        TRACE(traceAppMsg, 0, "����: �Ի��򴴽�ʧ�ܣ�Ӧ�ó���������ֹ��\n");
        TRACE(traceAppMsg, 0, "����: ������ڶԻ�����ʹ�� MFC �ؼ������޷� #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS��\n");
    }

    // ɾ�����洴���� shell ��������
    if (pShellManager != NULL)
    {
        delete pShellManager;
    }

    SaveConfig();

    // ���ڶԻ����ѹرգ����Խ����� FALSE �Ա��˳�Ӧ�ó���
    //  ����������Ӧ�ó������Ϣ�á�
    return FALSE;
}



void CPluginTesterApp::OnHelp()
{
    ShellExecute(NULL, _T("open"), _T("https://github.com/zhongyang219/TrafficMonitorPlugins/wiki/%E6%8F%92%E4%BB%B6%E6%B5%8B%E8%AF%95%E5%99%A8"), NULL, NULL, SW_SHOW);
}
