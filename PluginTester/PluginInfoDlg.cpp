// PluginInfoDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "PluginInfoDlg.h"
#include "PluginTester.h"
#include "..\utilities\FilePathHelper.h"

// CPluginInfoDlg 对话框

IMPLEMENT_DYNAMIC(CPluginInfoDlg, CDialog)

CPluginInfoDlg::CPluginInfoDlg(PluginInfo plugin_info, CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_DETAIL_DIALOG, pParent), m_plugin_info(plugin_info)
{
}

CPluginInfoDlg::~CPluginInfoDlg()
{
}

void CPluginInfoDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_INFO_LIST1, m_info_list);
}

void CPluginInfoDlg::ShowInfo()
{
    m_info_list.SetItemText(RI_NAME, 1, m_plugin_info.Property(ITMPlugin::TMI_NAME).c_str());
    m_info_list.SetItemText(RI_DESCRIPTION, 1, m_plugin_info.Property(ITMPlugin::TMI_DESCRIPTION).c_str());
    m_info_list.SetItemText(RI_FILE_NAME, 1, utilities::CFilePathHelper(m_plugin_info.file_path).GetFileName().c_str());
    m_info_list.SetItemText(RI_FILE_PATH, 1, m_plugin_info.file_path.c_str());
    m_info_list.SetItemText(RI_ITEM_NUM, 1, std::to_wstring(m_plugin_info.plugin_items.size()).c_str());
    std::wstring item_names;
    std::wstring item_id;
    for (const auto& item : m_plugin_info.plugin_items)
    {
        item_names += item->GetItemName();
        item_names += L";";
        item_id += item->GetItemId();
        item_id += L";";
    }
    if (!m_plugin_info.plugin_items.empty())
    {
        item_names.pop_back();
        item_id.pop_back();
    }
    m_info_list.SetItemText(RI_ITEM_NAMES, 1, item_names.c_str());
    m_info_list.SetItemText(RI_ITEM_ID, 1, item_id.c_str());
    m_info_list.SetItemText(RI_AUTHOR, 1, m_plugin_info.Property(ITMPlugin::TMI_AUTHOR).c_str());
    m_info_list.SetItemText(RI_COPYRIGHT, 1, m_plugin_info.Property(ITMPlugin::TMI_COPYRIGHT).c_str());
    m_info_list.SetItemText(RI_URL, 1, m_plugin_info.Property(ITMPlugin::TMI_URL).c_str());
    m_info_list.SetItemText(RI_VERSION, 1, m_plugin_info.Property(ITMPlugin::TMI_VERSION).c_str());
    if (m_plugin_info.plugin != nullptr)
        m_info_list.SetItemText(RI_API_VERSION, 1, std::to_wstring(m_plugin_info.plugin->GetAPIVersion()).c_str());
}


BEGIN_MESSAGE_MAP(CPluginInfoDlg, CDialog)
END_MESSAGE_MAP()


// CPluginInfoDlg 消息处理程序


BOOL CPluginInfoDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化
    HICON hIcon{};
    if (m_plugin_info.plugin != nullptr && m_plugin_info.plugin->GetAPIVersion() >= 5)
        hIcon = (HICON)m_plugin_info.plugin->GetPluginIcon();
    if (hIcon == nullptr)
        hIcon = theApp.m_plugin_icon;
    SetIcon(hIcon, FALSE);

    //初始化列表控件
    CRect rect;
    m_info_list.GetClientRect(rect);
    m_info_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
    int width0, width1;
    width0 = rect.Width() / 4;
    width1 = rect.Width() - width0 - theApp.DPI(20) - 1;
    m_info_list.InsertColumn(0, CPluginTesterApp::LoadText(IDS_ITEM), LVCFMT_LEFT, width0);
    m_info_list.InsertColumn(1, CPluginTesterApp::LoadText(IDS_VALUE), LVCFMT_LEFT, width1);

    //向列表中插入行
    for (int i = 0; i < RI_MAX; i++)
    {
        m_info_list.InsertItem(i, GetRowName(i));
    }

    //显示列表中的信息
    ShowInfo();

    m_info_list.GetToolTips()->SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

    //m_menu.LoadMenu(IDR_INFO_MENU); //装载右键菜单

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


CString CPluginInfoDlg::GetRowName(int row_index)
{
    switch (row_index)
    {
    case CPluginInfoDlg::RI_NAME:
        return CPluginTesterApp::LoadText(IDS_NAME);
    case CPluginInfoDlg::RI_DESCRIPTION:
        return CPluginTesterApp::LoadText(IDS_DESCRIPTION);
    case CPluginInfoDlg::RI_FILE_NAME:
        return CPluginTesterApp::LoadText(IDS_FILE_NAME);
    case CPluginInfoDlg::RI_FILE_PATH:
        return CPluginTesterApp::LoadText(IDS_FILE_PATH);
    case CPluginInfoDlg::RI_ITEM_NUM:
        return CPluginTesterApp::LoadText(IDS_ITEM_NUM);
    case CPluginInfoDlg::RI_ITEM_NAMES:
        return CPluginTesterApp::LoadText(IDS_ITEM_NAMES);
    case CPluginInfoDlg::RI_ITEM_ID:
        return CPluginTesterApp::LoadText(IDS_DISP_ITEM_ID);
    case CPluginInfoDlg::RI_AUTHOR:
        return CPluginTesterApp::LoadText(IDS_AUTHOR);
    case CPluginInfoDlg::RI_COPYRIGHT:
        return CPluginTesterApp::LoadText(IDS_COPYRIGHT);
    case CPluginInfoDlg::RI_URL:
        return CPluginTesterApp::LoadText(IDS_URL);
    case CPluginInfoDlg::RI_VERSION:
        return CPluginTesterApp::LoadText(IDS_VERSION);
    case CPluginInfoDlg::RI_API_VERSION:
        return CPluginTesterApp::LoadText(IDS_PLUGIN_API_VERSION);
    default:
        break;
    }
    return CString();
}
