// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "Stock.h"
#include "OptionsDlg.h"
#include "afxdialogex.h"
#include "DataManager.h"
#include "Common.h"

// COptionsDlg 对话框

IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(const std::wstring& code, CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_OPTIONS_DIALOG, pParent)
    , m_stock_code(code.c_str())
    , m_radio_stock_types(0)
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
    DDX_Radio(pDX, IDC_RADIO_SZ, m_radio_stock_types);
    DDV_MinMaxInt(pDX, m_radio_stock_types, 0, 5);
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_EN_CHANGE(IDC_CODE_EDIT, &COptionsDlg::OnChangeCodeEdit)
    ON_BN_CLICKED(IDOK, &COptionsDlg::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &COptionsDlg::OnBnClickedCancel)
    ON_BN_CLICKED(IDC_RADIO_SZ, &COptionsDlg::OnRadioClickedStockTypes)
    ON_BN_CLICKED(IDC_RADIO_HK, &COptionsDlg::OnRadioClickedStockTypes)
    ON_BN_CLICKED(IDC_RADIO_BJ, &COptionsDlg::OnRadioClickedStockTypes)
    ON_BN_CLICKED(IDC_RADIO_SH, &COptionsDlg::OnRadioClickedStockTypes)
    ON_BN_CLICKED(IDC_RADIO_GB, &COptionsDlg::OnRadioClickedStockTypes)
    ON_BN_CLICKED(IDC_RADIO_OTHER, &COptionsDlg::OnRadioClickedStockTypes)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    //设置标题
    if (m_stock_code.IsEmpty())
        SetWindowText(g_data.StringRes(IDS_ADD_STOCK));
    else
        SetWindowText(g_data.StringRes(IDS_EDIT_STOCK));

    if (!m_stock_code.IsEmpty())
    {
        CString type = GetCodeType(m_stock_code);
        if (type == kSZ)
            m_radio_stock_types = 0;
        else if (type == kHK)
            m_radio_stock_types = 1;
        else if (type == kBJ)
            m_radio_stock_types = 2;
        else if (type == kSH)
            m_radio_stock_types = 3;
        else if (type == kMG)
            m_radio_stock_types = 4;
        else
            m_radio_stock_types = 5;
        UpdateData(FALSE);
    }

    RemoveTypeFromCode(m_stock_code);
    SetDlgItemText(IDC_CODE_EDIT, m_stock_code);

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

void COptionsDlg::RemoveTypeFromCode(CString& code)
{
    for (const auto& type : StockTypeSet)
    {
        if (code.Left(type.GetLength()) == type)
        {
            code = code.Right(code.GetLength() - type.GetLength());
            return;
        }
    }
}

CString COptionsDlg::GetCodeType(const CString & code)
{
    for (const auto& type : StockTypeSet)
    {
        if (code.Left(type.GetLength()) == type)
        {
            return type;
        }
    }
    return CString();
}

void COptionsDlg::OnBnClickedOk()
{
    CString code;
    GetDlgItemText(IDC_CODE_EDIT, code);
    if (code.IsEmpty())
    {
        CDialog::OnCancel();
        return;
    }
    CString type = "";
    switch (m_radio_stock_types)
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
    RemoveTypeFromCode(code);
    m_stock_code = type + code;
    CDialog::OnOK();
}


void COptionsDlg::OnBnClickedCancel()
{
    CDialog::OnCancel();
}



void COptionsDlg::OnRadioClickedStockTypes()
{
    UpdateData(TRUE);
    Log1("OnRadioClickedStockTypes: %d\n", m_radio_stock_types);
}
