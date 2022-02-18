// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "GP.h"
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

void COptionsDlg::EnableUpdateBtn(bool enable)
{
    ::EnableWindow(GetDlgItem(IDC_UPDATE_BUTTON)->GetSafeHwnd(), enable);
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CODE_EDIT, m_code_edit);
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_EN_CHANGE(IDC_CODE_EDIT, &COptionsDlg::OnChangeCodeEdit)
    ON_BN_CLICKED(IDC_FULL_DAY_CHECK, &COptionsDlg::OnClickedFullDayCheck)
    ON_BN_CLICKED(IDC_UPDATE_BUTTON, &COptionsDlg::OnClickedUpdateButton)
    ON_BN_CLICKED(IDOK, &COptionsDlg::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &COptionsDlg::OnBnClickedCancel)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化

    SetDlgItemText(IDC_CODE_EDIT, g_data.m_setting_data.m_gp_code);

    CheckDlgButton(IDC_FULL_DAY_CHECK, g_data.m_setting_data.m_full_day);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void COptionsDlg::OnChangeCodeEdit()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}


void COptionsDlg::OnClickedFullDayCheck()
{
}


void COptionsDlg::OnClickedUpdateButton()
{
    GP::Instance().SendGPInfoQequest();
}


void COptionsDlg::OnBnClickedOk()
{
    GetDlgItemText(IDC_CODE_EDIT, m_data.m_gp_code);
    m_data.m_full_day = (IsDlgButtonChecked(IDC_FULL_DAY_CHECK) != 0);
    //g_data.SaveConfig();
    CDialog::OnOK();
}


void COptionsDlg::OnBnClickedCancel()
{
    CDialog::OnCancel();
}

