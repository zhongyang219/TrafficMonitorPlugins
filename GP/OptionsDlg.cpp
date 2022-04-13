// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "GP.h"
#include "OptionsDlg.h"
#include "afxdialogex.h"
#include "DataManager.h"
#include "Common.h"

// COptionsDlg 对话框

IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_OPTIONS_DIALOG, pParent)
    , m_radio_gp_types(0)
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
    DDX_Radio(pDX, IDC_RADIO_SZ, m_radio_gp_types);
	DDV_MinMaxInt(pDX, m_radio_gp_types, 0, 5);
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_EN_CHANGE(IDC_CODE_EDIT, &COptionsDlg::OnChangeCodeEdit)
    ON_BN_CLICKED(IDOK, &COptionsDlg::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &COptionsDlg::OnBnClickedCancel)
    ON_BN_CLICKED(IDC_RADIO_SZ, &COptionsDlg::OnRadioClickedGpTypes)
    ON_BN_CLICKED(IDC_RADIO_HK, &COptionsDlg::OnRadioClickedGpTypes)
    ON_BN_CLICKED(IDC_RADIO_BJ, &COptionsDlg::OnRadioClickedGpTypes)
    ON_BN_CLICKED(IDC_RADIO_SH, &COptionsDlg::OnRadioClickedGpTypes)
    ON_BN_CLICKED(IDC_RADIO_GB, &COptionsDlg::OnRadioClickedGpTypes)
    ON_BN_CLICKED(IDC_RADIO_OTHER, &COptionsDlg::OnRadioClickedGpTypes)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化

    SetDlgItemText(IDC_CODE_EDIT, m_gp_code);

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


void COptionsDlg::OnBnClickedOk()
{
    CString code;
    GetDlgItemText(IDC_CODE_EDIT, code);
    if (code.IsEmpty())
    {
        CDialog::OnCancel();
    }
    CString type = "";
    switch (m_radio_gp_types)
    {
    case 0:
        type = kSZ;
        break;
    case 1:
        type = kHK;
        break;
    case 2:
        type = kBJ;
        break;
    case 3:
        type = kSH;
        break;
    case 4:
        type = kMG;
        break;
    }
    m_gp_code = "";
    m_gp_code.Append(type);
    m_gp_code.Append(code);
    CDialog::OnOK();
}


void COptionsDlg::OnBnClickedCancel()
{
    CDialog::OnCancel();
}



void COptionsDlg::OnRadioClickedGpTypes()
{
    UpdateData(TRUE);
    Log1("OnRadioClickedGpTypes: %d\n", m_radio_gp_types);
}
