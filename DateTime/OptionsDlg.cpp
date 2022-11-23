// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "DateTime.h"
#include "OptionsDlg.h"
#include "afxdialogex.h"
#include "MessageDlg.h"


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
    ON_EN_CHANGE(IDC_TIME_FORMAT_EDIT, &COptionsDlg::OnEnChangeTimeFormatEdit)
    ON_EN_CHANGE(IDC_DATE_FORMAT_EDIT, &COptionsDlg::OnEnChangeDateFormatEdit)
    ON_WM_GETMINMAXINFO()
    ON_BN_CLICKED(IDC_HELP_BUTTON, &COptionsDlg::OnBnClickedHelpButton)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    HICON hIcon = g_data.GetIcon(IDI_ICON1);
    SetIcon(hIcon, FALSE);

    //获取初始时的大小
    CRect rect;
    GetWindowRect(rect);
    m_min_size = rect.Size();

    //初始化控件状态
    SetDlgItemText(IDC_TIME_FORMAT_EDIT, m_data.time_format.c_str());
    SetDlgItemText(IDC_DATE_FORMAT_EDIT, m_data.date_format.c_str());

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void COptionsDlg::OnEnChangeTimeFormatEdit()
{
    // TODO:  如果该控件是 RICHEDIT 控件，它将不
    // 发送此通知，除非重写 CDialog::OnInitDialog()
    // 函数并调用 CRichEditCtrl().SetEventMask()，
    // 同时将 ENM_CHANGE 标志“或”运算到掩码中。

    // TODO:  在此添加控件通知处理程序代码
    CString str;
    GetDlgItemText(IDC_TIME_FORMAT_EDIT, str);
    m_data.time_format = str;
    CString time_preview = g_data.m_format_helper.GetCurrentDateTimeByFormat(str);
    SetDlgItemText(IDC_TIME_PREVIEW_STATIC, time_preview);
}


void COptionsDlg::OnEnChangeDateFormatEdit()
{
    // TODO:  如果该控件是 RICHEDIT 控件，它将不
    // 发送此通知，除非重写 CDialog::OnInitDialog()
    // 函数并调用 CRichEditCtrl().SetEventMask()，
    // 同时将 ENM_CHANGE 标志“或”运算到掩码中。

    // TODO:  在此添加控件通知处理程序代码
    CString str;
    GetDlgItemText(IDC_DATE_FORMAT_EDIT, str);
    m_data.date_format = str;
    CString date_preview = g_data.m_format_helper.GetCurrentDateTimeByFormat(str);
    SetDlgItemText(IDC_DATE_PREVIEW_STATIC, date_preview);
}


void COptionsDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    //限制窗口最小大小
    lpMMI->ptMinTrackSize.x = m_min_size.cx;
    lpMMI->ptMinTrackSize.y = m_min_size.cy;

    CDialog::OnGetMinMaxInfo(lpMMI);
}


void COptionsDlg::OnBnClickedHelpButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CMessageDlg dlg(this);
    dlg.SetTitle(g_data.StringRes(IDS_HELP));
    dlg.SetText(g_data.StringRes(IDS_FORMAT_HELP));
    dlg.DoModal();
}
