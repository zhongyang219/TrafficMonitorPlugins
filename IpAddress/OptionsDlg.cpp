// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "IpAddress.h"
#include "OptionsDlg.h"
#include "afxdialogex.h"
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
}

void COptionsDlg::InitConnectionsCombobox()
{
    m_connections_combo.ResetContent();
    //填充下拉列表
    for (const auto& connection : g_data.GetAllConnections())
    {
        m_connections_combo.AddString(connection.description.c_str());
    }
    //设置选择项
    int index{};
    for (int i = 0; i < static_cast<int>(g_data.GetAllConnections().size()); i++)
    {
        if (m_data.current_connection_name == g_data.GetAllConnections()[i].description)
        {
            index = i;
            break;
        }
    }
    m_connections_combo.SetCurSel(index);
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_BN_CLICKED(IDC_BUTTON1, &COptionsDlg::OnBnClickedButton1)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    //初始化下拉列表
    InitConnectionsCombobox();

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void COptionsDlg::OnOK()
{
    CString str;
    m_connections_combo.GetWindowText(str);
    m_data.current_connection_name = str.GetString();

    CDialog::OnOK();
}


void COptionsDlg::OnBnClickedButton1()
{
    g_data.UpdateConnections();
    InitConnectionsCombobox();
}
