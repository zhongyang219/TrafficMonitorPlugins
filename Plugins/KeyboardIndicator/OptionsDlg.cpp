// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "KeyboardIndicator.h"
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

    CheckDlgButton(IDC_SHOW_CAPS_LOCK_CHECK, m_data.show_caps_lock);
    CheckDlgButton(IDC_SHOW_NUM_LOCK_CHECK, m_data.show_num_lock);
    CheckDlgButton(IDC_SHOW_SCROLL_LOCK_CHECK, m_data.show_scroll_lock);
    CheckDlgButton(IDC_DRAW_ROUND_RECT_CHECK, m_data.draw_round_rect);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void COptionsDlg::OnOK()
{
    m_data.show_caps_lock = (IsDlgButtonChecked(IDC_SHOW_CAPS_LOCK_CHECK) != 0);
    m_data.show_num_lock = (IsDlgButtonChecked(IDC_SHOW_NUM_LOCK_CHECK) != 0);
    m_data.show_scroll_lock = (IsDlgButtonChecked(IDC_SHOW_SCROLL_LOCK_CHECK) != 0);
    m_data.draw_round_rect = (IsDlgButtonChecked(IDC_DRAW_ROUND_RECT_CHECK) != 0);

    if (!m_data.show_caps_lock && !m_data.show_num_lock && !m_data.show_scroll_lock)
    {
        MessageBox(g_data.StringRes(IDS_SELECT_AT_LEAST_WARNING), NULL, MB_ICONWARNING | MB_OK);
        return;
    }

    CDialog::OnOK();
}
