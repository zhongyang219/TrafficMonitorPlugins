// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "LoudnessMeter.h"
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
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetIcon(g_data.GetIcon(IDI_ICON1), FALSE);

    CheckDlgButton(IDC_SHOW_DB_CHECK, m_data.show_db);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void COptionsDlg::OnOK()
{
    m_data.show_db = (IsDlgButtonChecked(IDC_SHOW_DB_CHECK) != 0);

    CDialog::OnOK();
}
