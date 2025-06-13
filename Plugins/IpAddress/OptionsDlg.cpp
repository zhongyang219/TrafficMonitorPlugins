// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "IpAddress.h"
#include "OptionsDlg.h"
#include "DataManager.h"

// COptionsDlg 对话框

IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_OPTIONS_DIALOG, pParent)
{
}

COptionsDlg::~COptionsDlg()
{
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CONNECTIONS_COMBO, m_connections_combo);
    DDX_Control(pDX, IDC_IP_QUERY_INTERVAL_SPIN, m_ip_query_interval_spin);
    DDX_Text(pDX, IDC_IP_QUERY_INTERVAL_EDIT, m_data.ip_query_interval);
    DDX_Control(pDX, IDC_IP_PROVIDER_COMBO, m_ip_provider_combo);
}

void COptionsDlg::InitConnectionsCombobox()
{
    m_connections_combo.ResetContent();
    //填充下拉列表
    for (const auto& pair : g_data.GetAllConnections())
    {
        m_connections_combo.AddString(pair.first.c_str());
    }
    //设置选择项
    m_connections_combo.SelectString(-1, m_data.current_connection_name.c_str());
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_BN_CLICKED(IDC_BUTTON1, &COptionsDlg::OnBnClickedButton1)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(g_data.GetIcon(IDI_IP_ADDRESS), FALSE);

    //初始化下拉列表
    InitConnectionsCombobox();

    m_ip_query_interval_spin.SetRange(1, 3600);
    m_ip_query_interval_spin.SetPos(m_data.ip_query_interval);

    for (const auto& pair : g_data.GetIpProviders())
    {
        m_ip_provider_combo.AddString(pair.first.c_str());
    }
    m_ip_provider_combo.SelectString(-1, m_data.ip_provider_name.c_str());

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void COptionsDlg::OnOK()
{
    CString str;
    m_connections_combo.GetWindowText(str);
    m_data.current_connection_name = str.GetString();

    m_data.ip_query_interval = GetDlgItemInt(IDC_IP_QUERY_INTERVAL_EDIT);

    m_ip_provider_combo.GetWindowText(str);
    m_data.ip_provider_name = str.GetString();

    CDialog::OnOK();
}


void COptionsDlg::OnBnClickedButton1()
{
    g_data.UpdateConnections();
    InitConnectionsCombobox();
}
