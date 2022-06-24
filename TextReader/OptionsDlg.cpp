// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "TextReader.h"
#include "OptionsDlg.h"
#include "DataManager.h"

// COptionsDlg 对话框

IMPLEMENT_DYNAMIC(COptionsDlg, CDialogEx)

COptionsDlg::COptionsDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_OPTIONS_DIALOG, pParent)
{
}

COptionsDlg::~COptionsDlg()
{
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BROWSE_BUTTON, &COptionsDlg::OnBnClickedBrowseButton)
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // TODO:  在此添加额外的初始化
    SetBackgroundColor(RGB(255, 255, 255));

    m_file_path_ori = m_data.file_path;

    SetDlgItemText(IDC_FILE_PATH_EDIT, m_data.file_path.c_str());
    CString str;
    str.Format(_T("%d"), m_data.current_position);
    SetDlgItemText(IDC_READ_POSITION_EDIT, str);
    str.Format(_T("%d"), m_data.window_width);
    SetDlgItemText(IDC_WINDOW_WIDTH_EDIT, str);
    str.Format(_T("%d"), g_data.GetTextContexts().size());
    SetDlgItemText(IDC_TOTAL_CHAR_STATIC, str);
    str.Format(_T("(%.2f%%)"), static_cast<double>(m_data.current_position) * 100 / g_data.GetTextContexts().size());
    SetDlgItemText(IDC_PERCENT_STATIC, str);
    CheckDlgButton(IDC_ENABLE_MULTI_LINE_CHECK, m_data.enable_mulit_line);
    CheckDlgButton(IDC_HIDE_WHEN_LOSE_FOCUS_CHECK, m_data.hide_when_lose_focus);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void COptionsDlg::OnBnClickedBrowseButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CFileDialog fileDlg(TRUE, _T("txt"), NULL, 0, g_data.StringRes(IDS_FILTER), this);
    if (fileDlg.DoModal() == IDOK)
    {
        m_data.file_path = fileDlg.GetPathName().GetString();
        SetDlgItemText(IDC_FILE_PATH_EDIT, m_data.file_path.c_str());
        if (m_data.file_path != m_file_path_ori)
        {
            SetDlgItemText(IDC_READ_POSITION_EDIT, _T("0"));    //打开新文件时，将阅读进度置为0
        }
    }

}


void COptionsDlg::OnOK()
{
    // TODO: 在此添加专用代码和/或调用基类

    //载入文本
    g_data.LoadTextContents(m_data.file_path.c_str());
    CString current_position_str;
    GetDlgItemText(IDC_READ_POSITION_EDIT, current_position_str);
    m_data.current_position = _ttoi(current_position_str.GetString());
    CString window_width_str;
    GetDlgItemText(IDC_WINDOW_WIDTH_EDIT, window_width_str);
    m_data.window_width = _ttoi(window_width_str);
    m_data.enable_mulit_line = (IsDlgButtonChecked(IDC_ENABLE_MULTI_LINE_CHECK) != 0);
    m_data.hide_when_lose_focus = (IsDlgButtonChecked(IDC_HIDE_WHEN_LOSE_FOCUS_CHECK) != 0);

    CDialogEx::OnOK();
}
